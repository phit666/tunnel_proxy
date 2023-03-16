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

 /** @file tunnel_proxy.cpp
	A client proxy system for Tunnel Proxy.
 */

#include "common.h"


static int IDLE_TUNNELS = 100;
static int IDLE_TUNNELS_MIN = 90;

struct _BEVINFO
{
	BYTE status;
	struct bufferevent* proxy_bev;
	struct bufferevent* tunnel_bev;
	unsigned long long tick;
};

static struct event_base* base;

static int manage_port = 8020;
static int tunnel_port = 8000;
static int proxy_port = 8123;

static void signal_handler(int signal);
static void le_listener_cb(struct evconnlistener*, evutil_socket_t,
	struct sockaddr*, int socklen, void*);
static void le_tunnel_readcb(struct bufferevent*, void*);
static void le_proxy_readcb(struct bufferevent*, void*);
static void le_manage_readcb(struct bufferevent*, void*);
static void le_eventcb(struct bufferevent*, short, void*);
static void le_manage_eventcb(struct bufferevent*, short, void*);
static void le_timercb(evutil_socket_t, short, void*);


static _BEVINFO* getbevinfomap(intptr_t fd_index);
static void delbevmap(intptr_t fd_index);
static int countbevmapidle();
static intptr_t getidlemap();
static void flagusebevmap(intptr_t fd_index);
static int reqmanager(struct bufferevent* bev, eREQTYPE reqtype, int data);

static struct bufferevent* manage_bev = NULL;
static unsigned long long manager_tick = 0;

static std::map <intptr_t, _BEVINFO*> gBevMap;
static std::vector <struct bufferevent*> gBevWaitin;

int main(int argc, char* argv[])
{
	struct evconnlistener* manage_listener;
	struct evconnlistener* tunnel_listener;
	struct evconnlistener* proxy_listener;

	struct sockaddr_in sin;
	struct event* timeout;
	struct timeval tv;


	if (argc < 5) {
		std::cout << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << "tunnel_proxy <max-tunnel-counts> <min-tunnel-counts> <manage port> <tunnel port> <proxy port>" << std::endl;
		std::cout << std::endl;
		return -1;
	}

	std::stringstream s;
	s << argv[3]; s >> manage_port; s.clear();
	s << argv[4]; s >> tunnel_port; s.clear();
	s << argv[5]; s >> proxy_port; s.clear();
	s << argv[1]; s >> IDLE_TUNNELS; s.clear();
	s << argv[2]; s >> IDLE_TUNNELS_MIN;

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

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(manage_port);

	manage_listener = evconnlistener_new_bind(base, le_listener_cb, (void*)1,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
		(struct sockaddr*)&sin,
		sizeof(sin));

	if (!manage_listener) {
		msglog(eMSGTYPE::ERROR, "evconnlistener_new_bind failed at port %d, %s (%d).", manage_port, __func__, __LINE__);
		event_base_free(base);
		return -1;
	}

	msglog(eMSGTYPE::INFO, "Tunnel Proxy is listening to manage port %d.", manage_port);


	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(tunnel_port);

	tunnel_listener = evconnlistener_new_bind(base, le_listener_cb, (void*)3,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
		(struct sockaddr*)&sin,
		sizeof(sin));

	if (!tunnel_listener) {
		msglog(eMSGTYPE::ERROR, "evconnlistener_new_bind failed at port %d, %s (%d).", tunnel_port, __func__, __LINE__);
		evconnlistener_free(manage_listener);
		event_base_free(base);
		return -1;
	}

	msglog(eMSGTYPE::INFO, "Tunnel Proxy is listening to tunnel port %d.", tunnel_port);


	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(proxy_port);

	proxy_listener = evconnlistener_new_bind(base, le_listener_cb, (void*)2,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
		(struct sockaddr*)&sin,
		sizeof(sin));

	if (!proxy_listener) {
		msglog(eMSGTYPE::ERROR, "evconnlistener_new_bind failed at port %d, %s (%d).", proxy_port, __func__, __LINE__);
		evconnlistener_free(manage_listener);
		evconnlistener_free(tunnel_listener);
		event_base_free(base);
		return -1;
	}

	msglog(eMSGTYPE::INFO, "Tunnel Proxy is listening to proxy port %d.", proxy_port);

	timeout = event_new(base, -1, EV_PERSIST, le_timercb, NULL);
	evutil_timerclear(&tv);
	tv.tv_sec = 2;
	event_add(timeout, &tv);

	event_base_dispatch(base);

	event_del(timeout);
	event_free(timeout);

	evconnlistener_free(manage_listener);
	evconnlistener_free(tunnel_listener);
	evconnlistener_free(proxy_listener);

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

	std::vector <struct bufferevent*>::iterator viter;
	for (viter = gBevWaitin.begin(); viter != gBevWaitin.end(); viter++) {
		if (*viter != NULL) {
			bufferevent_free(*viter);
		}
	}

	gBevWaitin.clear();

	if (manage_bev != NULL)
		bufferevent_free(manage_bev);

	event_base_free(base);

#ifdef _WIN32
	WSACleanup();
#endif

	msglog(eMSGTYPE::DEBUG, "Memory cleanup done.");

	return 0;
}

