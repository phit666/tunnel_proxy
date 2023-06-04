#pragma once
#include "common.h"
#include "game.h"
#include "user.h"
#include <map>
#include <vector>

struct _USER_KICK_INFO
{
	uintptr_t tick;
	uintptr_t userid;
};

class gamecontrol
{
public:
	gamecontrol();
	~gamecontrol();

	void run();
	void clear();
	void kickusers();
	void checkaliveusers();
	void addgame(uintptr_t user1, uintptr_t user2, uintptr_t user3, unsigned char gametype);
	game* getgame(int64_t serial);
	game* getgameslot();

	bool getusersessioninfo(uintptr_t token, uintptr_t userid);
	void endgamesession(uintptr_t token, uintptr_t userid);
	void startgamesession(uintptr_t token, uintptr_t userid);
	uintptr_t getsessionuserid(uintptr_t token);

	void addkickuser(uintptr_t userid);

private:

	std::map <uintptr_t, uintptr_t> m_gamesessions;
	std::vector <game*> m_games;
	std::vector <_USER_KICK_INFO> m_kickusers;
};

extern gamecontrol gcontrol;
