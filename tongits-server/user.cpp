#include "user.h"
#include "game.h"
#include "gamectrl.h"
#include "socket.h"
//#include "db.h"
#include "md5.h"
#include "conf.h"
#include "sms.h"


user guser;

user::user()
{
	this->m_waitings = 0;
	this->m_userindex = 1;
	for (int i = 1; i < MAX_USERS + 1; i++) {
		this->adduser(i, NULL);
	}
}

user::~user()
{

}

void user::clear()
{
	std::map <uintptr_t, _USER_INFO*>::iterator iter;// m_mUsers
	for (iter = this->m_mUsers.begin(); iter != this->m_mUsers.end(); iter++) {
		bufferevent_free(iter->second->packetdata.bev);
		delete iter->second;
	}
	this->m_mUsers.clear();
}

void user::adduser(uintptr_t userindex, struct bufferevent* bev)
{
	_USER_INFO* userinfo = new _USER_INFO;
	userinfo->packetdata.bev = bev;
	this->m_mUsers[userindex] = userinfo;
	//userinfo->name = names[this->m_mUsers.size()-1];
	//msglog(DEBUG, "adduser, name %s userindex %d.", userinfo->name.c_str(), userindex);
}

bool user::isuserloggedin(uintptr_t token)
{
	if (token == 0)
		return false;

	std::map <uintptr_t, _USER_INFO*>::iterator iter;
	for (iter = this->m_mUsers.begin(); iter != this->m_mUsers.end(); iter++) {
		_USER_INFO* _user = iter->second;
		if (token == _user->token && _user->isloggedin() && !_user->isdc())
			return true;
	}
	return false;
}

uintptr_t user::getuserindex()
{
	uintptr_t freeindex = 0;
	int passctr = 0;

	while (true) {

		if (this->m_userindex >= MAX_USERS + 1)
			this->m_userindex = 1;

		if (this->m_mUsers[this->m_userindex]->isfreeuser && 
			GetTickCount64() > this->m_mUsers[this->m_userindex]->deltick) {
			freeindex = this->m_userindex;
			this->m_userindex += 1;
			break;
		}

		if (passctr >= MAX_USERS) {
			this->m_userindex += 1;
			break;
		}

		this->m_userindex += 1;
		passctr++;
	}

	return freeindex;
}

_USER_INFO* user::getuser(uintptr_t userindex) 
{
	std::map <uintptr_t, _USER_INFO*>::iterator iter;
	iter = this->m_mUsers.find(userindex);
	if (iter == this->m_mUsers.end()) {
		msglog(DEBUG, "getuser, userindex %llu does not exist.", userindex);
		return NULL;
	}
	return iter->second;
}

void user::checkaliveusers()
{
	/*std::map <uintptr_t, _USER_INFO*>::iterator iter;
	for (iter = this->m_mUsers.begin(); iter != this->m_mUsers.end(); iter++) {
		_USER_INFO* userinfo = iter->second;
		if (userinfo->alivetick != 0 && 
			GetTickCount64() > (userinfo->alivetick + ALIVE_TICK_TIMEOUT)) { // timeout occured, should save session info to try resuming the game later...
			if (iter == this->m_mUsers.end())
				break;
		}
	}*/
}


void user::sendnotice(uintptr_t userindex, unsigned char type, const char* msg, ...)
{
	char szBuffer[1024] = { 0 };
	va_list pArguments;
	va_start(pArguments, msg);
	size_t iSize = strlen(szBuffer);
	vsprintf(&szBuffer[iSize], msg, pArguments);
	va_end(pArguments);

	_PMSG_NOTICEMSG pMsg = { 0 };
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF2;
	pMsg.hdr.len = sizeof(_PMSG_NOTICEMSG);
	pMsg.sub = 0x00;
	pMsg.type = type;
	memcpy(pMsg.msg, szBuffer, sizeof(pMsg.msg));

	::datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
}

