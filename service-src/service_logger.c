#include "skynet.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define LOG_FILE_SIZE 20480000
#define LOG_MESSAGE_SIZE 20480

struct skynet_logger {
    int size;
    int level;
    int close;
    FILE * handle;
    char basename[64];
    char filename[128];
    struct skynet_service * ctx;
};

void _format_name(char * filename, size_t size, const char * basename) {
    struct tm * newtime;
    time_t aclock;
    time(&aclock);
    newtime = localtime(&aclock);
    snprintf(filename, size, "%s-%02d%02d%02d-%02d%02d%02d.log", basename, newtime->tm_year+1900, 
        newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
}

void _format_time(char * buffer, size_t size) {
    struct tm * newtime;
    time_t aclock;
    time(&aclock);
    newtime = localtime(&aclock);
    snprintf(buffer, size, "[%02d:%02d:%02d %02d:%02d:%02d] ", newtime->tm_year+1900, 
        newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
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
    struct skynet_logger * l = skynet_malloc(sizeof(struct skynet_logger));
    l->size = 0;
    l->close = 0;
    l->level = 0;
    l->handle = NULL;
    ctx->hook = l;

    sscanf(args, "%[^','],%d", l->basename, &l->level);

    if (strlen(l->basename) > 0) {
        l->handle = _open_file(l->filename, sizeof(l->filename), l->basename);
        if (l->handle == NULL) {
            return false;
        }
        l->close = 1;
    }
    skynet_logger_start(ctx);
    return true;
}

void logger_release(struct skynet_service * ctx) {
    struct skynet_logger * l = ctx->hook;
    if (l->close) {
        fclose(l->handle);
    }
    skynet_logger_exit();
    skynet_free(l);
}

bool logger_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int level, void * msg, size_t sz) {
    struct skynet_logger * l = ctx->hook;
    if (level < l->level) {
        return false;
    }

    if (sz >= LOG_MESSAGE_SIZE) {
        sz = LOG_MESSAGE_SIZE - 1;
    }

    char head[64] = {0}, time[64] = {0}, content[LOG_MESSAGE_SIZE] = {0};
    _format_time(time, sizeof(time));
    _format_head(head, sizeof(head), level, source);
    snprintf(content, sz+1, "%s", (const char*)msg);

    if (l->size >= LOG_FILE_SIZE) {
        l->close = 0;
        fclose(l->handle);

        l->handle = _open_file(l->filename, sizeof(l->filename), l->basename);
        if (l->handle) {
            l->size = 0;
            l->close = 1;
        }
    }

    if (l->handle) {
        fprintf(l->handle, time);
        fprintf(l->handle, head);
        fprintf(l->handle, content);
        fprintf(l->handle, "\n");
        fflush(l->handle);
        l->size += strlen(content);
    }

    return true;
}
