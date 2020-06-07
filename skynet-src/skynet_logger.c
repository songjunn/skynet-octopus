#include "skynet.h"
#include "skynet_logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

#define LOG_MESSAGE_SIZE 204800
#define LOG_MESSAGE_FORMAT LOG_MESSAGE_SIZE+1024

static struct skynet_service * LOGGER = 0;

int gettid() {
#ifdef __linux__
    return syscall(SYS_gettid);
#endif
#ifdef WIN32
    return GetCurrentThreadId();
#endif
}

void skynet_logger_start(struct skynet_service * ctx) {
    LOGGER = ctx;
}

void skynet_logger_exit() {
    LOGGER = NULL;
}

void skynet_logger_format_head(char * buffer, size_t size, int level, uint32_t source) {
    switch (level) {
    case LOGGER_DEBUG: snprintf(buffer, size, "[:%04x][%d] DEBUG ", source, gettid()); break;
    case LOGGER_WARN: snprintf(buffer, size, "[:%04x][%d] WARN ", source, gettid()); break;
    case LOGGER_NOTICE: snprintf(buffer, size, "[:%04x][%d] NOTICE ", source, gettid()); break;
    case LOGGER_ERROR: snprintf(buffer, size, "[:%04x][%d] ERROR ", source, gettid()); break;
    default: break;
    }
}

void skynet_logger_format_time(char * buffer, size_t size) {
    struct tm * newtime;
    time_t aclock;
    time(&aclock);
    newtime = localtime(&aclock);

    struct timeval tv;
    gettimeofday(&tv,NULL);

    snprintf(buffer, size, "[%02d:%02d:%02d %02d:%02d:%02d.%06d] ", newtime->tm_year+1900, 
        newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec, tv.tv_usec);
}

void skynet_logger_print(uint32_t source, int level, const char * msg, ...) {
    va_list ap;
    va_start(ap, msg);
    char tmp[LOG_MESSAGE_SIZE] = {0};
    int len = vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
    if (len >= LOG_MESSAGE_SIZE) {
        len = LOG_MESSAGE_SIZE - 1;
    }
    va_end(ap);

    char head[64] = {0}, time[64] = {0}, message[LOG_MESSAGE_FORMAT] = {0};
    skynet_logger_format_time(time, sizeof(time));
    skynet_logger_format_head(head, sizeof(head), level, source);
    snprintf(message, LOG_MESSAGE_FORMAT, "%s%s%.*s", time, head, len, tmp);

    fprintf(stdout, message);
    fprintf(stdout, "\n");

    if (LOGGER) {
        skynet_send(LOGGER, source, 0, level, (void *)message, strlen(message));
    }
}
