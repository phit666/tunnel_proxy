#pragma once
#include "common.h"

class protocol
{
public:
	protocol();
	~protocol();

	void reqotpcode(_PMSG_OTPCODE_REQ* lpMsg, uintptr_t userindex);
	void reqotplogin(_PMSG_OTP_REQ* lpMsg, uintptr_t userindex);
	void reqtokenlogin(_PMSG_LOGIN_TOKEN* lpMsg, uintptr_t userindex);
	void requserlogin(_PMSG_LOGIN_USER* lpMsg, uintptr_t userindex);
	void reqjoingame(_PMSG_JOINGAME_INFO* lpMsg, uintptr_t userindex);
	void reqgpsinfo(_PMSG_GPS_INFO* lpMsg, uintptr_t userindex);
	void reqresetinfo(_PMSG_RESETINFO_REQ* lpMsg, uintptr_t userindex);

	void reqaddecoins(_PMSG_ADDECOINS_REQ* lpMsg, uintptr_t userindex);
	void reqgetecoins(_PMSG_GETECOINS_REQ* lpMsg, uintptr_t userindex);

	void reqreloadconf(_PMSG_RELOAD_REQ* lpMsg, uintptr_t userindex);


	void reqmulogin(_PMSG_MUADMIN_LOGIN* lpMsg, uintptr_t userindex);

	void reqalive(_PMSG_ALIVE* lpMsg, uintptr_t userindex);
	void reqgamestart(_PMSG_REQ_DRAWINIT* lpMsg, uintptr_t userindex);
	void requsercards(_PMSG_REQ_USERCARDS* lpMsg, uintptr_t userindex);
	void reqmovecardpos(_PMSG_MOVECARD_REQ* lpMsg, uintptr_t userindex);
	void reqdropcard(_PMSG_DROP_CARD_REQ* lpMsg, uintptr_t userindex);
	void reqdrawcard(_PMSG_DRAW_CARD_REQ* lpMsg, uintptr_t userindex);
	void reqchowcard(unsigned char* lpMsg, uintptr_t userindex);
	void reqdowncard(unsigned char* lpMsg, uintptr_t userindex);
	void reqgroupcard(unsigned char* lpMsg, uintptr_t userindex);
	void requngroupcard(_PMSG_UNGRPCARD_REQ* lpMsg, uintptr_t userindex);
	void reqsapawcard(unsigned char* lpMsg, uintptr_t userindex);

	void reqfightcard(_PMSG_FIGHTCARD_REQ* lpMsg, uintptr_t userindex);
	void reqfight2card(_PMSG_FIGHTCARD_REQ* lpMsg, uintptr_t userindex);

	bool parsedata(uintptr_t userindex);
	bool doprotocol(uintptr_t userindex, unsigned char* data, unsigned char head);

	//void sendinitcards(uintptr_t userindex, _PMSG_SEND_INIT_CARDS);

private:
};

extern protocol gprotocol;


