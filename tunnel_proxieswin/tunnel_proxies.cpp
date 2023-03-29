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

 /** @file tunnel_proxyies.cpp
	A client proxy system capable of multiple proxies with yaml config for Tunnel Proxy.
 */

#include "common.h"
#include <yaml-cpp/yaml.h>




struct _TunnelProxiesInfo
{
	_TunnelProxiesInfo()
	{
		memset(name, 0, sizeof(name));
		manage_listener = NULL;
		tunnel_listener = NULL;
		proxy_listener = NULL;
		timeout = NULL;
		manage_port = -1;
		tunnel_port = -1;
		proxy_port = -1;
		max_tunnels = 0;
		min_tunnels = 0;
		manage_bev = NULL;
		manager_tick = 0;
		vBevWaitin.clear();
	}
	char name[50];
	struct evconnlistener* manage_listener;
	struct evconnlistener* tunnel_listener;
	struct evconnlistener* proxy_listener;
	struct event* timeout;
	int manage_port;
	int tunnel_port;
	int proxy_port;
	int max_tunnels;
	int min_tunnels;
	struct bufferevent* manage_bev;
	unsigned long long manager_tick;
	std::vector <struct bufferevent*> vBevWaitin;
};

struct _BEVINFO
{
	_BEVINFO()
	{
		proxy_bev = NULL;
		tunnel_bev = NULL;
		tick = 0;
		tunnelproxyinfo = NULL;
	}
	BYTE status;
	struct bufferevent* proxy_bev;
	struct bufferevent* tunnel_bev;
	unsigned long long tick;
	_TunnelProxiesInfo* tunnelproxyinfo;
};

static std::vector< _TunnelProxiesInfo*> vTunnelProxies;
static struct event_base* base;

static void signal_handler(int signal);

static void le_managelistener_cb(struct evconnlistener*, evutil_socket_t,
	struct sockaddr*, int socklen, void*);
static void le_tunnellistener_cb(struct evconnlistener*, evutil_socket_t,
	struct sockaddr*, int socklen, void*);
static void le_proxylistener_cb(struct evconnlistener*, evutil_socket_t,
	struct sockaddr*, int socklen, void*);

static void le_tunnel_readcb(struct bufferevent*, void*);
static void le_proxy_readcb(struct bufferevent*, void*);
static void le_manage_readcb(struct bufferevent*, void*);
static void le_eventcb(struct bufferevent*, short, void*);
static void le_manage_eventcb(struct bufferevent*, short, void*);
static void le_timercb(evutil_socket_t, short, void*);


static _BEVINFO* getbevinfomap(intptr_t fd_index);
static void delbevmap(intptr_t fd_index);
static int countbevmapidle(int port);
static intptr_t getidlemap(int port);
static void flagusebevmap(intptr_t fd_index);
static int reqmanager(struct bufferevent* bev, eREQTYPE reqtype, int data);


static std::map <intptr_t, _BEVINFO*> gBevMap;

