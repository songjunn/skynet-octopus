#ifndef SKYNET_SERVICE_H
#define SKYNET_SERVICE_H

#include <string.h>
#include <stdint.h>

struct skynet_message;
struct skynet_service;

void skynet_service_init(const char * path);
uint32_t skynet_service_create(const char * name, int harbor, const char * module, const char * args, int concurrent);
void skynet_service_close(uint32_t handle);
void skynet_service_closename(const char * name);
void skynet_service_release(struct skynet_service * ctx);
void skynet_service_releaseall();

struct skynet_service * skynet_service_findname(const char * name);
struct skynet_service * skynet_service_find(uint32_t handle);
uint32_t skynet_service_handle(struct skynet_service * ctx);

int skynet_service_pushmsg(uint32_t handle, struct skynet_message * message);
int skynet_service_sendmsg(struct skynet_service * ctx, struct skynet_message * message);

#endif
