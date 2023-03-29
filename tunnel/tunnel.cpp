/*
 * MIT License
 *
 * Copyright (c) 2023 phit666
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

 /** @file tunnel.cpp
	A tunnel system capable of multiple tunnels with yaml config for Tunnel Proxy.
 */

#include "common.h"
#include <yaml-cpp/yaml.h>

struct _BEVINFO
{
	struct bufferevent* proxy_bev;
	struct bufferevent* tunnel_bev;
};

struct bufferevent* le_connect(DWORD ip, WORD port, int index);
static void signal_handler(int signal);
static void le_proxy_readcb(struct bufferevent*, void*);
static void le_tunnel_readcb(struct bufferevent*, void*);
static void le_manage_readcb(struct bufferevent*, void*);
static void le_eventcb(struct bufferevent*, short, void*);
static void le_manage_eventcb(struct bufferevent*, short, void*);
static void le_timercb(evutil_socket_t, short, void*);

struct event_base* base;

static _BEVINFO* getbevinfomap(intptr_t fd_index);
static void delbevmap(intptr_t fd_index);
static std::map <intptr_t, _BEVINFO*> gBevMap;

struct _TunnelsInfo
{
	_TunnelsInfo()
	{
		memset(name, 0, sizeof(name));
		timeout = NULL;
		memset(manageip, 0, sizeof(manageip));
		manageport = -1;
		tunnelport = -1;
		memset(local_serverip, 0, sizeof(local_serverip));
		local_serverport = -1;
		manage_bev = NULL;
		manager_tick = 0;
	}

	struct event* timeout;
	char name[50];
	char manageip[16];
	int manageport;
	int tunnelport;
	char local_serverip[16];
	int local_serverport;
	struct bufferevent* manage_bev;
	unsigned long long manager_tick;
};

static std::vector< _TunnelsInfo*> vTunnels;

int main()
{
	struct timeval tv;

	std::signal(SIGINT, signal_handler);

	gBevMap.clear();

#ifdef _WIN32
	SYSTEM_INFO SystemInfo;
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);

	GetSystemInfo(&SystemInfo);
	evthread_use_windows_threads();

	event_config* pConfig = event_config_new();
	event_config_set_flag(pConfig, EVENT_BASE_FLAG_STARTUP_IOCP);
	event_config_set_num_cpus_hint(pConfig, SystemInfo.dwNumberOfProcessors);
	base = event_base_new_with_config(pConfig);
	event_config_free(pConfig);
#else
	base = event_base_new();
#endif

	msglog(eMSGTYPE::INFO, "Tunnel Proxy %d.%d.%d.%s, socket backend is %s.",
		TUNNEL_PROXY_VER_MAJOR,
		TUNNEL_PROXY_VER_MINOR,
		TUNNEL_PROXY_VER_REV,
		TUNNEL_PROXY_VER_STG,
		event_base_get_method(base));


	try {

		YAML::Node configs = YAML::LoadFile("tunnel.yaml");

		if (configs["Debug Message"].as<bool>() == true) {
			LOGTYPEENABLED |= eMSGTYPE::DEBUG;
		}

		YAML::Node tunnellist = configs["Tunnel Servers"];

		YAML::iterator iter = tunnellist.begin();
		while (iter != tunnellist.end()) {
			const YAML::Node& _tunnelinfo = *iter;
			_TunnelsInfo* tunnelinfo = new _TunnelsInfo;
			memcpy(tunnelinfo->name, _tunnelinfo["Name"].as<std::string>().c_str(), sizeof(tunnelinfo->name));
			tunnelinfo->manageport = _tunnelinfo["Manage Port"].as<int>();
			memcpy(tunnelinfo->manageip, _tunnelinfo["Manage IP"].as<std::string>().c_str(), sizeof(tunnelinfo->local_serverip));
			tunnelinfo->tunnelport = _tunnelinfo["Tunnel Port"].as<int>();
			tunnelinfo->local_serverport = _tunnelinfo["Local Server Port"].as<int>();
			memcpy(tunnelinfo->local_serverip, _tunnelinfo["Local Server IP"].as<std::string>().c_str(), sizeof(tunnelinfo->local_serverip));

			tunnelinfo->timeout = event_new(base, -1, EV_PERSIST, le_timercb, (void*)tunnelinfo);
			evutil_timerclear(&tv);
			tv.tv_sec = 2;
			event_add(tunnelinfo->timeout, &tv);

			vTunnels.push_back(tunnelinfo);
			iter++;
		}
	}
	catch (const YAML::BadFile& e) {
		msglog(eMSGTYPE::ERROR, "YAML error, %s.", e.msg.c_str());
		return -1;
	}
	catch (const YAML::ParserException& e) {
		msglog(eMSGTYPE::ERROR, "YAML error, %s.", e.msg.c_str());
		return -1;
	}

	event_base_dispatch(base);

	std::vector<_TunnelsInfo*>::iterator viter = vTunnels.begin();
	while (viter != vTunnels.end()) {
		_TunnelsInfo* _tunneninfo = *viter;
		event_del(_tunneninfo->timeout);
		event_free(_tunneninfo->timeout);

		if (_tunneninfo->manage_bev != NULL)
			bufferevent_free(_tunneninfo->manage_bev);

		delete _tunneninfo;
		viter++;
	}

	vTunnels.clear();


	std::map <intptr_t, _BEVINFO*>::iterator iter;
	for (iter = gBevMap.begin(); iter != gBevMap.end(); iter++) {
		if (iter->second->proxy_bev != NULL)
			bufferevent_free(iter->second->proxy_bev);
		if (iter->second->tunnel_bev != NULL)
			bufferevent_free(iter->second->tunnel_bev);

		delete iter->second;

		msglog(eMSGTYPE::DEBUG, "Exit cleanup, deleted map index %d.", (int)iter->first);

	}

	gBevMap.clear();


	event_base_free(base);