int main()
{
	struct sockaddr_in sin;
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

		YAML::Node liclist = YAML::LoadFile("tunnelproxies.yaml");
		YAML::iterator iter = liclist.begin();
		while (iter != liclist.end()) {
			const YAML::Node& licinfo = *iter;

			_TunnelProxiesInfo* tunnelproxyinfo = new _TunnelProxiesInfo;

			memcpy(tunnelproxyinfo->name, licinfo["Proxy Name"].as<std::string>().c_str(), sizeof(tunnelproxyinfo->name));
			tunnelproxyinfo->max_tunnels = licinfo["Max Tunnels"].as<int>();
			tunnelproxyinfo->min_tunnels = licinfo["Min Tunnels"].as<int>();
			tunnelproxyinfo->manage_port = licinfo["Manage Port"].as<int>();
			tunnelproxyinfo->tunnel_port = licinfo["Tunnel Port"].as<int>();
			tunnelproxyinfo->proxy_port = licinfo["Proxy Port"].as<int>();

			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons(tunnelproxyinfo->manage_port);

			tunnelproxyinfo->manage_listener = evconnlistener_new_bind(base, le_managelistener_cb, (void*)tunnelproxyinfo,
				LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
				(struct sockaddr*)&sin,
				sizeof(sin));

			if (!tunnelproxyinfo->manage_listener) {
				msglog(eMSGTYPE::ERROR, "evconnlistener_new_bind failed at port %d, %s (%d).", tunnelproxyinfo->manage_port, __func__, __LINE__);
				event_base_free(base);
				return -1;
			}

			msglog(eMSGTYPE::INFO, "%s Tunnel Proxy is listening to manage port %d.", tunnelproxyinfo->name, tunnelproxyinfo->manage_port);


			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons(tunnelproxyinfo->tunnel_port);

			tunnelproxyinfo->tunnel_listener = evconnlistener_new_bind(base, le_tunnellistener_cb, (void*)tunnelproxyinfo,
				LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
				(struct sockaddr*)&sin,
				sizeof(sin));

			if (!tunnelproxyinfo->tunnel_listener) {
				msglog(eMSGTYPE::ERROR, "evconnlistener_new_bind failed at port %d, %s (%d).", tunnelproxyinfo->tunnel_port, __func__, __LINE__);
				evconnlistener_free(tunnelproxyinfo->manage_listener);
				event_base_free(base);
				return -1;
			}

			msglog(eMSGTYPE::INFO, "%s Tunnel Proxy is listening to tunnel port %d.", tunnelproxyinfo->name, tunnelproxyinfo->tunnel_port);


			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons(tunnelproxyinfo->proxy_port);

			tunnelproxyinfo->proxy_listener = evconnlistener_new_bind(base, le_proxylistener_cb, (void*)tunnelproxyinfo,
				LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
				(struct sockaddr*)&sin,
				sizeof(sin));

			if (!tunnelproxyinfo->proxy_listener) {
				msglog(eMSGTYPE::ERROR, "evconnlistener_new_bind failed at port %d, %s (%d).", tunnelproxyinfo->proxy_port, __func__, __LINE__);
				evconnlistener_free(tunnelproxyinfo->manage_listener);
				evconnlistener_free(tunnelproxyinfo->tunnel_listener);
				event_base_free(base);
				return -1;
			}

			msglog(eMSGTYPE::INFO, "%s Tunnel Proxy is listening to proxy port %d.", tunnelproxyinfo->name, tunnelproxyinfo->proxy_port);

			tunnelproxyinfo->timeout = event_new(base, -1, EV_PERSIST, le_timercb, (void*)tunnelproxyinfo);
			evutil_timerclear(&tv);
			tv.tv_sec = 2;
			event_add(tunnelproxyinfo->timeout, &tv);

			vTunnelProxies.push_back(tunnelproxyinfo);
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

	std::vector <_TunnelProxiesInfo*>::iterator viter;
	for (viter = vTunnelProxies.begin(); viter != vTunnelProxies.end(); viter++) {
		_TunnelProxiesInfo* _tunnelproxyinfo = *viter;
		event_del(_tunnelproxyinfo->timeout);
		event_free(_tunnelproxyinfo->timeout);
		evconnlistener_free(_tunnelproxyinfo->manage_listener);
		evconnlistener_free(_tunnelproxyinfo->tunnel_listener);
		evconnlistener_free(_tunnelproxyinfo->proxy_listener);

		std::vector <struct bufferevent*>::iterator _viter;
		for (_viter = _tunnelproxyinfo->vBevWaitin.begin(); _viter != _tunnelproxyinfo->vBevWaitin.end(); _viter++) {
			if (*_viter != NULL) {
				bufferevent_free(*_viter);
			}
		}

		_tunnelproxyinfo->vBevWaitin.clear();

		if (_tunnelproxyinfo->manage_bev != NULL)
			bufferevent_free(_tunnelproxyinfo->manage_bev);

		delete _tunnelproxyinfo;
	}

	vTunnelProxies.clear();

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

static void le_timercb(evutil_socket_t fd, short event, void* arg)
{
	_TunnelProxiesInfo* tunnelproxyinfo = (_TunnelProxiesInfo*)arg;

	if (tunnelproxyinfo->manage_bev != NULL) {
		if ((GetTickCount64() - tunnelproxyinfo->manager_tick) >= 10000) {
			bufferevent_free(tunnelproxyinfo->manage_bev);
			tunnelproxyinfo->manage_bev = NULL;
			msglog(eMSGTYPE::DEBUG, "%s proxy manager connection deleted, idled for 10 sec.", tunnelproxyinfo->name);
		}
	}

	std::map <intptr_t, _BEVINFO*>::iterator iter = gBevMap.begin();
	while (iter != gBevMap.end()) {
		if (iter->second->status == 1) {
			if ((GetTickCount64() - iter->second->tick) >= 180000) {
				if (iter->second->proxy_bev != NULL)
					bufferevent_free(iter->second->proxy_bev);
				if (iter->second->tunnel_bev != NULL)
					bufferevent_free(iter->second->tunnel_bev);
				msglog(eMSGTYPE::DEBUG, "%s Idle connection (active) deleted, map index %d.", iter->second->tunnelproxyinfo->name, (int)iter->first);
				delete iter->second;
				iter = gBevMap.erase(iter);
				continue;
			}
		}
		else if (iter->second->status == 0) {
			if ((GetTickCount64() - iter->second->tick) >= 60000) {
				if (iter->second->proxy_bev != NULL)
					bufferevent_free(iter->second->proxy_bev);
				if (iter->second->tunnel_bev != NULL)
					bufferevent_free(iter->second->tunnel_bev);
				msglog(eMSGTYPE::DEBUG, "%s Idle connection (waiting) deleted, map index %d.", iter->second->tunnelproxyinfo->name, (int)iter->first);
				delete iter->second;
				iter = gBevMap.erase(iter);
				continue;
			}
		}
		iter++;
	}

	if (tunnelproxyinfo->manage_bev != NULL) {

		int idlecounts = countbevmapidle(tunnelproxyinfo->tunnel_port);
		if (idlecounts < tunnelproxyinfo->min_tunnels) {
			if (reqmanager(tunnelproxyinfo->manage_bev, eREQTYPE::CREATE_TUNNEL, tunnelproxyinfo->max_tunnels - tunnelproxyinfo->min_tunnels) == 0) {
				msglog(eMSGTYPE::DEBUG, "%s Proxy manager requested for %d tunnels.", tunnelproxyinfo->name, tunnelproxyinfo->max_tunnels - tunnelproxyinfo->min_tunnels);
			}
		}

		if (reqmanager(tunnelproxyinfo->manage_bev, eREQTYPE::KEEP_ALIVE, 2) == 0) {
			//msglog(eMSGTYPE::DEBUG, "%s Proxy manager sent keep alive packet.", tunnelproxyinfo->name);
		}
	}
}

static void
le_manage_readcb(struct bufferevent* _bev, void* user_data)
{
	_TunnelProxiesInfo* tunnelproxyinfo = (_TunnelProxiesInfo*)user_data;

	if (tunnelproxyinfo->manage_bev == NULL)
		return;

	char buffer[8192] = { 0 };
	int len = bufferevent_read(_bev, (char*)buffer, 8192);

	if (len < 2)
		return;

	_PckCmd* lpMsg = (_PckCmd*)buffer;

	if (lpMsg->cmd == eREQTYPE::KEEP_ALIVE && lpMsg->data == 1) { // KEEP ALIVE
		tunnelproxyinfo->manager_tick = GetTickCount64();
		//msglog(eMSGTYPE::DEBUG, "%s Proxy manager received keep alive packet.", tunnelproxyinfo->name);
	}
}

static void
le_tunnel_readcb(struct bufferevent* _bev, void* user_data)
{
	intptr_t fd_index = (intptr_t)user_data;
	_BEVINFO* bevinfo = getbevinfomap(fd_index);
	if (bevinfo == NULL || bevinfo->status == 0)
		return;
	if (bufferevent_read_buffer(_bev, bufferevent_get_output(bevinfo->proxy_bev)) == -1) {
		msglog(eMSGTYPE::ERROR, "bufferevent_read_buffer failed, %s (%d).", __func__, __LINE__);
	}
}

static void
le_proxy_readcb(struct bufferevent* _bev, void* user_data)
{
	intptr_t fd_index = (intptr_t)user_data;
	_BEVINFO* bevinfo = getbevinfomap(fd_index);
	if (bevinfo == NULL || bevinfo->status == 0)
		return;

	bevinfo->tick = GetTickCount64();

	if (bufferevent_read_buffer(_bev, bufferevent_get_output(bevinfo->tunnel_bev)) == -1) {
		msglog(eMSGTYPE::ERROR, "bufferevent_read_buffer failed, %s (%d).", __func__, __LINE__);
	}
}

static void le_managelistener_cb(struct evconnlistener* listener, evutil_socket_t fd,
	struct sockaddr* sa, int socklen, void* user_data) {

	_TunnelProxiesInfo* tunnelproxyinfo = (_TunnelProxiesInfo*)user_data;

	int flag = (int)user_data;
	struct bufferevent* tunnel_bev;
	struct bufferevent* proxy_bev;

	tunnelproxyinfo->manage_bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE
#ifdef _WIN32
		| BEV_OPT_THREADSAFE
#endif
	);
	if (!tunnelproxyinfo->manage_bev)
	{
		msglog(eMSGTYPE::ERROR, "%s bufferevent_socket_new failed, %s (%d).", tunnelproxyinfo->name, __func__, __LINE__);
		return;
	}

	msglog(eMSGTYPE::DEBUG, "%s Proxy manager is connected.", tunnelproxyinfo->name);

	bufferevent_setcb(tunnelproxyinfo->manage_bev, le_manage_readcb, NULL, le_manage_eventcb, user_data);
	bufferevent_enable(tunnelproxyinfo->manage_bev, EV_WRITE);
	bufferevent_enable(tunnelproxyinfo->manage_bev, EV_READ);
	tunnelproxyinfo->manager_tick = GetTickCount64();
}

static void le_tunnellistener_cb(struct evconnlistener* listener, evutil_socket_t fd,
	struct sockaddr* sa, int socklen, void* user_data) {

	_TunnelProxiesInfo* tunnelproxyinfo = (_TunnelProxiesInfo*)user_data;

	int flag = (int)user_data;
	struct bufferevent* tunnel_bev;

	tunnel_bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE
#ifdef _WIN32
		| BEV_OPT_THREADSAFE
#endif
	/* | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_UNLOCK_CALLBACKS*/);
	if (!tunnel_bev)
	{
		msglog(eMSGTYPE::ERROR, "%s bufferevent_socket_new failed, %s (%d).", tunnelproxyinfo->name, __func__, __LINE__);
		return;
	}

	intptr_t fd_index = bufferevent_getfd(tunnel_bev);
	_BEVINFO* bevinfo = new _BEVINFO;
	bevinfo->tunnel_bev = tunnel_bev;
	bevinfo->proxy_bev = NULL;
	bevinfo->status = 0;
	bevinfo->tick = GetTickCount64();
	bevinfo->tunnelproxyinfo = tunnelproxyinfo;

	gBevMap[fd_index] = bevinfo;

	bufferevent_setcb(tunnel_bev, le_tunnel_readcb, NULL, le_eventcb, (void*)fd_index);
	bufferevent_enable(tunnel_bev, EV_READ | EV_WRITE);

	// lets connect the first one in the waiting list to this tunnel
	std::vector <struct bufferevent*>::iterator iter;
	for (iter = tunnelproxyinfo->vBevWaitin.begin(); iter != tunnelproxyinfo->vBevWaitin.end(); iter++) {
		if (*iter != NULL) {
			bevinfo->proxy_bev = *iter;
			bevinfo->tick = GetTickCount64();
			bufferevent_setcb(*iter, le_proxy_readcb, NULL, le_eventcb, (void*)fd_index);
			bufferevent_enable(*iter, EV_READ | EV_WRITE);
			bevinfo->status = 1;
			tunnelproxyinfo->vBevWaitin.erase(iter);
			break;
		}
	}
}

