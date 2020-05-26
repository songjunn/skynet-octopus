#include "skynet.h"
#include "skynet_logger.h"

#include <stdarg.h>
#include <stdio.h>

#define LOG_MESSAGE_SIZE 204800
#define LOG_MESSAGE_FORMAT 205824

static struct skynet_service * LOGGER = 0;

void skynet_logger_start(struct skynet_service * ctx) {
    LOGGER = ctx;
}

void skynet_logger_exit() {
    LOGGER = NULL;
}

void skynet_logger_format_head(char * buffer, size_t size, int level, uint32_t source) {
    switch (level) {
    case LOGGER_DEBUG: snprintf(buffer, size, "DEBUG [:%04x][%d] ", source, getpid()); break;
    case LOGGER_WARN: snprintf(buffer, size, "WARN [:%04x][%d] ", source, getpid()); break;
    case LOGGER_NOTICE: snprintf(buffer, size, "NOTICE [:%04x][%d] ", source, getpid()); break;
    case LOGGER_ERROR: snprintf(buffer, size, "ERROR [:%04x][%d] ", source, getpid()); break;
    default: break;
    }
}

void skynet_logger_format_time(char * buffer, size_t size) {
    struct tm * newtime;
    time_t aclock;
    time(&aclock);
    newtime = localtime(&aclock);
    snprintf(buffer, size, "[%02d:%02d:%02d %02d:%02d:%02d] ", newtime->tm_year+1900, 
        newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
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
    skynet_logger_format_time(time, time);
    skynet_logger_format_head(head, sizeof(head), level, source);
    snprintf(message, LOG_MESSAGE_FORMAT, "%s%s%.*s", time, head, tmp, len);

    fprintf(stdout, message);
    fprintf(stdout, "\n");

    if (LOGGER) {
        skynet_send(LOGGER, source, 0, level, (void *)message, len);
    }
}
