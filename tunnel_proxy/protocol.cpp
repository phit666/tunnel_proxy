#include "protocol.h"
#include "user.h"
#include "game.h"
#include "gamectrl.h"
#include "socket.h"
#include "conf.h"

protocol gprotocol;

protocol::protocol()
{

}

protocol::~protocol()
{

}

bool protocol::doprotocol(uintptr_t userindex, unsigned char* data, unsigned char head)
{

	//msglog(DEBUG, "doprotocol, 0x%X", head);

	switch (head) {

	case 0xF1:
	{
		_PMSG_DEF_SUB* _sub = (_PMSG_DEF_SUB*)data;

		switch (_sub->sub) {

		case 0x00: // login user/pass
			this->reqotplogin((_PMSG_OTP_REQ*)data, userindex);
			break;
		case 0x01: // login token
			this->reqtokenlogin((_PMSG_LOGIN_TOKEN*)data, userindex);
			break;
		case 0x02:
			this->requserlogin((_PMSG_LOGIN_USER*)data, userindex);
			break;
		case 0x03: // join game
			this->reqjoingame((_PMSG_JOINGAME_INFO *)data, userindex);
			break;
		case 0x04: // gps info
			this->reqgpsinfo((_PMSG_GPS_INFO*)data, userindex);
			break;
		case 0x05: //_PMSG_RESETINFO_REQ
			this->reqresetinfo((_PMSG_RESETINFO_REQ *)data, userindex);
			break;
		case 0x06: // _PMSG_OTPCODE_REQ
			this->reqotpcode((_PMSG_OTPCODE_REQ *)data, userindex);
			break;
		}
	}
		break;

	case 0xF2:
	{
		_PMSG_DEF_SUB* _sub = (_PMSG_DEF_SUB*)data;

		switch (_sub->sub) {
		case 0x00:
			this->reqmovecardpos((_PMSG_MOVECARD_REQ*)data, userindex);
			break;
		case 0x01: // alive
			this->reqalive((_PMSG_ALIVE*)data, userindex);
			break;
		case 0x02: // _PMSG_REQ_USERCARDS
			this->requsercards((_PMSG_REQ_USERCARDS*)data, userindex);
			break;
		case 0x03: // init draw cards
			this->reqgamestart((_PMSG_REQ_DRAWINIT *)data, userindex);
			break;
		case 0x06: // resume
			guser.getuser(userindex)->m_resumeflag = 2;
			break;
		}

	}
		break;

	case 0xF3:
	{
		_PMSG_DEF_SUB* _sub = (_PMSG_DEF_SUB*)data;

		if (guser.getuser(userindex)->iskick == true)
			return true;

		switch (_sub->sub) {
		case 0x00:
			this->reqdrawcard((_PMSG_DRAW_CARD_REQ*)data, userindex);
			break;
		case 0x01:
			this->reqdropcard((_PMSG_DROP_CARD_REQ*)data, userindex);
			break;
		case 0x02:
			this->reqchowcard(data, userindex);
			break;
		case 0x03:
			this->reqdowncard(data, userindex);
			break;
		case 0x04:
			this->reqsapawcard(data, userindex);
			break;
		case 0x05: // group
			this->reqgroupcard(data, userindex);
			break;
		case 0x06: // ungroup
			this->requngroupcard((_PMSG_UNGRPCARD_REQ *)data, userindex);
			break;
		case 0x07: // fight
			this->reqfightcard((_PMSG_FIGHTCARD_REQ *)data, userindex);
			break;
		case 0x08: // fight2
			this->reqfight2card((_PMSG_FIGHTCARD_REQ*)data, userindex);
			break;
		}

	}
	break;

	case 0xF4: // MU Admin
	{
		_PMSG_DEF_SUB_MU* _sub = (_PMSG_DEF_SUB_MU*)data;

		//msglog(DEBUG, "doprotocol, 0x%X 0x%X", head, _sub->sub);

		switch (_sub->sub) {
		case 0x00: // mu admin login //_PMSG_MUADMIN_LOGIN
			this->reqmulogin((_PMSG_MUADMIN_LOGIN *)data, userindex);
			break;
		case 0x01: // add ecoins/jewels
			this->reqaddecoins((_PMSG_ADDECOINS_REQ *)data, userindex);
			break;
		case 0x02: // get ecoins/jewels
			this->reqgetecoins((_PMSG_GETECOINS_REQ*)data, userindex);
			break;
		case 0x03: // reload conf
			this->reqreloadconf((_PMSG_RELOAD_REQ*)data, userindex);
			break;
		}

	}
	break;

	}
	return true;
}

