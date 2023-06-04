#pragma once
struct _PMSG_HDR_MU
{
	unsigned char c;
	unsigned char len[2];
	unsigned char h;
};

struct _PMSG_HDR
{
	unsigned char c;
	unsigned short len;
	unsigned char h;
};

struct _PMSG_DEF_SUB
{
	_PMSG_HDR hdr;
	unsigned char sub;
};

struct _PMSG_DEF_SUB_MU
{
	_PMSG_HDR_MU hdr;
	unsigned char sub;
};

struct _INIT_CARDS
{
	unsigned char sub;
	unsigned char userpos;
	unsigned char cardcounts;
	unsigned char cardtype[13];
	unsigned char cardnum[13];
};

struct _PMSG_SEND_INIT_CARDS
{
	_PMSG_HDR hdr;
	_INIT_CARDS data;
};

struct _PMSG_MOVECARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char group;
	unsigned char s_pos[2];
	unsigned char t_pos[2];
};

struct _PMSG_DRAW_CARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
	unsigned char init;
	unsigned char cardtype;
	unsigned char cardnum;
};

struct _PMSG_DRAW_CARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
};

struct _PMSG_DROP_CARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
	unsigned char pos;
	unsigned char cardtype;
	unsigned char cardnum;
};

struct _PMSG_DROP_CARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char pos[2];
};

struct _PMSG_CARD_STOCKINFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char stockcount;
};

struct _PMSG_CHOWCARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
	unsigned char pos[2];
	unsigned char count;
	// cards...
};

struct _PMSG_CARD_INFO
{
	unsigned char cardtype;
	unsigned char cardnum;
};

struct _PMSG_CHOWCARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char downpos;
	unsigned char gamepos;
	unsigned char userpos;
	unsigned char chowpos[2];
	unsigned char count;
	// cards...
};

struct _PMSG_DOWNCARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char count;
	// cards...
};

struct _PMSG_DOWNCARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char downpos;
	unsigned char gamepos;
	unsigned char count;
	// cards...
};

struct _PMSG_SAPAWCARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
	unsigned char downpos;
	unsigned char count;
	// cards...
};

struct _PMSG_SAPAWCARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char downpos;
	unsigned char gamepos;
	unsigned char usergamepos;
	unsigned char count;
	// cards...
};

struct _PMSG_ACTIVEINFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char activeuserpos;
	unsigned int timelimit_msec;
	unsigned char activegamestate;
	int activehitteruserpos;
	int activegamecounter;
	unsigned int activehitprizeecoins;
};

struct _PMSG_USERNAMEINFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char gamepos;
	char name[10];
};

struct _PMSG_USERECOINSINFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char gamepos;
	unsigned int ecoins;
};

struct _PMSG_USERCARDSINFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char gamepos;
	unsigned int cardcounts;
};

struct _PMSG_RESETGAMEINFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
};

struct _PMSG_NOTICEMSG
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
	unsigned char type;
	char msg[100];
};

struct _PMSG_GRPCARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char count;
	// cards...
};

struct _PMSG_GRPCARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	int grppos;
	unsigned char gamepos;
	unsigned char count;
	// cards...
};

struct _PMSG_UNGRPCARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	int downpos;
};

struct _PMSG_UNGRPCARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char downpos;
	unsigned char gamepos;
};

struct _PMSG_TIMEOUT_INFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned int timemsecleft;
};

struct _PMSG_FIGHTMODE_INFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char enable;
};

struct _PMSG_FIGHTCARD_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char isfight;
};

struct _PMSG_FIGHTCARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
};

struct _PMSG_FIGHT2CARD_ANS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
	unsigned char isfight;
};

struct _PMSG_ALIVE
{
	_PMSG_HDR hdr;
	unsigned char sub;
};

struct _PMSG_SHOW_USERCARDS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
	unsigned char count;
	// cards
};

struct _PMSG_SHOW_GRPCARDS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos;
	unsigned char count;
	// cards
};

struct _PMSG_ACTION_RESULT
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char result;
};

struct _PMSG_LOGIN_TOKEN
{
	_PMSG_HDR hdr;
	unsigned char sub;
	char md5token[33];
	unsigned char gamever;
};

struct _PMSG_LOGIN_USER
{
	_PMSG_HDR hdr;
	unsigned char sub;
	char user[20];
	char secret[20];
	unsigned char gamever;
};

struct _PMSG_ACTIVESTATUS
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char userpos[3];
};

struct _PMSG_INITINFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char init;
	unsigned char gamepos;
	unsigned char resume;
	unsigned char ectype;
};


struct _PMSG_TOKEN_INFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char flag;
	char token[33];
};


struct _USER_TRANSACT_INFO
{
	unsigned char cardcounts;
	unsigned int current_ecoins;
	unsigned int regular;
	unsigned int quadra;
	unsigned int royal;
	unsigned int ace;
	unsigned int fight;
	unsigned int burned;
};

struct _PMSG_TRANSACT_INFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char winnerpos;
	unsigned int hitecoins;
	_USER_TRANSACT_INFO users[3];
};

struct _PMSG_REQ_USERCARDS
{
	_PMSG_HDR hdr;
	unsigned char sub;
};

struct _PMSG_REQ_DRAWINIT
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char flag;
};

struct _PMSG_ACCOUNT_INFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	char accountid[20];
	unsigned int ecoins;
	unsigned int jewels;
};

struct _PMSG_LOGIN_RESULT
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char result;
	char accountid[20];
	unsigned int ecoins;
	unsigned int jewels;
	char ecoinsnote[10];
	char jewelsnote[10];
	unsigned char isuseradmin;
};

struct _PMSG_JOINGAME_INFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char gametype;
};

struct _PMSG_MUADMIN_LOGIN
{
	_PMSG_HDR_MU hdr;
	unsigned char sub;
	char secret[33];
};

struct _PMSG_ADDECOINS_REQ
{
	_PMSG_HDR_MU hdr;
	unsigned char sub;
	char accountid[20];
	unsigned char ectype;
	int ecoins;
	int aindex;
};

struct _PMSG_ADDECOINS_RES
{
	_PMSG_HDR_MU hdr;
	unsigned char sub;
	unsigned char ectype;
	int ecoins;
	int ecointstotal;
	int aindex;
	unsigned char result;
};

struct _PMSG_GETECOINS_REQ
{
	_PMSG_HDR_MU hdr;
	unsigned char sub;
	char accountid[20];
	unsigned char ectype;
	int ecoins;
	int aindex;
};

struct _PMSG_GETECOINS_ANS
{
	_PMSG_HDR_MU hdr;
	unsigned char sub;
	unsigned char ectype;
	int ecoins;
	int ecointstotal;
	int aindex;
	unsigned char result;
};

struct _PMSG_MULOGIN_RES
{
	_PMSG_HDR_MU hdr;
	unsigned char sub;
	unsigned char result;
};

struct _PMSG_GPS_INFO
{
	_PMSG_HDR hdr;
	unsigned char sub;
	double longitude;
	double latitue;
};

struct _PMSG_RESETINFO_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
};

struct _PMSG_RELOAD_REQ
{
	_PMSG_HDR_MU hdr;
	unsigned char sub;
	int aindex;
};

struct _PMSG_OTP_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	char mobilenum[12]; // 09176390701
};

struct _PMSG_OTPCODE_REQ
{
	_PMSG_HDR hdr;
	unsigned char sub;
	int otpcode;
};

struct _PMSG_OTP_RES
{
	_PMSG_HDR hdr;
	unsigned char sub;
	unsigned char result;
};