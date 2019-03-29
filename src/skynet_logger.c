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

#define LOG_FILE_SIZE 20480000
#define LOG_MESSAGE_SIZE 204800

struct skynet_logger {
    int size;
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

FILE * _open_file(char * filename, size_t size, const char * basename) {
    _format_name(filename, size, basename);
    FILE * handle = fopen(filename, "wb");
    return handle;
}

bool logger_create(struct skynet_service * ctx, int harbor, const char * args) {
    instance = skynet_malloc(sizeof(*instance));
    instance->ctx = ctx;
    instance->size = 0;
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

void logger_release(struct skynet_service * ctx) {
    if (instance->close) {
        fclose(instance->handle);
    }
    skynet_free(instance);
}

bool logger_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int level, void * msg, size_t sz) {
    if (level < instance->level) {
        return false;
    }

    char head[64] = {0}, time[64] = {0}, content[LOG_MESSAGE_SIZE] = {0};
    _format_time(time, sizeof(time));
    _format_head(head, sizeof(head), level, source);
    snprintf(content, sz, "%s", (const char*)msg);

    if (instance->size >= LOG_FILE_SIZE) {
        instance->close = 0;
        fclose(instance->handle);

        instance->handle = _open_file(instance->filename, sizeof(instance->filename), instance->basename);
        if (instance->handle) {
            instance->size = 0;
            instance->close = 1;
        }
    }

    if (instance->handle) {
        fprintf(instance->handle, time);
        fprintf(instance->handle, head);
        fprintf(instance->handle, content);
        fprintf(instance->handle, "\n");
        fflush(instance->handle);
        instance->size += strlen(content);
    }

    /*fprintf(stdout, time);
    fprintf(stdout, head);
    fprintf(stdout, content);
    fprintf(stdout, "\n");*/

    return true;
}

void skynet_logger_init(int harbor, const char * filename) {
    struct skynet_service * ctx = skynet_malloc(sizeof(*ctx));
    ctx->name = skynet_strdup("logger");
    ctx->create = logger_create;
    ctx->release = logger_release;
    ctx->cb = logger_callback;

    skynet_service_insert(ctx, harbor, filename, 0);
    skynet_free(ctx->name);
    skynet_free(ctx);
}

void skynet_logger_print(struct skynet_service * context, int level, const char * msg, ...) {
    va_list ap;
    va_start(ap, msg);
    char tmp[LOG_MESSAGE_SIZE] = {0};
    int len = vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
    va_end(ap);

    uint32_t source = 0;
    if (context != NULL) {
        source = skynet_service_handle(context);
    }

    if (len < 0) {
        char err[32] = {};
        sprintf(err, "vsnprintf error len=%d", len);
        skynet_send(instance->ctx, source, 0, LOGGER_ERROR, (void *)err, strlen(err));
        return;
    } else if (len > LOG_MESSAGE_SIZE) {
        char err[32] = {};
        sprintf(err, "vsnprintf error len=%d", len);
        skynet_send(instance->ctx, source, 0, LOGGER_ERROR, (void *)err, strlen(err));
        len = LOG_MESSAGE_SIZE;
    }

    fprintf(stdout, tmp);
    fprintf(stdout, "\n");
    skynet_send(instance->ctx, source, 0, level, (void *)tmp, len);
}