static void le_proxylistener_cb(struct evconnlistener* listener, evutil_socket_t fd,
	struct sockaddr* sa, int socklen, void* user_data) {

	_TunnelProxiesInfo* tunnelproxyinfo = (_TunnelProxiesInfo*)user_data;

	int flag = (int)user_data;
	struct bufferevent* proxy_bev;

	proxy_bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE
#ifdef _WIN32
		| BEV_OPT_THREADSAFE
#endif
	);

	if (!proxy_bev)
	{
		msglog(eMSGTYPE::ERROR, "%s bufferevent_socket_new failed, %s (%d).", tunnelproxyinfo->name, __func__, __LINE__);
		return;
	}

	msglog(eMSGTYPE::DEBUG, "%s Client connection accepted...", tunnelproxyinfo->name);

	int idlecountmap = countbevmapidle(tunnelproxyinfo->tunnel_port);

	msglog(eMSGTYPE::DEBUG, "%s Current available tunnel count is %d.", tunnelproxyinfo->name, idlecountmap);

	// check if there is available tunnel
	if (idlecountmap == 0) {
		if (tunnelproxyinfo->manage_bev == NULL) {
			msglog(eMSGTYPE::DEBUG, "%s Proxy manager is not connected, failed to request for new tunnel.", tunnelproxyinfo->name);
		}
		else {
			if (reqmanager(tunnelproxyinfo->manage_bev, eREQTYPE::CREATE_TUNNEL, 1) == 0) {
				msglog(eMSGTYPE::DEBUG, "%s Proxy manager requested for %d tunnels.", tunnelproxyinfo->name, 1);
			}
			// there has no tunnel available atm., lets add this in the waiting list for now...
			tunnelproxyinfo->vBevWaitin.push_back(proxy_bev);
			msglog(eMSGTYPE::DEBUG, "%s There are %d client(s) in the waiting list right now.", tunnelproxyinfo->name, tunnelproxyinfo->vBevWaitin.size());
			return;
		}
	}


	// bridge proxy and tunnel
	intptr_t idleindex = getidlemap(tunnelproxyinfo->tunnel_port);
	if (idleindex == -1)
		return;

	_BEVINFO* bevinfo = getbevinfomap(idleindex);
	bevinfo->proxy_bev = proxy_bev;
	bevinfo->tick = GetTickCount64();
	bufferevent_setcb(proxy_bev, le_proxy_readcb, NULL, le_eventcb, (void*)idleindex);
	bufferevent_enable(proxy_bev, EV_READ | EV_WRITE);
	flagusebevmap(idleindex);
}

