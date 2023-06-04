#pragma once
#include "common.h"

struct _CARD_INFO
{
	_CARD_TYPE type;
	unsigned char cards[MAX_CARDS_PER_TYPE];
};

enum class _ACTIVE_STATE
{
	_NONE = 0,
	_DRAWN = 1,
	_CHOWED = 2,
	_DOWNED = 4,
	_DROPPED = 8,
	_FOUGHT = 16,
	_STOCKZERO = 32,
	_RESET = 64,
};

enum class _ACTIONS
{
	_NONE = 0,
	_DRAW = 1,
	_CHOW = 2,
	_DOWN = 4,
	_GROUP = 128,
	_UNGROUP = 256,
	_DROP = 8,
	_SAPAW = 16,
	_FIGHT = 32,
	_FIGHT2 = 64
};

struct _DROPCARD_INFO
{
	_PMSG_CARD_INFO card;
	unsigned char pos;
	unsigned char userpos;
};

struct _USER_CARD_INFO
{
	std::vector<std::vector <_PMSG_CARD_INFO>> group;
	std::vector<std::vector <_PMSG_CARD_INFO>> down;
	std::vector <_PMSG_CARD_INFO> user;
	_PMSG_CARD_INFO lastdrawcard;
	bool iskick;
};

struct _GAME_TYPE_ECOINSINFO
{
	int hitbaseaddecoins;
	int hitaddecoins;
	int nodownaddecoins;
	int nontongitaddecoins;
	int tongitaddecoins;
	int sagasaaddecoins;
	int royaladdecoins;
	int quadraaddecoins;
	int foughtbaseaddecoins;
	int foughtpercardaddecoins;
	int aceaddecoins;
};

class game
{
public:
	game();
	~game();


	void run();
	void reset();
	void loadgameconf();

	bool chowcard(uintptr_t userindex, unsigned char userpos, unsigned char* pos, unsigned char count, unsigned char* cardpos);
	bool dropcard(uintptr_t userindex, unsigned char* pos);
	bool drawfromstock(uintptr_t userindex);
	bool movecardpos(uintptr_t userindex, unsigned char group, unsigned char* srcpos, unsigned char* targetpos);
	bool downcards(uintptr_t userindex, unsigned char count, unsigned char* cardpos);
	bool sapawcard(uintptr_t userindex, unsigned userpos, unsigned downpos, unsigned char count, unsigned char* cardpos, bool test = false,  bool skipactioncheck = false);
	bool groupcards(uintptr_t userindex, unsigned char count, unsigned char* cardpos);
	bool ungroupcards(uintptr_t userindex, int downpos);
	bool fightcard(uintptr_t userindex);
	bool fight2card(uintptr_t userindex, unsigned char isfight);
	bool usercards(uintptr_t userindex);
	bool gamestart(uintptr_t userindex);

	intptr_t m_users[MAX_USERS_PERGAME];

	std::map<uintptr_t, int>mUserWinnings;

	void setstate(_GAME_STATE state, bool flag=false);
	_GAME_STATE getstate() { return m_state; }

	void setgameserial(int64_t serial) { this->m_gameserial = serial; }
	int64_t getgameserial() { return this->m_gameserial; }

	bool checkecoins();
	int checkactiveusers();

	void setgametype(unsigned char gametype) { this->m_ectype = gametype; }
	unsigned char getgametype() { return this->m_ectype; }

	void sendresult(uintptr_t userindex, unsigned char result);

	uintptr_t m_hitter;
	int m_active_userindex;

	void senduserecoinsinfo(uintptr_t userindex = 0);

private:

	bool checkgpsdistance();

	bool datasend(intptr_t userindex, unsigned char* data, int len);

	_USER_CARD_INFO m_usercardinfo[3];
	unsigned char initdrawcards[3];

	std::vector<_PMSG_CARD_INFO> vStockCards;
	std::vector<_DROPCARD_INFO> vDroppedCards;
	unsigned char m_dropcardctr;

	void countusercards(uintptr_t userindex, bool isfoughtstatus = false);
	void getwinner(int flag = 0);
	bool updatehitecoins();

	int countstockcards();

	int getposfromdropcard(unsigned char userpos, unsigned char* card);
	int getposfromusercard(uintptr_t userindex, unsigned char* card);

	bool removefromdropped(unsigned userpos, unsigned char* pos);
	bool removefromusercards(uintptr_t userindex, unsigned char* pos, bool isfree = false);
	bool chooserandomcard(uintptr_t userindex, unsigned char* rcard);
	bool addtousercards(uintptr_t userindex, unsigned char* pos);
	bool getcardfromusercards(uintptr_t userindex, unsigned char pos, unsigned char* card);
	int adddowncards(uintptr_t userindex, std::vector <_PMSG_CARD_INFO> v, bool isgroup = false);

	bool isactionvalid(uintptr_t userindex, _ACTIONS action);

	bool trysapawcard(uintptr_t userindex, unsigned char* cardpos);
	void shufflecards();

	void sendstockcardcount(uintptr_t userindex = 0);
	void sendshuffledcards();

	void sendusercards(uintptr_t userindex, bool isresumed = false);
	void sendinitusercards();
	void sendgroupcards(uintptr_t userindex);
	void senddowncards(uintptr_t userindex);

	void sendactiveinfo(uintptr_t userindex = 0);
	void sendbothcardstouser(uintptr_t userindex);

	void sendusercardcountsinfo(uintptr_t userindex, uintptr_t touserindex = 0);
	void sendusernameinfo(uintptr_t userindex = 0);
	void sendtimeoutleft(uintptr_t timemsec);
	void sendclosedinfo();
	void sendwaitinfo();
	void sendendedinfo();
	void sendfightmode(uintptr_t userindex, unsigned char enable);
	void sendactivestatus();

	void sendnotice(uintptr_t userindex, unsigned char type, const char* msg, ...);

	void setstate_restarted();
	void setstate_started(bool isrestarted=false);
	void setstate_notice();
	void setstate_prepare();
	void setstate_closed();
	void setstate_ended();
	void setstate_waiting();

	void procstate_notice();
	void procstate_prepare();
	void procstate_started();
	void procstate_closed();
	void procstate_restarted();

	void resumedcuser();

	void msglog(BYTE type, const char* msg, ...);

	_GAME_TYPE_ECOINSINFO m_ecinfo[2];
	// server constant config specifics
	/*uintptr_t m_hitbaseaddecoins;
	uintptr_t m_hitaddecoins;
	uintptr_t m_nodownaddecoins;
	uintptr_t m_nontongitaddecoins;
	uintptr_t m_tongitaddecoins;
	uintptr_t m_royaladdecoins;
	uintptr_t m_quadraaddecoins;
	uintptr_t m_foughtbaseaddecoins;
	uintptr_t m_foughtpercardaddecoins;
	uintptr_t m_aceaddecoins;
	uintptr_t m_sagasaaddecoins;*/

	// running values
	uintptr_t m_hitprizeecoins;

	uintptr_t m_winner;
	_GAME_STATE m_state;
	int m_counter;

	uintptr_t m_gametick;
	int64_t m_gameserial;

	int m_active_pos;
	int m_active_status;

	uintptr_t m_fightuserindex;
	bool m_resumed;
	int m_resumemsleft;
	unsigned char m_ectype;
};



