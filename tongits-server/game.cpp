#include "game.h"
#include "gamectrl.h"
#include "socket.h"
#include "user.h"
#include <algorithm>
#include <random>
#include <cassert>
#include "conf.h"

static char monetary[2][7]{
	"eCoins",
	"Jewels"
};

game::game()
{
	this->m_hitter = 0;
	this->m_state = _GAME_STATE::_FREE;
	this->m_counter = 0;
	this->m_gametick = 0;
	this->m_active_pos = -1;
	this->m_resumed = false;
	this->m_resumemsleft = 0;
	this->m_dropcardctr = 0;
	this->m_ectype = 0;

	for (int n = 0; n < 3; n++) {
		this->m_usercardinfo[n].user.clear();
		this->m_usercardinfo[n].down.clear();
		this->m_usercardinfo[n].group.clear();
		this->m_usercardinfo[n].lastdrawcard.cardtype = 0;
		this->m_usercardinfo[n].lastdrawcard.cardnum = 0;
		this->m_usercardinfo[n].iskick = false;
	}
}

game::~game()
{
	this->reset();
}

void game::loadgameconf()
{
	_TONGITS_BET_INFO ecoins = c.getbetmode(0);
	this->m_ecinfo[0].hitbaseaddecoins = ecoins.hits;
	this->m_ecinfo[0].hitaddecoins = ecoins.hitsadd;
	this->m_ecinfo[0].nodownaddecoins = ecoins.burned;
	this->m_ecinfo[0].nontongitaddecoins = ecoins.nontongits;
	this->m_ecinfo[0].tongitaddecoins = ecoins.tongits;
	this->m_ecinfo[0].sagasaaddecoins = ecoins.sagasa;
	this->m_ecinfo[0].royaladdecoins = ecoins.royal;
	this->m_ecinfo[0].quadraaddecoins = ecoins.quadra;
	this->m_ecinfo[0].foughtbaseaddecoins = ecoins.fight;
	this->m_ecinfo[0].foughtpercardaddecoins = ecoins.fightpercard;
	this->m_ecinfo[0].aceaddecoins = ecoins.ace;
	_TONGITS_BET_INFO jewels = c.getbetmode(1);
	this->m_ecinfo[1].hitbaseaddecoins = jewels.hits;
	this->m_ecinfo[1].hitaddecoins = jewels.hitsadd;
	this->m_ecinfo[1].nodownaddecoins = jewels.burned;
	this->m_ecinfo[1].nontongitaddecoins = jewels.nontongits;
	this->m_ecinfo[1].tongitaddecoins = jewels.tongits;
	this->m_ecinfo[1].sagasaaddecoins = jewels.sagasa;
	this->m_ecinfo[1].royaladdecoins = jewels.royal;
	this->m_ecinfo[1].quadraaddecoins = jewels.quadra;
	this->m_ecinfo[1].foughtbaseaddecoins = jewels.fight;
	this->m_ecinfo[1].foughtpercardaddecoins = jewels.fightpercard;
	this->m_ecinfo[1].aceaddecoins = jewels.ace;
}

void game::reset()
{
	this->m_winner = 0;
	this->m_fightuserindex = 0;
	this->vStockCards.clear();
	this->vDroppedCards.clear();
	this->loadgameconf();
	for (int n = 0; n < 3; n++) {
		this->m_usercardinfo[n].user.clear();
		this->m_usercardinfo[n].down.clear();
		this->m_usercardinfo[n].group.clear();
		this->m_usercardinfo[n].lastdrawcard.cardtype = 0;
		this->m_usercardinfo[n].lastdrawcard.cardnum = 0;
	}
}

void game::run()
{
	switch (this->m_state) {
	case _GAME_STATE::_NONE:
		break;
	case _GAME_STATE::_NOTICE:
		this->procstate_notice();
		break;
	case _GAME_STATE::_PREPARE:
		this->procstate_prepare();
		break;
	case _GAME_STATE::_STARTED:
		this->procstate_started();
		break;
	case _GAME_STATE::_RESTARTED:
		this->procstate_restarted();
		break;
	case _GAME_STATE::_WAITING:
		//this->procstate_waiting();
		break;
	case _GAME_STATE::_CLOSED:
		this->procstate_closed();
		break;
	case _GAME_STATE::_ENDED:
		break;
	}
}

void game::setstate(_GAME_STATE state, bool flag)
{
	if ((this->m_state == _GAME_STATE::_ENDED || this->m_state == _GAME_STATE::_FREE)
		&& 
		state == _GAME_STATE::_CLOSED) {
		return;
	}

	this->m_state = state;

	switch (state) {
	case _GAME_STATE::_NONE:
		this->msglog(DEBUG, "Set game state to NONE.");
		break;
	case _GAME_STATE::_NOTICE:
		this->setstate_notice();
		this->msglog(DEBUG, "Set game state to NOTICE.");
		break;
	case _GAME_STATE::_PREPARE:
		this->setstate_prepare();
		this->msglog(DEBUG, "Set game state to NOTICE.");
		break;
	case _GAME_STATE::_STARTED:
		this->setstate_started(flag);
		this->msglog(DEBUG, "Set game state to STARTED.");
		break;
	case _GAME_STATE::_RESTARTED:
		this->msglog(DEBUG, "Set game state to RESTARTED.");
		this->setstate_restarted();
		break;
	case _GAME_STATE::_WAITING:
		this->setstate_waiting();
		this->msglog(DEBUG, "Set game state to WAITING.");
		break;
	case _GAME_STATE::_CLOSED:
		this->setstate_closed();
		this->msglog(DEBUG, "Set game state to CLOSED.");
		break;
	case _GAME_STATE::_ENDED:
		this->setstate_ended();
		this->msglog(DEBUG, "Set game state to ENDED.");
		break;
	}
}

void game::shufflecards()
{
	std::vector<unsigned char> _userpos;

	int cardscount = 12;

	if (this->m_hitter == 0 || this->m_hitter == this->m_users[0])
		cardscount = 13;

	for (int i = 0; i < cardscount; i++)
		_userpos.push_back(0);

	cardscount = 12;
	if (this->m_hitter == this->m_users[1])
		cardscount = 13;

	for (int i = 0; i < cardscount; i++)
		_userpos.push_back(1);

	cardscount = 12;
	if (this->m_hitter == this->m_users[2])
		cardscount = 13;

	for (int i = 0; i < cardscount; i++)
		_userpos.push_back(2);

	//m_usercardifo

	for (int i = 0; i < MAX_CARD_TYPE; i++) {
		for (int n = 1; n < MAX_CARDS_PER_TYPE + 1; n += 1) {
			_PMSG_CARD_INFO _c;
			_c.cardtype = i + 1;
			_c.cardnum = n;
			this->vStockCards.push_back(_c);
		}

		auto rng = std::default_random_engine{};
		rng.seed(GetTickCount64());
		std::shuffle(std::begin(this->vStockCards), std::end(this->vStockCards), rng);
	}


	auto rng = std::default_random_engine{};
	rng.seed(GetTickCount64());
	std::shuffle(std::begin(_userpos), std::end(_userpos), rng);

	int ctr = 0;
	std::vector<unsigned char>::iterator iter;

	iter = _userpos.begin();

	while (iter != _userpos.end()) {
		unsigned char _pos = *iter;
		this->m_usercardinfo[_pos].user.push_back(this->vStockCards[ctr]);
		this->vStockCards[ctr].cardtype = 0;
		this->vStockCards[ctr].cardnum = 0;
		iter++;
		ctr++;
	}
}

void game::sendresult(uintptr_t userindex, unsigned char result)
{
	_PMSG_ACTION_RESULT pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.hdr.len = sizeof(_PMSG_ACTION_RESULT);
	pMsg.sub = 0x04;
	pMsg.result = result;
	this->datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
}

bool game::checkecoins()
{
	bool result = true;
	uintptr_t reqecoinstoplay = 0;
	reqecoinstoplay += this->m_ecinfo[this->m_ectype].hitbaseaddecoins;
	reqecoinstoplay += this->m_ecinfo[this->m_ectype].nodownaddecoins;
	reqecoinstoplay += this->m_ecinfo[this->m_ectype].tongitaddecoins;
	reqecoinstoplay += this->m_ecinfo[this->m_ectype].royaladdecoins;
	reqecoinstoplay += this->m_ecinfo[this->m_ectype].quadraaddecoins;
	reqecoinstoplay += this->m_ecinfo[this->m_ectype].aceaddecoins * 4;

	this->msglog(DEBUG, "checkecoins, reqecoinstoplay %d.", reqecoinstoplay);

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (this->m_winner != this->m_users[i]) {
			if (guser.getuser(this->m_users[i]) == NULL ||
				guser.getuser(this->m_users[i])->ecoins[this->m_ectype] < reqecoinstoplay) {
				this->sendnotice(0, 1, "%s does'nt have enough %s to continue.", guser.getuser(this->m_users[i])->name.c_str(), monetary[this->m_ectype]);
				this->sendnotice(0, 1, "%d %s minimum is required to play.", reqecoinstoplay, monetary[this->m_ectype]);
				result = false;
			}
		}
	}
	return result;
}

bool game::updatehitecoins()
{
	for (int i = 0; i < MAX_USER_POS; i++) {
		if (this->m_hitter == 0) {
			this->m_hitprizeecoins += this->m_ecinfo[this->m_ectype].hitbaseaddecoins;
			guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= this->m_ecinfo[this->m_ectype].hitbaseaddecoins;
			this->msglog(DEBUG, "updatehitecoins, deducted %d ecoins from %s (%s), hit ecoins base.",
				this->m_ecinfo[this->m_ectype].hitbaseaddecoins, guser.getuser(this->m_users[i])->name.c_str(), guser.getuser(this->m_users[i])->account.c_str());
		}
		else {
			guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= this->m_ecinfo[this->m_ectype].hitaddecoins;
			this->m_hitprizeecoins += this->m_ecinfo[this->m_ectype].hitaddecoins;
			this->msglog(DEBUG, "updatehitecoins, deducted %d ecoins from %s (%s), hit ecoins top up.",
				this->m_ecinfo[this->m_ectype].hitaddecoins, guser.getuser(this->m_users[i])->name.c_str(), guser.getuser(this->m_users[i])->account.c_str());
		}

	}
	return true;
}

void game::getwinner(int flag)
{
	if (this->m_winner != 0) {

		_PMSG_TRANSACT_INFO pMsg = { 0 };
		pMsg.hdr.c = 0xC1;
		pMsg.hdr.h = 0xF2;
		pMsg.hdr.len = sizeof(_PMSG_TRANSACT_INFO);
		pMsg.sub = 0x08;

		_USER_INFO* winnerinfo = guser.getuser(this->m_winner);

		if (winnerinfo->m_cardcount == 0) {
			this->sendnotice(0, 3, "%s Tongits!", winnerinfo->name.c_str());
			this->sendresult(this->m_winner, 2);
			this->msglog(DEBUG, "getwinner, %s (%s) is tongits.", winnerinfo->name.c_str(), winnerinfo->account.c_str());
		}
		else {
			this->msglog(DEBUG, "getwinner, %s (%s) won with card count of %d.", winnerinfo->name.c_str(), winnerinfo->account.c_str(), winnerinfo->m_cardcount);
			this->sendresult(this->m_winner, 2);
		}

		int totalwonprize = 0;

		// save the current ecoins
		for (int i = 0; i < MAX_USER_POS; i++) {
			pMsg.users[i].current_ecoins = guser.getuser(this->m_users[i])->ecoins[this->m_ectype];
			pMsg.users[i].cardcounts = guser.getuser(this->m_users[i])->m_cardcount;
		}

		unsigned char winpos = guser.getuser(this->m_winner)->m_gamepos;

		for (int i = 0; i < MAX_USER_POS; i++) {

			if (this->m_winner != this->m_users[i]) {
				// fight challenge
				if (winnerinfo->m_cardcount != 0 && guser.getuser(this->m_users[i])->fought == true) {

					int fightpay = ((guser.getuser(this->m_users[i])->m_cardcount - winnerinfo->m_cardcount) *
						this->m_ecinfo[this->m_ectype].foughtpercardaddecoins) + this->m_ecinfo[this->m_ectype].foughtbaseaddecoins;

					guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= fightpay;
					totalwonprize += fightpay;

					pMsg.users[i].fight = fightpay;

					this->msglog(INFO, "getwinner, deducted %d ecoins from %s (%s) for challenging a fight.",
						fightpay, guser.getuser(this->m_users[i])->name.c_str(), guser.getuser(this->m_users[i])->account.c_str());
				}

				// tongits regular payment
				if (winnerinfo->m_cardcount == 0) {

					guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= this->m_ecinfo[this->m_ectype].tongitaddecoins;
					totalwonprize += this->m_ecinfo[this->m_ectype].tongitaddecoins;
					pMsg.users[i].regular = this->m_ecinfo[this->m_ectype].tongitaddecoins;
					this->msglog(INFO, "getwinner, deducted %d ecoins from %s (%s) for tongits payment.",
						this->m_ecinfo[this->m_ectype].tongitaddecoins, guser.getuser(this->m_users[i])->name.c_str(), guser.getuser(this->m_users[i])->account.c_str());

				}
				else {
					guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= this->m_ecinfo[this->m_ectype].nontongitaddecoins;
					totalwonprize += this->m_ecinfo[this->m_ectype].nontongitaddecoins;
					pMsg.users[i].regular = this->m_ecinfo[this->m_ectype].nontongitaddecoins;
					this->msglog(INFO, "getwinner, deducted %d ecoins from %s (%s) for non-tongits payment.",
						this->m_ecinfo[this->m_ectype].nontongitaddecoins, guser.getuser(this->m_users[i])->name.c_str(), guser.getuser(this->m_users[i])->account.c_str());
				}

				// quadra prize
				if (winnerinfo->m_quadracount != 0) {
					guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= this->m_ecinfo[this->m_ectype].quadraaddecoins * winnerinfo->m_quadracount;
					totalwonprize += this->m_ecinfo[this->m_ectype].quadraaddecoins * winnerinfo->m_quadracount;
					pMsg.users[i].quadra = this->m_ecinfo[this->m_ectype].quadraaddecoins * winnerinfo->m_quadracount;
					this->msglog(INFO, "getwinner, deducted %d ecoins from %s (%s) for %d quadra.",
						this->m_ecinfo[this->m_ectype].quadraaddecoins * winnerinfo->m_quadracount, guser.getuser(this->m_users[i])->name.c_str()
						, guser.getuser(this->m_users[i])->account.c_str()
						, winnerinfo->m_quadracount);
				}

				// royal prize
				if (winnerinfo->m_royalcount != 0) {
					guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= this->m_ecinfo[this->m_ectype].royaladdecoins * winnerinfo->m_royalcount;
					totalwonprize += this->m_ecinfo[this->m_ectype].royaladdecoins * winnerinfo->m_royalcount;
					pMsg.users[i].royal = this->m_ecinfo[this->m_ectype].royaladdecoins * winnerinfo->m_royalcount;
					this->msglog(INFO, "getwinner, deducted %d ecoins from %s (%s) for %d royal.",
						this->m_ecinfo[this->m_ectype].royaladdecoins * winnerinfo->m_royalcount, guser.getuser(this->m_users[i])->name.c_str()
						, guser.getuser(this->m_users[i])->account.c_str()
						, winnerinfo->m_royalcount);

				}

				// aces prize
				if (winnerinfo->m_acecount != 0) {

					guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= winnerinfo->m_acecount * this->m_ecinfo[this->m_ectype].aceaddecoins;
					totalwonprize += winnerinfo->m_acecount * this->m_ecinfo[this->m_ectype].aceaddecoins;
					pMsg.users[i].ace = winnerinfo->m_acecount * this->m_ecinfo[this->m_ectype].aceaddecoins;
					this->msglog(INFO, "getwinner, deducted %d ecoins from %s (%s) for %d ace(s).",
						winnerinfo->m_acecount * this->m_ecinfo[this->m_ectype].aceaddecoins, guser.getuser(this->m_users[i])->name.c_str()
						, guser.getuser(this->m_users[i])->account.c_str()
						, winnerinfo->m_acecount);
				}

				// down cards
				if (guser.getuser(this->m_users[i])->m_isdowncard == false &&
					guser.getuser(this->m_users[i])->m_quadracount == 0 &&
					guser.getuser(this->m_users[i])->m_royalcount == 0
					) {

					//this->sendnotice(0, 1, "%s is burned.", guser.getuser(this->m_users[i])->name.c_str());

					guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= this->m_ecinfo[this->m_ectype].nodownaddecoins;
					totalwonprize += this->m_ecinfo[this->m_ectype].nodownaddecoins;
					pMsg.users[i].burned = this->m_ecinfo[this->m_ectype].nodownaddecoins;

					this->msglog(INFO, "getwinner, deducted %d ecoins from %s (%s) for no downcards payment.",
						this->m_ecinfo[this->m_ectype].nodownaddecoins, guser.getuser(this->m_users[i])->name.c_str(), guser.getuser(this->m_users[i])->account.c_str());

				}

				this->sendresult(this->m_users[i], 4);
			}
		}

		if (this->m_hitter == this->m_winner) {
			//this->sendnotice(0, 3, "%s got the hit amount of %d ecoins.", winnerinfo->name.c_str(), this->m_hitprizeecoins);
			float tax = c.getax() * (float)this->m_hitprizeecoins;
			int finalhitprize = (int)((float)this->m_hitprizeecoins - tax);
			totalwonprize += finalhitprize;
			pMsg.hitecoins = finalhitprize;
			this->m_hitprizeecoins = 0;
			this->m_hitter = 0;
			this->msglog(INFO, "getwinner, %s (%s) got the hit amount of %d ecoins, tax is %0.3f ecoins.", 
				winnerinfo->name.c_str(), winnerinfo->account.c_str(), finalhitprize, tax);
		}
		else {
			this->m_hitter = this->m_winner;
		}

		//this->sendnotice(this->m_winner, 5, "You won a total of %d ecoins.", totalwonprize);

		winnerinfo->ecoins[this->m_ectype] += totalwonprize;

		this->msglog(INFO, "getwinner, added total of %d ecoins winning prize to %s (%s).",
			totalwonprize, winnerinfo->name.c_str(), winnerinfo->account.c_str());

		pMsg.winnerpos = winpos;

		if (flag == 1) {
			this->datasend(this->m_winner, (unsigned char*)&pMsg, pMsg.hdr.len);
		}
		else {

			// send results
			for (int i = 0; i < MAX_USER_POS; i++) {
				this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
			}
		}

	}
}

