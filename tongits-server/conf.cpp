#include "conf.h"
#include "user.h"
#include <fstream>

conf c;

conf::conf()
{
	this->m_ispassmd5 = false;
	this->m_serverport = 0;
	this->m_isdebug = false;
	this->m_gpslimitdis = 0.0f;
}

conf::~conf()
{

}

void conf::load()
{
	try {
		msglog(INFO, "Loading configurations...");
		this->configs = YAML::LoadFile(YAML_CONF);
		this->m_isdebug = configs["Debug Message"].as<bool>();
		this->m_serverport = configs["Server Port"].as<int>();
		this->m_ispassmd5 = configs["Secret Is MD5"].as<bool>();
		this->m_gpslimitdis = configs["GPS Limit Distance"].as<float>();
		this->m_tax = configs["Tax"].as<float>();
		this->sql.dbname = configs["SQL OdbcName"].as<std::string>();
		this->sql.user = configs["SQL User"].as<std::string>();
		this->sql.secret = configs["SQL Secret"].as<std::string>();
		this->musecret = configs["MU Secret"].as<std::string>();
		YAML::Node betmodes = configs["Tongits Bet Modes"];
		YAML::iterator iter = betmodes.begin();
		while (iter != betmodes.end()) {
			const YAML::Node& betmode = *iter;
			_TONGITS_BET_INFO betinfo;
			betinfo.name = betmode["Name"].as<std::string>();
			betinfo.enable = betmode["Enable"].as<bool>();
			betinfo.type = betmode["Type"].as<int>();
			betinfo.hits = betmode["Hits Base"].as<int>();
			betinfo.hitsadd = betmode["Hits Add"].as<int>();
			betinfo.tongits = betmode["Tongits"].as<int>();
			betinfo.nontongits = betmode["Non-Tongits"].as<int>();
			betinfo.fight = betmode["Fight"].as<int>();
			betinfo.fightpercard = betmode["Fight Per Card"].as<int>();
			betinfo.quadra = betmode["Quadra"].as<int>();
			betinfo.royal = betmode["Royal"].as<int>();
			betinfo.ace = betmode["Ace"].as<int>();
			betinfo.sagasa = betmode["Sagasa"].as<int>();
			betinfo.burned = betmode["Burned"].as<int>();
			this->mTongitsBetModes.insert(std::make_pair(betinfo.type, betinfo));
			iter++;
		}
		msglog(INFO, "Loading configurations done.");
	}
	catch (const YAML::BadFile& e) {
		msglog(eMSGTYPE::ERROR, "YAML error, %s.", e.msg.c_str());
	}
	catch (const YAML::ParserException& e) {
		msglog(eMSGTYPE::ERROR, "YAML error, %s.", e.msg.c_str());
	}
}

_TONGITS_BET_INFO conf::getbetmode(int type)
{
	return this->mTongitsBetModes[type];
}
