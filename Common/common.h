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

 /** @file common.h
	A shared common header for Tunnel Proxy.
 */
#pragma once

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#pragma comment(lib,"Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <string.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/thread.h>

#include <chrono>
#include <ctime>

#include <map>
#include <vector>

#define TUNNEL_PROXY_VER_MAJOR 1
#define TUNNEL_PROXY_VER_MINOR 2
#define TUNNEL_PROXY_VER_REV 0
#define TUNNEL_PROXY_VER_STG "-b.1"


#ifndef _WIN32
#define DWORD unsigned int
#define BYTE unsigned char
#define WORD unsigned short int
#endif

enum eMSGTYPE
{
	INFO = 1,
	ERROR = 2,
	DEBUG = 4
};

enum eREQTYPE
{
	CREATE_TUNNEL = 0xA1,
	KEEP_ALIVE = 0xA2,
};

struct _PckCmd
{
	BYTE head;
	BYTE len;
	BYTE cmd;
	WORD data;
};


#define LOGTYPEENABLED  ((DWORD)eMSGTYPE::INFO | (DWORD)eMSGTYPE::ERROR | (DWORD)eMSGTYPE::DEBUG)

void msglog(BYTE type, const char* msg, ...);
DWORD host2ip(const char* hostname);
#ifndef _WIN32
unsigned long long GetTickCount64();
#endif
