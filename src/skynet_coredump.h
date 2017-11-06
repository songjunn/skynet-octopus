#pragma once
#ifndef _WIN32
#include <execinfo.h>
#include <sys/sysinfo.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include <Windows.h> 
#include <dbghelp.h>
#include <pthread.h>
#pragma comment(lib,"DbgHelp.lib") 
LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS *pException);
#endif

void saveBackTrace(int signal);
typedef void (*FnErrorExit)();
extern FnErrorExit pErrorExit;
