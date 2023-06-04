#include "gamectrl.h"
#include "socket.h"
#include "user.h"
#include <algorithm>
#include <random>
#include "conf.h"

gamecontrol gcontrol;


gamecontrol::gamecontrol()
{
	this->m_games.reserve(MAX_GAME_SLOT);
	msglog(DEBUG, "Loading %d game slots...", MAX_GAME_SLOT);
	for (int n = 1; n < MAX_GAME_SLOT + 1; n++) {
		game* g = new game;
		g->setgameserial(n);
		g->setstate(_GAME_STATE::_FREE);
		this->m_games.push_back(g);
	}
	msglog(DEBUG, "Loading game slots done.");
}

gamecontrol::~gamecontrol()
{
}

void gamecontrol::run()
{
	int activegames = 0;
	char sbuf[100] = { 0 };

	this->kickusers();

	std::vector <game*>::iterator iter;
	iter = this->m_games.begin();

	while (iter != this->m_games.end()) {
		game* g = *iter;
		if (g->getstate() == _GAME_STATE::_FREE) {
			iter++;
			continue;
		}
		g->run();
		activegames++;
		iter++;
	}

	sprintf(sbuf, "Tongits Server Active: %d Ver: %d.%02d.%03d%s",
		activegames,
		TUNNEL_PROXY_VER_MAJOR, TUNNEL_PROXY_VER_MINOR, TUNNEL_PROXY_VER_REV, TUNNEL_PROXY_VER_STG);
	//SetConsoleTitle(sbuf);
}

void gamecontrol::clear()
{
	msglog(DEBUG, "Clear %d game slots...", MAX_GAME_SLOT);
	std::vector <game*>::iterator iter;
	iter = this->m_games.begin();
	while (iter != this->m_games.end()) {
		game* g = *iter;
		delete g;
		iter++;
	}
	msglog(DEBUG, "Clear game slots done.");
}

game* gamecontrol::getgameslot()
{
	std::vector <game*>::iterator iter;// m_games
	for (iter = this->m_games.begin(); iter != this->m_games.end(); iter++) {
		game* _g = *iter;
		if (_g->getstate() == _GAME_STATE::_FREE)
			return _g;
	}
	return NULL;

}

game* gamecontrol::getgame(int64_t serial)
{
	std::vector <game*>::iterator iter;// m_games
	for (iter = this->m_games.begin(); iter != this->m_games.end(); iter++) {
		game* _g = *iter;
		if (_g->getgameserial() == serial)
			return _g;
	}
	return NULL;
}

static const char names[3][10] = {
	"Aida",
	"Lorna",
	"Fei"
};

void gamecontrol::addgame(uintptr_t user1, uintptr_t user2, uintptr_t user3, unsigned char gametype)
{
	game* g = this->getgameslot();

	if (g == NULL) {
		// send notice
		msglog(DEBUG, "gamecontroller, addgame returned no slot available.");
		return;
	}

	g->reset();
	g->setgametype(gametype);

	msglog(DEBUG, "gamecontroller, addgame serial %d.", g->getgameserial());

	g->m_users[0] = user1;
	g->m_users[1] = user2;
	g->m_users[2] = user3;

	if (g->checkactiveusers() < 3) {
		g->setstate(_GAME_STATE::_ENDED);
		msglog(DEBUG, "gamecontroller, addgame checkactiveusers failed.");
		return;
	}

	if (!g->checkecoins()) {
		g->setstate(_GAME_STATE::_ENDED);
		msglog(DEBUG, "gamecontroller, addgame checkecoins failed.");
		return;
	}

	_USER_INFO* _userinfo1 = guser.getuser(user1);
	_USER_INFO* _userinfo2 = guser.getuser(user2);
	_USER_INFO* _userinfo3 = guser.getuser(user3);

	_userinfo1->init();
	_userinfo2->init();
	_userinfo3->init();

	_userinfo1->m_gameserial = g->getgameserial();
	_userinfo2->m_gameserial = g->getgameserial();
	_userinfo3->m_gameserial = g->getgameserial();

	_userinfo1->m_gamepos = 0;
	_userinfo2->m_gamepos = 1;
	_userinfo3->m_gamepos = 2;

	_userinfo1->m_state ^= (unsigned char)_USER_STATE::_WAITING;
	_userinfo2->m_state ^= (unsigned char)_USER_STATE::_WAITING;
	_userinfo3->m_state ^= (unsigned char)_USER_STATE::_WAITING;

	_userinfo1->m_state |= (unsigned char)_USER_STATE::_PLAYING;
	_userinfo2->m_state |= (unsigned char)_USER_STATE::_PLAYING;
	_userinfo3->m_state |= (unsigned char)_USER_STATE::_PLAYING;

	// temporary give a unique name
	_userinfo1->name = names[0];
	_userinfo2->name = names[1];
	_userinfo3->name = names[2];


	msglog(DEBUG, "Added game %llu with %s (%d), %s (%d) and %s (%d).",
		g->getgameserial(),
		_userinfo1->name.c_str(), user1,
		_userinfo2->name.c_str(), user2,
		_userinfo3->name.c_str(), user3
	);

	g->setstate(_GAME_STATE::_NOTICE);
}

