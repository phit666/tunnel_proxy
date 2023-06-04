#pragma once
#include "prodef.h"

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define YAML_CPP_STATIC_DEFINE
#include <winsock2.h>
#pragma comment(lib,"Ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
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
#include <event2/http.h>
#include <evhttp.h>

#include <yaml-cpp/yaml.h>


#include <chrono>
#include <ctime>

#include <map>
#include <vector>
#include <string>
#include <mutex>

#define GAME_TYPE	1	// 1 is for public, cash mode
#define APK_VER 5


#define TUNNEL_PROXY_VER_MAJOR 1
#define TUNNEL_PROXY_VER_MINOR 4
#define TUNNEL_PROXY_VER_REV 44
#define TUNNEL_PROXY_VER_STG "-rc.30"

#define SMS_API_KEY "VUoQ-xdNuB0KYuBsQo_WMPSVlURjsQ"
#define SMS_API_SECRET "9qdvuuMwQJT8U82BzOSruHi4DOiBFL"
#define SMS_CB_URL "http://www.muengine.org/tongits_proc.php"

#define MAX_BUFFER_DATA 8192
#define MAX_USERS_PERGAME 3
#define MAX_CARD_TYPE 4
#define MAX_CARDS_PER_TYPE 13
#define NOTICE_SECONDS_DURATION 3
#define MAX_MSECONDS_EACHTURN_TIMEOUT 60000
#define MAX_MSECONDS_GROUPCARD_TIMEOUT 30000
#define MAX_MSECONDS_SHOWRESULT_TIMEOUT 10000
#define MAX_MSECONDS_SHOWCARD_TIMEOUT 30000
#define CARD_ENTITY_LEFT_CURSOR_POS 17
#define CARD_ENTITY_RIGHT_CURSOR_POS 17
#define CARD_TABLE_CURSOR_POS 16
#define CARD_STOCK_CURSOR_POS 18
#define CARD_DROP_CURSOR_POS 19 //
#define CARD_DROP_CURSOR_POS_MAX (CARD_DROP_CURSOR_POS+2) //
#define CARD_DOWN_CURSOR_POS 22
#define CARD_DOWN_CURSOR_POS_MAX (CARD_DOWN_CURSOR_POS+2) //
#define GRPBUTTON_CURSOR_POS 64
#define UNGRPBUTTON_CURSOR_POS 65
#define FIGHTBUTTON_CURSOR_POS 66
#define SHOWCARDBUTTON_CURSOR_POS 67
#define HIDECARDBUTTON_CURSOR_POS 68
#define DRAWCARDBUTTON_CURSOR_POS 69
#define DUMPCARDBUTTON_CURSOR_POS 70

#define ECOINSMODE1_CURSOR_POS 71
#define JEWELSMODE1_CURSOR_POS 72
#define RELOGIN_CURSOR_POS 73


#define NO_CURSOR_POS 255
#define CARD_CURSOR_POS 32
#define CARD_CURSOR_POS_MAX 45
#define CARD2_CURSOR_POS 46
#define CARD2_CURSOR_POS_MAX 59
#define CARD_GRP_CURSOR_POS 30
#define MAX_USER_POS 3
#define SHOWCARDATCENTER_DURATION_MSEC	2000
#define ALIVE_TICK_TIMEOUT 30000

#define ADD_TEST_ECOINS 1000

#define MAX_GAME_SLOT 1000
#define MAX_USERS MAX_GAME_SLOT * 5

#define SET_NUMBERH(x) ( (BYTE)((DWORD)(x)>>(DWORD)8) )
#define SET_NUMBERL(x) ( (BYTE)((DWORD)(x) & 0xFF) )
#define MAKE_NUMBERW(x,y)  ( (WORD)(((BYTE)((y)&0xFF)) |   ((BYTE)((x)&0xFF)<<8 ))  )


#ifndef _WIN32
#define DWORD unsigned int
#define BYTE unsigned char
#define WORD unsigned short int
#endif

#ifdef _WIN32
#undef ERROR
#undef INFO
#undef DEBUG
#endif

enum eMSGTYPE
{
	INFO = 1,
	ERROR = 2,
	DEBUG = 4,
	SQL = 8
};

enum eREQTYPE
{
	CREATE_TUNNEL = 0xA1,
	KEEP_ALIVE = 0xA2,
};

struct _PckCmd
{
	BYTE head;
	BYTE len;
	BYTE cmd;
	WORD data;
};

enum class _CARD_TYPE
{
	_CLUBS = 1,
	_SPADES,
	_HEARTS,
	_DIAMONDS
};

// server
enum class _GAME_STATE
{
	_NONE,
	_NOTICE,
	_PREPARE,
	_WAITING,
	_STARTED,
	_RESTARTED,
	_CLOSED,
	_ENDED,
	_FREE
};

// client
enum class _STATE
{
	_NONE,
	_CONNEC_STATE,
	_LOAD_STATE,
	_LOGIN_STATE,
	_OTPCODE_STATE,
	_CREATE_STATE,
	_RESUME_STATE,
	_WAITING_STATE,
	_GAME_STARTED,
	_GAME_CLOSED,
	_USER_ADD_EC,
	_USER_GET_EC
};

extern DWORD LOGTYPEENABLED;

void msglog(BYTE type, const char* msg, ...);
DWORD host2ip(const char* hostname);
#ifndef _WIN32
unsigned long long GetTickCount64();
#endif
int SQLSyntexCheck(char* SQLString);
DWORD MakeAccountKey(char* lpszAccountID);