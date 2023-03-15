/*
 * MIT License
 *
 * Copyright (c) 2023 phit666
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

 /** @file common.cpp
	A shared common system for Tunnel Proxy.
 */

#include "common.h"

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
	}

	va_start(pArguments, msg);
	size_t iSize = strlen(szBuffer);
	vsprintf(&szBuffer[iSize], msg, pArguments);
	va_end(pArguments);

	auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char* curstrtime = ctime(&timenow);
	curstrtime[strlen(curstrtime) - 1] = '\0';
	std::cout << curstrtime << szBuffer << std::endl;
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