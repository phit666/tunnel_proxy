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
	A tunnel system for Tunnel Proxy.
 */

#include "common.h"


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
static struct bufferevent* manage_bev = NULL;
static unsigned long long manager_tick = 0;

static int portProxy;
static int portServer;
static int portManage;
static char proxyip[50] = { 0 };
static char svrip[50] = { 0 };
static char manageip[50] = { 0 };

static _BEVINFO* getbevinfomap(intptr_t fd_index);
static void delbevmap(intptr_t fd_index);
static std::map <intptr_t, _BEVINFO*> gBevMap;

int main(int argc, char* argv[])
{
	struct evconnlistener* listener;
	struct sockaddr_in sin;
	struct event* timeout;
	struct timeval tv;

	if (argc < 5) {
		std::cout << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << "tunnel <manage ip> <manage port> <tunnel port>  <server-local-ip> <server-local-port>" << std::endl;
		std::cout << std::endl;
		return -1;
	}

	std::signal(SIGINT, signal_handler);

	std::stringstream s;
	s << argv[2]; s >> portManage; s.clear();
	s << argv[3]; s >> portProxy; s.clear();
	s << argv[5]; s >> portServer;
	memcpy(&manageip, argv[1], sizeof(manageip));
	memcpy(&proxyip, argv[1], sizeof(proxyip));
	memcpy(&svrip, argv[4], sizeof(svrip));

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


	timeout = event_new(base, -1, EV_PERSIST, le_timercb, NULL);
	evutil_timerclear(&tv);
	tv.tv_sec = 2;
	event_add(timeout, &tv);

	event_base_dispatch(base);

	event_del(timeout);
	event_free(timeout);

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

	if(manage_bev != NULL)
		bufferevent_free(manage_bev);

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
	char buffer[8192] = { 0 };
	int len = bufferevent_read(_bev, (char*)buffer, 8192);

	if (len < 2)
		return;

	_PckCmd* lpMsg = (_PckCmd*)buffer;

	if (lpMsg->cmd == eREQTYPE::CREATE_TUNNEL) {

		msglog(eMSGTYPE::DEBUG, "proxy requested for %d tunnels.", lpMsg->data);

		// lets spawn the requested idle tunnels
		for (int i = 0; i < lpMsg->data; i++) {

			struct bufferevent* _bev1 = le_connect(host2ip(proxyip), portProxy, 1);

			if (_bev1 == NULL) {
				msglog(eMSGTYPE::ERROR, "le_connect failed, %s (%d).", __func__, __LINE__);
				return;
			}

			struct bufferevent* _bev2 = le_connect(host2ip(svrip), portServer, 2);

			if (_bev2 == NULL) {
				msglog(eMSGTYPE::ERROR, "le_connect failed, %s (%d).", __func__, __LINE__);
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
		manager_tick = GetTickCount64();
		msglog(eMSGTYPE::DEBUG, "Tunnel received keep alive packet.");
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
	if (manage_bev != NULL && GetTickCount64() - manager_tick >= 10000) {
		bufferevent_free(manage_bev);
		manage_bev = NULL;
		msglog(eMSGTYPE::DEBUG, "proxy manager connection deleted, idled for 10 sec.");
	}

	if (manage_bev == NULL) {
		manage_bev = le_connect(host2ip(manageip), portManage, 0);
		if (!manage_bev) {
			msglog(eMSGTYPE::ERROR, "le_connect failed, %s (%d).", __func__, __LINE__);
			return;
		}
		manager_tick = GetTickCount64();
		bufferevent_setcb(manage_bev, le_manage_readcb, NULL, le_manage_eventcb, NULL);
		msglog(eMSGTYPE::DEBUG, "connected to proxy.");
	}
	else {
		_PckCmd pMsg = { 0 };
		pMsg.head = 0xC1;
		pMsg.len = sizeof(pMsg);
		pMsg.cmd = 0xA2;
		pMsg.data = 1;
		if (bufferevent_write(manage_bev, (unsigned char*)&pMsg, pMsg.len) == -1) {
			msglog(eMSGTYPE::ERROR, "bufferevent_write failed, %s (%d).", __func__, __LINE__);
		}
		msglog(eMSGTYPE::DEBUG, "Tunnel sent keep alive packet.");
	}
}

static void
le_manage_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	if (events & BEV_EVENT_EOF || events & BEV_EVENT_ERROR)
	{
		bufferevent_free(manage_bev);
		manage_bev = NULL;
	}
	else if (events & BEV_EVENT_CONNECTED)
	{
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