void game::countusercards(uintptr_t userindex, bool isfoughtstatus)
{
	_USER_INFO* userinfo = guser.getuser(userindex);
	unsigned char _pos = userinfo->m_gamepos;

	if (_pos >= 3) {
		this->setstate(_GAME_STATE::_ENDED);
		return;
	}

	std::vector<std::vector <_PMSG_CARD_INFO>>::iterator _iter;
	std::vector <_PMSG_CARD_INFO>::iterator __iter;

	userinfo->resetcount();

	this->msglog(DEBUG, "countusercards, get ace count from user downcards");

	for (_iter = this->m_usercardinfo[_pos].down.begin(); _iter != this->m_usercardinfo[_pos].down.end(); _iter++) {
		std::vector <_PMSG_CARD_INFO> _down = *_iter;

		if (_down.size() == 0)
			continue;

		int typecounter = 0;
		int numcounter = 0;
		_PMSG_CARD_INFO rc = _down.front();

		for (__iter = _down.begin(); __iter != _down.end(); __iter++) {

			_PMSG_CARD_INFO c = *__iter;

			if (c.cardnum == 1) {
				guser.getuser(userindex)->m_acecount += 1;
			}
		}
	}
	

	this->msglog(DEBUG, "countusercards, get royal, quadra and ace count from group.");

	for (_iter = this->m_usercardinfo[_pos].group.begin(); _iter != this->m_usercardinfo[_pos].group.end(); _iter++) {
		std::vector <_PMSG_CARD_INFO> _group = *_iter;

		if (_group.size() == 0)
			continue;

		int typecounter = 0;
		int numcounter = 0;
		_PMSG_CARD_INFO rc = _group.front();

		guser.getuser(userindex)->m_cardquantity += _group.size();

		for (__iter = _group.begin(); __iter != _group.end(); __iter++) {

			_PMSG_CARD_INFO c = *__iter;

			if (c.cardnum == 1) {
				guser.getuser(userindex)->m_acecount += 1;
			}
			if (rc.cardtype == c.cardtype)
				typecounter++;
			if (rc.cardnum == c.cardnum)
				numcounter++;
		}

		userinfo->m_royalcount = typecounter / 5;
		userinfo->m_quadracount = numcounter / 4;
	}


	this->msglog(DEBUG, "countusercards, get ace count from user cards");

	guser.getuser(userindex)->m_cardquantity += this->m_usercardinfo[_pos].user.size();

	for (__iter = this->m_usercardinfo[_pos].user.begin(); __iter != this->m_usercardinfo[_pos].user.end(); __iter++) {

		_PMSG_CARD_INFO c = *__iter;

		if (c.cardnum == 1) {
			guser.getuser(userindex)->m_acecount += 1;
		}

		userinfo->m_cardcount += (c.cardnum > 10) ? 10 : c.cardnum;

	}


	this->msglog(DEBUG, "countusercards, %s (%s) cards count %d quantity %d quadra %d royal %d ace %d.",
		userinfo->name.c_str(), userinfo->account.c_str(), userinfo->m_cardcount, userinfo->m_cardquantity, userinfo->m_quadracount, userinfo->m_royalcount, userinfo->m_acecount);

	if (userinfo->m_cardcount == 0) {
		this->m_winner = userindex;
	}
	else {
		if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT && userinfo->m_isdowncard == true) {
			userinfo->canfight = true;
			this->sendfightmode(userindex, 1);
		}
		else if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT && userinfo->m_isdowncard == false &&
			(userinfo->m_quadracount > 0 || userinfo->m_royalcount > 0)) {
			userinfo->canfight = true;
			this->sendfightmode(userindex, 1);
		}
		else if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT && userinfo->m_isdowncard == false &&
			userinfo->m_quadracount == 0 && userinfo->m_royalcount == 0)
		{
			userinfo->canfight = false;
			this->sendfightmode(userindex, 0);
		}
	}
}

int  game::countstockcards()
{
	std::vector<_PMSG_CARD_INFO>::iterator _iterstock;
	int n = 0;
	for (_iterstock = this->vStockCards.begin(); _iterstock != this->vStockCards.end(); _iterstock++) {
		_PMSG_CARD_INFO c = *_iterstock;
		if (c.cardtype == 0) {
			continue;
		}
		n++;
	}
	return n;
}

int game::adddowncards(uintptr_t userindex, std::vector <_PMSG_CARD_INFO> v, bool isgroup)
{
	unsigned char _pos = guser.getuser(userindex)->m_gamepos;
	if (isgroup) {
		this->m_usercardinfo[_pos].group.push_back(v);
		return this->m_usercardinfo[_pos].group.size() - 1;
	}
	else {
		this->m_usercardinfo[_pos].down.push_back(v);
		return this->m_usercardinfo[_pos].down.size() - 1;
	}
}

bool game::getcardfromusercards(uintptr_t userindex, unsigned char pos, unsigned char* card)
{
	if (pos >= MAX_CARDS_PER_TYPE)
		return false;

	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	if (pos >= this->m_usercardinfo[_pos].user.size())
		return false;

	std::vector <_PMSG_CARD_INFO>::iterator iter;
	int n = 0;
	for (iter = this->m_usercardinfo[_pos].user.begin(); iter != this->m_usercardinfo[_pos].user.end(); iter++) {
		if (n == pos) {
			_PMSG_CARD_INFO c = *iter;
			card[0] = c.cardtype;
			card[1] = c.cardnum;
			return true;
		}
		n++;
	}

	return false;
}

bool game::addtousercards(uintptr_t userindex, unsigned char* card)
{
	unsigned char _pos = guser.getuser(userindex)->m_gamepos;
	_PMSG_CARD_INFO cardinfo;
	cardinfo.cardtype = card[0];
	cardinfo.cardnum = card[1];
	this->m_usercardinfo[_pos].user.push_back(cardinfo);
	return true;
}

bool game::chooserandomcard(uintptr_t userindex, unsigned char* rcard)
{
	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	std::vector <_PMSG_CARD_INFO> _vcards;
	std::vector <_PMSG_CARD_INFO>::iterator _iter;

	for (_iter = this->m_usercardinfo[_pos].user.begin(); _iter != this->m_usercardinfo[_pos].user.end(); _iter++) {
		_PMSG_CARD_INFO c = *_iter;
		if (c.cardtype == 0)
			continue;
		_vcards.push_back(c);
	}

	if (_vcards.size() == 0)
		return false;

	unsigned char rpos = rand() % _vcards.size(); // choose random card

	rcard[0] = _vcards[rpos].cardtype;
	rcard[1] = _vcards[rpos].cardnum;

	return true;
}

bool game::removefromusercards(uintptr_t userindex, unsigned char* pos, bool isfree)
{
	_PMSG_CARD_INFO _usercard = { 0 };
	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	if (pos == NULL)
		return false;

	std::vector <_PMSG_CARD_INFO>::iterator _iter;
	for (_iter = this->m_usercardinfo[_pos].user.begin(); _iter != this->m_usercardinfo[_pos].user.end(); _iter++) {
		_usercard = *_iter;
		if (_usercard.cardtype == pos[0] && _usercard.cardnum == pos[1]) {
			this->m_usercardinfo[_pos].user.erase(_iter);
			return true;
		}
	}

	return false;
}

bool game::removefromdropped(unsigned userpos, unsigned char* pos)
{
	if (pos == NULL)
		return false;
	std::vector<_DROPCARD_INFO>::iterator _iterdropped;
	for (_iterdropped = this->vDroppedCards.begin(); _iterdropped != this->vDroppedCards.end(); _iterdropped++) {
		_DROPCARD_INFO dropinfo = *_iterdropped;
		if (dropinfo.userpos == userpos && dropinfo.card.cardtype == pos[0] && dropinfo.card.cardnum == pos[1]) {
			this->vDroppedCards.erase(_iterdropped);
			return true;
			break;
		}
	}
	return false;
}

bool game::drawfromstock(uintptr_t userindex)
{
	if (isactionvalid(userindex, _ACTIONS::_DRAW) == false)
		return false;

	_PMSG_CARD_INFO card = { 0 };
	int n = 0, ctr = 0;

	std::vector<_PMSG_CARD_INFO>::iterator _iterstock;

	for (_iterstock = this->vStockCards.begin(); _iterstock != this->vStockCards.end(); _iterstock++) {
		if (n == 0) {
			card = *_iterstock;
			if (card.cardtype == 0) {
				ctr++;
				continue;
			}
			this->vStockCards[ctr].cardtype = 0;
			break;
		}
		ctr++;
		n++;
	}


	if (card.cardtype == 0) {
		this->msglog(DEBUG, "removefromstock,  card.cardtype == 0.");
		return false;
	}


	this->sendstockcardcount();

	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	this->m_usercardinfo[_pos].user.push_back(card);

	this->m_usercardinfo[_pos].lastdrawcard.cardtype = card.cardtype;
	this->m_usercardinfo[_pos].lastdrawcard.cardnum = card.cardnum;

	_PMSG_DRAW_CARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;
	pMsg.hdr.len = sizeof(_PMSG_DRAW_CARD_ANS);
	pMsg.sub = 0;
	pMsg.init = 0;

	pMsg.userpos = guser.getuser(userindex)->m_gamepos;

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (userindex == this->m_users[i]) {
			pMsg.cardtype = card.cardtype;
			pMsg.cardnum = card.cardnum;
			this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
		}
		else {
			pMsg.cardtype = 0;
			pMsg.cardnum = 0;
			this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
		}
	}

	if (this->countstockcards() == 0) {
		this->m_active_status |= (int)_ACTIVE_STATE::_STOCKZERO;
		this->msglog(DEBUG, "%s (%s), Flag active status _STOCKZERO.", guser.getuser(this->m_active_userindex)->name.c_str(), guser.getuser(this->m_active_userindex)->account.c_str());
		this->sendtimeoutleft(MAX_MSECONDS_GROUPCARD_TIMEOUT);
		this->m_gametick = GetTickCount64() + MAX_MSECONDS_GROUPCARD_TIMEOUT;
		this->sendnotice(0, 0, "You have %d sec. to group your cards, ungrouped cards will be counted.", MAX_MSECONDS_GROUPCARD_TIMEOUT / 1000);
	}

	this->m_active_status |= (int)_ACTIVE_STATE::_DRAWN;
	this->msglog(DEBUG, "%s (%s), Flag active status _DRAWN.", guser.getuser(this->m_active_userindex)->name.c_str(), guser.getuser(this->m_active_userindex)->account.c_str());

	// check sagasa
	/*
	std::vector<std::vector <_PMSG_CARD_INFO>>::iterator iter;
	n = 0;
	for (iter = this->m_usercardinfo[_pos].down.begin(); iter != this->m_usercardinfo[_pos].down.end(); iter++) {
		std::vector <_PMSG_CARD_INFO> v = *iter;
		if (v[0].cardnum == card.cardnum) {
			if (this->sapawcard(userindex, _pos, n, 1, (unsigned char*)&card, false, true)) {
				this->msglog(INFO, "drawfromstock, %s sagasa, card %d %d", guser.getuser(userindex)->name.c_str(), card.cardtype, card.cardnum);
				return true;
			}
		}
		n++;
	}*/

	return true;
}

