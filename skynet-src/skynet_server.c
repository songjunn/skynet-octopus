#include "skynet.h"
#include "skynet_server.h"
#include "skynet_service.h"
#include "skynet_mq.h"
#include "skynet_malloc.h"
#include "skynet_harbor.h"

int skynet_message_dispatch() {
    struct message_queue * q = skynet_globalmq_pop();
    if (q == NULL) {
        return 1;
    }

    uint32_t handle = skynet_mq_handle(q);
    struct skynet_service * ctx = skynet_service_find(handle);
    if (ctx == NULL || ctx->handle == 0) {
        return 0;
    }
    assert(q == ctx->queue);

    if (ctx->closing) {
        while (!skynet_service_message_dispatch(ctx));
        skynet_service_release(ctx);
    } else {
        int ret = skynet_service_message_dispatch(ctx);
        if (ret == 0 && skynet_mq_concurrent(q) == 0) {
            skynet_globalmq_push(ctx->queue);
        }
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
        skynet_logger_warn(ctx->handle, "[skynet]May overload, message queue length = %d", overload);
    }

    if (ctx->cb != NULL) {
        ctx->cb(ctx, msg.source, msg.session, msg.type, msg.data, msg.size);
    }

    skynet_malloc_remove(msg.data);
    skynet_free(msg.data);
    return 0;
}

void skynet_send(struct skynet_service * context, uint32_t source, uint32_t session, int type, void * data, size_t size) {
    struct skynet_message smsg;
    smsg.source = source;
    smsg.type = type;
    smsg.size = size;
    smsg.session = session;
    smsg.data = NULL;
    if (data != NULL) {
        smsg.data = skynet_malloc(size);
        skynet_malloc_insert(smsg.data, size, __FILE__, __LINE__);
        memcpy(smsg.data, data, size);
    }
    if (skynet_service_sendmsg(context, &smsg)) {
        skynet_malloc_remove(smsg.data);
        skynet_free(smsg.data);
    }
}

void skynet_sendname(const char * name, uint32_t source, uint32_t session, int type, void * data, size_t size) {
    struct skynet_service * context = skynet_service_findname(name);
    if (context != NULL) {
        skynet_send(context, source, session, type, data, size);
    } else {
        skynet_harbor_sendname(name, source, session, type, data, size);
    }
}

void skynet_sendhandle(uint32_t target, uint32_t source, uint32_t session, int type, void * data, size_t size) {
    if (skynet_harbor_isremote(target)) {
        skynet_harbor_sendhandle(target, source, session, type, data, size);
    } else {
        struct skynet_service * context = skynet_service_find(target);
        if (context != NULL) {
            skynet_send(context, source, session, type, data, size);
        }
    }
}

char * skynet_strdup(const char * str) {
    size_t sz = strlen(str);
    char * ret = skynet_malloc(sz+1);
    skynet_malloc_insert(ret, sz+1, __FILE__, __LINE__);
    memcpy(ret, str, sz+1);
    return ret;
}
