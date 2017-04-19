#ifndef SKYNET_SERVICE_H
#define SKYNET_SERVICE_H

#include <string.h>
#include <stdint.h>

struct skynet_message;
struct skynet_service;

void skynet_service_init(const char * path);
struct skynet_service * skynet_service_create(const char * name, int harbor, const char *param, int concurrent);
struct skynet_service * skynet_service_insert(struct skynet_service * ctx, int harbor, const char * param, int concurrent);
struct skynet_service * skynet_service_query(const char * name);
struct skynet_service * skynet_service_grab(uint32_t handle);
void skynet_service_release(struct skynet_service * ctx);
uint32_t skynet_service_handle(struct skynet_service * ctx);

void skynet_service_send(struct skynet_service * ctx, void * msg, size_t sz, uint32_t source, int type);
void skynet_service_sendmsg(struct skynet_service * ctx, struct skynet_message * message);
int skynet_service_pushmsg(uint32_t handle, struct skynet_message * message);

#endif