bool game::isactionvalid(uintptr_t userindex, _ACTIONS action)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (this->getstate() != _GAME_STATE::_STARTED) {
		this->msglog(DEBUG, "user %llu requested an action but game is not started.", userindex);
		return false;
	}

	if (userinfo->isauto == false && GetTickCount64() < userinfo->lastactiontick) {
		this->msglog(DEBUG, "user %llu requested an action but still under time restriction.", userindex);
		return false;
	}

	switch (action) {
	case _ACTIONS::_DRAW:

		if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT || 
			this->m_active_status & (int)_ACTIVE_STATE::_STOCKZERO) // no message needed
			return false;

		if (userindex != this->m_active_userindex) {
			this->sendnotice(userindex, 7, "It's not your turn yet!");
			this->msglog(DEBUG, "user %llu requested to draw card but current active is user %llu.", userindex, this->m_active_userindex);
			return false;
		}

		if (this->countstockcards() == 0) {
			this->msglog(DEBUG, "user %llu requested to draw card but current card count is zero.", userindex);
			return false;
		}

		if (this->m_active_status & (int)_ACTIVE_STATE::_DROPPED || this->m_active_status & (int)_ACTIVE_STATE::_DRAWN
			|| this->m_active_status & (int)_ACTIVE_STATE::_CHOWED/* || this->m_active_status == _ACTIVE_STATE::_DOWNED*/
			|| this->m_active_status & (int)_ACTIVE_STATE::_STOCKZERO || this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT
			) {
			this->msglog(DEBUG, "user %llu requested to draw card but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		break;
	case _ACTIONS::_CHOW:

		if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT ||
			this->m_active_status & (int)_ACTIVE_STATE::_STOCKZERO) // no message needed
			return false;

		if (userindex != this->m_active_userindex) {
			this->sendnotice(userindex, 7, "It's not your turn yet!");
			this->msglog(DEBUG, "user %llu requested to chow card but current active is user %llu.", userindex, this->m_active_userindex);
			return false;
		}

		if (this->m_active_status & (int)_ACTIVE_STATE::_DROPPED ||
			this->m_active_status & (int)_ACTIVE_STATE::_DRAWN ||
			this->m_active_status & (int)_ACTIVE_STATE::_CHOWED ||
			this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT ||
			this->m_active_status & (int)_ACTIVE_STATE::_DOWNED) {
			this->msglog(DEBUG, "user %llu requested to chow card but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		break;

	case _ACTIONS::_DOWN:

		if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT) // no message needed
			return false;

		if (userindex != this->m_active_userindex) {
			this->sendnotice(userindex, 7, "It's not your turn yet!");
			this->msglog(DEBUG, "user %llu requested to chow card but current active is user %llu.", userindex, this->m_active_userindex);
			return false;
		}

		if (userinfo->m_isdowncard == true) {
			this->msglog(DEBUG, "user %llu requested to down cards but the user already have a down.", userindex);
			return false;
		}

		if (this->m_active_status & (int)_ACTIVE_STATE::_CHOWED ||
			this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT ||
			this->m_active_status & (int)_ACTIVE_STATE::_DOWNED) {
			this->msglog(DEBUG, "user %llu requested to down cards but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		break;

	case _ACTIONS::_GROUP:

		if (this->m_active_status & (int)_ACTIVE_STATE::_RESET) {
			this->msglog(DEBUG, "user %llu requested to group cards but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		break;

	case _ACTIONS::_UNGROUP:

		if (this->m_active_status & (int)_ACTIVE_STATE::_RESET) {
			this->msglog(DEBUG, "user %llu requested to ungroup cards but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		break;

	case _ACTIONS::_DROP:

		if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT) // no message needed
			return false;

		if (userindex != this->m_active_userindex) {
			this->sendnotice(userindex, 7, "It's not your turn yet!");
			this->msglog(DEBUG, "user %llu requested to drop card but current active is user %llu.", userindex, this->m_active_userindex);
			return false;
		}

		if (!(this->m_active_status & (int)_ACTIVE_STATE::_DRAWN) && !(this->m_active_status & (int)_ACTIVE_STATE::_CHOWED))
		{
			this->sendnotice(userindex, 7, "You have'nt drawn nor chow card(s) yet!");
			this->msglog(DEBUG, "user %llu requested to drop card but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		if (this->m_active_status == (int)_ACTIVE_STATE::_NONE || 
			this->m_active_status & (int)_ACTIVE_STATE::_DROPPED ||
			this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT) {
			this->msglog(DEBUG, "user %llu requested to drop card but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		break;
	case _ACTIONS::_FIGHT:

		if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT ||
			this->m_active_status & (int)_ACTIVE_STATE::_STOCKZERO) // no message needed
			return false;

		if (userindex != this->m_active_userindex) {
			this->sendnotice(userindex, 7, "It's not your turn yet!");
			this->msglog(DEBUG, "user %llu requested to fight but current active is user %llu.", userindex, this->m_active_userindex);
			return false;
		}

		if (userinfo->m_isdowncard == false) {
			this->sendnotice(userindex, 7, "You don't have a house yet!");
			this->msglog(DEBUG, "user %llu requested to fight cards but the user has no down.", userindex);
			return false;
		}

		if (userinfo->canfight == false) {
			this->msglog(DEBUG, "user %llu requested to fight cards but the user can't fight at this moment.", userindex);
			return false;
		}

		if (userinfo->fought == true) {
			this->sendnotice(userindex, 7, "You already fought!");
			return false;
		}

		if (this->m_active_status != (int)_ACTIVE_STATE::_NONE) {
			this->msglog(DEBUG, "user %llu requested to fight but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT) {
			this->msglog(DEBUG, "user %llu requested to fight but current active status is %d.", userindex, this->m_active_status);
			return false;
		}
		break;

	case _ACTIONS::_FIGHT2:

		if (this->m_active_status & (int)_ACTIVE_STATE::_STOCKZERO) // no message needed
			return false;


		if (userinfo->m_isdowncard == false) {
			// check if user has royal or quadra to allow
			if (userinfo->m_quadracount == 0 && userinfo->m_royalcount == 0) {
				this->sendnotice(userindex, 7, "You don't have a house nor quadra nor royal!");
				this->msglog(DEBUG, "user %llu requested to fight2 cards but the user has no down/quadra/royal.", userindex);
				return false;
			}
		}

		if (userinfo->fought == true) {
			this->sendnotice(userindex, 7, "You already fought!");
			return false;
		}

		if (!(this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT)) {
			this->msglog(DEBUG, "user %llu requested to fight2 but current active status is %d.", userindex, this->m_active_status);
			return false;
		}

		break;

	case _ACTIONS::_SAPAW:

		if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT)
			return false;

		if (userindex != this->m_active_userindex) {
			this->sendnotice(userindex, 7, "It's not your turn yet!");
			this->msglog(DEBUG, "user %llu requested to sapaw card but current active is user %llu.", userindex, this->m_active_userindex);
			return false;
		}

		if (this->m_active_status & (int)_ACTIVE_STATE::_DROPPED ||
			this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT
			) {
			this->msglog(DEBUG, "user %llu requested to sapaw card but current active status is %d.", userindex, this->m_active_status);
			return false;
		}
		break;
	}

	userinfo->lastactiontick = GetTickCount64() + 500;

	return true;
}

bool game::trysapawcard(uintptr_t userindex, unsigned char* cardpos)
{
	for (int userpos = 0; userpos < 3; userpos++) {
		int size = this->m_usercardinfo[userpos].down.size();
		for (int downpos = 0; downpos < size; downpos++) {
			if (this->sapawcard(userindex, userpos, downpos, 1, cardpos,
				guser.getuser(userindex)->isdc() ? false : true,
				true)) {
				return true;
			}
		}
	}
	return false;
}

bool game::sapawcard(uintptr_t userindex, unsigned userpos, unsigned downpos, unsigned char count, unsigned char* cardpos, bool test, bool skipactioncheck)
{
	if (cardpos == NULL || count == 0) {
		this->msglog(DEBUG, "sapawcard, count/selected card(s) is NULL.");
		return false;
	}

	if (count >= MAX_CARDS_PER_TYPE) {
		this->msglog(DEBUG, "sapawcard, count %d of selected cards exceeded the limit.", count);
		return false;
	}

	if (userpos >= 3) {
		this->msglog(DEBUG, "sapawcard, userpos is out of bound.", userpos);
		return false;
	}

	if (skipactioncheck == false && isactionvalid(userindex, _ACTIONS::_SAPAW) == false)
		return false;

	if (downpos >= this->m_usercardinfo[userpos].down.size()) {
		this->msglog(DEBUG, "sapawcard, downpos %d >= this->mUserDownCards.size().", downpos);
		return false;
	}

	std::vector <_PMSG_CARD_INFO> _down = this->m_usercardinfo[userpos].down[downpos];

	this->msglog(DEBUG, "sapawcard, userpos %d pos %d cards %d.", userpos, downpos, _down.size());

	if (_down.size() == 0) {
		return false;
	}

	int requserpos = guser.getuser(userindex)->m_gamepos;
	int usergamepos = userpos;

	// check if selected cards do exist in user card list
	for (int n = 0; n < count; n++) {
		_PMSG_CARD_INFO* _card = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
		unsigned char _cardinfo[2];
		_cardinfo[0] = _card->cardtype;
		_cardinfo[1] = _card->cardnum;

		this->msglog(DEBUG, "sapawcard, selected card %d/%d (%d) requserpos %d.", _card->cardtype, _card->cardnum, n, requserpos);

		if (this->getposfromusercard(userindex, _cardinfo) == -1) {
			this->msglog(DEBUG, "sapawcard failed, card %d/%d doest not exist in requserpos %d.", _card->cardtype, _card->cardnum, requserpos);
			return false;
		}
	}

	bool issamenumber = true;
	bool issametype = true;

	unsigned char c[2];

	this->msglog(DEBUG, "sapawcard, get card to drop info...");

	_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos);

	c[0] = _cardinfo->cardtype;
	c[1] = _cardinfo->cardnum;

	this->msglog(DEBUG, "sapawcard, get card to drop info done, %d %d.", c[0], c[1]);

	std::vector <_PMSG_CARD_INFO>::iterator _viter;

	_viter = _down.begin();
	_PMSG_CARD_INFO _c = *_viter;
	_viter++;
	for (; _viter != _down.end(); _viter++) {
		_PMSG_CARD_INFO __c = *_viter;
		if (_c.cardnum != __c.cardnum) {
			issamenumber = false;
			break;
		}
	}

	this->msglog(DEBUG, "sapawcard, issamenumber %d.", issamenumber);


	_viter = _down.begin();
	_viter++;
	for (; _viter != _down.end(); _viter++) {
		_PMSG_CARD_INFO __c = *_viter;
		if (_c.cardtype != __c.cardtype) {
			issametype = false;
			break;
		}
	}

	this->msglog(DEBUG, "sapawcard, issametype %d.", issametype);


	if (issamenumber && count == 1) { // only possible to be formed is quadra with one selected card

		this->msglog(DEBUG, "sapawcard, issamenumber...");


		if (c[1] != _c.cardnum) {

			this->msglog(DEBUG, "sapawcard, quadra failed bec. selected card num. %d differ with downcard card num. %d.",
				c[1], _c.cardnum);

			return false;
		}

		// quadra formed
		if (test == true) {
			return true;
		}

		// delete the card pointer from user list
		if (!this->removefromusercards(userindex, c)) {

			this->msglog(DEBUG, "sapawcard, removefromusercards failed with card %d %d.",
				c[0], c[1]);

			return false;
		}

		// get the card pointer from user list
		_PMSG_CARD_INFO* cardinfo = (_PMSG_CARD_INFO*)cardpos;

		// add the sapaw card to downcard list
		_PMSG_CARD_INFO _cardinfo;
		_cardinfo.cardtype = cardinfo->cardtype;
		_cardinfo.cardnum = cardinfo->cardnum;

		this->m_usercardinfo[userpos].down[downpos].push_back(_cardinfo);

		if (userpos == guser.getuser(userindex)->m_gamepos) {
			if (this->m_active_status & (int)_ACTIVE_STATE::_DRAWN) {
				if (this->m_usercardinfo[userpos].lastdrawcard.cardtype != 0 && 
					this->m_usercardinfo[userpos].lastdrawcard.cardtype == cardinfo->cardtype &&
					this->m_usercardinfo[userpos].lastdrawcard.cardnum == cardinfo->cardnum) {
					this->sendnotice(0, 3, "%s Sagasa!", guser.getuser(userindex)->name.c_str());
					this->msglog(INFO, "%s (%s) is sagasa.", guser.getuser(userindex)->name.c_str(), guser.getuser(userindex)->account.c_str());
					for (int i = 0; i < 3; i++) {
						if (userpos == i)
							guser.getuser(this->m_users[i])->ecoins[this->m_ectype] += this->m_ecinfo[this->m_ectype].sagasaaddecoins * 2;
						else
							guser.getuser(this->m_users[i])->ecoins[this->m_ectype] -= this->m_ecinfo[this->m_ectype].sagasaaddecoins;
						this->sendresult(this->m_users[i], 3);
					}
					this->senduserecoinsinfo();
				}
			}
		}

		// send result to users
		unsigned char buf[100] = { 0 };
		_PMSG_SAPAWCARD_ANS pMsg = { 0 };
		pMsg.hdr.c = 0xC1;
		pMsg.hdr.h = 0xF3;
		int size = sizeof(_PMSG_SAPAWCARD_ANS);
		pMsg.hdr.len = size + sizeof(_PMSG_CARD_INFO);
		pMsg.sub = 0x04;
		pMsg.count = 1;
		pMsg.downpos = downpos;
		pMsg.gamepos = requserpos;
		pMsg.usergamepos = usergamepos;

		memcpy(&buf[0], (unsigned char*)&pMsg, size);
		memcpy(&buf[size], cardpos, sizeof(_PMSG_CARD_INFO));

		for (int i = 0; i < MAX_USER_POS; i++) {
			this->datasend(this->m_users[i], buf, pMsg.hdr.len);
		}

		// disable user's fight mode when the user's down cards has sapaw
		_USER_INFO* _userinfo = guser.getuser(this->m_users[userpos]);
		_userinfo->canfight = false;
		if (this->m_users[userpos] == userindex)
			_userinfo->isselfblock = true;
		this->sendfightmode(this->m_users[userpos], 0);

		this->countusercards(userindex);
		this->sendusercardcountsinfo(userindex);
		this->getwinner();

		return true;
	}
	else if (issametype) {

		this->msglog(DEBUG, "sapawcard, issametype...");


		std::vector <unsigned char> __v; // card number list

		for (int n = 0; n < count; n += 1) {
			_cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			__v.push_back(_cardinfo->cardnum); // add sapaw cards in list

			if (c[0] != _cardinfo->cardtype) { // recheck card type of sapaw cards
				return false;
			}
		}

		for (_viter = _down.begin(); _viter != _down.end(); _viter++) {

			_PMSG_CARD_INFO __c = *_viter;

			this->msglog(DEBUG, "sapawcard, downcard num %d.", __c.cardnum);

			if (c[0] != __c.cardtype) { // recheck card type of downcards
				return false;
			}

			__v.push_back(__c.cardnum); // add downcards in list
		}

		// sort desc
		std::sort(__v.begin(), __v.end(), std::greater<unsigned char>());

		for (int i = 0; i < __v.size() - 1; i++) {
			if ((__v[i] - __v[i + 1]) != 1) {

				this->msglog(DEBUG, "straight failed, card num. %d differ more than 1 with card num. %d.",
					__v[i], __v[i + 1]);

				return false;
			}
		}


		// straight formed
				// quadra formed
		if (test == true) {
			return true;
		}

		for (int n = 0; n < count; n += 1) {
			// get the card pointer from user list

			_PMSG_CARD_INFO* cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));

			// add the sapaw card to downcard list
			_PMSG_CARD_INFO _cardinfo;
			_cardinfo.cardtype = cardinfo->cardtype;
			_cardinfo.cardnum = cardinfo->cardnum;
			this->m_usercardinfo[userpos].down[downpos].push_back(_cardinfo);
		}


		for (int n = 0; n < count; n += 1) {
			_cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			unsigned char _c[2];
			_c[0] = _cardinfo->cardtype;
			_c[1] = _cardinfo->cardnum;

			// delete the card pointer from user list
			if (!this->removefromusercards(userindex, _c)) {

				this->msglog(DEBUG, "sapawcard, removefromusercards failed with card %d %d.",
					_c[0], _c[1]);

				return false;
			}
		}

		// send result to users
		unsigned char buf[100] = { 0 };
		_PMSG_SAPAWCARD_ANS pMsg = { 0 };
		pMsg.hdr.c = 0xC1;
		pMsg.hdr.h = 0xF3;
		int size = sizeof(_PMSG_SAPAWCARD_ANS);
		pMsg.hdr.len = size + (sizeof(_PMSG_CARD_INFO) * count);
		pMsg.sub = 0x04;
		pMsg.count = count;
		pMsg.downpos = downpos;
		pMsg.gamepos = requserpos;
		pMsg.usergamepos = usergamepos;

		memcpy(&buf[0], (unsigned char*)&pMsg, size);
		memcpy(&buf[size], cardpos, sizeof(_PMSG_CARD_INFO) * count);

		for (int i = 0; i < MAX_USER_POS; i++) {
			this->datasend(this->m_users[i], buf, pMsg.hdr.len);
		}

		// disable user's fight mode when the user's down cards has sapaw
		_USER_INFO* _userinfo = guser.getuser(this->m_users[userpos]);
		_userinfo->canfight = false;
		if (this->m_users[userpos] == userindex)
			_userinfo->isselfblock = true;
		this->sendfightmode(this->m_users[userpos], 0);

		this->countusercards(userindex);
		this->sendusercardcountsinfo(userindex);
		this->getwinner();

		return true;
	}


	this->msglog(DEBUG, "sapawcard, failed bec. no place to sapaw card %d %d.", cardpos[0], cardpos[1]);

	return false;
}

bool game::ungroupcards(uintptr_t userindex, int downpos)
{
	if (isactionvalid(userindex, _ACTIONS::_UNGROUP) == false)
		return false;

	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	if (downpos >= this->m_usercardinfo[_pos].group.size()) {
		this->msglog(DEBUG, "ungroupcards, downpos %d is out of bound.", downpos);
		return false;
	}

	std::vector <_PMSG_CARD_INFO> _v = this->m_usercardinfo[_pos].group[downpos];
	std::vector <_PMSG_CARD_INFO>::iterator _viter;

	for (_viter = _v.begin(); _viter != _v.end(); _viter++) {

		_PMSG_CARD_INFO _card = *_viter;

		unsigned char cc[2];
		cc[0] = _card.cardtype;
		cc[1] = _card.cardnum;

		if (!this->addtousercards(userindex, cc)) {
			this->msglog(DEBUG, "ungroupcards, addtousercards failed.", userindex);
			return false;
		}

	}

	this->m_usercardinfo[_pos].group[downpos].clear();

	_PMSG_UNGRPCARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;
	pMsg.hdr.len = sizeof(_PMSG_UNGRPCARD_ANS);
	pMsg.sub = 0x06;
	pMsg.downpos = downpos;
	pMsg.gamepos = _pos;

	this->datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);

	this->countusercards(userindex);
	
	return true;
}

bool game::groupcards(uintptr_t userindex, unsigned char count, unsigned char* cardpos)
{
	if (cardpos == NULL) {
		this->msglog(DEBUG, "groupcards, drop/selected card(s) is NULL.");
		return false;
	}

	if (count >= MAX_CARDS_PER_TYPE) {
		this->msglog(DEBUG, "groupcards, count %d of selected cards exceeded the limit.", count);
		return false;
	}

	if (isactionvalid(userindex, _ACTIONS::_GROUP) == false)
		return false;

	if (count < 3) {
		this->msglog(DEBUG, "3 cards atleast is needed to group a carda.");
		return false;
	}

	int requserpos = guser.getuser(userindex)->m_gamepos;

	// check if selected cards do exist in user card list
	for (int n = 0; n < count; n++) {
		_PMSG_CARD_INFO* _card = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
		unsigned char _cardinfo[2];
		_cardinfo[0] = _card->cardtype;
		_cardinfo[1] = _card->cardnum;

		this->msglog(DEBUG, "groupcards, selected card %d/%d (%d) requserpos %d.", _card->cardtype, _card->cardnum, n, requserpos);

		if (this->getposfromusercard(userindex, _cardinfo) == -1) {
			this->msglog(DEBUG, "groupcards failed, card %d/%d doest not exist in requserpos %d.", _card->cardtype, _card->cardnum, requserpos);
			return false;
		}
	}

	unsigned char buf[100] = { 0 };
	_PMSG_GRPCARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;

	int size = sizeof(_PMSG_GRPCARD_ANS);


	bool issamenumber = true;
	bool issametype = true;

	unsigned char c[2];

	_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos);// + (0 * sizeof(_PMSG_CARD_INFO)));
	c[0] = _cardinfo->cardtype;
	c[1] = _cardinfo->cardnum;

	for (int n = 1; n < count; n += 1) {
		_cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
		if (c[1] != _cardinfo->cardnum) { // card number
			issamenumber = false;
			break;
		}
	}

	for (int n = 1; n < count; n += 1) {
		_cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
		if (c[0] != _cardinfo->cardtype) { // card type
			issametype = false;
			break;
		}
	}

	if (issamenumber) {
		std::vector <_PMSG_CARD_INFO> _vdowncards;

		for (int n = 0; n < count; n++) {
			_PMSG_CARD_INFO* _card = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			_PMSG_CARD_INFO _cinfo;
			_cinfo.cardtype = _card->cardtype;
			_cinfo.cardnum = _card->cardnum;
			_vdowncards.push_back(_cinfo);
		}

		for (int n = 0; n < count; n += 1) {
			_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			unsigned char __card[2];
			__card[0] = _cardinfo->cardtype;
			__card[1] = _cardinfo->cardnum;

			memcpy(&buf[size], (unsigned char*)_cardinfo, sizeof(_PMSG_CARD_INFO));
			size += sizeof(_PMSG_CARD_INFO);

			// remove card from user list
			this->removefromusercards(userindex, __card);
		}

		int downpos = this->adddowncards(userindex, _vdowncards, true);

		this->msglog(DEBUG, "groupcards, grouped trio/quadra at position %d size %d.", downpos, _vdowncards.size());

		pMsg.hdr.len = size;
		pMsg.sub = 0x05;
		pMsg.count = count;
		pMsg.grppos = downpos;
		pMsg.gamepos = requserpos;

		memcpy(&buf[0], (unsigned char*)&pMsg, sizeof(_PMSG_GRPCARD_ANS));

		this->datasend(userindex, buf, pMsg.hdr.len);

		this->countusercards(userindex);

		return true;
	}
	else if (issametype) {

		std::vector <unsigned char> _v;

		for (int n = 0; n < count; n++) {
			_PMSG_CARD_INFO* _card = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			this->msglog(DEBUG, "groupcards, user selected card %d %d.", _card->cardtype, _card->cardnum);
			_v.push_back(_card->cardnum);
		}

		// sort desc
		std::sort(_v.begin(), _v.end(), std::greater<unsigned char>());

		for (int i = 0; i < count - 1; i++) {
			if ((_v[i] - _v[i + 1]) != 1) {

				this->msglog(DEBUG, "straight failed, card num. %d differ more than 1 with card num. %d.",
					_v[i], _v[i + 1]);

				return false;
			}
		}

		// its straight
		std::vector <_PMSG_CARD_INFO> _vdowncards;

		// sort asc
		std::sort(_v.begin(), _v.end(), std::less<unsigned char>());


		_PMSG_CARD_INFO cc = { 0 };

		for (int i = 0; i < _v.size(); i++) {

			cc.cardnum = _v[i];
			cc.cardtype = c[0];

			memcpy(&buf[size], (unsigned char*)&cc, sizeof(_PMSG_CARD_INFO));
			size += sizeof(_PMSG_CARD_INFO);

			this->msglog(DEBUG, "straight formed cards, type %d num %d size %d.", cc.cardtype, cc.cardnum, size);


			_vdowncards.push_back(cc);
		}


		for (int n = 0; n < count; n += 1) {
			_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			unsigned char __card[2];
			__card[0] = _cardinfo->cardtype;
			__card[1] = _cardinfo->cardnum;
			// remove card from user list
			this->removefromusercards(userindex, __card);
		}

		int downpos = this->adddowncards(userindex, _vdowncards, true);

		this->msglog(DEBUG, "groupcards, grouped straight at position %d size %d.", downpos, _vdowncards.size());

		pMsg.hdr.len = size;
		pMsg.sub = 0x05;
		pMsg.count = count;
		pMsg.gamepos = requserpos;
		pMsg.grppos = downpos;

		memcpy(&buf[0], (unsigned char*)&pMsg, sizeof(_PMSG_GRPCARD_ANS));

		this->datasend(userindex, buf, pMsg.hdr.len);

		this->countusercards(userindex);

		return true;
	}

	this->msglog(DEBUG, "groupcards, failed to create trio/quadra/straight from the selected cards.");

	return false;
}

bool game::downcards(uintptr_t userindex, unsigned char count, unsigned char* cardpos)
{
	if (cardpos == NULL) {
		this->msglog(DEBUG, "downcards, drop/selected card(s) is NULL.");
		return false;
	}

	if (count >= MAX_CARDS_PER_TYPE) {
		this->msglog(DEBUG, "downcards, count %d of selected cards exceeded the limit.", count);
		return false;
	}

	if (isactionvalid(userindex, _ACTIONS::_DOWN) == false)
		return false;

	if (count < 3) {
		this->msglog(DEBUG, "3 cards atleast is needed to down a card.");
		return false;
	}

	int requserpos = guser.getuser(userindex)->m_gamepos;

	// check if selected cards do exist in user card list
	for (int n = 0; n < count; n++) {
		_PMSG_CARD_INFO* _card = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
		unsigned char _cardinfo[2];
		_cardinfo[0] = _card->cardtype;
		_cardinfo[1] = _card->cardnum;

		this->msglog(DEBUG, "downcards, selected card %d/%d (%d) requserpos %d.", _card->cardtype, _card->cardnum, n, requserpos);

		if (this->getposfromusercard(userindex, _cardinfo) == -1) {
			this->msglog(DEBUG, "downcards failed, card %d/%d doest not exist in requserpos %d.", _card->cardtype, _card->cardnum, requserpos);
			return false;
		}
	}

	unsigned char buf[100] = { 0 };
	_PMSG_DOWNCARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;

	int size = sizeof(_PMSG_DOWNCARD_ANS);


	bool issamenumber = true;
	bool issametype = true;

	unsigned char c[2];

	_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos);// + (0 * sizeof(_PMSG_CARD_INFO)));
	c[0] = _cardinfo->cardtype;
	c[1] = _cardinfo->cardnum;

	for (int n = 1; n < count; n += 1) {
		_cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
		if (c[1] != _cardinfo->cardnum) { // card number
			issamenumber = false;
			break;
		}
	}

	for (int n = 1; n < count; n += 1) {
		_cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
		if (c[0] != _cardinfo->cardtype) { // card type
			issametype = false;
			break;
		}
	}

	if (issamenumber) {
		std::vector <_PMSG_CARD_INFO> _vdowncards;

		for (int n = 0; n < count; n++) {
			_PMSG_CARD_INFO* _card = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			_PMSG_CARD_INFO _cinfo;
			_cinfo.cardtype = _card->cardtype;
			_cinfo.cardnum = _card->cardnum;
			_vdowncards.push_back(_cinfo);
		}

		for (int n = 0; n < count; n += 1) {
			_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			unsigned char __card[2];
			__card[0] = _cardinfo->cardtype;
			__card[1] = _cardinfo->cardnum;

			memcpy(&buf[size], (unsigned char*)_cardinfo, sizeof(_PMSG_CARD_INFO));
			size += sizeof(_PMSG_CARD_INFO);

			// remove card from user list
			this->removefromusercards(userindex, __card);
		}

		int downpos = this->adddowncards(userindex, _vdowncards);

		pMsg.hdr.len = size;
		pMsg.sub = 0x03;
		pMsg.count = count;
		pMsg.downpos = downpos;
		pMsg.gamepos = requserpos;

		memcpy(&buf[0], (unsigned char*)&pMsg, sizeof(_PMSG_DOWNCARD_ANS));

		for (int i = 0; i < MAX_USER_POS; i++) {
			this->datasend(this->m_users[i], buf, pMsg.hdr.len);
		}

		this->m_active_status |= (int)_ACTIVE_STATE::_DOWNED;
		this->msglog(DEBUG, "%s (%s), Flag active status _DOWNED.", guser.getuser(this->m_active_userindex)->name.c_str(), guser.getuser(this->m_active_userindex)->account.c_str());
		guser.getuser(userindex)->m_isdowncard = true;

		this->countusercards(userindex);
		this->sendusercardcountsinfo(userindex);
		this->getwinner();

		return true;
	}
	else if (issametype) {

		std::vector <unsigned char> _v;

		for (int n = 0; n < count; n++) {
			_PMSG_CARD_INFO* _card = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			this->msglog(DEBUG, "downcards, user selected card %d %d.", _card->cardtype, _card->cardnum);
			_v.push_back(_card->cardnum);
		}

		// sort desc
		std::sort(_v.begin(), _v.end(), std::greater<unsigned char>());

		for (int i = 0; i < count - 1; i++) {
			if ((_v[i] - _v[i + 1]) != 1) {

				this->msglog(DEBUG, "straight failed, card num. %d differ more than 1 with card num. %d.",
					_v[i], _v[i + 1]);

				return false;
			}
		}

		// its straight
		std::vector <_PMSG_CARD_INFO> _vdowncards;

		// sort asc
		std::sort(_v.begin(), _v.end(), std::less<unsigned char>());


		_PMSG_CARD_INFO cc = { 0 };

		for (int i = 0; i < _v.size(); i++) {

			cc.cardnum = _v[i];
			cc.cardtype = c[0];

			memcpy(&buf[size], (unsigned char*)&cc, sizeof(_PMSG_CARD_INFO));
			size += sizeof(_PMSG_CARD_INFO);

			this->msglog(DEBUG, "straight formed cards, type %d num %d size %d.", cc.cardtype, cc.cardnum, size);


			// add to chowlist the cards with its new pointer
			_vdowncards.push_back(cc);
		}


		for (int n = 0; n < count; n += 1) {
			_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			unsigned char __card[2];
			__card[0] = _cardinfo->cardtype;
			__card[1] = _cardinfo->cardnum;
			// remove card from user list then delte the pointers bec. we generated new pointer for the cards
			this->removefromusercards(userindex, __card, true);
		}

		int downpos = this->adddowncards(userindex, _vdowncards);

		pMsg.hdr.len = size;
		pMsg.sub = 0x03;
		pMsg.count = count;
		pMsg.gamepos = requserpos;
		pMsg.downpos = downpos;

		memcpy(&buf[0], (unsigned char*)&pMsg, sizeof(_PMSG_DOWNCARD_ANS));

		for (int i = 0; i < MAX_USER_POS; i++) {
			this->datasend(this->m_users[i], buf, pMsg.hdr.len);
		}

		this->m_active_status |= (int)_ACTIVE_STATE::_DOWNED;
		this->msglog(DEBUG, "%s (%s), Flag active status _DOWNED.", guser.getuser(this->m_active_userindex)->name.c_str(), 
			guser.getuser(this->m_active_userindex)->account.c_str());
		guser.getuser(userindex)->m_isdowncard = true;

		this->countusercards(userindex);
		this->sendusercardcountsinfo(userindex);
		this->getwinner();

		return true;
	}

	this->msglog(DEBUG, "Downcards, failed to create trio/quadra/straight from the selected cards.");
	return false;
}

bool game::gamestart(uintptr_t userindex)
{
	unsigned char pos = guser.getuser(userindex)->m_gamepos;
	this->initdrawcards[pos] = 1;
	return true;
}

bool game::usercards(uintptr_t userindex)
{
	this->sendusercards(userindex);
	return true;
}

bool game::chowcard(uintptr_t userindex, unsigned char userpos, unsigned char* pos, unsigned char count, unsigned char* cardpos)
{
	if (pos == NULL || cardpos == NULL) {
		this->msglog(DEBUG, "chowcard, drop/selected card(s) is NULL.");
		return false;
	}

	if (count >= MAX_CARDS_PER_TYPE) {
		this->msglog(DEBUG, "chowcard, count %d of selected cards exceeded the limit.", count);
		return false;
	}

	std::vector<_DROPCARD_INFO>::iterator iter;

	if (isactionvalid(userindex, _ACTIONS::_CHOW) == false)
		return false;

	if (count <= 1) {
		this->msglog(DEBUG, "2 cards atleast is needed to chow a card.");
		return false;
	}

	int requserpos = guser.getuser(userindex)->m_gamepos;

	this->msglog(DEBUG, "chowcard, requserpos %d userpos %d count %d.", requserpos, userpos, count);

	if (requserpos == 0 && userpos == 2 ||
		requserpos == 1 && userpos == 0 ||
		requserpos == 2 && userpos == 1
		) {

		int _pos = this->getposfromdropcard(userpos, pos);

		if (_pos == -1) {
			this->msglog(DEBUG, "chowcard, failed to find drop card %d/%d from userpos %d.", pos[0], pos[1], userpos);
			return false;
		}

		this->msglog(DEBUG, "chowcard, found dropped card %d/%d from userpos %d drop list at pos %d.", pos[0], pos[1], userpos, _pos);


		// pos should be the last drop by the user
		iter = this->vDroppedCards.begin();
		while (iter != this->vDroppedCards.end()) {
			_DROPCARD_INFO dropinfo = *iter;
			if (dropinfo.userpos == userpos) {
				if (dropinfo.pos > _pos) {
					this->msglog(DEBUG, "chowcard failed, dropped pos %d is not the last drop of userpos %d, it is %d.", 
						_pos, userpos, dropinfo.pos);
					return false;
				}
			}
			iter++;
		}

		// check if selected cards do exist in user card list
		for (int n = 0; n < count; n++) {
			_PMSG_CARD_INFO* _card = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
			unsigned char _cardinfo[2];
			_cardinfo[0] = _card->cardtype;
			_cardinfo[1] = _card->cardnum;

			this->msglog(DEBUG, "chowcard, selected card %d/%d (%d) userpos %d.", _card->cardtype, _card->cardnum, n, userpos);

			if (this->getposfromusercard(userindex, _cardinfo) == -1) {
				this->msglog(DEBUG, "chowcard failed, card %d/%d doest not exist in userpos %d.", _card->cardtype, _card->cardnum, userpos);
				return false;
			}
		}

		iter = this->vDroppedCards.begin();

		while (iter != this->vDroppedCards.end()) {

			_DROPCARD_INFO dropinfo = *iter;

			this->msglog(DEBUG, "Drop info, dropped card pos %d userpos %d.", dropinfo.pos, dropinfo.userpos);

			if (dropinfo.userpos == userpos && dropinfo.card.cardtype == pos[0] && dropinfo.card.cardnum == pos[1]) {

				this->msglog(DEBUG, "Found match, dropped card pos %d userpos %d.", dropinfo.pos, dropinfo.userpos);

				unsigned char buf[100] = { 0 };
				_PMSG_CHOWCARD_ANS pMsg = { 0 };
				pMsg.hdr.c = 0xC1;
				pMsg.hdr.h = 0xF3;

				int size = sizeof(_PMSG_CHOWCARD_ANS);

				bool issamenumber = true;
				bool issametype = true;

				unsigned char c[2];

				_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos);// + (0 * sizeof(_PMSG_CARD_INFO)));
				c[0] = _cardinfo->cardtype;
				c[1] = _cardinfo->cardnum;



				for (int n = 1; n < count; n += 1) {

					_cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));

					if (c[1] != _cardinfo->cardnum) { // card number
						issamenumber = false;
						break;
					}
				}

				for (int n = 1; n < count; n += 1) {

					_cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));

					if (c[0] != _cardinfo->cardtype) { // card type
						issametype = false;
						break;
					}
				}

				if (issamenumber) { // trio/quadra

					if (dropinfo.card.cardnum != c[1]) { // card number
						this->msglog(DEBUG, "trio/quadra failed, dropped cardnum %d is not equal to user card %d.", 
							dropinfo.card.cardnum, c[1]);
						return false;
					}

					// its trio/quadra
					std::vector<_PMSG_CARD_INFO> _vchowcards;
					_vchowcards.push_back(dropinfo.card);

					this->msglog(DEBUG, "chowcard, drop card num %d.", dropinfo.card.cardnum);

					_PMSG_CARD_INFO cc = { 0 };
					cc.cardnum = dropinfo.card.cardnum;
					cc.cardtype = dropinfo.card.cardtype;
					memcpy(&buf[size], (unsigned char*)&cc, sizeof(_PMSG_CARD_INFO));
					size += sizeof(_PMSG_CARD_INFO);

					for (int n = 0; n < count; n += 1) {
						_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));

						this->msglog(DEBUG, "chowcard, user selected card %d %d.", _cardinfo->cardtype, _cardinfo->cardnum);

						memcpy(&buf[size], (unsigned char*)_cardinfo, sizeof(_PMSG_CARD_INFO));
						size += sizeof(_PMSG_CARD_INFO);

						_PMSG_CARD_INFO _usercardinfo;
						_usercardinfo.cardtype = _cardinfo->cardtype;
						_usercardinfo.cardnum = _cardinfo->cardnum;
						_vchowcards.push_back(_usercardinfo);
					}

					for (int n = 0; n < count; n += 1) {

						_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
						unsigned char __card[2];
						__card[0] = _cardinfo->cardtype;
						__card[1] = _cardinfo->cardnum;
						// remove card from user list
						this->removefromusercards(userindex, __card);
					}

					this->msglog(DEBUG, "trio/quadra formed from dropped card %d %d.", dropinfo.card.cardtype, dropinfo.card.cardnum);

					int downpos = this->adddowncards(userindex, _vchowcards);

					pMsg.hdr.len = size;
					pMsg.sub = 0x02;
					pMsg.count = count + 1;
					pMsg.gamepos = requserpos;
					pMsg.userpos = userpos;
					pMsg.chowpos[0] = pos[0];
					pMsg.chowpos[1] = pos[1];
					pMsg.downpos = downpos;

					memcpy(&buf[0], (unsigned char*)&pMsg, sizeof(_PMSG_CHOWCARD_ANS));

					for (int i = 0; i < MAX_USER_POS; i++) {
						this->datasend(this->m_users[i], buf, pMsg.hdr.len);
					}

					this->m_active_status |= (int)_ACTIVE_STATE::_CHOWED;
					this->msglog(DEBUG, "%s (%s), Flag active status _CHOWED.", guser.getuser(this->m_active_userindex)->name.c_str(), 
						guser.getuser(this->m_active_userindex)->account.c_str());
					guser.getuser(userindex)->m_isdowncard = true;

					this->removefromdropped(userpos, pos);

					this->countusercards(userindex);
					this->sendusercardcountsinfo(userindex);
					this->getwinner();

					return true;
				}
				else if (issametype) {

					if (dropinfo.card.cardtype != c[0]) {
						this->msglog(DEBUG, "straight failed, dropped card type %d is not equal to user card type %d.",
							dropinfo.card.cardtype, c[0]);
						return false;
					}

					std::vector <unsigned char> _v;
					_v.push_back(dropinfo.card.cardnum);

					this->msglog(DEBUG, "chowcard, drop card num %d.", dropinfo.card.cardnum);

					for (int n = 0; n < count; n++) {

						_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
							

						this->msglog(DEBUG, "chowcard, user selected card %d %d.", _cardinfo->cardtype, _cardinfo->cardnum);

						if (c[0] != _cardinfo->cardtype) { // recheck card type
							return false;
						}

						_v.push_back(_cardinfo->cardnum);
					}

					// sort desc
					std::sort(_v.begin(), _v.end(), std::greater<unsigned char>());

					for (int i = 0; i < count; i++) {
						if ((_v[i] - _v[i + 1]) != 1) {

							this->msglog(DEBUG, "straight failed, card num. %d differ more than 1 with card num. %d.",
								_v[i], _v[i + 1]);

							return false;
						}
					}

					// its straight
					std::vector <_PMSG_CARD_INFO> _vchowcards;

					// sort asc
					std::sort(_v.begin(), _v.end(), std::less<unsigned char>());

					_PMSG_CARD_INFO cc = { 0 };

					for (int i = 0; i < _v.size(); i++) {

						cc.cardnum = _v[i];
						cc.cardtype = c[0];

						memcpy(&buf[size], (unsigned char*)&cc, sizeof(_PMSG_CARD_INFO));
						size += sizeof(_PMSG_CARD_INFO);

						this->msglog(DEBUG, "straight formed cards, type %d num %d size %d.", cc.cardtype, cc.cardnum, size);


						// add to chowlist the cards with its new pointer
						_vchowcards.push_back(cc);
					}


					for (int n = 0; n < count; n += 1) {
						_PMSG_CARD_INFO* _cardinfo = (_PMSG_CARD_INFO*)(cardpos + (n * sizeof(_PMSG_CARD_INFO)));
						unsigned char __card[2];
						__card[0] = _cardinfo->cardtype;
						__card[1] = _cardinfo->cardnum;
						// remove card from user list
						this->removefromusercards(userindex, __card);
					}

					this->msglog(DEBUG, "straight formed from dropped card %d %d.", dropinfo.card.cardtype, dropinfo.card.cardnum);

					int downpos = this->adddowncards(userindex, _vchowcards);

					pMsg.hdr.len = size;
					pMsg.sub = 0x02;
					pMsg.count = count + 1;
					pMsg.gamepos = requserpos;
					pMsg.userpos = userpos;
					pMsg.chowpos[0] = pos[0];
					pMsg.chowpos[1] = pos[1];
					pMsg.downpos = downpos;

					memcpy(&buf[0], (unsigned char*)&pMsg, sizeof(_PMSG_CHOWCARD_ANS));

					for (int i = 0; i < MAX_USER_POS; i++) {
						this->datasend(this->m_users[i], buf, pMsg.hdr.len);
					}

					this->m_active_status |= (int)_ACTIVE_STATE::_CHOWED;
					this->msglog(DEBUG, "%s (%s), Flag active status _CHOWED.", guser.getuser(this->m_active_userindex)->name.c_str(), 
						guser.getuser(this->m_active_userindex)->account.c_str());
					guser.getuser(userindex)->m_isdowncard = true;

					this->removefromdropped(userpos, pos);

					this->countusercards(userindex);
					this->sendusercardcountsinfo(userindex);
					this->getwinner();

					return true;
				}

				this->msglog(DEBUG, "chowcard, failed to form trio/quadra/straight, type/number test both false.");

				return false;
			}


			iter++;
		}

		return false;
	}

	this->msglog(DEBUG, "The dropped card userpos %d is trying to chow does not belong to this user, card dropped by user %d.",
		requserpos, userpos);

	return false;
}

