#include "common.h"
#include "socket.h"
#include "user.h"
#include "protocol.h"
#include "game.h"
#include "gamectrl.h"
#include "conf.h"
#include "sms.h"
#include <mutex>
#include <thread>


std::mutex mlock;

static struct event_base* base;


static void signal_handler(int signal);

static void le_listener_cb(struct evconnlistener*, evutil_socket_t,
	struct sockaddr*, int socklen, void*);

static void le_readcb(struct bufferevent*, void*);
static void le_eventcb(struct bufferevent*, short, void*);
static void le_timercb(evutil_socket_t, short, void*);

int le_start()
{
	struct sockaddr_in sin;
	struct timeval tv;
	struct event* timeout;
	struct evconnlistener* listener;

	std::signal(SIGINT, signal_handler);

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

	msglog(eMSGTYPE::INFO, "Tongits Server %d.%d.%d.%s, socket backend is %s.",
		TUNNEL_PROXY_VER_MAJOR,
		TUNNEL_PROXY_VER_MINOR,
		TUNNEL_PROXY_VER_REV,
		TUNNEL_PROXY_VER_STG,
		event_base_get_method(base));

	if (c.isdebug() == true) {
		LOGTYPEENABLED |= eMSGTYPE::DEBUG;
	}

	int serverport = c.getserverport();

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(serverport);

	listener = evconnlistener_new_bind(base, le_listener_cb, (void*)NULL,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
		(struct sockaddr*)&sin,
		sizeof(sin));

	if (!listener) {
		msglog(eMSGTYPE::ERROR, "evconnlistener_new_bind failed at port %d, %s (%d).", serverport, __func__, __LINE__);
		event_base_free(base);
		return -1;
	}

	msglog(eMSGTYPE::INFO, "Server is listening to port %d.", serverport);

	timeout = event_new(base, -1, EV_PERSIST, le_timercb, NULL);
	evutil_timerclear(&tv);
	tv.tv_usec = 1000000 / 2;
	tv.tv_sec = 0;
	event_add(timeout, &tv);

#if GAME_TYPE == 1
	std::thread t(smsworker);
#endif

	event_base_dispatch(base);

	if (timeout != NULL) {
		event_del(timeout);
		event_free(timeout);
	}

	evconnlistener_free(listener);

	event_base_free(base);

	gcontrol.clear();

#ifdef _WIN32
	WSACleanup();
#endif

	msglog(eMSGTYPE::DEBUG, "Memory cleanup done.");

	return 0;
}

static void le_timercb(evutil_socket_t fd, short event, void* arg)
{
	gcontrol.run();
}

static void le_listener_cb(struct evconnlistener* listener, evutil_socket_t fd,
	struct sockaddr* sa, int socklen, void* user_data) {

	struct bufferevent* _bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE
//#ifdef _WIN32
		| BEV_OPT_THREADSAFE | BEV_OPT_UNLOCK_CALLBACKS | BEV_OPT_DEFER_CALLBACKS
//#endif
	);

	if (!_bev)
	{
		msglog(eMSGTYPE::ERROR, "bufferevent_socket_new failed, %s (%d).", __func__, __LINE__);
		return;
	}


	//uintptr_t newfd = bufferevent_getfd(_bev);
	//guser.adduser(newfd, _bev);

	uintptr_t newfd = guser.getuserindex();

	msglog(eMSGTYPE::DEBUG, "Client connection accepted, userindex %llu.", newfd);

	if (newfd == 0) {
		bufferevent_free(_bev);
		msglog(eMSGTYPE::DEBUG, "Client connection failed, max user reached.");
		return;
	}

	_USER_INFO* userinfo = guser.getuser(newfd);

	if (userinfo == NULL) {
		bufferevent_free(_bev);
		return;
	}

	if (userinfo->packetdata.bev != NULL) {
		bufferevent_free(userinfo->packetdata.bev);
	}

	userinfo->reset();
	userinfo->set();

	//msglog(eMSGTYPE::DEBUG, "userindex %llu ecoins %d.", newfd, (int)userinfo->ecoins);

	userinfo->isfreeuser = false;
	userinfo->packetdata.bev = _bev;
	userinfo->m_state = (unsigned char)_USER_STATE::_CONNECTED;

	// temporarily we will force login and waiting status of user here to trigger the game
	//userinfo->m_state  |= (unsigned char)_USER_STATE::_LOGGEDIN;
	//userinfo->m_state |= (unsigned char)_USER_STATE::_WAITING;

	bufferevent_setcb(_bev, le_readcb, NULL, le_eventcb, (void*)newfd);
	bufferevent_enable(_bev, EV_READ | EV_WRITE);
}

