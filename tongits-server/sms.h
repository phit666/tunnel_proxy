#pragma once
#include <vector>
#include <mutex>

struct _SMS_INFO
{
	std::string mobilenumber;
	std::string otpmsg;
};

void smsworker();
extern std::mutex lock;
extern std::vector<_SMS_INFO>vsmsinfo;
extern bool endworker;

