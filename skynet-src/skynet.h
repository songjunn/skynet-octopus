#ifndef SKYNET_H
#define SKYNET_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#define SERVICE_TEXT 0
#define SERVICE_SOCKET 1
#define SERVICE_TIMER 2
#define SERVICE_REMOTE 3
#define SERVICE_RESPONSE 4

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

#define skynet_logger_debug(source, msg, ...) skynet_logger_print(source, LOGGER_DEBUG, msg, ##__VA_ARGS__)
#define skynet_logger_warn(source, msg, ...) skynet_logger_print(source, LOGGER_WARN, msg, ##__VA_ARGS__)
#define skynet_logger_notice(source, msg, ...) skynet_logger_print(source, LOGGER_NOTICE, msg, ##__VA_ARGS__)
#define skynet_logger_error(source, msg, ...) skynet_logger_print(source, LOGGER_ERROR, msg, ##__VA_ARGS__)

struct skynet_service;
struct skynet_remote_message;

typedef int (*service_dl_create)(struct skynet_service * ctx, int harbor, const char * parm);
typedef void (*service_dl_release)(struct skynet_service * ctx);
typedef int (*service_dl_callback)(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz);

struct skynet_service {
	int closing;
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
	uint32_t source;
	uint32_t session;
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
extern void skynet_logger_start(struct skynet_service * ctx);
extern void skynet_logger_exit(void);
extern void skynet_logger_print(uint32_t source, int level, const char * msg, ...);

// service
extern void skynet_send(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz);
extern void skynet_sendname(const char * name, uint32_t source, uint32_t session, int type, void * msg, size_t sz);
extern void skynet_sendhandle(uint32_t target, uint32_t source, uint32_t session, int type, void * msg, size_t sz);

// harbor
extern int skynet_harbor_local_id();
extern int skynet_harbor_id(uint32_t handle);
extern void skynet_harbor_start(struct skynet_service * ctx);
extern void skynet_harbor_exit(void);
extern size_t skynet_remote_message_header(void);
extern size_t skynet_remote_message_push(struct skynet_remote_message * rmsg, void * data, size_t size);
extern void skynet_local_message_forward(void * data, size_t size);

// tcp socket
extern void skynet_socket_start(struct skynet_service * ctx, int id);
extern void skynet_socket_close(struct skynet_service * ctx, int id);
extern int skynet_socket_connect(struct skynet_service * ctx, const char * host, int port);
extern int skynet_socket_listen(struct skynet_service * ctx, const char * host, int port, int backlog);
extern int skynet_socket_send(struct skynet_service * ctx, int id, void * buffer, int sz);

// timer
extern uint64_t skynet_timer_now(void);
extern void skynet_timer_register(uint32_t handle, uint32_t session, const void * args, size_t size, int time);

// config
extern int skynet_config_int(const char * section, const char * option, int * value);
extern int skynet_config_string(const char * section, const char * option, char *value, int size);

// utils
extern char * skynet_strdup(const char * str);

#endif