bool game::dropcard(uintptr_t userindex, unsigned char* pos)
{
	if (pos == NULL) {
		this->msglog(DEBUG, "dropcard, requested card to drop is NULL.");
		return false;
	}

	if (isactionvalid(userindex, _ACTIONS::_DROP) == false)
		return false;

	if (this->trysapawcard(userindex, pos)) {
		this->sendnotice(userindex, 7, "You can't dump the card.");
		this->msglog(DEBUG, "dropcard, request failed as the card can be used for sapaw.");
		return false;
	}

	int gamepos = guser.getuser(userindex)->m_gamepos;

	this->msglog(DEBUG, "dropcard, userpos %d requested card %d/%d to drop.", gamepos, pos[0], pos[1]);

	_PMSG_CARD_INFO _droppedcard = { 0 };
	
	// remove from user card list
	std::vector <_PMSG_CARD_INFO>::iterator _iter;

	for (_iter = this->m_usercardinfo[gamepos].user.begin(); _iter != this->m_usercardinfo[gamepos].user.end(); _iter++) {
		_droppedcard = *_iter;
		if (_droppedcard.cardtype == pos[0] && _droppedcard.cardnum == pos[1]) {
			this->msglog(DEBUG, "dropcard, erased dropped card %d/%d from user list.", _droppedcard.cardtype, _droppedcard.cardnum);
			this->m_usercardinfo[gamepos].user.erase(_iter);
			break;
		}
	}

	if (_droppedcard.cardtype == 0) {
		this->msglog(DEBUG, "dropcard, failed to find card %d/%d from user list.", pos[0], pos[1]);
		return false;
	}

	this->m_dropcardctr++;

	_DROPCARD_INFO dropinfo = { 0 };
	dropinfo.card.cardtype = _droppedcard.cardtype;
	dropinfo.card.cardnum = _droppedcard.cardnum;
	dropinfo.pos = this->m_dropcardctr;
	dropinfo.userpos = guser.getuser(userindex)->m_gamepos;

	this->msglog(DEBUG, "dropcard, add drop card %d %d to %d drop list.", _droppedcard.cardtype, _droppedcard.cardnum, userindex);

	this->vDroppedCards.push_back(dropinfo);

	_PMSG_DROP_CARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;
	pMsg.hdr.len = sizeof(_PMSG_DROP_CARD_ANS);
	pMsg.sub = 1;
	pMsg.pos = this->m_dropcardctr;
	pMsg.userpos = guser.getuser(userindex)->m_gamepos;
	pMsg.cardtype = _droppedcard.cardtype;
	pMsg.cardnum = _droppedcard.cardnum;

	for (int i = 0; i < MAX_USER_POS; i++) {
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}

	this->countusercards(userindex);
	this->sendusercardcountsinfo(userindex);
	this->getwinner();

	// enable user's fight mode 
	_USER_INFO* _userinfo = guser.getuser(userindex);
	if (_userinfo != NULL && _userinfo->m_isdowncard) {
		if (_userinfo->isselfblock == true) {
			_userinfo->isselfblock = false;
		}
		else {
			_userinfo->canfight = true;
			this->sendfightmode(userindex, 1);
		}
	}

	this->m_active_status |= (int)_ACTIVE_STATE::_DROPPED;
	this->msglog(DEBUG, "%s (%s), Flag active status _DROPPED.", guser.getuser(this->m_active_userindex)->name.c_str(), guser.getuser(this->m_active_userindex)->account.c_str());
	return true;

}

