#include "skynet.h"
#include "skynet_logger.h"

#include <stdarg.h>
#include <stdio.h>

#define LOG_MESSAGE_SIZE 204800

static struct skynet_service * LOGGER = 0;

void skynet_logger_start(struct skynet_service * ctx) {
    LOGGER = ctx;
}

void skynet_logger_exit() {
    LOGGER = NULL;
}

void skynet_logger_print(struct skynet_service * ctx, int level, const char * msg, ...) {
    uint32_t source;
    va_list ap;
    va_start(ap, msg);
    char tmp[LOG_MESSAGE_SIZE] = {0};
    int len = vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
    va_end(ap);

    fprintf(stdout, tmp);
    fprintf(stdout, "\n");

    if (LOGGER) {
        if (ctx != NULL) {
            source = skynet_service_handle(ctx);
        } else {
            source = 0;
        }
        if (len >= LOG_MESSAGE_SIZE) {
            len = LOG_MESSAGE_SIZE - 1;
        }
        skynet_send(LOGGER, source, 0, level, (void *)tmp, len);
    }
}