void protocol::reqgamestart(_PMSG_REQ_DRAWINIT* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto || lpMsg->flag != 1)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;


	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	_g->gamestart(userindex);
}

void protocol::requsercards(_PMSG_REQ_USERCARDS* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;


	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	_g->usercards(userindex);
}

void protocol::reqresetinfo(_PMSG_RESETINFO_REQ* lpMsg, uintptr_t userindex)
{
	guser.deluser(userindex, true);
	guser.getuser(userindex)->relog();

	_PMSG_LOGIN_RESULT pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.hdr.len = sizeof(_PMSG_LOGIN_RESULT);
	pMsg.sub = 0x09;
	pMsg.result = 1; // option to create game
	pMsg.isuseradmin = (guser.getuser(userindex)->isuseradmin) ? 1 : 0;
	strcpy(pMsg.accountid, guser.getuser(userindex)->account.c_str());
	pMsg.ecoins = guser.getuser(userindex)->ecoins[0];
	pMsg.jewels = guser.getuser(userindex)->ecoins[1];
	strcpy(pMsg.ecoinsnote, c.getbetmode(0).name.c_str());
	strcpy(pMsg.jewelsnote, c.getbetmode(1).name.c_str());

	::datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
}

void protocol::reqgpsinfo(_PMSG_GPS_INFO* lpMsg, uintptr_t userindex)
{
	_USER_INFO* _user = guser.getuser(userindex);
	_user->gps.longitude = lpMsg->longitude;
	_user->gps.latitude = lpMsg->latitue;
	_user->gps.tick = GetTickCount64() + 60000;
	//if(_user->isplaying())
		//msglog(INFO, "GPS Info, %s (%s) %llu long:%f lat:%f", _user->name.c_str(), _user->account.c_str(), userindex, lpMsg->longitude, lpMsg->latitue);
	//else
		//msglog(INFO, "GPS Info, %llu long:%f lat:%f", userindex, lpMsg->longitude, lpMsg->latitue);
}

void protocol::reqjoingame(_PMSG_JOINGAME_INFO* lpMsg, uintptr_t userindex)
{
	if (lpMsg->gametype > 1)
		return;

	if (c.getbetmode(lpMsg->gametype).enable == false) {
		guser.sendnotice(userindex, 8, "The game mode you chosed is disabled at the moment.");
		return;
	}

	guser.getuser(userindex)->setgametype(lpMsg->gametype);
	guser.getuser(userindex)->setlognwait();
	guser.trystartgame(lpMsg->gametype);

	_PMSG_LOGIN_RESULT pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.hdr.len = sizeof(_PMSG_LOGIN_RESULT);
	pMsg.sub = 0x09;
	pMsg.result = 3; 
	::datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
}

void protocol::reqmulogin(_PMSG_MUADMIN_LOGIN* lpMsg, uintptr_t userindex)
{
	guser.mulogin(lpMsg->secret, userindex);
}

void protocol::reqaddecoins(_PMSG_ADDECOINS_REQ* lpMsg, uintptr_t userindex)
{
	if (guser.getuser(userindex)->ismuadmin == false && guser.getuser(userindex)->isuseradmin == false) {
		msglog(INFO, "reqaddecoins, %s requesting to top up user ecoins but not an admin.", guser.getuser(userindex)->account.c_str());
		return;
	}
	guser.adduserecoins(userindex, lpMsg->accountid, lpMsg->ecoins, lpMsg->ectype, lpMsg->aindex);
}