bool game::fightcard(uintptr_t userindex)
{
	if (isactionvalid(userindex, _ACTIONS::_FIGHT) == false)
		return false;

	this->msglog(DEBUG, "reqfightcard %s (%s) ask to fight.", guser.getuser(userindex)->name.c_str(), guser.getuser(userindex)->account.c_str());

	guser.getuser(userindex)->fought = true;
	this->m_fightuserindex = userindex;
	this->m_active_status |= (int)_ACTIVE_STATE::_FOUGHT;
	this->msglog(DEBUG, "%s (%s), Flag active status _FOUGHT.", guser.getuser(this->m_active_userindex)->name.c_str(), 
		guser.getuser(this->m_active_userindex)->account.c_str());

	_PMSG_FIGHTCARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;
	pMsg.hdr.len = sizeof(_PMSG_FIGHTCARD_ANS);
	pMsg.sub = 0x07;
	pMsg.userpos = guser.getuser(userindex)->m_gamepos;

	for (int i = 0; i < MAX_USER_POS; i++) {
		this->countusercards(this->m_users[i]);
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
		this->sendnotice(this->m_users[i], 6, 
			"You have %d sec. to group your cards, only ungrouped cards will be counted.", MAX_MSECONDS_GROUPCARD_TIMEOUT / 1000);
	}

	this->sendtimeoutleft(MAX_MSECONDS_GROUPCARD_TIMEOUT);
	this->m_gametick = GetTickCount64() + MAX_MSECONDS_GROUPCARD_TIMEOUT;

	return true;
}