void gamecontrol::checkaliveusers()
{
	//guser.checkaliveusers();
}

void gamecontrol::addkickuser(uintptr_t userid)
{
	_USER_KICK_INFO kickinfo;
	kickinfo.tick = GetTickCount64() + 5000;
	kickinfo.userid = userid;
	this->m_kickusers.push_back(kickinfo);
	guser.getuser(userid)->iskick = true;
	msglog(INFO, "addkickuser, added %s (%s) userid %d.", 
		guser.getuser(userid)->name.c_str(),
		guser.getuser(userid)->account.c_str(),
		userid
		);
}

void gamecontrol::kickusers()
{
	std::vector <_USER_KICK_INFO>::iterator iter; //m_kickusers

	_PMSG_LOGIN_RESULT pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.hdr.len = sizeof(_PMSG_LOGIN_RESULT);
	pMsg.sub = 0x09;
	pMsg.result = 1; // option to create game
	strcpy(pMsg.ecoinsnote, c.getbetmode(0).name.c_str());
	strcpy(pMsg.jewelsnote, c.getbetmode(1).name.c_str());

	for (iter = this->m_kickusers.begin(); iter != this->m_kickusers.end(); iter++) {
		_USER_KICK_INFO kickinfo = *iter;
		if (GetTickCount64() > kickinfo.tick) {

			strcpy(pMsg.accountid, guser.getuser(kickinfo.userid)->account.c_str());
			pMsg.isuseradmin = (guser.getuser(kickinfo.userid)->isuseradmin) ? 1 : 0;
			pMsg.ecoins = guser.getuser(kickinfo.userid)->ecoins[0];
			pMsg.jewels = guser.getuser(kickinfo.userid)->ecoins[1];
			::datasend(kickinfo.userid, (unsigned char*)&pMsg, pMsg.hdr.len);
			
			guser.deluser(kickinfo.userid, true);

			iter = this->m_kickusers.erase(iter);
			if (iter == this->m_kickusers.end())
				break;
		}
	}
}

uintptr_t gamecontrol::getsessionuserid(uintptr_t token)
{
	std::map <uintptr_t, uintptr_t>::iterator iter;
	iter = this->m_gamesessions.find(token);
	if (iter != this->m_gamesessions.end())
		return iter->second;
	return 0;
}

void gamecontrol::endgamesession(uintptr_t token, uintptr_t userid)
{
	std::map <uintptr_t, uintptr_t>::iterator iter;
	iter = this->m_gamesessions.find(token);
	if (iter != this->m_gamesessions.end()) {
		this->m_gamesessions.erase(iter);
		msglog(DEBUG, "endgamesession, removed game session, token %llu.", token);
	}
}

