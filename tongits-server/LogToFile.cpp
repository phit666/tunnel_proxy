#include "LogToFile.h"
#include <Windows.h>
#include <stdio.h>

CLogToFile::CLogToFile(const char* LogFileName, const char* LogDirectoryName, int bWithDate)
{
	if (strcmp(LogFileName, "") == 0)
	{
		strcpy(this->m_szLogFileName, "LOGDATA");
	}
	else
	{
		strcpy(this->m_szLogFileName, LogFileName);
	}


	if (!strcmp(LogDirectoryName, ""))
	{
		strcpy(this->m_szLogDirectoryName, "LOG");
	}
	else
	{
		strcpy(this->m_szLogDirectoryName, LogDirectoryName);
	}

	this->m_bWithDate = bWithDate;

	CreateDirectory(LogDirectoryName, NULL);

	//InitializeCriticalSection(&this->m_critLogToFile );
}

CLogToFile::~CLogToFile()
{
	fclose(this->m_fLogFile);
}

void CLogToFile::Output( LPSTR fmt, ...)
{
	va_list argptr;
	int iChrWritten;
	char szLogFileName[260]; // Is StringZero Terminated

	SYSTEMTIME strSystime;
	
	//EnterCriticalSection(&this->m_critLogToFile );

	va_start(argptr, fmt);
	iChrWritten=vsprintf(this->m_cBUFFER , fmt, argptr);
	va_end(argptr);

	GetLocalTime(&strSystime);

	if(this->day != strSystime.wDay)
	{
		this->yr = strSystime.wYear;
		this->mo = strSystime.wMonth;
		this->day = strSystime.wDay;

		sprintf(szLogFileName, "%s\\%s_%04d-%02d-%02d.txt",
			&this->m_szLogDirectoryName[0] , &this->m_szLogFileName [0],
			strSystime.wYear, strSystime.wMonth, strSystime.wDay);

		if (this->m_fLogFile != NULL)
			fclose(this->m_fLogFile);

		this->m_fLogFile = fopen(szLogFileName, "a+t");
	}

	if ( this->m_fLogFile == NULL )
	{
		//LeaveCriticalSection(&this->m_critLogToFile );	
	}
	else
	{
		if (this->m_bWithDate ==0)
		{
			fprintf(this->m_fLogFile , "%s\n", this->m_cBUFFER);
		}
		else
		{
			fprintf(this->m_fLogFile , "%04d:%02d:%02d  %s\n", strSystime.wHour, strSystime.wMinute, strSystime.wSecond, this->m_cBUFFER);
		}

		//LeaveCriticalSection(&this->m_critLogToFile );
	}
}
