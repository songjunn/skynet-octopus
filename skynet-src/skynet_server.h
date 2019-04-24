#ifndef SKYNET_SERVER_H
#define SKYNET_SERVER_H

#include <stdint.h>
#include <stdlib.h>

struct skynet_service;
struct skynet_message;

int skynet_message_dispatch();
int skynet_service_message_dispatch(struct skynet_service * ctx);

void skynet_send(struct skynet_service * context, uint32_t source, uint32_t session, int type, void * data, size_t size);
void skynet_sendname(const char * name, uint32_t source, uint32_t session, int type, void * data, size_t size);
void skynet_sendhandle(uint32_t target, uint32_t source, uint32_t session, int type, void * data, size_t size);

char * skynet_strdup(const char *str);

#endif