void protocol::reqreloadconf(_PMSG_RELOAD_REQ* lpMsg, uintptr_t userindex)
{
	if (guser.getuser(userindex)->ismuadmin == false && guser.getuser(userindex)->isuseradmin == false) {
		msglog(INFO, "reqreloadconf, %s requesting to reload conf but not an admin.", guser.getuser(userindex)->account.c_str());
		return;
	}

	msglog(INFO, "Reloaded configs via admin command.");
	c.load();
}

void protocol::reqgetecoins(_PMSG_GETECOINS_REQ* lpMsg, uintptr_t userindex)
{
	if (guser.getuser(userindex)->ismuadmin == false && guser.getuser(userindex)->isuseradmin == false) {
		msglog(INFO, "reqgetecoins, %s requesting to get user ecoins but not an admin.", guser.getuser(userindex)->account.c_str());
		return;
	}
	guser.getuserecoins(userindex, lpMsg->accountid, lpMsg->ecoins, lpMsg->ectype, lpMsg->aindex);
}

void protocol::reqotpcode(_PMSG_OTPCODE_REQ* lpMsg, uintptr_t userindex)
{
	guser.otpcode(lpMsg->otpcode, userindex);
}

void protocol::reqotplogin(_PMSG_OTP_REQ* lpMsg, uintptr_t userindex)
{
	guser.otplogin(lpMsg->mobilenum, userindex);
}

void protocol::reqtokenlogin(_PMSG_LOGIN_TOKEN* lpMsg, uintptr_t userindex)
{
	if (lpMsg->gamever != APK_VER) {
#if GAME_TYPE == 1
		guser.sendnotice(userindex, 8, "You are using an outdated app, please update our app from playstore.");
#else
		guser.sendnotice(userindex, 8, "Your app is outdated, you can get the updated version from our Download page.");
#endif
		return;
	}
	
	guser.tokenlogin(lpMsg->md5token, userindex);
}

void protocol::requserlogin(_PMSG_LOGIN_USER* lpMsg, uintptr_t userindex)
{
	if (lpMsg->gamever != APK_VER) {
#if GAME_TYPE == 1
		guser.sendnotice(userindex, 8, "You are using an outdated app, please update our app from playstore.");
#else
		guser.sendnotice(userindex, 8, "Your app is outdated, you can get the updated version from our Download page.");
#endif
		return;
	}

	guser.userlogin(lpMsg->user, lpMsg->secret, userindex);
}

void protocol::reqsapawcard(unsigned char* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;


	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	_PMSG_SAPAWCARD_REQ* _lpMsg = (_PMSG_SAPAWCARD_REQ*)lpMsg;
	int size = sizeof(_PMSG_SAPAWCARD_REQ);

	if(!_g->sapawcard(userindex, _lpMsg->userpos, _lpMsg->downpos, _lpMsg->count, lpMsg + sizeof(_PMSG_SAPAWCARD_REQ)))
		_g->sendresult(userindex, 0);
}

void protocol::requngroupcard(_PMSG_UNGRPCARD_REQ* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;

	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	if(!_g->ungroupcards(userindex, lpMsg->downpos))
		_g->sendresult(userindex, 0);
}

void protocol::reqgroupcard(unsigned char* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;

	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	_PMSG_GRPCARD_REQ* _lpMsg = (_PMSG_GRPCARD_REQ*)lpMsg;
	int size = sizeof(_PMSG_GRPCARD_REQ);

	if(!_g->groupcards(userindex, _lpMsg->count, lpMsg + size))
		_g->sendresult(userindex, 0);
}