bool user::adduserecoins(uintptr_t userindex, char* accountid, int ecoins, unsigned char ectype, int aindex)
{
	if (accountid == NULL || ectype > 1)
		return false;

	uintptr_t _userindex = 0;
	char sbuf[11] = { 0 };
	std::map <uintptr_t, _USER_INFO*>::iterator iter;
	for (iter = this->m_mUsers.begin(); iter != this->m_mUsers.end(); iter++) {

		if (iter->second->isfreeuser)
			continue;

		if (iter->second->ismuadmin)
			continue;

		memcpy(sbuf, iter->second->account.c_str(), 10);

		if (accountid[0] == sbuf[0] && accountid[3] == sbuf[3]) {
			if (strncmp(accountid, sbuf, 10) == 0) {
				_userindex = iter->first;
				break;
			}
		}
	}

	_PMSG_ADDECOINS_RES pMsg = { 0 };
	pMsg.hdr.c = 0xC2;
	pMsg.hdr.h = 0xF4;
	int size = sizeof(_PMSG_ADDECOINS_RES);
	pMsg.hdr.len[0] = SET_NUMBERH(size);
	pMsg.hdr.len[1] = SET_NUMBERL(size);
	pMsg.sub = 0x01;
	pMsg.ectype = ectype;
	pMsg.aindex = aindex;
	pMsg.ecoins = ecoins;

	if (_userindex != 0) { 

		_USER_INFO* userinfo = guser.getuser(_userindex);

		if (userinfo == NULL)
			return false;

		userinfo->ecoins[ectype] += ecoins;

		msglog(INFO, "%d eCoins (%d) added to %s, ecoins now is %d.", ecoins, ectype, userinfo->account.c_str(), userinfo->ecoins[ectype]);

		if ((userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING)) {
			int64_t gameserial = userinfo->m_gameserial;
			if (gameserial == 0) {
				this->saveecoins(_userindex, ectype);
			}
			else {
				game* _g = gcontrol.getgame(gameserial);
				if (_g != NULL) {
					_g->senduserecoinsinfo(_userindex);
				}
				else {
					this->saveecoins(_userindex, ectype);
				}
			}
		}
		else {
			this->saveecoins(_userindex, ectype);
		}

		if(ectype == 0)
			this->sendnotice(_userindex, 0, "%d eCoins has been added to your account.", ecoins);
		else if (ectype == 1)
			this->sendnotice(_userindex, 0, "%d Jewels has been added to your account.", ecoins);

		pMsg.ecointstotal = guser.getuser(_userindex)->ecoins[ectype];
		pMsg.result = 1;

		if (!guser.getuser(_userindex)->isplaying()) {
			guser.deluser(_userindex, true);
			guser.getuser(_userindex)->relog();
			_PMSG_LOGIN_RESULT pMsg = { 0 };
			pMsg.hdr.c = 0xC1;
			pMsg.hdr.h = 0xF2;
			pMsg.hdr.len = sizeof(_PMSG_LOGIN_RESULT);
			pMsg.sub = 0x09;
			pMsg.result = 1; // option to create game
			pMsg.isuseradmin = (guser.getuser(userindex)->isuseradmin) ? 1 : 0;
			strcpy(pMsg.accountid, guser.getuser(_userindex)->account.c_str());
			pMsg.ecoins = guser.getuser(_userindex)->ecoins[0];
			pMsg.jewels = guser.getuser(_userindex)->ecoins[1];
			strcpy(pMsg.ecoinsnote, c.getbetmode(0).name.c_str());
			strcpy(pMsg.jewelsnote, c.getbetmode(1).name.c_str());
			::datasend(_userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
		}
	}
	else {
		/*db.ExecQuery("select guiid, ecoins, jewels from tongits where account_id = '%s'", accountid);
		if (db.Fetch() != SQL_NO_DATA) 
		{
			uintptr_t guiid = db.Geti64("guiid");
			int currentecoins = db.GetInt("ecoins");
			int currentjewels = db.GetInt("jewels");
			db.Clear();
			if (ectype == 0) {
				db.ExecQuery("update tongits set ecoins = ecoins + %d where guiid =  %llu", ecoins, guiid);
				pMsg.ecointstotal = currentecoins + ecoins;
			}
			else if(ectype == 1) {
				db.ExecQuery("update tongits set jewels = jewels + %d where guiid =  %llu", ecoins, guiid);
				pMsg.ecointstotal = currentjewels + ecoins;
			}
			pMsg.result = 1;
			::datasend(userindex, (unsigned char*)&pMsg, size);
			return true;
		}
		else {
			db.Clear();
			pMsg.result = 0;
		}*/
	}

	::datasend(userindex, (unsigned char*)&pMsg, size);
	return true;
}

bool user::getuserecoins(uintptr_t userindex, char* accountid, int ecoins, unsigned char ectype, int aindex)
{
	if (accountid == NULL || ectype > 1)
		return false;

	uintptr_t _userindex = 0;
	char sbuf[11] = { 0 };
	std::map <uintptr_t, _USER_INFO*>::iterator iter;
	for (iter = this->m_mUsers.begin(); iter != this->m_mUsers.end(); iter++) {

		if (iter->second->isfreeuser)
			continue;

		if (iter->second->ismuadmin)
			continue;

		memcpy(sbuf, iter->second->account.c_str(), 10);

		if (accountid[0] == sbuf[0] && accountid[3] == sbuf[3]) {
			if (strncmp(accountid, sbuf, 10) == 0) {
				_userindex = iter->first;
				break;
			}
		}
	}

	_PMSG_GETECOINS_ANS pMsg = { 0 };
	pMsg.hdr.c = 0xC2;
	pMsg.hdr.h = 0xF4;
	int size = sizeof(_PMSG_GETECOINS_ANS);
	pMsg.hdr.len[0] = SET_NUMBERH(size);
	pMsg.hdr.len[1] = SET_NUMBERL(size);
	pMsg.sub = 0x02;
	pMsg.ectype = ectype;
	pMsg.aindex = aindex;
	pMsg.ecoins = ecoins;

	if (_userindex != 0) {

		_USER_INFO* userinfo = guser.getuser(_userindex);

		if (userinfo == NULL)
			return false;

		if (ecoins > userinfo->ecoins[ectype]) {
			pMsg.ecointstotal = userinfo->ecoins[ectype];
			pMsg.result = 2; // not enough ecoins
			::datasend(userindex, (unsigned char*)&pMsg, size);
			return true;
		}

		if ((userinfo->m_state & (unsigned char)_USER_STATE::_PLAYING)) {
			pMsg.result = 3; // currently playing
			::datasend(userindex, (unsigned char*)&pMsg, size);
			return true;
		}

		userinfo->ecoins[ectype] -= ecoins;

		msglog(INFO, "%d eCoins (%d) cashedout to %s, ecoins now is %d.", ecoins, ectype, userinfo->account.c_str(), userinfo->ecoins[ectype]);

		this->saveecoins(_userindex, ectype);

		if (ectype == 0)
			this->sendnotice(_userindex, 0, "%d eCoins has been deducted to your account.", ecoins);
		else if (ectype == 1)
			this->sendnotice(_userindex, 0, "%d Jewels has been deducted to your account.", ecoins);

		pMsg.ecointstotal = guser.getuser(_userindex)->ecoins[ectype];
		pMsg.result = 1;

		if (!guser.getuser(_userindex)->isplaying()) {
			guser.deluser(_userindex, true);
			guser.getuser(_userindex)->relog();
			_PMSG_LOGIN_RESULT pMsg = { 0 };
			pMsg.hdr.c = 0xC1;
			pMsg.hdr.h = 0xF2;
			pMsg.hdr.len = sizeof(_PMSG_LOGIN_RESULT);
			pMsg.sub = 0x09;
			pMsg.result = 1; // option to create game
			pMsg.isuseradmin = (guser.getuser(userindex)->isuseradmin) ? 1 : 0;
			strcpy(pMsg.accountid, guser.getuser(_userindex)->account.c_str());
			pMsg.ecoins = guser.getuser(_userindex)->ecoins[0];
			pMsg.jewels = guser.getuser(_userindex)->ecoins[1];
			strcpy(pMsg.ecoinsnote, c.getbetmode(0).name.c_str());
			strcpy(pMsg.jewelsnote, c.getbetmode(1).name.c_str());
			::datasend(_userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
		}
	}
	else {
		/*db.ExecQuery("select guiid, ecoins, jewels from tongits where account_id = '%s'", accountid);
		if (db.Fetch() != SQL_NO_DATA) {
			uintptr_t guiid = db.Geti64("guiid");
			int currentecoins = db.GetInt("ecoins");
			int currentjewels = db.GetInt("jewels");
			db.Clear();

			if (currentecoins < ecoins && ectype == 0) {
				pMsg.ecointstotal = currentecoins;
				pMsg.result = 2; // not enough ecoins
				::datasend(userindex, (unsigned char*)&pMsg, size);
				return true;
			}

			if (currentjewels < ecoins && ectype == 1) {
				pMsg.ecointstotal = currentjewels;
				pMsg.result = 2; // not enough ecoins
				::datasend(userindex, (unsigned char*)&pMsg, size);
				return true;
			}

			if (ectype == 0) {
				db.ExecQuery("update tongits set ecoins = ecoins - %d where guiid =  %llu", ecoins, guiid);
				pMsg.ecointstotal = currentecoins - ecoins;
			}
			else if (ectype == 1) {
				db.ExecQuery("update tongits set jewels = jewels - %d where guiid =  %llu", ecoins, guiid);
				pMsg.ecointstotal = currentjewels - ecoins;
			}
			pMsg.result = 1;
		}
		else {
			db.Clear();
			pMsg.result = 0;
		}*/
	}

	::datasend(userindex, (unsigned char*)&pMsg, size);
	return true;
}


void user::saveecoins(uintptr_t userindex, unsigned char type)
{
	char szTemp[256] = { 0 };

	if (type == 0) { // ecoins
		if (this->getuser(userindex)->ecoins[0] < 0)
			this->getuser(userindex)->ecoins[0] = 0;
		sprintf(szTemp, "update tongits set ecoins = %d where guiid =  %llu", this->getuser(userindex)->ecoins[0], this->getuser(userindex)->token);
	}
	else if (type == 1) {
		if (this->getuser(userindex)->ecoins[1] < 0)
			this->getuser(userindex)->ecoins[1] = 0;
		sprintf(szTemp, "update tongits set jewels = %d where guiid =  %llu", this->getuser(userindex)->ecoins[1], this->getuser(userindex)->token);
	}
	//db.ExecQuery(szTemp);
	//db.Fetch();
	//db.Clear();
}

void user::otpcode(int otpcode, uintptr_t userindex)
{
	char sbuf[100] = { 0 };
	std::string _mobilenum;

	int _otpcode = this->getuser(userindex)->otpcode;
	_mobilenum = this->getuser(userindex)->mobilenum;

	if (_otpcode == 0)
		return;
	if (_otpcode != otpcode) {
		this->sendnotice(userindex, 8, "You have entered a wrong OTP code.");
		msglog(INFO, "Wrong OTP Code %d / %d, mobile num %s.", otpcode, _otpcode, _mobilenum.c_str());
		return;
	}

	char logintoken[33] = { 0 };
	MD5 pMD5Hash;
	DWORD dwAccKey = MakeAccountKey((char*)_mobilenum.c_str());
	sprintf(sbuf, "%s%d%llu", _mobilenum.c_str(), otpcode, GetTickCount64());
	pMD5Hash.MD5_EncodeString(sbuf, logintoken, dwAccKey);

	sprintf(sbuf, "select guiid, ecoins from tongits where mobile_num='%s' and mode = 1", _mobilenum.c_str());
	/*if (db.ExecQuery(sbuf)) {
		if (db.Fetch() == SQL_NO_DATA) {
			this->sendnotice(userindex, 8, "Fetching your account's data failed!");
			msglog(SQL, "otplogin, failed (%s).", sbuf);
			return;
		}
		else {
			this->getuser(userindex)->token = db.Geti64("guiid");
			this->getuser(userindex)->ecoins[0] = db.GetInt("ecoins");
			this->getuser(userindex)->account = _mobilenum;
		}
	}
	else {
		this->sendnotice(userindex, 8, "Fetching your account's data failed!");
		msglog(SQL, "otplogin, failed (%s).", sbuf);
		return;
	}*/

	//db.Clear();

	/*if (db.ExecQuery("update tongits set acctoken='%s' where guiid = %llu", logintoken, guser.getuser(userindex)->token)) {
		_PMSG_TOKEN_INFO pMsg = { 0 };
		pMsg.hdr.c = 0xC1;
		pMsg.hdr.h = 0xF2;
		pMsg.hdr.len = sizeof(_PMSG_TOKEN_INFO);
		pMsg.sub = 0x07;
		pMsg.flag = 1; // set token
		memcpy(pMsg.token, logintoken, sizeof(pMsg.token));
		msglog(DEBUG, "otpcode, assigned token %s to %s.", logintoken, _mobilenum.c_str());
		::datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
	}*/
	this->getuser(userindex)->setlogged();
	gcontrol.getusersessioninfo(guser.getuser(userindex)->token, userindex);

}

void user::otplogin(char* mobilenum, uintptr_t userindex)
{
	char sbuf[100] = { 0 };
	std::string _m;
	std::string _mobilenum;

	if (mobilenum == NULL)
		return;
	if (strlen(mobilenum) < 11)
		return;

	if (!SQLSyntexCheck(mobilenum))
		return;

	int otpcode = rand() % 9000 + 1000;

	msglog(INFO, "otplogin, mobile number %s.", mobilenum);

	if (mobilenum[0] == '0' && (mobilenum[1] == '8' || mobilenum[1] == '9') && strlen(mobilenum) == 11) {
		_m = mobilenum;
		msglog(INFO, "otplogin, mobile number %s otp code %d.", _m.c_str(), otpcode);
		_mobilenum = "63" + _m.substr(1, _m.length() - 1);
		msglog(INFO, "otplogin, mobile number %s otp code %d.", _mobilenum.c_str(), otpcode);
	}
	else if (mobilenum[0] == '6' && mobilenum[1] == '3' && (mobilenum[2] == '8' || mobilenum[2] == '9') && strlen(mobilenum) == 12)
	{
		_mobilenum = mobilenum;
		msglog(INFO, "otplogin, mobile number %s otp code %d.", _mobilenum.c_str(), otpcode);
	}
	else {
		guser.sendnotice(userindex, 8, "You mobile number %s is invalid, format should be like 09170342328.");
		return;
	}

	sprintf(sbuf, "select guiid from tongits where mobile_num='%s' and mode = 1", _mobilenum.c_str());

	uintptr_t _token = 0;

	/*if (db.ExecQuery(sbuf)) {
		if (db.Fetch() == SQL_NO_DATA) {
			db.Clear();
			sprintf(sbuf, "insert into tongits (account_id, mobile_num, acctoken, mode) values ('%s', '%s', 'NONE', 1)",
				_mobilenum.c_str(),
				_mobilenum.c_str()
			);
			if (!db.ExecQuery(sbuf)) {
				msglog(SQL, "otplogin, failed (%s).", sbuf);
				return;
			}
		}
		else {
			_token = db.Geti64("guiid");
		}
	}
	else {
		msglog(SQL, "otplogin, failed (%s).", sbuf);
		return;
	}*/

	//db.Clear();

	if (this->isuserloggedin(_token)) {
		this->sendnotice(userindex, 8, "Your account is already logged in!");
		return;
	}

	sprintf(sbuf, "Your Tongits Classic OTP Code is %d.", otpcode);

	if (!this->sendsmsotp(_mobilenum.c_str(), sbuf)) {
		this->sendnotice(userindex, 8, "Something went wrong with our OTP system, please try again in few minutes.");
		return;
	}
	else {
		this->getuser(userindex)->otpcode = otpcode;
		this->getuser(userindex)->mobilenum = _mobilenum;
		this->sendnotice(userindex, 8, "OTP code is sent to your mobile number %s.", mobilenum);
	}

	_PMSG_OTP_RES pMsg;
	pMsg.hdr.c = 0xC1;
	pMsg.hdr.h = 0xF1;
	pMsg.hdr.len = sizeof(_PMSG_OTP_RES);
	pMsg.sub = 0x09;
	pMsg.result = 1;
	::datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
}

void user::mulogin(char* musecret, uintptr_t userindex)
{
	if (strncmp(musecret, c.getmusecret().c_str(), c.getmusecret().length()) == 0) {
		this->getuser(userindex)->setmuadmin();

		_PMSG_MULOGIN_RES pMsg = { 0 };
		pMsg.hdr.c = 0xC2;
		pMsg.hdr.h = 0xF4;
		int size = sizeof(_PMSG_MULOGIN_RES);
		pMsg.hdr.len[0] = SET_NUMBERH(size);
		pMsg.hdr.len[1] = SET_NUMBERL(size);
		pMsg.sub = 0x00;
		pMsg.result = 1;
		::datasend(userindex, (unsigned char*)&pMsg, size);

		msglog(INFO, "mulogin, MU Admin set at index %d.", userindex);
	}
}

void user::tokenlogin(char* logintoken, uintptr_t userindex)
{
	char szTemp[256] = { 0 };
	bool clearlogintoken = false;

	if (logintoken == NULL) {
		clearlogintoken = true;
	}

	if (logintoken != NULL && strlen(logintoken) != 32) {
		clearlogintoken = true;
	}

	uintptr_t _token = 0;

	if (clearlogintoken == false) {

		if (!SQLSyntexCheck(logintoken)) {
			msglog(SQL, "tokenlogin, anti-sql injection invoked, logintoken %s", logintoken);
			return;
		}

		sprintf(szTemp, "SELECT guiid, account_id, acctoken, mobile_num, ecoins, jewels, gametoken, mode, nogps, isadmin from tongits where acctoken='%s'", logintoken);

		/*if (db.ExecQuery(szTemp)) {
			if (db.Fetch() == SQL_NO_DATA) {
				this->sendnotice(userindex, 8, "Fetching your account's data failed!");
				clearlogintoken = true;
			}
			else {
				_token = db.Geti64("guiid");
			}
		}
		else {
			this->sendnotice(userindex, 8, "Fetching your account's data failed!");
			msglog(SQL, "Failed query, (%s).", szTemp);
			clearlogintoken = true;
		}*/

		//db.Clear();
	}

	if (this->isuserloggedin(_token)) {
		this->sendnotice(userindex, 8, "Your account is already logged in!");
		return;
	}
	/*
	int gametype = db.GetInt("mode");

	if (clearlogintoken || gametype != GAME_TYPE) {
		_PMSG_TOKEN_INFO pMsg = { 0 };
		pMsg.hdr.c = 0xC1;
		pMsg.hdr.h = 0xF2;
		pMsg.hdr.len = sizeof(_PMSG_TOKEN_INFO);
		pMsg.sub = 0x07;
		pMsg.flag = 0; // delete token
		::datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
		msglog(DEBUG, "sent clear login token packet, %d %d.", clearlogintoken, gametype);
		return;
	}

	_USER_INFO* _user = this->getuser(userindex);
	char sbuf[20] = { 0 };
	db.GetStr("account_id", sbuf);
	_user->account = sbuf;
	db.GetStr("mobile_num", sbuf);
	_user->mobilenum = sbuf;
	if (_user->account.length() == 0)
		_user->account = _user->mobilenum;
	_user->token = db.Geti64("guiid");
	_user->ecoins[0] = db.GetInt("ecoins");
	_user->ecoins[1] = db.GetInt("jewels");
	_user->gametoken = db.GetInt("gametoken");
	_user->isuseradmin = (db.GetInt("isadmin") == 1) ? true : false;
	_user->isnogps = (db.GetInt("nogps") == 1) ? true : false;
	
	msglog(INFO, "[tokenlogin] %s login data, login token: %s session token: %d ecoins: %d jewels: %d gametoken: %d",
		_user->account,
		logintoken,
		_user->token,
		_user->ecoins[0],
		_user->ecoins[1],
		_user->gametoken
	);
	*/
	//_user->setlogged();
	//gcontrol.getusersessioninfo(_user->token, userindex);

}

void user::userlogin(char* username, char* secret, uintptr_t userindex)
{
	char sbuf[50] = { 0 };
	char logintoken[33] = { 0 };
	char szTemp[256] = { 0 };
	BYTE btBinaryPass[16] = { 0 };
	int err = 0;

	if (username == NULL || secret == NULL)
		return;

	if (strlen(username) >= 20 || strlen(secret) >= 20)
		return;

	if (!SQLSyntexCheck(username) || !SQLSyntexCheck(secret)) {
		msglog(SQL, "userlogin, anti-sql injection invoked, user %s pass %s", username, secret);
		return;
	}

	try
	{
		MD5 pMD5Hash;
		DWORD dwAccKey = MakeAccountKey(username);

		/*if (c.issecretmd5() == false) {
			sprintf(szTemp, "SELECT memb__pwd from MEMB_INFO where memb___id='%s' and memb__pwd = '%s'", username, secret);
			if (db.ExecQuery(szTemp)) {
				if (db.Fetch() == SQL_NO_DATA) {
					this->sendnotice(userindex, 8, "Wrong username or password!");
					msglog(SQL, "Account %s does not exist.", username);
					db.Clear();
					return;
				}
			}
			else {
				this->sendnotice(userindex, 8, "Wrong username or password!");
				msglog(SQL, "Failed query, (%s).", szTemp);
				db.Clear();
				return;
			}
		}
		else {

			sprintf(szTemp, "SELECT memb__pwd from MEMB_INFO where memb___id='%s'", username);
			if (db.ReadBlob(szTemp, btBinaryPass) < 0) {
				this->sendnotice(userindex, 8, "Wrong username or password!");
				msglog(SQL, "Account %s does not exist.", username);
				db.Clear();
				return;
			}

			if (pMD5Hash.MD5_CheckValue(secret, (char*)btBinaryPass, dwAccKey) == false)
			{
				this->sendnotice(userindex, 8, "Wrong username or password!");
				msglog(SQL, "%s wrong password.", username);
				db.Clear();
				return;
			}
		}

		db.Clear();*/
		msglog(INFO, "%s logged in.", username);

		sprintf(szTemp, "SELECT guiid, mobile_num, ecoins, jewels, gametoken, nogps from tongits where account_id='%s'", username);

		/*if (db.ExecQuery(szTemp)) {
			if (db.Fetch() == SQL_NO_DATA) {
				msglog(INFO, "%s does not exist in tongits db, inserting...", username);
				sprintf(szTemp, "insert into tongits (account_id, acctoken) values ('%s', 'NONE')", username);
				db.Clear();
				if (db.ExecQuery(szTemp)) {
					msglog(INFO, "%s does not exist in tongits db, inserting done.", username);
					sprintf(szTemp, "SELECT guiid, acctoken, mobile_num, ecoins, jewels, gametoken, nogps from tongits where account_id='%s'", username);
					if (db.ExecQuery(szTemp)) {
						if (db.Fetch() == SQL_NO_DATA) {
							this->sendnotice(userindex, 8, "Fetching your account's data failed!");
							return;
						}
					}
					else {
						this->sendnotice(userindex, 8, "Fetching your account's data failed!");
						msglog(SQL, "Failed query, (%s).", szTemp);
						return;
					}
				}
				else {
					this->sendnotice(userindex, 8, "Fetching your account's data failed!");
					msglog(SQL, "Failed query, (%s).", szTemp);
					return;
				}
			}
		}
		else {
			msglog(SQL, "Failed query, (%s).", szTemp);
			this->sendnotice(userindex, 8, "Fetching your account's data failed!");
			return;
		}*/


		_USER_INFO* _user = this->getuser(userindex);
		_user->account = username;
		//_user->token = db.Geti64("guiid");
		//_user->ecoins[0] = db.GetInt("ecoins");
		//_user->ecoins[1] = db.GetInt("jewels");
		//_user->gametoken = db.GetInt("gametoken");
		//_user->isnogps = (db.GetInt("nogps") == 1) ? true : false;

		if (this->isuserloggedin(_user->token)) {
			this->sendnotice(userindex, 8, "Your account is already logged in!");
			return;
		}

		sprintf(sbuf, "%s%llu%llu", username, _user->token, GetTickCount64());
		pMD5Hash.MD5_EncodeString(sbuf, logintoken, dwAccKey);


		msglog(INFO, "[userlogin] %s login data, login token: %s session token: %d ecoins: %d jewels: %d gametoken: %d", 
			username,
			logintoken, 
			_user->token, 
			_user->ecoins[0],
			_user->ecoins[1],
			_user->gametoken
		);

		//db.Clear();

		/*if (db.ExecQuery("update tongits set acctoken='%s' where guiid = %llu", logintoken, _user->token)) {
			_PMSG_TOKEN_INFO pMsg = { 0 };
			pMsg.hdr.c = 0xC1;
			pMsg.hdr.h = 0xF2;
			pMsg.hdr.len = sizeof(_PMSG_TOKEN_INFO);
			pMsg.sub = 0x07;
			pMsg.flag = 1; // set token
			memcpy(pMsg.token, logintoken, sizeof(pMsg.token));
			msglog(DEBUG, "userlogin, assignd token %s to %s.", logintoken, username);
			::datasend(userindex, (unsigned char*)&pMsg, pMsg.hdr.len);
		}*/

		_user->setlogged();
		gcontrol.getusersessioninfo(_user->token, userindex);
	}
	catch (...)
	{
		msglog(SQL, "MD5 Password Decrypt Failed - AccountId : %s", username);
	}
}

void user::delmuadmin(uintptr_t userindex)
{
	_USER_INFO* userinfo = this->m_mUsers[userindex];
	userinfo->set();
	userinfo->init();
}

void user::deluser(uintptr_t userindex, bool isreset)
{
	_USER_INFO* userinfo = this->m_mUsers[userindex];

	if (isreset) {
		gcontrol.endgamesession(userinfo->token, userindex);
		if(userinfo->isdc())
			userinfo->set();
		else
			userinfo->endgame();
		userinfo->init();
		return;
	}

	// the session data should be saved to database before resetting this user info so later the user can resume the game
	if (userinfo->m_gameserial == 0) { // we can end the session now as the  user is not linked to game
		msglog(DEBUG, "deluser, userindex %llu endgamesession.", userindex);
		gcontrol.endgamesession(userinfo->token, userindex);
		// finally we will do init to reset info
		userinfo->set();
		userinfo->init();
	}
	else {
		msglog(DEBUG, "deluser, userindex %llu disconnected with resume option.", userindex);
		userinfo->disconnected(); // player can go back and resume game session later
	}
}

void user::updateuserbev(uintptr_t userid, uintptr_t resume_userid)
{
	bufferevent_free(this->getuser(resume_userid)->packetdata.bev);
	this->getuser(resume_userid)->packetdata.bev = this->getuser(userid)->packetdata.bev;
	this->getuser(resume_userid)->packetdata.bufferlen = 0;
}

double user::getdistancegps(double lat1, double long1, double lat2, double long2)
{
	double dist;
	dist = sin(toRad(lat1)) * sin(toRad(lat2)) + cos(toRad(lat1)) * cos(toRad(lat2)) * cos(toRad(long1 - long2));
	dist = acos(dist);
	//        dist = (6371 * pi * dist) / 180;
		//got dist in radian, no need to change back to degree and convert to rad again.
	dist = 6371 * dist;
	return dist;
}

bool user::sendsmsotp(const char* smsnum, char* otpmsg) 
{
	lock.lock();
	_SMS_INFO smsinfo;
	smsinfo.mobilenumber = smsnum;
	smsinfo.otpmsg = otpmsg;
	vsmsinfo.push_back(smsinfo);
	lock.unlock();
	return true;
}

void user::trystartgame(unsigned char gametype)
{
	int ctr = 0;
	uintptr_t user[3];
	std::map <uintptr_t, _USER_INFO*>::iterator iter;
	for (iter = this->m_mUsers.begin(); iter != this->m_mUsers.end(); iter++) {

		if (iter->second->isfreeuser)
			continue;

		if (!(iter->second->m_state & (unsigned char)_USER_STATE::_WAITING))
			continue;

		if (iter->second->ectype != gametype)
			continue;

		if (iter->second->isnogps)
			continue;


		if (c.getgpslimitdis() != 0.0f) {

			if (iter->second->gps.longitude == 0.000000f || iter->second->gps.latitude == 0.000000f || GetTickCount64() > iter->second->gps.tick)
				continue;

			// check distance using gps
			for (int n = 0; n < ctr; n++) {

				double dist = this->getdistancegps(this->getuser(user[n])->gps.latitude, this->getuser(user[n])->gps.longitude,
					this->getuser(iter->first)->gps.latitude, this->getuser(iter->first)->gps.longitude);

				msglog(INFO, "trystartgame, %s and %s distance %f",
					this->getuser(user[n])->account.c_str(), this->getuser(iter->first)->account.c_str(), dist);

				if (dist < c.getgpslimitdis()) {
					msglog(INFO, "Failed to join %s to %s game because the players distance of %f < %f.",
						this->getuser(iter->first)->account.c_str(), this->getuser(user[n])->account.c_str(), dist, c.getgpslimitdis());
					continue;
				}
			}
		}



		user[ctr++] = iter->first;
		
		if (ctr >= 3) {
			gcontrol.startgamesession(guser.getuser(user[0])->token, user[0]);
			gcontrol.startgamesession(guser.getuser(user[1])->token, user[1]);
			gcontrol.startgamesession(guser.getuser(user[2])->token, user[2]);
			gcontrol.addgame(user[0], user[1], user[2], gametype);
			break;
		}
	}

	for (iter = this->m_mUsers.begin(); iter != this->m_mUsers.end(); iter++) {

		if (iter->second->isfreeuser)
			continue;

		if (!(iter->second->m_state & (unsigned char)_USER_STATE::_WAITING))
			continue;

		if (iter->second->ectype != gametype)
			continue;

		if (!iter->second->isnogps)
			continue;

		user[ctr++] = iter->first;

		if (ctr >= 3) {
			gcontrol.startgamesession(guser.getuser(user[0])->token, user[0]);
			gcontrol.startgamesession(guser.getuser(user[1])->token, user[1]);
			gcontrol.startgamesession(guser.getuser(user[2])->token, user[2]);
			gcontrol.addgame(user[0], user[1], user[2], gametype);
			break;
		}
	}
}


