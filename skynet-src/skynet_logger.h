#ifndef SKYNET_LOGGER_H
#define SKYNET_LOGGER_H

void skynet_logger_start(struct skynet_service * ctx);
void skynet_logger_exit();
void skynet_logger_print(uint32_t source, int level, const char * msg, ...);

#endif 
