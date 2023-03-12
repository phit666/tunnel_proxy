// tunnel.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#pragma comment(lib,"Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#define DWORD unsigned int
#define BYTE unsigned char
#define WORD unsigned short int
#endif
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <string.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/thread.h>

#include <map>
#include <vector>


struct bufferevent* le_connect(DWORD ip, WORD port, int index);
DWORD host2ip(const char* hostname);
static void signal_handler(int signal);
static void le_proxy_readcb(struct bufferevent*, void*);
static void le_tunnel_readcb(struct bufferevent*, void*);
static void le_manage_readcb(struct bufferevent*, void*);
static void le_eventcb(struct bufferevent*, short, void*);
static void le_manage_eventcb(struct bufferevent*, short, void*);
static void le_timercb(evutil_socket_t, short, void*);

struct event_base* base;
static struct bufferevent* manage_bev = NULL;

struct _BEVINFO
{
	struct bufferevent* proxy_bev;
	struct bufferevent* tunnel_bev;
};


struct _ConnectInfo
{
	int event_id;
	int event_id2;
	char ip[50];
	int port;
};

_ConnectInfo gConnectInfo[2] = { 0 };

struct _PckCmd
{
	BYTE head;
	BYTE len;
	BYTE cmd;
	WORD data;
};

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

	if (argc < 6) {
		std::cout << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << "tunnel <manage ip> <manage port> <tunnel ip> <tunnel port>  <ha ip> <ha port>" << std::endl;
		std::cout << std::endl;
		system("pause");
		return -1;
	}

	std::signal(SIGINT, signal_handler);

	std::stringstream s;
	s << argv[2]; s >> portManage; s.clear();
	s << argv[4]; s >> portProxy; s.clear();
	s << argv[6]; s >> portServer;
	memcpy(&manageip, argv[1], sizeof(manageip));
	memcpy(&proxyip, argv[3], sizeof(proxyip));
	memcpy(&svrip, argv[5], sizeof(svrip));

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

	printf("Using Libevent with backend method %s.\n",
		event_base_get_method(base));

	timeout = event_new(base, -1, EV_PERSIST, le_timercb, NULL);
	evutil_timerclear(&tv);
	tv.tv_sec = 1;
	event_add(timeout, &tv);

	event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY);

	event_del(timeout);
	event_free(timeout);

	std::map <intptr_t, _BEVINFO*>::iterator iter;
	for (iter = gBevMap.begin(); iter != gBevMap.end(); iter++) {
		if (iter->second->proxy_bev != NULL)
			bufferevent_free(iter->second->proxy_bev);
		if (iter->second->tunnel_bev != NULL)
			bufferevent_free(iter->second->tunnel_bev);
		delete iter->second;

		printf("Exit cleanup, deleted fd index %d.\n", (int)iter->first);

	}

	gBevMap.clear();

	if(manage_bev != NULL)
		bufferevent_free(manage_bev);

	event_base_free(base);

#ifdef _WIN32
	WSACleanup();
#endif

	printf("main->exit.\n");
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
		//| BEV_OPT_DEFER_CALLBACKS
		//| BEV_OPT_UNLOCK_CALLBACKS
	);

	if (!_bev) {
		printf("bufferevent_socket_new Error, %s (%d).\n", __func__, __LINE__);
		return NULL;
	}

	remote_address.sin_family = AF_INET;
	remote_address.sin_addr.s_addr = htonl(ip);
	remote_address.sin_port = htons(port);

	result = bufferevent_socket_connect(_bev, (struct sockaddr*)&remote_address, sizeof(remote_address));

	if (result == -1) {
		printf("bufferevent_socket_connect Error, %s (%d).\n", __func__, __LINE__);
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

	if (lpMsg->cmd == 0xA1) {

		printf("Manager requested for %d tunnels.\n", lpMsg->data);

		// lets spawn the requested idle tunnels
		for (int i = 0; i < lpMsg->data; i++) {

			struct bufferevent* _bev1 = le_connect(host2ip(proxyip), portProxy, 1);

			if (_bev1 == NULL) {
				printf("le_connect Error, %s (%d).\n", __func__, __LINE__);
				return;
			}


			struct bufferevent* _bev2 = le_connect(host2ip(svrip), portServer, 2);

			if (_bev2 == NULL) {
				printf("le_connect Error, %s (%d).\n", __func__, __LINE__);
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
}

static void
le_tunnel_readcb(struct bufferevent* _bev, void* user_data)
{
	intptr_t fd_index = (intptr_t)user_data;
	_BEVINFO* bevinfo = getbevinfomap(fd_index);
	if (bevinfo == NULL)
		return;
	if (bufferevent_read_buffer(_bev, bufferevent_get_output(bevinfo->proxy_bev)) == -1) {
		printf("bufferevent_read_buffer Error, %s (%d).\n", __func__, __LINE__);
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
		printf("bufferevent_read_buffer Error, %s (%d).\n", __func__, __LINE__);
		return;
	}
}

static void le_timercb(evutil_socket_t fd, short event, void* arg)
{
	if (manage_bev == NULL) {
		manage_bev = le_connect(host2ip(manageip), portManage, 0);
		if (!manage_bev) {
			printf("le_connect Error, %s (%d).\n", __func__, __LINE__);
			return;
		}
		bufferevent_setcb(manage_bev, le_manage_readcb, NULL, le_manage_eventcb, NULL);
	}
	else {
		char buffer[1] = { 0x01 };
		bufferevent_write(manage_bev, buffer, 1);
	}
}

static void
le_manage_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	if (events & BEV_EVENT_EOF || events & BEV_EVENT_ERROR)
	{
		bufferevent_free(bev);
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

		printf("Deleted fd index %d.\n", (int)fd_index);

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

DWORD host2ip(const char* hostname)
{
	struct hostent* h = gethostbyname(hostname);
	return (h != NULL) ? ntohl(*(DWORD*)h->h_addr) : 0;
}

static void signal_handler(int signal)
{
	event_base_loopbreak(base);
}

