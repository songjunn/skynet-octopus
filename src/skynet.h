#ifndef SKYNET_H
#define SKYNET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define SERVICE_SOCKET 1
#define SERVICE_TIMER 2
#define SERVICE_REMOTE 3

#define LOGGER_DEBUG 0
#define LOGGER_WARN 1
#define LOGGER_NOTICE 2
#define LOGGER_ERROR 3

#define SKYNET_SOCKET_TYPE_DATA 1
#define SKYNET_SOCKET_TYPE_CONNECT 2
#define SKYNET_SOCKET_TYPE_CLOSE 3
#define SKYNET_SOCKET_TYPE_ACCEPT 4
#define SKYNET_SOCKET_TYPE_ERROR 5
#define SKYNET_SOCKET_TYPE_UDP 6
#define SKYNET_SOCKET_TYPE_WARNING 7

#define skynet_malloc malloc
#define skynet_calloc calloc
#define skynet_realloc realloc
#define skynet_free free

#define skynet_logger_debug(ctx, msg, ...) skynet_logger_print(ctx, LOGGER_DEBUG, msg, ##__VA_ARGS__)
#define skynet_logger_warn(ctx, msg, ...) skynet_logger_print(ctx, LOGGER_WARN, msg, ##__VA_ARGS__)
#define skynet_logger_notice(ctx, msg, ...) skynet_logger_print(ctx, LOGGER_NOTICE, msg, ##__VA_ARGS__)
#define skynet_logger_error(ctx, msg, ...) skynet_logger_print(ctx, LOGGER_ERROR, msg, ##__VA_ARGS__)

struct skynet_service;

typedef bool (*service_dl_create)(struct skynet_service * ctx, int harbor, const char * parm);
typedef void (*service_dl_release)(struct skynet_service * ctx);
typedef bool (*service_dl_callback)(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz);

struct skynet_service {
	char * name;
	void * module;
	void * hook;
	uint32_t handle;
	struct message_queue * queue;

	service_dl_create create;
	service_dl_release release;
	service_dl_callback cb;
};

struct skynet_remote_message {
	char name[32];
	uint32_t handle;
	int type;
	size_t size;
	void * data;
};

struct skynet_socket_message {
	int type;
	int id;
	int ud;
	char * buffer;
};

// logger
extern void skynet_logger_print(struct skynet_service * ctx, int level, const char * msg, ...);

// service
extern void skynet_send(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz);
extern void skynet_sendname(const char * name, uint32_t source, uint32_t session, int type, void * msg, size_t sz);
extern void skynet_sendhandle(uint32_t target, uint32_t source, uint32_t session, int type, void * msg, size_t sz);

// harbor
extern void skynet_harbor_start(struct skynet_service * ctx);
extern void skynet_harbor_exit();
extern int skynet_harbor_id(uint32_t handle);

// tcp socket
extern void skynet_socket_start(struct skynet_service * ctx, int id);
extern void skynet_socket_close(struct skynet_service * ctx, int id);
extern int skynet_socket_connect(struct skynet_service * ctx, const char * host, int port);
extern int skynet_socket_listen(struct skynet_service * ctx, const char * host, int port, int backlog);
extern int skynet_socket_send(struct skynet_service * ctx, int id, void * buffer, int sz);

// timer
extern uint64_t skynet_timer_now(void);
extern void skynet_timer_register(uint32_t handle, const void * args, size_t size, int time);

extern int skynet_config_int(const char * section, const char * option, int * value);
extern int skynet_config_string(const char * section, const char * option, char *value, int size);

// utils
extern char * skynet_strdup(const char * str);

#endif