void le_updatecbfd(uintptr_t userid, uintptr_t resume_userid)
{
	bufferevent_setcb(guser.getuser(userid)->packetdata.bev, 
		le_readcb, NULL, le_eventcb, (void*)resume_userid);
}

static void
le_readcb(struct bufferevent* bev, void* user_data)
{
	uintptr_t fd = (uintptr_t)user_data;
	_USER_INFO* userinfo = guser.getuser(fd);

	if (userinfo == NULL) {
		bufferevent_free(bev);
		return;
	}

	int len = bufferevent_read(bev, userinfo->packetdata.buffer + userinfo->packetdata.bufferlen, MAX_BUFFER_DATA - userinfo->packetdata.bufferlen);

	if (len == 0) {
		guser.deluser(fd);
		return;
	}

	userinfo->packetdata.bufferlen += len;

	gprotocol.parsedata(fd);
}

static void
le_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	uintptr_t fd = (uintptr_t)user_data;

	if ((events & BEV_EVENT_EOF) || (events & BEV_EVENT_ERROR))
	{
		if (guser.getuser(fd)->ismuadmin) {
			msglog(eMSGTYPE::DEBUG, "MU Admin disconnected, fd %llu.", fd);
			guser.delmuadmin(fd);
		}
		else {
			msglog(eMSGTYPE::DEBUG, "Client disconnected, fd %llu.", fd);
			guser.deluser(fd);
		}
	}
	else if (events & BEV_EVENT_CONNECTED)
	{
	}
}

bool datasend(intptr_t userindex, unsigned char* data, int len)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return false;
	}

	struct bufferevent* bev = userinfo->packetdata.bev;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_CONNECTED)) {
		msglog(eMSGTYPE::ERROR, "user fd %llu is not connected.", userindex);
		return false;
	}

	if (bufferevent_write(bev, data, len) == -1) {
		msglog(eMSGTYPE::ERROR, "bufferevent_write failed, fd %llu.", userindex);
		return false;
	}

	//msglog(eMSGTYPE::DEBUG, "bufferevent_write %d data, fd %llu.", len, userindex);

	return true;
}

struct _HTTP_PTR
{
	struct evhttp_connection* conn;
	struct evhttp_request* req;
};

void http_request_done(struct evhttp_request* req, void* arg) {

	msglog(DEBUG, "http_request_done.");

	_HTTP_PTR* _ptr = (_HTTP_PTR*)arg;
	evhttp_connection_free(_ptr->conn);
	evhttp_request_free(_ptr->req);
	delete _ptr;
}

bool sendotp(const char *smsnum, char* otpmsg)
{
	char reqpath[100] = { 0 };
	//sprintf(reqpath, "/index.php?smsnumber=%s&otpmsg=%s", smsnum, otpmsg);

	_HTTP_PTR* _ptr = new _HTTP_PTR;

	_ptr->conn = evhttp_connection_base_new(base, NULL, "139.99.38.94", 80);

	if (_ptr->conn == NULL) {
		delete _ptr;
		return false;
	}

	_ptr->req = evhttp_request_new(http_request_done, (void*)_ptr);

	if (_ptr->req == NULL) {
		evhttp_connection_free(_ptr->conn);
		delete _ptr;
		return false;
	}

	evhttp_add_header(_ptr->req->output_headers, "Host", "muengine.org");

	if (evhttp_make_request(_ptr->conn, _ptr->req, EVHTTP_REQ_GET, "/index.php?v=1") != 0) {
		evhttp_connection_free(_ptr->conn);
		delete _ptr;
		return false;
	}

	evhttp_connection_set_timeout(_ptr->req->evcon, 60);
	msglog(DEBUG, "sendotp.");
	return true;
}

static void signal_handler(int signal)
{
	event_base_loopbreak(base);
	endworker = true;
}
