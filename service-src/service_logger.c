#include "skynet.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define LOG_FILE_SIZE 20480000

struct logger {
    int size;
    int level;
    int close;
    FILE * handle;
    char basename[64];
    char filename[128];
};

void logger_format_name(char * filename, size_t size, const char * basename) {
    struct tm * newtime;
    time_t aclock;
    time(&aclock);
    newtime = localtime(&aclock);
    snprintf(filename, size, "%s-%02d%02d%02d-%02d%02d%02d.log", basename, newtime->tm_year+1900, 
        newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
}

void logger_format_time(char * buffer, size_t size) {
    struct tm * newtime;
    time_t aclock;
    time(&aclock);
    newtime = localtime(&aclock);
    snprintf(buffer, size, "[%02d:%02d:%02d %02d:%02d:%02d] ", newtime->tm_year+1900, 
        newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
}

void logger_format_head(char * buffer, size_t size, int level, uint32_t source) {
    switch (level) {
    case LOGGER_DEBUG: snprintf(buffer, size, "DEBUG [:%04x] ", source); break;
    case LOGGER_WARN: snprintf(buffer, size, "WARN [:%04x] ", source); break;
    case LOGGER_NOTICE: snprintf(buffer, size, "NOTICE [:%04x] ", source); break;
    case LOGGER_ERROR: snprintf(buffer, size, "ERROR [:%04x] ", source); break;
    default: break;
    }
}

FILE * logger_open_file(char * filename, size_t size, const char * basename) {
    logger_format_name(filename, size, basename);
    FILE * handle = fopen(filename, "wb");
    return handle;
}

int logger_create(struct skynet_service * ctx, int harbor, const char * args) {
    struct logger * l = skynet_malloc(sizeof(struct logger));
    l->size = 0;
    l->close = 0;
    l->level = 0;
    l->handle = NULL;
    ctx->hook = l;

    sscanf(args, "%[^','],%d", l->basename, &l->level);

    if (strlen(l->basename) > 0) {
        l->handle = logger_open_file(l->filename, sizeof(l->filename), l->basename);
        if (l->handle == NULL) {
            return 1;
        }
        l->close = 1;
    }
    skynet_logger_start(ctx);
    return 0;
}

void logger_release(struct skynet_service * ctx) {
    struct logger * l = ctx->hook;
    if (l->close) {
        fclose(l->handle);
    }
    skynet_logger_exit();
    skynet_free(l);
}

int logger_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int level, void * msg, size_t sz) {
    struct logger * l = ctx->hook;
    if (level < l->level) {
        return 1;
    }

    char head[64] = {0}, time[64] = {0};
    logger_format_time(time, sizeof(time));
    logger_format_head(head, sizeof(head), level, source);

    if (l->size >= LOG_FILE_SIZE) {
        l->close = 0;
        fclose(l->handle);

        l->handle = logger_open_file(l->filename, sizeof(l->filename), l->basename);
        if (l->handle) {
            l->size = 0;
            l->close = 1;
        }
    }

    if (l->handle) {
        fprintf(l->handle, time);
        fprintf(l->handle, head);
        fprintf(l->handle, "%.*s", sz, msg);
        fprintf(l->handle, "\n");
        fflush(l->handle);
        l->size += sz;
    }

    return 0;
}
