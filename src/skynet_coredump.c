#include "coredump.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <string.h>

int g_processingCmd = 0;
long long g_processingUID = 0;
FnErrorExit pErrorExit = NULL;

#ifndef _WIN32

struct flock* file_lock(short type, short whence)
{
	static struct flock ret;
	ret.l_type = type ;
	ret.l_start = 0;
	ret.l_whence = whence;
	ret.l_len = 0;
	ret.l_pid = getpid();
	return &ret;
}
#endif

void saveBackTrace(int signal)
{
#ifndef	WIN32
	if (pErrorExit != NULL)
	{
		(*pErrorExit)();
	}
	time_t tSetTime;
	time( &tSetTime);
	tm ptm;
	localtime_r(&tSetTime, &ptm) ;
	char fname[256] = {0};
	sprintf(fname, "core.%d-%d-%d %d:%d:%d",
		ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday,
		ptm.tm_hour, ptm.tm_min, ptm.tm_sec);

	FILE* f = fopen(fname, "a");
	if (f == NULL)
	{
		return;
	}
	int fd = fileno(f);
	fcntl(fd, F_SETLKW, file_lock(F_WRLCK, SEEK_SET));

	char buffer[4096] = {0};
	int count = readlink("/proc/self/exe", buffer, 4096);
	if(count > 0)
	{
		buffer[count] = '\n';
		buffer[count + 1] = 0;
		fwrite(buffer, 1, count + 1, f);
		fflush(f);
	}

	sprintf(buffer, "Dump Time: %d-%d-%d %d:%d:%d\n",
		ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday,
		ptm.tm_hour, ptm.tm_min, ptm.tm_sec);
	fwrite(buffer, 1, strlen(buffer), f);
	fflush(f);

	strcpy(buffer, "Catch signal: ");
	switch (signal)
	{
	case SIGSEGV: strcat(buffer, "SIGSEGV\n");
		break;
	case SIGILL: strcat(buffer, "SIGILL\n");
		break;
	case SIGFPE: strcat(buffer, "SIGFPE\n");
		break;
	case SIGABRT: strcat(buffer, "SIGABRT\n");
		break;
	case SIGTERM: strcat(buffer, "SIGTERM\n");
		break;
	case SIGKILL: strcat(buffer, "SIGKILL\n");
		break;
	case SIGXFSZ: strcat(buffer, "SIGXFSZ\n");
		break;
	default: sprintf(buffer, "Catch signal: %d\n", signal);
		break;
	}
	fwrite(buffer, 1, strlen(buffer), f);
	fflush(f);

	sprintf(buffer, "Processing cmd: %d\n", g_processingCmd);
	fwrite(buffer, 1, strlen(buffer), f);
	fflush(f);
	sprintf(buffer, "Processing uid: %lld\n", g_processingUID);
	fwrite(buffer, 1, strlen(buffer), f);
	fflush(f);

	void* DumpArray[256];
	int	nSize =	backtrace(DumpArray, 256);
	char** symbols = backtrace_symbols(DumpArray, nSize);
	if (symbols)
	{
		if (nSize > 256)
		{
			nSize = 256;
		}
		if (nSize > 0)
		{
			for (int i = 0; i < nSize; i++)
			{
				fwrite(symbols[i], 1, strlen(symbols[i]), f);
				fwrite("\n", 1, 1, f);
				fflush(f);
			}
		}
		free(symbols);
	}
	fcntl(fd, F_SETLK, file_lock(F_UNLCK, SEEK_SET));
	fclose(f);

	exit(1);
#endif
}
#ifdef _WIN32
LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS *pException)
{
	time_t tSetTime;
	time( &tSetTime);
	tm ptm;
	localtime_r(&tSetTime, &ptm) ;
	char fname[MAX_PATH] = {0};
	char strError[256] ={0};
	char strExePath[MAX_PATH] = {0};
	int nErrorCode = 0;
	HANDLE  hDumpFile = NULL;
	::GetModuleFileName(NULL, strExePath, MAX_PATH);
	sprintf(fname, "%s-%04d%02d%02d-%02d-%02d-%02d.dmp", strExePath, ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday,ptm.tm_hour, ptm.tm_min, ptm.tm_sec);
	hDumpFile = CreateFile(fname,GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hDumpFile != NULL && hDumpFile != INVALID_HANDLE_VALUE)
	{
		struct _MINIDUMP_EXCEPTION_INFORMATION  mei;
		mei.ThreadId   = GetCurrentThreadId();
		mei.ExceptionPointers  = (pException == NULL ? NULL : pException);
		mei.ClientPointers     = FALSE;
		MINIDUMP_TYPE mdt =  (MINIDUMP_TYPE)(MiniDumpNormal
			| MiniDumpWithHandleData
			| MiniDumpWithUnloadedModules
			| MiniDumpWithIndirectlyReferencedMemory
			| MiniDumpScanMemory
			| MiniDumpWithProcessThreadData | MiniDumpWithThreadInfo);
		BOOL bResult = MiniDumpWriteDump(GetCurrentProcess(),GetCurrentProcessId(),hDumpFile,mdt,&mei,NULL,NULL);
		CloseHandle(hDumpFile);
		if(!bResult)
			nErrorCode = GetLastError();
	}
	else
	{
		nErrorCode = GetLastError();
	}	
	return nErrorCode;
}
#endif