bool game::fight2card(uintptr_t userindex, unsigned char isfight)
{
	if (isfight && isactionvalid(userindex, _ACTIONS::_FIGHT2)) {
		guser.getuser(userindex)->fought = true;
	}
	else {
		return false;
	}

	_PMSG_FIGHT2CARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;
	pMsg.hdr.len = sizeof(_PMSG_FIGHT2CARD_ANS);
	pMsg.sub = 0x08;
	pMsg.userpos = guser.getuser(userindex)->m_gamepos;
	pMsg.isfight = isfight;
	for (int i = 0; i < MAX_USER_POS; i++) {
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}

	return true;
}

int game::checkactiveusers()
{
	int count = 0;
	for (int i = 0; i < MAX_USER_POS; i++) {
		if (!guser.getuser(this->m_users[i])->isdc() && this->m_usercardinfo[i].iskick == false) {
			count++;
		}
	}
	return count;
}

void game::sendactivestatus()
{
	_PMSG_ACTIVESTATUS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.hdr.len = sizeof(_PMSG_ACTIVESTATUS);
	pMsg.sub = 0x05;

	for (int i = 0; i < MAX_USER_POS; i++) {
		pMsg.userpos[i] = 1;
		if (guser.getuser(this->m_users[i])->isdc()) {
			pMsg.userpos[i] = 0;
		}
	}

	for (int i = 0; i < MAX_USER_POS; i++) {
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}
}

void game::sendwaitinfo()
{
	_PMSG_ACTIVEINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_ACTIVEINFO);
	pMsg.sub = 0x02;
	pMsg.activeuserpos = this->m_active_pos;
	pMsg.timelimit_msec = 0;
	pMsg.activegamestate = (unsigned char)this->getstate();
	pMsg.activegamecounter = this->m_counter;
	pMsg.activehitteruserpos = -1;

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (this->m_users[i] == this->m_hitter) {
			pMsg.activehitteruserpos = i;
			break;
		}
	}

	for (int i = 0; i < MAX_USER_POS; i++) {
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}
}

void game::sendclosedinfo()
{
	_PMSG_ACTIVEINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_ACTIVEINFO);
	pMsg.sub = 0x02;
	pMsg.activeuserpos = this->m_active_pos;
	pMsg.timelimit_msec = 0;
	pMsg.activegamestate = (unsigned char)this->getstate();
	pMsg.activegamecounter = this->m_counter;
	pMsg.activehitteruserpos = -1;
	for (int i = 0; i < MAX_USER_POS; i++) {
		if (this->m_users[i] == this->m_hitter) {
			pMsg.activehitteruserpos = i;
			break;
		}
	}

	for (int i = 0; i < MAX_USER_POS; i++) {
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}
}

void game::sendendedinfo()
{
	_PMSG_ACTIVEINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_ACTIVEINFO);
	pMsg.sub = 0x02;
	pMsg.activeuserpos = this->m_active_pos;
	pMsg.timelimit_msec = 0;
	pMsg.activegamestate = (unsigned char)this->getstate();
	pMsg.activegamecounter = this->m_counter;
	pMsg.activehitteruserpos = -1;

	for (int i = 0; i < MAX_USER_POS; i++) {
		this->msglog(DEBUG, "send ended info to %d.", this->m_users[i]);
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}

}

void game::sendusercardcountsinfo(uintptr_t userindex, uintptr_t touserindex)
{
	_PMSG_USERCARDSINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_USERCARDSINFO);
	pMsg.sub = 0x05;
	pMsg.gamepos = guser.getuser(userindex)->m_gamepos;
	pMsg.cardcounts = guser.getuser(userindex)->m_cardquantity;

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (touserindex != 0 && this->m_users[i] != touserindex)
			continue;
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}
}

void game::sendfightmode(uintptr_t userindex, unsigned char enable)
{
	_PMSG_FIGHTMODE_INFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_FIGHTMODE_INFO);
	pMsg.sub = 0x08;
	pMsg.enable = enable;
	this->datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
}

void game::senduserecoinsinfo(uintptr_t userindex)
{
	_PMSG_USERECOINSINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_USERECOINSINFO);
	pMsg.sub = 0x04;

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (userindex != 0 && userindex != this->m_users[i])
			continue;
		pMsg.gamepos = i;
		if (guser.getuser(this->m_users[i])->ecoins[this->m_ectype] < 0)
			guser.getuser(this->m_users[i])->ecoins[this->m_ectype] = 0;
		pMsg.ecoins = guser.getuser(this->m_users[i])->ecoins[this->m_ectype];
		this->msglog(DEBUG, "%s (%s) ecoins %d", guser.getuser(this->m_users[i])->name.c_str(), guser.getuser(this->m_users[i])->account.c_str(), pMsg.ecoins);
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
		guser.saveecoins(this->m_users[i], this->m_ectype);
	}
}

void game::sendusernameinfo(uintptr_t userindex)
{
	_PMSG_USERNAMEINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_USERNAMEINFO);
	pMsg.sub = 0x03;

	for (int i = 0; i < MAX_USER_POS; i++) {
		pMsg.gamepos = i;
		memcpy(pMsg.name, guser.getuser(this->m_users[i])->name.c_str(), sizeof(pMsg.name));
		if (userindex != 0) {
			this->datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
		}
		else {
			for (int n = 0; n < MAX_USER_POS; n++) {
				this->datasend(this->m_users[n], (unsigned char*)&pMsg, pMsg.hdr.len);
			}
		}
	}
}

void game::sendactiveinfo(uintptr_t userindex)
{
	_PMSG_ACTIVEINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_ACTIVEINFO);
	pMsg.sub = 0x02;
	pMsg.activeuserpos = this->m_active_pos;

	unsigned int msecleft = guser.getuser(this->m_active_userindex)->activetick + MAX_MSECONDS_EACHTURN_TIMEOUT - GetTickCount64();
	pMsg.timelimit_msec = msecleft;
	pMsg.activegamestate = (unsigned char)this->getstate();
	pMsg.activegamecounter = this->m_counter;
	pMsg.activehitprizeecoins = this->m_hitprizeecoins;

	if (this->m_hitter == 0) {
		pMsg.activehitteruserpos = -1;
	}
	else {
		pMsg.activehitteruserpos = guser.getuser(this->m_hitter)->m_gamepos;
	}

	for (int i = 0; i < MAX_USER_POS; i++) {

		if (userindex != 0 && userindex != this->m_users[i])
			continue;

		if (this->m_usercardinfo[i].iskick == true)
			continue;

		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}
}

void game::sendtimeoutleft(uintptr_t timemsec) {

	_PMSG_TIMEOUT_INFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_TIMEOUT_INFO);
	pMsg.sub = 0x07;
	pMsg.timemsecleft = timemsec;
	for (int i = 0; i < MAX_USER_POS; i++) {
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}
}

void game::sendnotice(uintptr_t userindex, unsigned char type, const char* msg, ...)
{
	char szBuffer[1024] = { 0 };
	va_list pArguments;
	va_start(pArguments, msg);
	size_t iSize = strlen(szBuffer);
	vsprintf(&szBuffer[iSize], msg, pArguments);
	va_end(pArguments);

	if (userindex == 0) {
		for (int i = 0; i < MAX_USER_POS; i++) {
			guser.sendnotice(this->m_users[i], type, szBuffer);
		}
	} else 
		guser.sendnotice(userindex, type, szBuffer);
}

void game::setstate_restarted()
{ 
	//
	this->m_active_status = (int)_ACTIVE_STATE::_NONE;

	// force stop timeout timer
	this->sendtimeoutleft(MAX_MSECONDS_SHOWCARD_TIMEOUT + MAX_MSECONDS_SHOWRESULT_TIMEOUT);

	// send some notice
	//this->sendnotice(0, 0, "Next game will resume after %d seconds.", 
		//(MAX_MSECONDS_SHOWCARD_TIMEOUT + MAX_MSECONDS_SHOWRESULT_TIMEOUT) / 1000);

	this->senduserecoinsinfo();

	// send all user cards so players will see each player cards
	for (int i = 0; i < MAX_USER_POS; i++) {
		this->sendbothcardstouser(this->m_users[i]);
	}

	// set 3 sec. delay before sending reset game command
	this->m_gametick = GetTickCount64() + MAX_MSECONDS_SHOWCARD_TIMEOUT + MAX_MSECONDS_SHOWRESULT_TIMEOUT;
}

void game::resumedcuser()
{
	bool found = false;

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (guser.getuser(this->m_users[i])->isresumed()) {
			found = true;
		}
	}

	if (found == false)
		return;

	_PMSG_INITINFO pMsgResumed = { 0 };
	pMsgResumed.hdr.c = 0xC1;
	pMsgResumed.hdr.h = 0xF2;
	pMsgResumed.hdr.len = sizeof(_PMSG_INITINFO);
	pMsgResumed.sub = 0x06;
	pMsgResumed.init = 1;
	pMsgResumed.ectype = this->m_ectype;

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (guser.getuser(this->m_users[i])->isresumed() && guser.getuser(this->m_users[i])->m_resumeflag == 0) {

			// send resume flag enabled
			pMsgResumed.gamepos = guser.getuser(this->m_users[i])->m_gamepos;
			pMsgResumed.resume = 1;
			if (this->datasend(this->m_users[i], (unsigned char*)&pMsgResumed, pMsgResumed.hdr.len))
				guser.getuser(this->m_users[i])->m_resumeflag = 1;

			// send active info
			this->sendactiveinfo(this->m_users[i]);
		}
	}

	// send cards
	for (int i = 0; i < MAX_USER_POS; i++) {
		if (guser.getuser(this->m_users[i])->isresumed() && guser.getuser(this->m_users[i])->m_resumeflag == 2) {

			// send user cards
			this->sendusercards(this->m_users[i]);

			// send group cards
			this->sendgroupcards(this->m_users[i]);

			// send drop cards
			guser.getuser(this->m_users[i])->m_gamedropctr = 0;

			std::vector<_DROPCARD_INFO>::iterator iter;
			for (iter = this->vDroppedCards.begin(); iter != this->vDroppedCards.end(); iter++) {
				_DROPCARD_INFO _droppedcard = *iter;
				_PMSG_DROP_CARD_ANS pMsg = { 0 };
				pMsg.hdr.c = 0xC1;
				pMsg.hdr.h = 0xF3;
				pMsg.hdr.len = sizeof(_PMSG_DROP_CARD_ANS);
				pMsg.sub = 1;
				pMsg.pos = _droppedcard.pos;
				pMsg.userpos = _droppedcard.userpos;
				pMsg.cardtype = _droppedcard.card.cardtype;
				pMsg.cardnum = _droppedcard.card.cardnum;
				this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
			}

			// send downcards
			this->senddowncards(this->m_users[i]);

			guser.getuser(this->m_users[i])->resumed();

			// disable resume flag in dced client
			pMsgResumed.gamepos = guser.getuser(this->m_users[i])->m_gamepos;
			pMsgResumed.resume = 0;
			pMsgResumed.init = 0;
			this->datasend(this->m_users[i], (unsigned char*)&pMsgResumed, pMsgResumed.hdr.len);

			this->countusercards(this->m_users[i]);

			for (int j = 0; j < 3; j++) {
				this->sendusercardcountsinfo(this->m_users[j], this->m_users[i]);
			}

			// send names
			this->sendusernameinfo(this->m_users[i]);
			// send ecoins
			this->senduserecoinsinfo(this->m_users[i]);
			// send stock card count
			this->sendstockcardcount(this->m_users[i]);
			// send active info
			this->sendactiveinfo(this->m_users[i]);
			break;
		}
	}
}

void game::procstate_restarted()
{
	if (GetTickCount64() < this->m_gametick) {
		return;
	}

	int activecounts = this->checkactiveusers();

	if (activecounts < 3)
	{
		if (activecounts == 0 || this->m_hitter == 0 || this->m_counter == 1) {
			this->msglog(INFO, "procstate_started, players are now less than 3, no hitter and activecounts is %d", activecounts);
			this->setstate(_GAME_STATE::_CLOSED);
			return;
		}
		else {
			if (this->m_hitter != 0) {
				if (guser.getuser(this->m_hitter)->isdc()) {
					this->msglog(INFO, "procstate_started, players are now less than 3, hitter is disconnected and activecounts is %d", activecounts);
					this->setstate(_GAME_STATE::_CLOSED);
					return;
				}
			}
		}
	}

	// reset user info
	for (int i = 0; i < MAX_USER_POS; i++) {
		guser.getuser(this->m_users[i])->reset();
	}

	this->m_active_pos = guser.getuser(this->m_winner)->m_gamepos;
	this->m_active_userindex = this->m_winner;

	// reset game info
	this->reset();

	// send reset
	_PMSG_RESETGAMEINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_RESETGAMEINFO);
	pMsg.sub = 0x06;
	for (int i = 0; i < MAX_USER_POS; i++) {
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}

	if (!this->checkecoins()) {
		this->setstate(_GAME_STATE::_CLOSED);
		return;
	}

	this->loadgameconf();

	// deduct ecoins from users to be added in game prize, some ecoins will be refunded to user accordingly
	this->updatehitecoins();

	// send ecoins
	this->senduserecoinsinfo();

	//initial shuffle
	this->shufflecards();

	// send shuffled chards
	this->sendshuffledcards();

	this->setstate(_GAME_STATE::_PREPARE);
}

