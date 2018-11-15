#ifndef SKYNET_LOGGER_H
#define SKYNET_LOGGER_H

void skynet_logger_init(int harbor, const char * filename);
void skynet_logger_print(struct skynet_service * context, int level, const char * msg, ...);

#endif 
