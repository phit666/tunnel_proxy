#pragma once
#include "common.h"
#define _USE_MATH_DEFINES
#include <math.h>

struct _PACKET_DATA
{
	_PACKET_DATA()
	{
		bev = NULL;
		bufferlen = 0;
		memset(buffer, 0, MAX_BUFFER_DATA);
	}
	struct bufferevent* bev;
	char buffer[MAX_BUFFER_DATA];
	int bufferlen;
};

enum class _USER_STATE
{
	_NONE,
	_CONNECTED = 1,
	_LOGGEDIN = 2,
	_WAITING = 4,
	_PLAYING = 8,
	_DISCONNECTED = 16,
	_RESUMED = 32,
};

struct _GPS_INFO
{
	uint64_t tick;
	double longitude;
	double latitude;
};

struct _USER_INFO
{
	_USER_INFO()
	{
		this->set();
		this->init();
	}

	void set()
	{
		account = "";
		name = "";
		mobilenum = "";
		token = -1;
		gametoken = 0;
		ecoins[0] = 0;
		ecoins[1] = 0;
		isfreeuser = true;
		m_state = (unsigned char)_USER_STATE::_NONE;
		deltick = GetTickCount64() + 1000;
		ectype = 0;
		ismuadmin = false;
		gps.tick = 0;
		gps.longitude = 0.000000f;
		gps.latitude = 0.000000f;
		otpcode = 0;
		isuseradmin = false;
		isnogps = false;
	}

	void setmuadmin()
	{
		account = "MU-Admin";
		name = "MU-Admin";
		mobilenum = "";
		token = -1;
		gametoken = 0;
		ecoins[0] = 0;
		ecoins[1] = 0;
		ectype = 0;
		ismuadmin = true;
	}

	void init()
	{
		m_resumeflag = 0;
		m_gameserial = 0;
		m_gamepos = -1;
		alivetick = 0;
		iskick = false;
		this->reset();
	}

	void kick()
	{
		m_gameserial = 0;
		m_gamepos = -1;
		alivetick = 0;
		iskick = false;
		this->reset();
	}

	void resetcount()
	{
		m_cardcount = 0;
		m_cardquantity = 0;
		m_royalcount = 0;
		m_quadracount = 0;
		m_acecount = 0;
	}

	void reset()
	{
		lastactiontick = 0;
		m_isdowncard = false;
		activetick = 0;
		m_gamedropctr = 0;
		fought = false;
		canfight = false;
		isselfblock = false;
		isauto = false;
		this->resetcount();
	}

	void setgametype(unsigned char gametype) {
		ectype = gametype;
	}

	void endgame()
	{
		if(isplaying())
			m_state ^= (unsigned char)_USER_STATE::_PLAYING;
		reset();
	}

	void resumegame()
	{
		m_state ^= (unsigned char)_USER_STATE::_DISCONNECTED;
		m_state |= (unsigned char)_USER_STATE::_RESUMED;
		m_state |= (unsigned char)_USER_STATE::_PLAYING;
		this->m_resumeflag = 0;
		disconnectedtick = 0;
	}

	void disconnected()
	{
		m_state ^= (unsigned char)_USER_STATE::_PLAYING;
		m_state |= (unsigned char)_USER_STATE::_DISCONNECTED;
		disconnectedtick = GetTickCount64();
	}

	void setlognwait()
	{
		m_state |= (unsigned char)_USER_STATE::_LOGGEDIN;
		m_state |= (unsigned char)_USER_STATE::_WAITING;
	}

	void setlogged()
	{
		m_state |= (unsigned char)_USER_STATE::_LOGGEDIN;
	}

	bool isresumed()
	{
		return (m_state & (unsigned char)_USER_STATE::_RESUMED);
	}

	void resumed()
	{
		if(isresumed())
			m_state ^= (unsigned char)_USER_STATE::_RESUMED;
	}

	bool isdc()
	{
		return (m_state & (unsigned char)_USER_STATE::_DISCONNECTED);
	}

	bool iswaiting()
	{
		return (m_state & (unsigned char)_USER_STATE::_WAITING);
	}

	bool isloggedin()
	{
		return (m_state & (unsigned char)_USER_STATE::_LOGGEDIN);
	}

	void relog()
	{
		if (iswaiting()) {
			m_state ^= (unsigned char)_USER_STATE::_WAITING;
		}
	}

	bool isplaying()
	{
		return (m_state & (unsigned char)_USER_STATE::_PLAYING);
	}

	int otpcode;

	std::string account;
	std::string name;
	std::string mobilenum;

	int64_t token;
	int gametoken;
	int ecoins[2];

	uint64_t alivetick;
	uint64_t lastactiontick;
	uint64_t activetick;
	uint64_t deltick;
	uint64_t disconnectedtick;

	int64_t m_gameserial;
	int m_gamepos;
	int m_gamedropctr;
	bool m_isdowncard;

	int m_cardcount;
	int m_cardquantity;
	int m_royalcount;
	int m_quadracount;
	int m_acecount;

	bool fought;
	bool canfight;
	bool isauto;
	bool isselfblock;

	bool isfreeuser;

	unsigned char m_state;
	unsigned char m_resumeflag;

	unsigned char ectype;

	bool ismuadmin;
	bool isuseradmin;
	bool isnogps;

	bool iskick;

	_GPS_INFO gps;
	_PACKET_DATA packetdata;
};

class user
{
public:

	user();
	~user();

	void clear();
	void adduser(uintptr_t userindex, struct bufferevent* bev);
	void deluser(uintptr_t userindex, bool isreset = false);
	void delmuadmin(uintptr_t userindex);
	void updateuserbev(uintptr_t userid, uintptr_t resume_userid);
	void checkaliveusers();
	_USER_INFO* getuser(uintptr_t userindex);

	int getcount() { return this->m_mUsers.size(); }

	void trystartgame(unsigned char gametype); // temporary function, starting game should be in protocol

	void setstate(_USER_STATE state) { m_state = state; }
	_USER_STATE getstate() { return m_state; }

	uintptr_t getuserindex();

	void otpcode(int otpcode, uintptr_t userindex);
	void otplogin(char* mobilenum, uintptr_t userindex);
	void mulogin(char* musecret, uintptr_t userindex);
	void tokenlogin(char* logintoken, uintptr_t userindex);
	void userlogin(char* username, char* secret, uintptr_t userindex);
	void sendnotice(uintptr_t userindex, unsigned char type, const char* msg, ...);

	void saveecoins(uintptr_t userindex, unsigned char type);

	bool adduserecoins(uintptr_t userindex, char* accountid, int ecoins, unsigned char ectype, int aindex);
	bool getuserecoins(uintptr_t userindex, char* accountid, int ecoins, unsigned char ectype, int aindex);


	double getdistancegps(double lat1, double long1, double lat2, double long2);

	int getwaitings() { return m_waitings; }

private:

	bool isuserloggedin(uintptr_t token);

	bool sendsmsotp(const char* smsnum, char* otpmsg);

	double toRad(double degree) {
		return degree / 180 * M_PI;
	}

	std::map <uintptr_t, _USER_INFO*> m_mUsers;
	_USER_STATE m_state;

	uintptr_t m_userindex;
	int m_waitings;

};

extern user guser;