// this will be called once only, when game is created
void game::setstate_started(bool isrestarted)
{

	this->m_active_status = (int)_ACTIVE_STATE::_DRAWN;
	this->msglog(DEBUG, "%s (%s), Flag active status _DRAWN.", guser.getuser(this->m_active_userindex)->name.c_str(), 
		guser.getuser(this->m_active_userindex)->account.c_str());


	this->sendactivestatus();

	guser.getuser(this->m_active_userindex)->activetick = GetTickCount64();
	this->m_gametick = GetTickCount64() + 1000;
	this->m_counter++;

	// send stock card count
	this->sendstockcardcount();

	// send active info
	this->sendactiveinfo();

	if (this->m_hitter != 0) {
		this->sendnotice(0, 1, "%s is the Hitter.", guser.getuser(this->m_hitter)->name.c_str());
		this->msglog(INFO, "%s (%s) is the hitter.", guser.getuser(this->m_hitter)->name.c_str(), guser.getuser(this->m_hitter)->account.c_str());
	}
}

void game::setstate_notice()
{
	this->m_gametick = GetTickCount64() + NOTICE_SECONDS_DURATION * 1000;

	// send init info
	_PMSG_INITINFO pMsgResumed = { 0 };
	pMsgResumed.hdr.c = 0xC1;
	pMsgResumed.hdr.h = 0xF2;
	pMsgResumed.hdr.len = sizeof(_PMSG_INITINFO);
	pMsgResumed.sub = 0x06;
	pMsgResumed.init = 1;
	pMsgResumed.resume = 0;
	pMsgResumed.ectype = this->m_ectype;

	for (int i = 0; i < 3; i++) {
		pMsgResumed.gamepos = i;
		this->datasend(this->m_users[i], (unsigned char*)&pMsgResumed, pMsgResumed.hdr.len);
	}

	this->m_hitter = 0;
	this->m_active_pos = 0;
	this->m_active_userindex = this->m_users[0];
}

void game::setstate_prepare()
{
	this->initdrawcards[0] = 0;
	this->initdrawcards[1] = 0;
	this->initdrawcards[2] = 0;
}

void game::setstate_waiting()
{
	this->m_gametick = GetTickCount64() + 1000;
	this->m_resumed = false;
	this->m_resumemsleft = guser.getuser(this->m_active_userindex)->activetick - GetTickCount64();
	if (this->m_resumemsleft < 0) {
		this->m_resumemsleft = 0;
	}
	this->sendwaitinfo();
}

void game::setstate_closed()
{
	//this->sendclosedinfo();
	if (this->m_hitprizeecoins > 0) {
		for (int i = 0; i < 3; i++) {
			guser.getuser(this->m_users[i])->ecoins[this->m_ectype] += this->m_hitprizeecoins / 3;
			this->msglog(INFO, "setstate_closed, %s (%s) received %d ecoins from equally divided hit prize of %d.", 
				guser.getuser(this->m_users[i])->name.c_str(), guser.getuser(this->m_users[i])->account.c_str(), 
				this->m_hitprizeecoins / 3, this->m_hitprizeecoins);
		}
		this->senduserecoinsinfo();
	}

	this->sendnotice(0, 0, "There has no enough players to continue, ending game...");
	this->m_gametick = GetTickCount64() + 5000;
}

void game::setstate_ended()
{
	this->reset();

	this->m_hitprizeecoins = 0;
	this->m_hitter = 0;
	this->m_state = _GAME_STATE::_FREE;
	this->m_counter = 0;
	this->m_gametick = 0;
	this->m_active_pos = -1;
	this->m_winner = 0;
	this->m_active_userindex = 0;
	this->m_active_status = (int)_ACTIVE_STATE::_NONE;

	//this->sendendedinfo();
	_PMSG_LOGIN_RESULT pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.hdr.len = sizeof(_PMSG_LOGIN_RESULT);
	pMsg.sub = 0x09;
	pMsg.result = 1; // option to create game
	strcpy(pMsg.ecoinsnote, c.getbetmode(0).name.c_str());
	strcpy(pMsg.jewelsnote, c.getbetmode(1).name.c_str());

	// reset user info
	for (int i = 0; i < MAX_USER_POS; i++) {
		if (this->m_usercardinfo[i].iskick == false) {
			pMsg.isuseradmin = (guser.getuser(this->m_users[i])->isuseradmin) ? 1 : 0;
			pMsg.ecoins = guser.getuser(this->m_users[i])->ecoins[0];
			pMsg.jewels = guser.getuser(this->m_users[i])->ecoins[1];
			strcpy(pMsg.accountid, guser.getuser(this->m_users[i])->account.c_str());
			::datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
			guser.deluser(this->m_users[i], true);
			guser.getuser(this->m_users[i])->relog();
		}
		this->m_usercardinfo[i].iskick = false;
	}

	this->m_users[0] = 0;
	this->m_users[1] = 0;
	this->m_users[2] = 0;

}

void game::procstate_notice()
{
	// send some notice
	if (GetTickCount64() < this->m_gametick)
		return;

	// send active info
	this->sendactiveinfo();

	// send the user profile info initially
	this->sendusernameinfo();

	if (!this->checkecoins()) {
		this->setstate(_GAME_STATE::_ENDED);
		return;
	}

	this->loadgameconf();

	// deduct ecoins from users to be added in game prize, some ecoins will be refunded to user accordingly
	this->updatehitecoins();

	// send ecoins
	this->senduserecoinsinfo();

	//initial shuffle
	this->shufflecards();

	// send shuffled chards
	this->sendshuffledcards();

	this->setstate(_GAME_STATE::_PREPARE);
}

void game::procstate_prepare()
{
	if (this->checkactiveusers() == 0)
	{
		this->msglog(INFO, "procstate_started, players are now less than 3, no hitter and activecounts is 0.");
		this->setstate(_GAME_STATE::_CLOSED);
		return;
	}

	if ((this->initdrawcards[0] == 1 || guser.getuser(this->m_users[0])->isdc()) &&
		(this->initdrawcards[1] == 1 || guser.getuser(this->m_users[1])->isdc()) &&
		(this->initdrawcards[2] == 1 || guser.getuser(this->m_users[2])->isdc())) {
		this->setstate(_GAME_STATE::_STARTED);
	}
}

void game::procstate_started()
{
	if (this->checkgpsdistance() == false)
		return;

	int activecounts = this->checkactiveusers();

	if (this->m_winner != 0) {
		this->setstate(_GAME_STATE::_RESTARTED);
		return;
	}

	this->sendactivestatus();

	this->resumedcuser();

	if (activecounts < 3)
	{
		if (activecounts == 0 && this->m_hitter == 0) {
			this->msglog(INFO, "procstate_started, players are now less than 3, no hitter and activecounts is %d", activecounts);
			this->setstate(_GAME_STATE::_CLOSED);
			return;
		}
	}

	if (this->m_active_status & (int)_ACTIVE_STATE::_FOUGHT) {

		if (GetTickCount64() > this->m_gametick) {

			uintptr_t winuser = 0;
			int cardcount = 120;

			unsigned char fightpos = guser.getuser(this->m_fightuserindex)->m_gamepos;
			unsigned char winpos = (fightpos == 0) ? 2 : fightpos - 1;
			unsigned char nextwinpos = (winpos == 0) ? 2 : winpos - 1;

			for (int i = 0; i < MAX_USER_POS; i++) {
				if (guser.getuser(this->m_users[i])->fought == false)
					continue;
				this->countusercards(this->m_users[i]);
				if (guser.getuser(this->m_users[i])->m_cardcount < cardcount) {
					cardcount = guser.getuser(this->m_users[i])->m_cardcount;
					winuser = this->m_users[i];
				}
				else if (guser.getuser(this->m_users[i])->m_cardcount == cardcount) {

					if (guser.getuser(winuser)->m_gamepos == winpos) {
						// last who fight should be the winner when card count is equal
					}
					else {
						if (guser.getuser(this->m_users[i])->m_gamepos == winpos) {
							winuser = this->m_users[i];
						}
						else if (guser.getuser(this->m_users[i])->m_gamepos == nextwinpos) {
							winuser = this->m_users[i];
						}
					}

				}
			}

			this->m_active_status |= (int)_ACTIVE_STATE::_RESET;
			this->msglog(DEBUG, "%s (%s), Flag active status _RESET.", guser.getuser(this->m_active_userindex)->name.c_str(), 
				guser.getuser(this->m_active_userindex)->account.c_str());
			this->m_winner = winuser;
			this->getwinner();
			this->setstate(_GAME_STATE::_RESTARTED);
		}
	}
	else if (this->m_active_status & (int)_ACTIVE_STATE::_STOCKZERO) {

		// should drop card next
		if (!(this->m_active_status & (int)_ACTIVE_STATE::_DROPPED) && 
			guser.getuser(this->m_active_userindex)->activetick != 0 &&
			GetTickCount64() > (guser.getuser(this->m_active_userindex)->activetick + MAX_MSECONDS_EACHTURN_TIMEOUT)) { // drop timeout, should auto drop random from ungrouped cards
			// auto drop random cards
			unsigned char cc[2] = { 0 };
			this->chooserandomcard(this->m_active_userindex, cc);
			this->dropcard(this->m_active_userindex, cc);
		}

		// auto check winner in this stage

		if (GetTickCount64() > this->m_gametick) {

			uintptr_t winuser = 0;
			int cardcount = 120;

			unsigned char winpos = this->m_active_pos;
			unsigned char nextwinpos = (winpos == 0) ? 2 : winpos - 1;

			bool allnodown = true;

			for (int i = 0; i < MAX_USER_POS; i++) {
				if (guser.getuser(this->m_users[i])->m_isdowncard == true || 
					guser.getuser(this->m_users[i])->m_quadracount > 0 || 
					guser.getuser(this->m_users[i])->m_royalcount > 0) {
					allnodown = false;
					break;
				}
			}

			for (int i = 0; i < MAX_USER_POS; i++) {
				
				if (allnodown == false && guser.getuser(this->m_users[i])->m_isdowncard == false 
					&& guser.getuser(this->m_users[i])->m_quadracount == 0 && guser.getuser(this->m_users[i])->m_royalcount == 0)
					continue;

				if (this->m_usercardinfo[i].iskick == true)
					continue;

				this->countusercards(this->m_users[i]);
				if (guser.getuser(this->m_users[i])->m_cardcount < cardcount) {
					cardcount = guser.getuser(this->m_users[i])->m_cardcount;
					winuser = this->m_users[i];
				}
				else if (guser.getuser(this->m_users[i])->m_cardcount == cardcount) {

					if (guser.getuser(winuser)->m_gamepos == winpos) {
						// last draw should be the winner when card count is equal
					}
					else {
						if (guser.getuser(this->m_users[i])->m_gamepos == winpos) {
							winuser = this->m_users[i];
						}
						else if (guser.getuser(this->m_users[i])->m_gamepos == nextwinpos) {
							winuser = this->m_users[i];
						}
					}

				}
			}

			this->m_active_status |= (int)_ACTIVE_STATE::_RESET;
			this->msglog(DEBUG, "%s (%s), Flag active status _RESET.", guser.getuser(this->m_active_userindex)->name.c_str(), 
				guser.getuser(this->m_active_userindex)->account.c_str());
			this->m_winner = winuser;
			this->getwinner();
			this->setstate(_GAME_STATE::_RESTARTED);
		}
	}
	else {
		if (this->m_active_status & (int)_ACTIVE_STATE::_DROPPED) {

			if (guser.getuser(this->m_active_userindex)->activetick != 0 &&
				GetTickCount64() > (guser.getuser(this->m_active_userindex)->activetick + MAX_MSECONDS_EACHTURN_TIMEOUT)) {
				guser.getuser(this->m_active_userindex)->isauto = false; // set auto to disable
			}

			this->m_active_pos++;
			if (this->m_active_pos >= MAX_USER_POS)
				this->m_active_pos = 0;
			guser.getuser(this->m_active_userindex)->activetick = 0;
			this->m_active_userindex = this->m_users[this->m_active_pos];

			this->m_active_status = (int)_ACTIVE_STATE::_NONE;
			this->msglog(DEBUG, "%s (%s), Flag active status NONE.", guser.getuser(this->m_active_userindex)->name.c_str(), guser.getuser(this->m_active_userindex)->account.c_str());
			if(guser.getuser(this->m_active_userindex)->isdc() || this->m_usercardinfo[this->m_active_pos].iskick == true)
				guser.getuser(this->m_active_userindex)->activetick = GetTickCount64() - MAX_MSECONDS_EACHTURN_TIMEOUT + 5;
			else
				guser.getuser(this->m_active_userindex)->activetick = GetTickCount64();

			this->sendactiveinfo();
		}
		else {

			if (!(this->m_active_status & (int)_ACTIVE_STATE::_DRAWN) && !(this->m_active_status & (int)_ACTIVE_STATE::_CHOWED)) {
				// should draw and drop card next

				if (guser.getuser(this->m_active_userindex)->activetick != 0 &&
					GetTickCount64() > (guser.getuser(this->m_active_userindex)->activetick + MAX_MSECONDS_EACHTURN_TIMEOUT)) { // timeout, should auto draw and drop random from ungrouped cards
					// set the user auto flag first to true to bypass action time check
					guser.getuser(this->m_active_userindex)->isauto = true;
					// auto draw
					this->drawfromstock(this->m_active_userindex);
					return;
				}
			}

			if (!(this->m_active_status & (int)_ACTIVE_STATE::_DROPPED)) {
				// should drop card
				if (guser.getuser(this->m_active_userindex)->activetick != 0 &&
					GetTickCount64() > (guser.getuser(this->m_active_userindex)->activetick + MAX_MSECONDS_EACHTURN_TIMEOUT)) { // drop timeout, should auto drop random from ungrouped cards
					// auto drop random cards
					unsigned char cc[2] = { 0 };
					this->chooserandomcard(this->m_active_userindex, cc);
					this->dropcard(this->m_active_userindex, cc);
					return;
				}
			}
		}
	}
}

void game::procstate_closed()
{
	if (GetTickCount64() < this->m_gametick) {
		return;
	}
	this->setstate(_GAME_STATE::_ENDED);
}