static void le_timercb(evutil_socket_t fd, short event, void* arg)
{
	if (manage_bev != NULL) {
		if ((GetTickCount64() - manager_tick) >= 10000) {
			bufferevent_free(manage_bev);
			manage_bev = NULL;
			msglog(eMSGTYPE::DEBUG, "proxy manager connection deleted, idled for 10 sec.");
		}
	}

	std::map <intptr_t, _BEVINFO*>::iterator iter;
	for (iter = gBevMap.begin(); iter != gBevMap.end(); iter++) {
		if (iter->second->status == 1) {
			if ((GetTickCount64() - iter->second->tick) >= 180000) {
				if (iter->second->proxy_bev != NULL)
					bufferevent_free(iter->second->proxy_bev);
				if (iter->second->tunnel_bev != NULL)
					bufferevent_free(iter->second->tunnel_bev);
				delete iter->second;
				msglog(eMSGTYPE::DEBUG, "Idle connection (active) deleted, map index %d.", (int)iter->first);
				gBevMap.erase(iter);
			}
		}
		else if (iter->second->status == 0) {
			if ((GetTickCount64() - iter->second->tick) >= 60000) {
				if (iter->second->proxy_bev != NULL)
					bufferevent_free(iter->second->proxy_bev);
				if (iter->second->tunnel_bev != NULL)
					bufferevent_free(iter->second->tunnel_bev);
				delete iter->second;
				msglog(eMSGTYPE::DEBUG, "Idle connection (waiting) deleted, map index %d.", (int)iter->first);
				gBevMap.erase(iter);
			}
		}
	}

	if (manage_bev != NULL) {

		int idlecounts = countbevmapidle();
		if (idlecounts < IDLE_TUNNELS_MIN) {
			if (reqmanager(manage_bev, eREQTYPE::CREATE_TUNNEL, IDLE_TUNNELS - IDLE_TUNNELS_MIN) == 0) {
				msglog(eMSGTYPE::DEBUG, "Proxy manager requested for %d tunnels.", IDLE_TUNNELS - IDLE_TUNNELS_MIN);
			}
		}

		if (reqmanager(manage_bev, eREQTYPE::KEEP_ALIVE, 2) == 0) {
			msglog(eMSGTYPE::DEBUG, "Proxy manager sent keep alive packet.");
		}
	}
}

