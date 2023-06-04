#ifndef LOGTOFILE_H
#define LOGTOFILE_H
#include <stdio.h>

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CLogToFile
{

public:

	void Output(char* fmt, ...);
	CLogToFile(const char* LogFileName, const char* LogDirectoryName, int bWithDate);
	~CLogToFile();
	unsigned char flushlog;

private:

	unsigned short yr;
	unsigned short mo;
	unsigned short day;

	FILE* m_fLogFile;	// 0
	char m_szLogFileName[260]; // 4
	char m_szLogDirectoryName[260]; // 108
	char m_cBUFFER[65536];	// 20C
	int m_bWithDate; // 1020C
};

#endif