#ifdef _WIN32
	WSACleanup();
#endif

	msglog(eMSGTYPE::DEBUG, "Memory cleanup done.");

	return 0;
}

struct bufferevent* le_connect(DWORD ip, WORD port, int index)
{
	struct sockaddr_in remote_address;
	int result;

	struct bufferevent* _bev = bufferevent_socket_new(base, -1,
		BEV_OPT_CLOSE_ON_FREE
#ifdef _WIN32
		| BEV_OPT_THREADSAFE
#endif
	);

	if (!_bev) {
		msglog(eMSGTYPE::ERROR, "bufferevent_socket_new failed, %s (%d).", __func__, __LINE__);
		return NULL;
	}

	remote_address.sin_family = AF_INET;
	remote_address.sin_addr.s_addr = htonl(ip);
	remote_address.sin_port = htons(port);

	result = bufferevent_socket_connect(_bev, (struct sockaddr*)&remote_address, sizeof(remote_address));

	if (result == -1) {
		msglog(eMSGTYPE::ERROR, "bufferevent_socket_connect failed, %s (%d).", __func__, __LINE__);
		return NULL;
	}

	bufferevent_enable(_bev, EV_WRITE);
	bufferevent_enable(_bev, EV_READ);
	return _bev;
}

static void
le_manage_readcb(struct bufferevent* _bev, void* user_data)
{
	_TunnelsInfo* tunnelinfo = (_TunnelsInfo*)user_data;

	char buffer[8192] = { 0 };
	int len = bufferevent_read(_bev, (char*)buffer, 8192);

	if (len < 2)
		return;

	_PckCmd* lpMsg = (_PckCmd*)buffer;

	if (lpMsg->cmd == eREQTYPE::CREATE_TUNNEL) {

		msglog(eMSGTYPE::DEBUG, "%s proxy requested for %d tunnels.", tunnelinfo->name, lpMsg->data);

		// lets spawn the requested idle tunnels
		for (int i = 0; i < lpMsg->data; i++) {

			struct bufferevent* _bev1 = le_connect(host2ip(tunnelinfo->manageip), tunnelinfo->tunnelport, 1);

			if (_bev1 == NULL) {
				msglog(eMSGTYPE::ERROR, "%s le_connect failed, %s (%d).", tunnelinfo->name, __func__, __LINE__);
				return;
			}

			struct bufferevent* _bev2 = le_connect(host2ip(tunnelinfo->local_serverip), tunnelinfo->local_serverport, 2);

			if (_bev2 == NULL) {
				msglog(eMSGTYPE::ERROR, "%s le_connect failed, %s (%d).", tunnelinfo->name, __func__, __LINE__);
				bufferevent_free(_bev1);
				return;
			}

			intptr_t fd_index = bufferevent_getfd(_bev1);
			_BEVINFO* bevinfo = new _BEVINFO;
			bevinfo->proxy_bev = _bev1;
			bevinfo->tunnel_bev = _bev2;
			gBevMap[fd_index] = bevinfo;

			// bridge the two sockets
			bufferevent_setcb(_bev1, le_proxy_readcb, NULL, le_eventcb, (void*)fd_index);
			bufferevent_setcb(_bev2, le_tunnel_readcb, NULL, le_eventcb, (void*)fd_index);
		}
	}
	else if (lpMsg->cmd == eREQTYPE::KEEP_ALIVE && lpMsg->data == 2) {
		tunnelinfo->manager_tick = GetTickCount64();
		//msglog(eMSGTYPE::DEBUG, "Tunnel received keep alive packet.");
	}
}

static void
le_tunnel_readcb(struct bufferevent* _bev, void* user_data)
{
	intptr_t fd_index = (intptr_t)user_data;
	_BEVINFO* bevinfo = getbevinfomap(fd_index);
	if (bevinfo == NULL)
		return;
	if (bufferevent_read_buffer(_bev, bufferevent_get_output(bevinfo->proxy_bev)) == -1) {
		msglog(eMSGTYPE::ERROR, "bufferevent_read_buffer failed, %s (%d).", __func__, __LINE__);
		return;
	}
}

