#ifndef SKYNET_HARBOR_H
#define SKYNET_HARBOR_H

#include <stdint.h>
#include <stdlib.h>

struct skynet_service;

void skynet_harbor_init(int harbor);
void skynet_harbor_start(struct skynet_service * ctx);
void skynet_harbor_exit();

void skynet_harbor_sendname(const char * name, uint32_t source, uint32_t session, int type, void * data, size_t size);
void skynet_harbor_sendhandle(uint32_t target, uint32_t source, uint32_t session, int type, void * data, size_t size);

int skynet_harbor_id(uint32_t handle);
int skynet_harbor_index(uint32_t handle);
int skynet_harbor_isremote(uint32_t handle);
uint32_t skynet_harbor_handle(int harbor, int index);

#endif