static void
le_manage_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	_TunnelProxiesInfo* tunnelproxyinfo = (_TunnelProxiesInfo*)user_data;

	if (events & BEV_EVENT_EOF)
	{
		msglog(eMSGTYPE::DEBUG, "%s Proxy manager is disconnected, event code is BEV_EVENT_EOF.", tunnelproxyinfo->name);
		if (tunnelproxyinfo->manage_bev != NULL)
			bufferevent_free(tunnelproxyinfo->manage_bev);
		tunnelproxyinfo->manage_bev = NULL;
	}
	else if (events & BEV_EVENT_ERROR)
	{
		msglog(eMSGTYPE::DEBUG, "%s Proxy manager is disconnected, event code is BEV_EVENT_ERROR.", tunnelproxyinfo->name);
		if (tunnelproxyinfo->manage_bev != NULL)
			bufferevent_free(tunnelproxyinfo->manage_bev);
		tunnelproxyinfo->manage_bev = NULL;
	}
	else if (events & BEV_EVENT_CONNECTED)
	{
	}
}

static void
le_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	intptr_t fd_index = (intptr_t)user_data;

	if ((events & BEV_EVENT_EOF) || (events & BEV_EVENT_ERROR))
	{
		_BEVINFO* bevinfo = getbevinfomap(fd_index);

		if (bevinfo == NULL) {
			bufferevent_free(bev);
			return;
		}

		if (bevinfo->tunnel_bev != NULL)
			bufferevent_free(bevinfo->tunnel_bev);
		if (bevinfo->proxy_bev != NULL)
			bufferevent_free(bevinfo->proxy_bev);

		delbevmap(fd_index);

		int idlecountmap = countbevmapidle(bevinfo->tunnelproxyinfo->tunnel_port);

		msglog(eMSGTYPE::DEBUG, "%s Deleted map index %d, idle count now is %d.", bevinfo->tunnelproxyinfo->name, (int)fd_index, idlecountmap);
		delete bevinfo;
	}
	else if (events & BEV_EVENT_CONNECTED)
	{
	}
}