void gamecontrol::startgamesession(uintptr_t token, uintptr_t userid)
{
	this->m_gamesessions.insert(std::make_pair(token, userid));
	msglog(DEBUG, "startgamesession, started a new game session, token %llu.", token);

}

bool gamecontrol::getusersessioninfo(uintptr_t token, uintptr_t userid)
{
	uintptr_t resume_userid = this->getsessionuserid(token);

	if (resume_userid != 0) {

		//le_updatecbfd(userid, resume_userid);

		// copy the current packet
		_PACKET_DATA _packet;
		memcpy(&_packet, &guser.getuser(userid)->packetdata, sizeof(_PACKET_DATA));

		// copy fresh ecoins
		int _ecoins[2];
		_ecoins[0] = guser.getuser(userid)->ecoins[0];
		_ecoins[1] = guser.getuser(userid)->ecoins[1];

		// copy fresh gametoken
		int _gametoken = guser.getuser(userid)->gametoken;

		// now copy all the old user info
		memcpy(guser.getuser(userid), guser.getuser(resume_userid), sizeof(_USER_INFO));
		
		// then restore back the current packet data
		memcpy(&guser.getuser(userid)->packetdata, &_packet, sizeof(_PACKET_DATA));

		// restore ecoins
		guser.getuser(userid)->ecoins[0] = _ecoins[0];
		guser.getuser(userid)->ecoins[1] = _ecoins[1];

		// restore game token
		guser.getuser(userid)->gametoken = _gametoken;

		// update the game user id with the new id
		unsigned char gamepos = guser.getuser(userid)->m_gamepos;
		game* _g = gcontrol.getgame(guser.getuser(userid)->m_gameserial);

		if (_g == NULL)
			return false;

		_PMSG_LOGIN_RESULT pMsg = { 0 };
		pMsg.hdr.c = 0xC1;
		pMsg.hdr.h = 0xF2;
		pMsg.hdr.len = sizeof(_PMSG_LOGIN_RESULT);
		pMsg.sub = 0x09;
		pMsg.result = 2; // resume game
		::datasend(userid, (unsigned char*)&pMsg, pMsg.hdr.len);

		// update hitter and active id accordingly
		_g->m_users[gamepos] = userid;
		if (_g->m_hitter == resume_userid)
			_g->m_hitter = userid;
		if (_g->m_active_userindex == resume_userid)
			_g->m_active_userindex = userid;

		// update the token userid
		this->m_gamesessions[token] = userid;

		//userid = resume_userid;
		guser.getuser(userid)->resumegame();

		// clear the old user id
		guser.getuser(resume_userid)->set();
		guser.getuser(resume_userid)->init();
		if(guser.getuser(resume_userid)->packetdata.bev != NULL)
			bufferevent_free(guser.getuser(resume_userid)->packetdata.bev);
		
		msglog(DEBUG, "getusersessioninfo, %s resume game session with serial no. %llu, token %llu.", 
			guser.getuser(userid)->name.c_str(),
			guser.getuser(userid)->m_gameserial, token);
	}
	else {
		_PMSG_LOGIN_RESULT pMsg = { 0 };
		pMsg.hdr.c = 0xC1;
		pMsg.hdr.h = 0xF2;
		pMsg.hdr.len = sizeof(_PMSG_LOGIN_RESULT);
		pMsg.sub = 0x09;
		pMsg.result = 1; // option to create game
		pMsg.isuseradmin = (guser.getuser(userid)->isuseradmin) ? 1 : 0;
		strcpy(pMsg.accountid, guser.getuser(userid)->account.c_str());
		pMsg.ecoins = guser.getuser(userid)->ecoins[0];
		pMsg.jewels = guser.getuser(userid)->ecoins[1];
		strcpy(pMsg.ecoinsnote, c.getbetmode(0).name.c_str());
		strcpy(pMsg.jewelsnote, c.getbetmode(1).name.c_str());
		::datasend(userid, (unsigned char*)&pMsg, pMsg.hdr.len);
	}

	return true;
}