static void
le_manage_readcb(struct bufferevent* _bev, void* user_data)
{
	if (manage_bev == NULL)
		return;

	char buffer[8192] = { 0 };
	int len = bufferevent_read(_bev, (char*)buffer, 8192);

	if (len < 2)
		return;

	_PckCmd* lpMsg = (_PckCmd*)buffer;

	if (lpMsg->cmd == eREQTYPE::KEEP_ALIVE && lpMsg->data == 1) { // KEEP ALIVE
		manager_tick = GetTickCount64();
		msglog(eMSGTYPE::DEBUG, "Proxy manager received keep alive packet.");
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

static void le_listener_cb(struct evconnlistener* listener, evutil_socket_t fd,
	struct sockaddr* sa, int socklen, void* user_data) {
	int flag = (int)user_data;
	struct bufferevent* tunnel_bev;
	struct bufferevent* proxy_bev;

	if (flag == 1) {
		manage_bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE
#ifdef _WIN32
			| BEV_OPT_THREADSAFE
#endif
		);
		if (!manage_bev)
		{
			msglog(eMSGTYPE::ERROR, "bufferevent_socket_new failed, %s (%d).", __func__, __LINE__);
			return;
		}

		msglog(eMSGTYPE::DEBUG, "Proxy manager is connected.");

		bufferevent_setcb(manage_bev, le_manage_readcb, NULL, le_manage_eventcb, NULL);
		bufferevent_enable(manage_bev, EV_WRITE);
		bufferevent_enable(manage_bev, EV_READ);
		manager_tick = GetTickCount64();
	}
	else if (flag == 3) {
		tunnel_bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE 
#ifdef _WIN32
			| BEV_OPT_THREADSAFE
#endif
		/* | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_UNLOCK_CALLBACKS*/);
		if (!tunnel_bev)
		{
			msglog(eMSGTYPE::ERROR, "bufferevent_socket_new failed, %s (%d).", __func__, __LINE__);
			return;
		}

		intptr_t fd_index = bufferevent_getfd(tunnel_bev);
		_BEVINFO* bevinfo = new _BEVINFO;
		bevinfo->tunnel_bev = tunnel_bev;
		bevinfo->proxy_bev = NULL;
		bevinfo->status = 0;
		bevinfo->tick = GetTickCount64();

		gBevMap[fd_index] = bevinfo;

		bufferevent_setcb(tunnel_bev, le_tunnel_readcb, NULL, le_eventcb, (void*)fd_index);
		bufferevent_enable(tunnel_bev, EV_READ | EV_WRITE);

		// lets connect the first one in the waiting list to this tunnel
		std::vector <struct bufferevent*>::iterator iter;
		for (iter = gBevWaitin.begin(); iter != gBevWaitin.end(); iter++) {
			if (*iter != NULL) {
				bevinfo->proxy_bev = *iter;
				bevinfo->tick = GetTickCount64();
				bufferevent_setcb(*iter, le_proxy_readcb, NULL, le_eventcb, (void*)fd_index);
				bufferevent_enable(*iter, EV_READ | EV_WRITE);
				bevinfo->status = 1;
				gBevWaitin.erase(iter);
				break;
			}
		}
	}
	else if (flag == 2) {

		proxy_bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE 
#ifdef _WIN32
			| BEV_OPT_THREADSAFE
#endif
		);

		if (!proxy_bev)
		{
			msglog(eMSGTYPE::ERROR, "bufferevent_socket_new failed, %s (%d).", __func__, __LINE__);
			return;
		}

		int idlecountmap = countbevmapidle();

		msglog(eMSGTYPE::DEBUG, "Client connection accepted, current available tunnel count is %d.", idlecountmap);

		// check if there is available tunnel
		if (idlecountmap == 0) {
			if (manage_bev == NULL) {
				msglog(eMSGTYPE::DEBUG, "Proxy manager is not connected, failed to request for new tunnel.");
			}
			else {
				if (reqmanager(manage_bev, eREQTYPE::CREATE_TUNNEL, 1) == 0) {
					msglog(eMSGTYPE::DEBUG, "Proxy manager requested for %d tunnels.", 1);
				}
				// there has no tunnel available atm., lets add this in the waiting list for now...
				gBevWaitin.push_back(proxy_bev);
				msglog(eMSGTYPE::DEBUG, "There are %d client(s) in the waiting list right now.", gBevWaitin.size());
				return;
			}
		}


		// bridge proxy and tunnel
		intptr_t idleindex = getidlemap();
		if (idleindex == -1)
			return;

		_BEVINFO* bevinfo = getbevinfomap(idleindex);
		bevinfo->proxy_bev = proxy_bev;
		bevinfo->tick = GetTickCount64();
		bufferevent_setcb(proxy_bev, le_proxy_readcb, NULL, le_eventcb, (void*)idleindex);
		bufferevent_enable(proxy_bev, EV_READ | EV_WRITE);
		flagusebevmap(idleindex);
	}
}

static void
le_manage_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	if (events & BEV_EVENT_EOF)
	{
		msglog(eMSGTYPE::DEBUG, "Proxy manager is disconnected, event code is BEV_EVENT_EOF.");
		bufferevent_free(manage_bev);
		manage_bev = NULL;
	}
	else if (events & BEV_EVENT_ERROR)
	{
		msglog(eMSGTYPE::DEBUG, "Proxy manager is disconnected, event code is BEV_EVENT_ERROR.");
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

		delete bevinfo;
		delbevmap(fd_index);

		int idlecountmap = countbevmapidle();

		msglog(eMSGTYPE::DEBUG, "Deleted map index %d, idle count now is %d.", (int)fd_index, idlecountmap);
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

static int countbevmapidle() {
	int idelctr = 0;
	std::map <intptr_t, _BEVINFO*>::iterator iter;
	for (iter = gBevMap.begin(); iter != gBevMap.end(); iter++) {
		if (iter->second->status == 0) {
			idelctr++;
		}
	}
	return idelctr;
}

static intptr_t getidlemap() {
	std::map <intptr_t, _BEVINFO*>::iterator iter;
	for (iter = gBevMap.begin(); iter != gBevMap.end(); iter++) {
		if (iter->second->status == 0) {
			return iter->first;
		}
	}
	return -1;
}

static void signal_handler(int signal)
{
	event_base_loopbreak(base);
}