void protocol::reqdowncard(unsigned char* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;

	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	_PMSG_DOWNCARD_REQ* _lpMsg = (_PMSG_DOWNCARD_REQ*)lpMsg;
	int size = sizeof(_PMSG_DOWNCARD_REQ);

	if(!_g->downcards(userindex, _lpMsg->count, lpMsg + size))
		_g->sendresult(userindex, 0);

}

void protocol::reqchowcard(unsigned char* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;


	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	_PMSG_CHOWCARD_REQ* _lpMsg = (_PMSG_CHOWCARD_REQ*)lpMsg;
	int size = sizeof(_PMSG_CHOWCARD_REQ);

	if(!_g->chowcard(userindex, _lpMsg->userpos, _lpMsg->pos, _lpMsg->count, lpMsg + size))
		_g->sendresult(userindex, 0);

}

void protocol::reqdropcard(_PMSG_DROP_CARD_REQ* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;

	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	if(!_g->dropcard(userindex, lpMsg->pos))
		_g->sendresult(userindex, 0);
}

void protocol::reqdrawcard(_PMSG_DRAW_CARD_REQ* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;

	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	if(!_g->drawfromstock(userindex))
		_g->sendresult(userindex, 0);

}

void protocol::reqalive(_PMSG_ALIVE* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL)
		return;

	userinfo->alivetick = GetTickCount64();

	//msglog(DEBUG, "reqalive, %s alive packet recvd.", userinfo->name.c_str());

	/*_PMSG_ALIVE pMsg;
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.hdr.len = sizeof(_PMSG_ALIVE);
	pMsg.sub = 0x01;*/
}

void protocol::reqmovecardpos(_PMSG_MOVECARD_REQ* lpMsg, uintptr_t userindex)
{
	if (lpMsg->group != 0)
		return;

	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;

	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	if (!_g->movecardpos(userindex, lpMsg->group, lpMsg->s_pos, lpMsg->t_pos))
		_g->sendresult(userindex, 0);
}

void protocol::reqfightcard(_PMSG_FIGHTCARD_REQ* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	if (userinfo->isauto)
		return;

	if (!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;

	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	if(!_g->fightcard(userindex))
		_g->sendresult(userindex, 0);

}

void protocol::reqfight2card(_PMSG_FIGHTCARD_REQ* lpMsg, uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL) {
		return;
	}

	msglog(DEBUG, "reqfight2card %s accepted fight.", userinfo->name.c_str());

	if (userinfo->isauto)
		return;

	if(!(userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING))
		return;

	int64_t gameserial = userinfo->m_gameserial;

	if (gameserial == 0)
		return;

	game* _g = gcontrol.getgame(gameserial);

	if (_g == NULL)
		return;

	if(!_g->fight2card(userindex, lpMsg->isfight))
		_g->sendresult(userindex, 0);

}

bool protocol::parsedata(uintptr_t userindex)
{
	_USER_INFO* userinfo = guser.getuser(userindex);

	if (userinfo == NULL)
		return false;

	while (true) {

		if (userinfo->packetdata.bufferlen < 5)
			break;

		_PMSG_HDR* data = (_PMSG_HDR*)userinfo->packetdata.buffer;
		int len = data->len;
		unsigned char head = data->h;

		if (data->c == 0xC2) {
			_PMSG_HDR_MU* data2 = (_PMSG_HDR_MU*)userinfo->packetdata.buffer;
			len = MAKE_NUMBERW(data2->len[0], data2->len[1]);
			head = data2->h;
		}

		if (doprotocol(userindex, (unsigned char*)userinfo->packetdata.buffer, head) == false) {
			guser.deluser(userindex);
			return false;
		}

		userinfo->packetdata.bufferlen -= len;

		if (userinfo->packetdata.bufferlen <= 0)
			break;

		memcpy(&userinfo->packetdata.buffer[0], userinfo->packetdata.buffer + len, userinfo->packetdata.bufferlen);
	}

	return true;
}

