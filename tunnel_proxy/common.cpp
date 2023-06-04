#include "common.h"
#ifndef _WIN32
//#include <android/log.h>
#else
#include "LogToFile.h"
#endif

#ifdef _WIN32
CLogToFile tongitslog("tongits", "Log", 1);
#endif


#if TONGITS_LOG_SILENT == 1
DWORD LOGTYPEENABLED = 0;
#else
DWORD LOGTYPEENABLED = ((DWORD)eMSGTYPE::INFO | (DWORD)eMSGTYPE::ERROR | (DWORD)eMSGTYPE::SQL | (DWORD)eMSGTYPE::DEBUG);
#endif

void msglog(BYTE type, const char* msg, ...) {

	if (!(LOGTYPEENABLED & (DWORD)type))
		return;

	char szBuffer[1024] = { 0 };
	va_list pArguments;

	switch (type) {
	case eMSGTYPE::INFO:
		sprintf(szBuffer, " [INFO] ");
		break;
	case eMSGTYPE::ERROR:
		sprintf(szBuffer, " [ERROR] ");
		break;
	case eMSGTYPE::DEBUG:
		sprintf(szBuffer, " [DEBUG] ");
		break;
	case eMSGTYPE::SQL:
		sprintf(szBuffer, " [SQL] ");
		break;
	}

	va_start(pArguments, msg);
	size_t iSize = strlen(szBuffer);
	vsprintf(&szBuffer[iSize], msg, pArguments);
	va_end(pArguments);


	auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char* curstrtime = ctime(&timenow);
	curstrtime[strlen(curstrtime) - 1] = '\0';

#ifndef _WIN32
	//__android_log_print(ANDROID_LOG_DEBUG, "TONGITS", "%s%s",curstrtime, szBuffer);
#else
	std::cout << curstrtime << szBuffer << std::endl;
	tongitslog.Output(szBuffer);
#endif
}

#ifndef _WIN32
unsigned long long GetTickCount64()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}
#endif

DWORD host2ip(const char* hostname)
{
	struct hostent* h = gethostbyname(hostname);
	return (h != NULL) ? ntohl(*(DWORD*)h->h_addr) : 0;
}

int SQLSyntexCheck(char* SQLString)
{
	char* temp;

	temp = strchr(SQLString, 39);
	if (temp == 0)
	{
		temp = strchr(SQLString, 32);
		if (temp == 0)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
}

DWORD MakeAccountKey(char* lpszAccountID)
{
	int len = strlen(lpszAccountID);

	if (len <= 0)
		return 0;

	DWORD dwKey = 0;

	for (int i = 0; i < len; i++)
		dwKey += lpszAccountID[i] + 17;

	dwKey = (dwKey + (10 - len) * 17) % 256;
	return dwKey % 256;
}