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

#define LOG_MESSAGE_SIZE 10240

struct skynet_logger {
	FILE * handle;
	char * filename;
	int close;
	int level;
	struct skynet_service * ctx;
};

static struct skynet_logger * instance = NULL;

bool logger_create(struct skynet_service * ctx, const char * args) {
	int level = 0;
	char filename[128] = {0};
	sscanf(args, "%[^','],%d", filename, &level);

	instance = skynet_malloc(sizeof(*instance));
	instance->ctx = ctx;
	instance->close = 0;
	instance->level = level;
	instance->handle = NULL;

	if (strlen(filename) > 0) {
		instance->handle = fopen(filename, "wb");
		if (instance->handle == NULL) {
			return false;
		}
		instance->close = 1;
		instance->filename = skynet_strdup(filename);
	}
	return true;
}

void logger_release() {
	if (instance->close) {
		fclose(instance->handle);
	}
	skynet_free(instance->filename);
	skynet_free(instance);
}

void _formatTime(char * buffer, size_t size) {
	struct tm * newtime;
	time_t aclock;
	time(&aclock);
	newtime = localtime(&aclock);
	snprintf(buffer, size, "[%02d:%02d:%02d %02d:%02d:%02d] ", newtime->tm_year+1900, 
			newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour,
			newtime->tm_min, newtime->tm_sec);
}

void _formatHead(char * buffer, size_t size, int level, uint32_t source) {
	switch (level) {
	case LOGGER_DEBUG: snprintf(buffer, size, "DEBUG [:%04x] ", source); break;
	case LOGGER_WARN: snprintf(buffer, size, "WARN [:%04x] ", source); break;
	case LOGGER_NOTICE: snprintf(buffer, size, "NOTICE [:%04x] ", source); break;
	case LOGGER_ERROR: snprintf(buffer, size, "ERROR [:%04x] ", source); break;
	default: break;
	}
}

bool logger_callback(int level, uint32_t source, void * msg, size_t sz) {
	if (level < instance->level) {
		return false;
	}

	char head[64] = {0}, time[64] = {0}, content[LOG_MESSAGE_SIZE] = {0};
	_formatTime(time, sizeof(time));
	_formatHead(head, sizeof(head), level, source);
	snprintf(content, sz+1, "%s", (const char*)msg);

	if (instance->handle) {
		fprintf(instance->handle, time);
		fprintf(instance->handle, head);
		fprintf(instance->handle, content);
		fprintf(instance->handle, "\n");
		fflush(instance->handle);
	}

	fprintf(stdout, time);
	fprintf(stdout, head);
	fprintf(stdout, content);
	fprintf(stdout, "\n");

	return true;
}

void skynet_logger_init(const char * filename) {
	struct skynet_service * ctx = skynet_malloc(sizeof(*ctx));
	ctx->name = skynet_strdup("logger");
	ctx->create = logger_create;
	ctx->release = logger_release;
	ctx->cb = logger_callback;

	skynet_service_insert(ctx, filename, 0);
}

void skynet_print(struct skynet_service * context, int level, const char * msg, ...) {
	char tmp[LOG_MESSAGE_SIZE];
	va_list ap;
	va_start(ap, msg);
	int len = vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
	va_end(ap);

	uint32_t source = 0;
	if (context != NULL) {
		source = skynet_service_handle(context);
	}
	skynet_send(instance->ctx, source, level, (void *)tmp, len);
}
