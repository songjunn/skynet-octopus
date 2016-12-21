#ifndef SKYNET_H
#define SKYNET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define PTYPE_TEXT 0
#define PTYPE_RESPONSE 1
#define PTYPE_MULTICAST 2
#define PTYPE_CLIENT 3
#define PTYPE_SYSTEM 4
#define PTYPE_HARBOR 5
#define PTYPE_SOCKET 6
#define PTYPE_TIMER 7

#define LOGGER_DEBUG 0
#define LOGGER_WARN 1
#define LOGGER_NOTICE 2
#define LOGGER_ERROR 3

#define skynet_malloc malloc
#define skynet_calloc calloc
#define skynet_realloc realloc
#define skynet_free free

#define skynet_logger_debug(ctx, msg, ...) skynet_print(ctx, LOGGER_DEBUG, msg, ##__VA_ARGS__)
#define skynet_logger_warn(ctx, msg, ...) skynet_print(ctx, LOGGER_WARN, msg, ##__VA_ARGS__)
#define skynet_logger_notice(ctx, msg, ...) skynet_print(ctx, LOGGER_NOTICE, msg, ##__VA_ARGS__)
#define skynet_logger_error(ctx, msg, ...) skynet_print(ctx, LOGGER_ERROR, msg, ##__VA_ARGS__)

struct skynet_service;

typedef bool (*service_dl_create)(struct skynet_service * ctx, const char * parm);
typedef void (*service_dl_release)(void);
typedef bool (*service_dl_callback)(int type, uint32_t source , void * msg, size_t sz);

struct skynet_service {
	int ref;
	char * name;
	void * module;
	uint32_t handle;
	struct message_queue * queue;

	service_dl_create create;
	service_dl_release release;
	service_dl_callback cb;
};

// logger
void skynet_print(struct skynet_service * context, int level, const char * msg, ...);

// service
void skynet_push(uint32_t target, uint32_t source, int type, void * msg, size_t sz);
void skynet_send(struct skynet_service * context, uint32_t source, int type, void * msg, size_t sz);
void skynet_trans(const char * name, uint32_t source, int type, void * msg, size_t sz);

// timer
uint64_t skynet_now(void);
void skynet_register_timer(uint32_t handle, const void * args, size_t size, int time);

#endif