static int reqmanager(struct bufferevent* bev, eREQTYPE reqtype, int data) {
	_PckCmd pMsg = { 0 };
	pMsg.head = 0xC1;
	pMsg.len = sizeof(pMsg);
	pMsg.cmd = reqtype;
	pMsg.data = data;
	if (bufferevent_write(bev, (unsigned char*)&pMsg, pMsg.len) == -1) {
		msglog(eMSGTYPE::ERROR, "bufferevent_write failed, %s (%d).", __func__, __LINE__);
		return -1;
	}
	return 0;
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

static void flagusebevmap(intptr_t fd_index) {
	std::map <intptr_t, _BEVINFO*>::iterator iter;
	iter = gBevMap.find(fd_index);
	if (iter != gBevMap.end()) {
		iter->second->status = 1;
	}
}

static int countbevmapidle(int port) {
	int idelctr = 0;
	std::map <intptr_t, _BEVINFO*>::iterator iter;
	for (iter = gBevMap.begin(); iter != gBevMap.end(); iter++) {
		if (iter->second->status == 0 && iter->second->tunnelproxyinfo->tunnel_port == port) {
			idelctr++;
		}
	}
	return idelctr;
}

static intptr_t getidlemap(int port) {
	std::map <intptr_t, _BEVINFO*>::iterator iter;
	for (iter = gBevMap.begin(); iter != gBevMap.end(); iter++) {
		if (iter->second->status == 0 && iter->second->tunnelproxyinfo->tunnel_port == port) {
			return iter->first;
		}
	}
	return -1;
}

static void signal_handler(int signal)
{
	event_base_loopbreak(base);
}
