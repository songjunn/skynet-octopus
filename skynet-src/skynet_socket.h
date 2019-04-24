#ifndef skynet_socket_h
#define skynet_socket_h

#include "socket_info.h"

struct skynet_service;
struct skynet_socket_message;

void skynet_socket_init();
void skynet_socket_exit();
void skynet_socket_free();
int skynet_socket_poll();
void skynet_socket_updatetime();

int skynet_socket_send(struct skynet_service *ctx, int id, void *buffer, int sz);
int skynet_socket_send_lowpriority(struct skynet_service *ctx, int id, void *buffer, int sz);
int skynet_socket_listen(struct skynet_service *ctx, const char *host, int port, int backlog);
int skynet_socket_connect(struct skynet_service *ctx, const char *host, int port);
int skynet_socket_bind(struct skynet_service *ctx, int fd);
void skynet_socket_close(struct skynet_service *ctx, int id);
void skynet_socket_shutdown(struct skynet_service *ctx, int id);
void skynet_socket_start(struct skynet_service *ctx, int id);
void skynet_socket_nodelay(struct skynet_service *ctx, int id);

int skynet_socket_udp(struct skynet_service *ctx, const char * addr, int port);
int skynet_socket_udp_connect(struct skynet_service *ctx, int id, const char * addr, int port);
int skynet_socket_udp_send(struct skynet_service *ctx, int id, const char * address, const void *buffer, int sz);
const char * skynet_socket_udp_address(struct skynet_socket_message *, int *addrsz);

struct socket_info * skynet_socket_info();

#endif
