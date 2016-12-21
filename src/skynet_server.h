#ifndef SKYNET_SERVER_H
#define SKYNET_SERVER_H

#include <stdint.h>
#include <stdlib.h>

struct skynet_message;

int skynet_service_message_dispatch();

char * skynet_strdup(const char * str);

#endif
