#include "skynet.h"
#include "skynet_logger.h"
#include "skynet_server.h"
#include "skynet_service.h"
#include "skynet_mq.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define LOG_MESSAGE_SIZE 204800
#define LOG_MESSAGE_LINE 10000

struct skynet_logger {
    int line;
    int level;
    int close;
    FILE * handle;
    char basename[64];
    char filename[128];
    struct skynet_service * ctx;
};

static struct skynet_logger * instance = NULL;

void _format_name(char * filename, size_t size, const char * basename) {
    struct tm * newtime;
    time_t aclock;
    time(&aclock);
    newtime = localtime(&aclock);
    snprintf(filename, size, "%s-%02d%02d%02d-%02d%02d%02d.log", basename, newtime->tm_year+1900, 
            newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour,
            newtime->tm_min, newtime->tm_sec);
}

void _format_time(char * buffer, size_t size) {
    struct tm * newtime;
    time_t aclock;
    time(&aclock);
    newtime = localtime(&aclock);
    snprintf(buffer, size, "[%02d:%02d:%02d %02d:%02d:%02d] ", newtime->tm_year+1900, 
            newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour,
            newtime->tm_min, newtime->tm_sec);
}

void _format_head(char * buffer, size_t size, int level, uint32_t source) {
    switch (level) {
    case LOGGER_DEBUG: snprintf(buffer, size, "DEBUG [:%04x] ", source); break;
    case LOGGER_WARN: snprintf(buffer, size, "WARN [:%04x] ", source); break;
    case LOGGER_NOTICE: snprintf(buffer, size, "NOTICE [:%04x] ", source); break;
    case LOGGER_ERROR: snprintf(buffer, size, "ERROR [:%04x] ", source); break;
    default: break;
    }
}

FILE * _open_file(char * filname, size_t size, const char * basename) {
    _format_name(filename, size, basename);
    FILE * handle = fopen(filename, "wb");
    return handle;
}

bool logger_create(struct skynet_service * ctx, int harbor, const char * args) {
    instance = skynet_malloc(sizeof(*instance));
    instance->ctx = ctx;
    instance->line = 0;
    instance->close = 0;
    instance->level = 0;
    instance->handle = NULL;

    sscanf(args, "%[^','],%d", instance->basename, &instance->level);

    if (strlen(instance->basename) > 0) {
        instance->handle = _open_file(instance->filename, sizeof(instance->filename), instance->basename);
        if (instance->handle == NULL) {
            return false;
        }
        instance->close = 1;
    }
    return true;
}

void logger_release() {
    if (instance->close) {
        fclose(instance->handle);
    }
    skynet_free(instance);
}

bool logger_callback(int level, uint32_t source, void * msg, size_t sz) {
    if (level < instance->level) {
        return false;
    }

    char head[64] = {0}, time[64] = {0}, content[LOG_MESSAGE_SIZE] = {0};
    _format_time(time, sizeof(time));
    _format_head(head, sizeof(head), level, source);
    snprintf(content, sz+1, "%s", (const char*)msg);

    if (instance->line >= LOG_MESSAGE_LINE) {
        instance->close = 0;
        fclose(instance->handle);

        instance->handle = _open_file(instance->filename, sizeof(instance->filename), instance->basename);
        if (instance->handle) {
            instance->line = 0;
            instance->close = 1;
        }
    }

    if (instance->handle) {
        fprintf(instance->handle, time);
        fprintf(instance->handle, head);
        fprintf(instance->handle, content);
        fprintf(instance->handle, "\n");
        fflush(instance->handle);
        instance->line++;
    }

    fprintf(stdout, time);
    fprintf(stdout, head);
    fprintf(stdout, content);
    fprintf(stdout, "\n");

    return true;
}

void skynet_logger_init(int harbor, const char * filename) {
    struct skynet_service * ctx = skynet_malloc(sizeof(*ctx));
    ctx->name = skynet_strdup("logger");
    ctx->create = logger_create;
    ctx->release = logger_release;
    ctx->cb = logger_callback;

    skynet_service_insert(ctx, harbor, filename, 0);
}

void skynet_print(struct skynet_service * context, int level, const char * msg, ...) {
    va_list ap;
    va_start(ap, msg);
    char tmp[LOG_MESSAGE_SIZE] = {0};
    int len = vsnprintf(tmp, LOG_MESSAGE_SIZE-1, msg, ap);
    va_end(ap);

    uint32_t source = 0;
    if (context != NULL) {
        source = skynet_service_handle(context);
    }
    skynet_send(instance->ctx, source, level, (void *)tmp, len);
}