void game::sendbothcardstouser(uintptr_t userindex)
{
	unsigned char buffer[100];
	unsigned char buffer2[100];
	int size = 0, size2 = 0;

	unsigned char userpos = guser.getuser(userindex)->m_gamepos;

	_PMSG_SHOW_USERCARDS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.sub = 0x02;
	pMsg.userpos = userpos;
	size = sizeof(_PMSG_SHOW_USERCARDS);

	std::vector <_PMSG_CARD_INFO>::iterator iter;

	iter = this->m_usercardinfo[userpos].user.begin();

	while (iter != this->m_usercardinfo[userpos].user.end()) {
		_PMSG_CARD_INFO c = *iter;

		pMsg.count++;
		memcpy(&buffer[size], (unsigned char*)&c, sizeof(_PMSG_CARD_INFO));
		size += sizeof(_PMSG_CARD_INFO);

		iter++;
	}

	pMsg.hdr.len = sizeof(_PMSG_SHOW_USERCARDS) + size;
	memcpy(&buffer[0], (unsigned char*)&pMsg, sizeof(_PMSG_SHOW_USERCARDS));

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (this->m_users[i] != userindex) {
			this->datasend(this->m_users[i], buffer, pMsg.hdr.len);
		}
	}

	_PMSG_SHOW_GRPCARDS pMsg2 = { 0 };
	pMsg2.hdr.c = 0xC1;
	pMsg2.hdr.h = 0xF2;
	pMsg2.sub = 0x03;
	pMsg2.userpos = userpos;
	pMsg2.count = 0;

	std::vector<std::vector <_PMSG_CARD_INFO>>::iterator iter2;// group;


	for (iter2 = this->m_usercardinfo[userpos].group.begin(); iter2 != this->m_usercardinfo[userpos].group.end(); iter2++) {

		std::vector <_PMSG_CARD_INFO> v = *iter2;
		std::vector <_PMSG_CARD_INFO>::iterator iter3;

		size2 = sizeof(_PMSG_SHOW_GRPCARDS);
		pMsg2.count = 0;

		for (iter3 = v.begin(); iter3 != v.end(); iter3++) {

			_PMSG_CARD_INFO cc = *iter3;
			this->msglog(DEBUG, "%s (%s) grp card %d %d", guser.getuser(userindex)->name.c_str(), guser.getuser(userindex)->account.c_str(), cc.cardtype, cc.cardnum);
			memcpy(&buffer2[size2], (unsigned char*)&cc, sizeof(_PMSG_CARD_INFO));
			size2 += sizeof(_PMSG_CARD_INFO);
			pMsg2.count++;

		}

		pMsg2.hdr.len = sizeof(_PMSG_SHOW_GRPCARDS) + size2;
		memcpy(&buffer2[0], (unsigned char*)&pMsg2, sizeof(_PMSG_SHOW_GRPCARDS));

		for (int i = 0; i < MAX_USER_POS; i++) {
			if (this->m_users[i] != userindex) {
				this->datasend(this->m_users[i], buffer2, pMsg2.hdr.len);
			}
		}
	}
}

void game::senddowncards(uintptr_t userindex)
{
	unsigned char buf[100] = { 0 };
	_PMSG_DOWNCARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;

	std::vector<_PMSG_CARD_INFO>::iterator iter;

	for (int userpos = 0; userpos < 3; userpos++) {

		for (int downpos = 0; downpos < this->m_usercardinfo[userpos].down.size(); downpos++) {
			std::vector<_PMSG_CARD_INFO> _v = this->m_usercardinfo[userpos].down[downpos];

			int size = sizeof(_PMSG_DOWNCARD_ANS);

			for (iter = _v.begin(); iter != _v.end(); iter++) {
				_PMSG_CARD_INFO _cardinfo = *iter;
				memcpy(&buf[size], (unsigned char*)&_cardinfo, sizeof(_PMSG_CARD_INFO));
				size += sizeof(_PMSG_CARD_INFO);
			}

			pMsg.hdr.len = size;
			pMsg.sub = 0x03;
			pMsg.count = _v.size();
			pMsg.downpos = downpos;
			pMsg.gamepos = userpos;
			memcpy(&buf[0], (unsigned char*)&pMsg, sizeof(_PMSG_DOWNCARD_ANS));
			this->datasend(userindex, buf, pMsg.hdr.len);
		}
	}
}

void game::sendgroupcards(uintptr_t userindex) {

	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	unsigned char buf[100] = { 0 };
	_PMSG_GRPCARD_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF3;

	std::vector<_PMSG_CARD_INFO>::iterator iter;

	for (int downpos = 0; downpos < this->m_usercardinfo[_pos].group.size(); downpos++) {
		std::vector<_PMSG_CARD_INFO> _v = this->m_usercardinfo[_pos].group[downpos];

		int size = sizeof(_PMSG_GRPCARD_ANS);

		for (iter = _v.begin(); iter != _v.end(); iter++) {
			_PMSG_CARD_INFO _cardinfo = *iter;
			memcpy(&buf[size], (unsigned char*)&_cardinfo, sizeof(_PMSG_CARD_INFO));
			size += sizeof(_PMSG_CARD_INFO);
		}

		pMsg.hdr.len = size;
		pMsg.sub = 0x05;
		pMsg.count = _v.size();
		pMsg.grppos = downpos;
		pMsg.gamepos = _pos;
		memcpy(&buf[0], (unsigned char*)&pMsg, sizeof(_PMSG_GRPCARD_ANS));
		this->datasend(userindex, buf, pMsg.hdr.len);
	}
}

void game::sendinitusercards()
{
	unsigned char pos = this->m_hitter ? guser.getuser(this->m_hitter)->m_gamepos : 0;
	unsigned char initpos = pos;

	for (int n = 0; n < 13; n++) {

		for (int nn = 0; nn < 3; nn++) {

			if (n > 11 && pos != initpos) {
				pos += 1;
				if (pos >= 3)
					pos = 0;
				continue;
			}

			_PMSG_CARD_INFO card = this->m_usercardinfo[pos].user[n];

			this->msglog(DEBUG, "sendinitusercards, pos %d n %d card %d %d", pos, n, card.cardtype, card.cardnum);

			_PMSG_DRAW_CARD_ANS pMsg = { 0 };
			pMsg.hdr.c = 0xC1;
			pMsg.hdr.h = 0xF3;
			pMsg.hdr.len = sizeof(_PMSG_DRAW_CARD_ANS);
			pMsg.sub = 0;
			pMsg.init = 1;

			pMsg.userpos = pos;

			for (int i = 0; i < MAX_USER_POS; i++) {
				if (pos == i) {
					pMsg.cardtype = card.cardtype;
					pMsg.cardnum = card.cardnum;
					this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
				}
				else {
					pMsg.cardtype = 0;
					pMsg.cardnum = 0;
					this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
				}
			}

			pos += 1;
			if (pos >= 3)
				pos = 0;
		}
	}
}

void game::sendusercards(uintptr_t userindex, bool isresumed)
{
	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	_PMSG_SEND_INIT_CARDS pMsg = { 0 };
	pMsg.data.cardcounts = 0;
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(pMsg);
	pMsg.data.sub = 0;
	pMsg.data.userpos = _pos;

	std::vector <_PMSG_CARD_INFO>::iterator iter;

	iter = this->m_usercardinfo[_pos].user.begin();
	while (iter != this->m_usercardinfo[_pos].user.end()) {
		_PMSG_CARD_INFO c = *iter;
		if (c.cardtype == 0) {
			iter++;
			continue;
		}
		pMsg.data.cardtype[pMsg.data.cardcounts] = c.cardtype;
		pMsg.data.cardnum[pMsg.data.cardcounts] = c.cardnum;
		pMsg.data.cardcounts++;
		iter++;
	}

	this->datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
}

void game::sendstockcardcount(uintptr_t userindex)
{
	_PMSG_CARD_STOCKINFO pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_CARD_STOCKINFO);
	pMsg.sub = 1;
	pMsg.stockcount = this->countstockcards();

	for (int i = 0; i < MAX_USER_POS; i++) {
		if (userindex != 0 && userindex != this->m_users[i])
			continue;
		this->datasend(this->m_users[i], (unsigned char*)&pMsg, pMsg.hdr.len);
	}
}

void game::sendshuffledcards()
{
	this->sendinitusercards();

	for (int i = 0; i < MAX_USERS_PERGAME; i++)
	{
		this->countusercards(this->m_users[i]);
		this->sendusercardcountsinfo(this->m_users[i]);
	}
}

int game::getposfromdropcard(unsigned char userpos, unsigned char* card)
{
	if (card == NULL)
		return -1;

	std::vector<_DROPCARD_INFO>::iterator iter;// vDroppedCards
	for (iter = this->vDroppedCards.begin(); iter != this->vDroppedCards.end(); iter++) {
		_DROPCARD_INFO dropinfo = *iter;
		if (dropinfo.userpos == userpos && card[0] == dropinfo.card.cardtype && card[1] == dropinfo.card.cardnum) {
			return dropinfo.pos;
		}
	}
	
	return -1;
}

int game::getposfromusercard(uintptr_t userindex, unsigned char* card)
{
	int pos = 0;
	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	if (card == NULL)
		return -1;

	std::vector <_PMSG_CARD_INFO>::iterator _iter;
	for (_iter = this->m_usercardinfo[_pos].user.begin(); _iter != this->m_usercardinfo[_pos].user.end(); _iter++) {
		_PMSG_CARD_INFO _c = *_iter;
		if (card[0] == _c.cardtype && card[1] == _c.cardnum) {
			return pos;
		}
		pos++;
	}

	return -1;
}

bool game::movecardpos(uintptr_t userindex, unsigned char group, unsigned char* srcpos, unsigned char* targetpos)
{
	unsigned char _pos = guser.getuser(userindex)->m_gamepos;

	int _srcpos = this->getposfromusercard(userindex, srcpos);

	/*if (targetpos == NULL || _srcpos == -1)
		return false;

	if (targetpos[0] == 0 && targetpos[1] == 128) // swipe left
	{
		int _targetpos = 0;
		int tpos = _srcpos + 1;
		int spos = _srcpos;
		while (true) {
			std::swap(this->m_usercardinfo[_pos].user.at(spos), this->m_usercardinfo[_pos].user.at(tpos));
			spos = tpos;
			if (tpos >= _targetpos)
				break;
			tpos += 1;
		}
		return true;
	}
	else if (targetpos[0] == 0 && targetpos[1] == 255) // swipe right
	{
		int _targetpos = this->m_usercardinfo[_pos].user.size() - 1;
		int tpos = _srcpos - 1;
		int spos = _srcpos;
		while (true) {
			std::swap(this->m_usercardinfo[_pos].user.at(spos), this->m_usercardinfo[_pos].user.at(tpos));
			spos = tpos;
			tpos -= 1;
			if (tpos <= _targetpos)
				break;
		}
		this->sendusercards(userindex);
		return true;
	}*/

	int _targetpos = this->getposfromusercard(userindex, targetpos);

	this->msglog(DEBUG, "movecardpos, source pos %d target pos %d.", _srcpos, _targetpos);

	if (_srcpos == -1 || _targetpos == -1)
		return false;

	if (_srcpos > _targetpos) { // move to the right
		int tpos = _srcpos - 1;
		int spos = _srcpos;
		while (true) {
			std::swap(this->m_usercardinfo[_pos].user.at(spos), this->m_usercardinfo[_pos].user.at(tpos));
			spos = tpos;
			tpos -= 1;
			if (tpos <= _targetpos)
				break;
		}
	}
	else { // move to the left
		int tpos = _srcpos + 1;
		int spos = _srcpos;
		while (true) {
			std::swap(this->m_usercardinfo[_pos].user.at(spos), this->m_usercardinfo[_pos].user.at(tpos));
			spos = tpos;
			if (tpos >= _targetpos)
				break;
			tpos += 1;
		}
	}

	this->sendusercards(userindex);
	return true;
}

void game::msglog(BYTE type, const char* msg, ...)
{
	char szBuffer[1024] = { 0 };
	va_list pArguments;
	va_start(pArguments, msg);
	sprintf(szBuffer, "[Serial:%llu Type:%d] ", this->m_gameserial, this->m_ectype);
	size_t iSize = strlen(szBuffer);
	vsprintf(&szBuffer[iSize], msg, pArguments);
	va_end(pArguments);
	::msglog(type, szBuffer);
}

bool game::checkgpsdistance()
{
	if (c.getgpslimitdis() == 0.0f)
		return true;

	if (guser.getuser(this->m_users[0])->isnogps)
		return true;

	// check distance using gps
	for (int n = 0; n < 3; n++) {

		if (this->m_usercardinfo[n].iskick == true)
			continue;

		_USER_INFO* user1 = guser.getuser(this->m_users[n]);

		if (GetTickCount64() > user1->gps.tick || user1->gps.longitude == 0.000000 || user1->gps.latitude == 0.000000) {
			this->sendnotice(this->m_users[n], 1, "Your location is invalid so you will be kicked from this game.");
			gcontrol.addkickuser(this->m_users[n]);
			this->m_usercardinfo[n].iskick = true;
		}
		else {
			for (int i = 0; i < 3; i++) {

				if (n == i || this->m_usercardinfo[i].iskick == true)
					continue;

				_USER_INFO* user2 = guser.getuser(this->m_users[i]);

				double dist = guser.getdistancegps(user1->gps.latitude, user1->gps.longitude,
					user2->gps.latitude, user2->gps.longitude);

				if (dist < c.getgpslimitdis()) {

					this->sendnotice(this->m_users[i], 1, "Your location is invalid so you will be kicked from this game.");
					gcontrol.addkickuser(this->m_users[i]);
					this->m_usercardinfo[i].iskick = true;

					if (this->m_usercardinfo[n].iskick == false) {
						this->sendnotice(this->m_users[n], 1, "Your location is invalid so you will be kicked from this game.");
						gcontrol.addkickuser(this->m_users[n]);
						this->m_usercardinfo[n].iskick = true;
					}
				}
			}
		}
	}

	int countactive = 0;
	int posactive = -1;

	for (int n = 0; n < 3; n++) {
		if (this->m_usercardinfo[n].iskick == true)
			continue;
		posactive = n;
		countactive++;
	}

	if (countactive <= 1) { // end the game when active is below or equal to 1 
		if (posactive != -1) {
			this->m_winner = this->m_users[posactive];
			this->sendnotice(0, 0, "%s won and got the hits by default!", guser.getuser(this->m_winner)->name.c_str());
			this->msglog(INFO, "%s (%s) won and got the hits by default.", guser.getuser(this->m_winner)->name.c_str(), guser.getuser(this->m_winner)->account.c_str());
			this->m_hitter = this->m_winner;
			this->getwinner(1);
			this->senduserecoinsinfo();
			this->setstate(_GAME_STATE::_ENDED);
		}
		else {
			this->senduserecoinsinfo();
			this->setstate(_GAME_STATE::_ENDED);
			msglog(INFO, "All users kicked so no hits (%d) refunded, game ended.", this->m_hitprizeecoins);
		}
		return false;
	}

	return true;
}

bool game::datasend(intptr_t userindex, unsigned char* data, int len)
{
	if (guser.getuser(userindex)->isplaying()) {
		return ::datasend(userindex, data, len);
	}
	return false;
}



