#ifndef SKYNET_SERVER_H
#define SKYNET_SERVER_H

#include <stdint.h>
#include <stdlib.h>

struct skynet_service;
struct skynet_message;

int skynet_message_dispatch();
int skynet_service_message_dispatch(struct skynet_service * ctx);

#endif
