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

struct bufferevent* le_connect(DWORD ip, WORD port);
static void signal_handler(int signal);
static void le_readcb(struct bufferevent*, void*);
static void le_eventcb(struct bufferevent*, short, void*);

static void le_proxylistener_cb(struct evconnlistener*, evutil_socket_t,
	struct sockaddr*, int socklen, void*);

struct event_base* base;

struct _TunnelsInfo
{
	_TunnelsInfo()
	{
		memset(name, 0, sizeof(name));
		memset(proxyip, 0, sizeof(proxyip));
		proxyport = -1;
		memset(local_serverip, 0, sizeof(local_serverip));
		local_serverport = -1;
		proxy_listener = NULL;
	}

	char name[50];
	char proxyip[16];
	int proxyport;
	char local_serverip[16];
	int local_serverport;
	struct evconnlistener* proxy_listener;
};

static std::vector< _TunnelsInfo*> vTunnels;

int main()
{
	struct sockaddr_in sin;

	std::signal(SIGINT, signal_handler);

#ifdef _WIN32
	SYSTEM_INFO SystemInfo;
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	int ret = WSAStartup(wVersionRequested, &wsaData);

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

	msglog(eMSGTYPE::INFO, "Proxy Server %d.%d.%d.%s, socket backend is %s.",
		TUNNEL_PROXY_VER_MAJOR,
		TUNNEL_PROXY_VER_MINOR,
		TUNNEL_PROXY_VER_REV,
		TUNNEL_PROXY_VER_STG,
		event_base_get_method(base));

	try {

		YAML::Node configs = YAML::LoadFile("proxy.yaml");

		if (configs["Debug Message"].as<bool>() == true) {
			LOGTYPEENABLED |= eMSGTYPE::DEBUG;
			msglog(eMSGTYPE::INFO, "Debug message is enabled.");
		}

		YAML::Node tunnellist = configs["Proxy  Servers"];

		msglog(eMSGTYPE::DEBUG, "Proxy server count is %d.", tunnellist.size());

		YAML::iterator iter = tunnellist.begin();

		while (iter != tunnellist.end()) 
		{
			const YAML::Node& _tunnelinfo = *iter;

			_TunnelsInfo* tunnelinfo = new _TunnelsInfo;

			memcpy(tunnelinfo->name, _tunnelinfo["Name"].as<std::string>().c_str(), sizeof(tunnelinfo->name));
			tunnelinfo->proxyport = _tunnelinfo["Proxy Port"].as<int>();
			memcpy(tunnelinfo->proxyip, _tunnelinfo["Proxy IP"].as<std::string>().c_str(), sizeof(tunnelinfo->local_serverip));
			tunnelinfo->local_serverport = _tunnelinfo["Local Server Port"].as<int>();
			memcpy(tunnelinfo->local_serverip, _tunnelinfo["Local Server IP"].as<std::string>().c_str(), sizeof(tunnelinfo->local_serverip));

			if (_tunnelinfo["Enable"].as<bool>() == false) {
				iter++;
				msglog(eMSGTYPE::DEBUG, "Proxy server %s is disabled.", tunnelinfo->name);
				continue;
			}

			msglog(eMSGTYPE::INFO, "%s Proxy Server IP %s.", tunnelinfo->name, tunnelinfo->proxyip);

			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = htonl(host2ip(tunnelinfo->proxyip));
			sin.sin_port = htons(tunnelinfo->proxyport);

			tunnelinfo->proxy_listener = evconnlistener_new_bind(base, le_proxylistener_cb, (void*)tunnelinfo,
				LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
				(struct sockaddr*)&sin,
				sizeof(sin));

			if (!tunnelinfo->proxy_listener) {
				msglog(eMSGTYPE::ERROR, "evconnlistener_new_bind failed at port %d, %s (%d).", tunnelinfo->proxyport, __func__, __LINE__);
				event_base_free(base);
				return -1;
			}

			msglog(eMSGTYPE::INFO, "%s Proxy Server is listening to proxy port %d.", tunnelinfo->name, tunnelinfo->proxyport);

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

	event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY);

	std::vector<_TunnelsInfo*>::iterator viter = vTunnels.begin();
	while (viter != vTunnels.end()) {
		_TunnelsInfo* _tunneninfo = *viter;
		delete _tunneninfo;
		viter++;
	}

	vTunnels.clear();

	event_base_free(base);

#ifdef _WIN32
	WSACleanup();
#endif

	msglog(eMSGTYPE::DEBUG, "Memory cleanup done.");

	return 0;
}

static void le_proxylistener_cb(struct evconnlistener* listener, evutil_socket_t fd,
	struct sockaddr* sa, int socklen, void* user_data) {

	_TunnelsInfo* tunnelproxyinfo = (_TunnelsInfo*)user_data;
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

	bufferevent* local_bev = le_connect(host2ip(tunnelproxyinfo->local_serverip), tunnelproxyinfo->local_serverport);

	if (local_bev == NULL) {
		bufferevent_free(proxy_bev);
		return;
	}

	bufferevent_setcb(proxy_bev, le_readcb, NULL, le_eventcb, (void*)local_bev);
	bufferevent_setcb(local_bev, le_readcb, NULL, le_eventcb, (void*)proxy_bev);


	bufferevent_enable(proxy_bev, EV_READ | EV_WRITE);
	bufferevent_enable(local_bev, EV_READ | EV_WRITE);
}

struct bufferevent* le_connect(DWORD ip, WORD port)
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
	return _bev;
}


static void
le_readcb(struct bufferevent* bev, void* user_data)
{
	bufferevent* _bev = (bufferevent*)user_data;
	if (bufferevent_read_buffer(bev, bufferevent_get_output(_bev)) == -1) {
		msglog(eMSGTYPE::ERROR, "bufferevent_read_buffer failed, %s (%d).", __func__, __LINE__);
	}
}

static void
le_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	bufferevent* _bev = (bufferevent*)user_data;
	if (events & BEV_EVENT_EOF || events & BEV_EVENT_ERROR)
	{
		bufferevent_free(bev);
		bufferevent_free(_bev);
		msglog(eMSGTYPE::DEBUG, "Proxy disconnected.");
	}
	else if (events & BEV_EVENT_CONNECTED)
	{
	}
}


static void signal_handler(int signal)
{
	event_base_loopbreak(base);
}