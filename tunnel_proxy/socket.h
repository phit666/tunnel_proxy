#pragma once

int le_start();
bool datasend(intptr_t userindex, unsigned char* data, int len);
bool sendotp(const char* smsnum, char* otpmsg);
void le_updatecbfd(uintptr_t userid, uintptr_t resume_userid);
extern std::mutex mlock;
