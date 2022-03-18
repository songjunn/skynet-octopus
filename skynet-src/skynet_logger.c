#include "skynet.h"
#include "skynet_logger.h"
#include "skynet_malloc.h"
#include "skynet_mq.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

#define LOG_MESSAGE_SIZE 204800

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
    time_t sec = time(0);
    struct tm newtime;
    localtime_r(&sec, &newtime);

    struct timeval tv;
    gettimeofday(&tv,NULL);

    snprintf(buffer, size, "[%02d:%02d:%02d %02d:%02d:%02d.%06d] ", newtime.tm_year+1900, 
        newtime.tm_mon+1, newtime.tm_mday, newtime.tm_hour, newtime.tm_min, newtime.tm_sec, tv.tv_usec);
}

void skynet_logger_print(uint32_t source, int level, const char * msg, ...) {
    char head[64] = {0}, time[64] = {0};
    char * data = skynet_malloc(LOG_MESSAGE_SIZE+128);
    skynet_malloc_insert(data, LOG_MESSAGE_SIZE+128, __FILE__, __LINE__);
    skynet_logger_format_time(time, sizeof(time));
    skynet_logger_format_head(head, sizeof(head), level, source);
    int sz = snprintf(data, 128, "%s%s", time, head);

    va_list ap;
    va_start(ap, msg);
    int len = vsnprintf(data+sz, LOG_MESSAGE_SIZE, msg, ap);
    if (len >= LOG_MESSAGE_SIZE) {
        len = LOG_MESSAGE_SIZE - 1;
    }
    va_end(ap);

    fprintf(stdout, data);
    fprintf(stdout, "\n");

    if (LOGGER) {
        struct skynet_message smsg;
        smsg.source = source;
        smsg.type = level;
        smsg.size = len+sz;
        smsg.session = 0;
        smsg.data = data;

        if (skynet_service_sendmsg(LOGGER, &smsg)) {
            skynet_free(smsg.data);
        }
    } else {
        skynet_free(data);
    }
}