static void
le_proxy_readcb(struct bufferevent* _bev, void* user_data)
{
	intptr_t fd_index = (intptr_t)user_data;
	_BEVINFO* bevinfo = getbevinfomap(fd_index);
	if (bevinfo == NULL)
		return;
	if (bufferevent_read_buffer(_bev, bufferevent_get_output(bevinfo->tunnel_bev)) == -1) {
		msglog(eMSGTYPE::ERROR, "bufferevent_read_buffer Error, %s (%d).", __func__, __LINE__);
		return;
	}
}

static void le_timercb(evutil_socket_t fd, short event, void* arg)
{
	_TunnelsInfo* tunnelinfo = (_TunnelsInfo*)arg;

	if (tunnelinfo->manage_bev != NULL && GetTickCount64() - tunnelinfo->manager_tick >= 10000) {
		bufferevent_free(tunnelinfo->manage_bev);
		tunnelinfo->manage_bev = NULL;
		msglog(eMSGTYPE::DEBUG, "%s proxy manager connection deleted, idled for 10 sec.", tunnelinfo->name);
	}

	if (tunnelinfo->manage_bev == NULL) {
		tunnelinfo->manage_bev = le_connect(host2ip(tunnelinfo->manageip), tunnelinfo->manageport, 0);
		if (tunnelinfo->manage_bev == NULL) {
			msglog(eMSGTYPE::ERROR, "%s le_connect failed, %s (%d).", tunnelinfo->name, __func__, __LINE__);
			return;
		}
		bufferevent_setcb(tunnelinfo->manage_bev, le_manage_readcb, NULL, le_manage_eventcb, arg);
	}
	else {
		_PckCmd pMsg = { 0 };
		pMsg.head = 0xC1;
		pMsg.len = sizeof(pMsg);
		pMsg.cmd = 0xA2;
		pMsg.data = 1;
		if (bufferevent_write(tunnelinfo->manage_bev, (unsigned char*)&pMsg, pMsg.len) == -1) {
			msglog(eMSGTYPE::ERROR, "%s bufferevent_write failed, %s (%d).", tunnelinfo->name, __func__, __LINE__);
		}
		//msglog(eMSGTYPE::DEBUG, "Tunnel sent keep alive packet.");
	}
}

static void
le_manage_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	_TunnelsInfo* tunnelinfo = (_TunnelsInfo*)user_data;

	if (events & BEV_EVENT_EOF)
	{
		bufferevent_free(tunnelinfo->manage_bev);
		tunnelinfo->manage_bev = NULL;
		msglog(eMSGTYPE::DEBUG, "%s disconnected from proxy manager (EOF).", tunnelinfo->name);
	}
	else if (events & BEV_EVENT_ERROR)
	{
		bufferevent_free(tunnelinfo->manage_bev);
		tunnelinfo->manage_bev = NULL;
		msglog(eMSGTYPE::DEBUG, "%s disconnected from proxy manager (ERROR).", tunnelinfo->name);
	}
	else if (events & BEV_EVENT_CONNECTED)
	{
		tunnelinfo->manager_tick = GetTickCount64();
		msglog(eMSGTYPE::DEBUG, "%s connected to proxy manager.", tunnelinfo->name);
	}
}

static void
le_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	intptr_t fd_index = (intptr_t)user_data;
	_BEVINFO* bevinfo = getbevinfomap(fd_index);
	if (events & BEV_EVENT_EOF || events & BEV_EVENT_ERROR)
	{
		if (bevinfo == NULL) {
			bufferevent_free(bev);
			return;
		}
		if (bevinfo->proxy_bev != NULL)
			bufferevent_free(bevinfo->proxy_bev);
		if (bevinfo->tunnel_bev != NULL)
			bufferevent_free(bevinfo->tunnel_bev);
		delete bevinfo;
		delbevmap(fd_index);

		msglog(eMSGTYPE::DEBUG, "Deleted tunnel index %d.", (int)fd_index);

	}
	else if (events & BEV_EVENT_CONNECTED)
	{
	}
}


static _BEVINFO* getbevinfomap(intptr_t fd_index) {
	std::map <intptr_t, _BEVINFO*>::iterator iter;
	iter = gBevMap.find(fd_index);
	if (iter != gBevMap.end()) {
		return iter->second;
	}
	return NULL;
}

static void delbevmap(intptr_t fd_index) {
	std::map <intptr_t, _BEVINFO*>::iterator iter;
	iter = gBevMap.find(fd_index);
	if (iter != gBevMap.end()) {
		gBevMap.erase(iter);
	}
}

static void signal_handler(int signal)
{
	event_base_loopbreak(base);
}