#pragma once
#include "common.h"
#include <map>
#include <vector>

#define YAML_CONF "conf.yaml"

struct _SQL
{
	std::string dbname;
	std::string  user;
	std::string  secret;
};

struct _TONGITS_BET_INFO
{
	std::string name;
	bool enable;
	int type;
	int hits;
	int hitsadd;
	int tongits;
	int nontongits;
	int fight;
	int fightpercard;
	int quadra;
	int royal;
	int ace;
	int sagasa;
	int burned;
};

class conf
{
public:
	conf();
	~conf();
	
	void load();

	unsigned short getserverport() { return m_serverport; }
	std::string getmusecret() { return musecret; }

	bool isdebug() { return m_isdebug; }
	bool issecretmd5() { return m_ispassmd5; }
	float getgpslimitdis() { return this->m_gpslimitdis; }
	float getax() { return this->m_tax; }

	_SQL getsql() { return sql; }

	_TONGITS_BET_INFO getbetmode(int type);

private:

	std::map <unsigned char, _TONGITS_BET_INFO> mTongitsBetModes;

	YAML::Node configs;

	bool m_ispassmd5;
	unsigned short m_serverport;
	bool m_isdebug;

	float m_gpslimitdis;
	float m_tax;

	std::string musecret;

	_SQL sql;
};

extern conf c;


