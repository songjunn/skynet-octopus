#include "skynet.h"
#include "skynet_server.h"
#include "skynet_service.h"
#include "skynet_mq.h"

int skynet_message_dispatch() {
    struct message_queue * q = skynet_globalmq_pop();
    if (q == NULL) {
        return 1;
    }

    uint32_t handle = skynet_mq_handle(q);
    struct skynet_service * ctx = skynet_service_grab(handle);
    if (ctx == NULL) {
        skynet_mq_release(q);
        return 0;
    }

    assert(q == ctx->queue);
    int ret = skynet_service_message_dispatch(ctx);
    if (ret == 0) {
        skynet_globalmq_push(ctx->queue);
    }

    return 0;
}

int skynet_service_message_dispatch(struct skynet_service * ctx) {
    struct skynet_message msg;
    if (skynet_mq_pop(ctx->queue, &msg)) {
        return 1;
    }
    int overload = skynet_mq_overload(ctx->queue);
    if (overload) {
        skynet_logger_error(ctx, "May overload, message queue length = %d", overload);
    }

    if (ctx->cb != NULL) {
        ctx->cb(ctx, msg.source, msg.session, msg.type, msg.data, msg.size);
    }

    if (msg.size > 0 && msg.data != NULL) {
        skynet_free(msg.data);
    }
    return 0;
}

void skynet_send(struct skynet_service * context, uint32_t source, uint32_t session, int type, void * data, size_t size) {
    struct skynet_message smsg;
    smsg.source = source;
    smsg.type = type;
    smsg.size = size;
    smsg.session = session;
    if (data != NULL) {
        smsg.data = skynet_malloc(size);
        memcpy(smsg.data, data, size);
    }
    skynet_service_sendmsg(context, &smsg);
}

void skynet_sendname(const char * name, uint32_t source, uint32_t session, int type, void * data, size_t size) {
    struct skynet_service * context = skynet_service_query(name);
    if (context == NULL) {
        return;
    }
    skynet_send(context, source, session, type, data, size);
}

void skynet_sendhandle(uint32_t target, uint32_t source, uint32_t session, int type, void * data, size_t size) {
    struct skynet_service * context = skynet_service_grab(target);
    if (context == NULL) {
        return;
    }
    skynet_send(context, source, session, type, data, size);
}

char * skynet_strdup(const char *str) {
    size_t sz = strlen(str);
    char * ret = skynet_malloc(sz+1);
    memcpy(ret, str, sz+1);
    return ret;
}
