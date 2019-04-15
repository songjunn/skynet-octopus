#include "skynet.h"
#include "hashid.h"
#include "databuffer.h"

#define BACKLOG 32
#define BUFFER_MAX 20480
#define MESSAGE_BUFFER_MAX 20496

struct connection {
    int fd;
    char remote_name[32];
    struct databuffer * buffer;
};

struct gate {
    int listen_fd;
    int listen_port;
    int connect_max;
    char forward[64];
    struct hashid hash;
    struct connection * conn;
};

void forward_message(struct skynet_service * ctx, struct connection * conn) {
    struct gate * g = ctx->hook;
    int sz = databuffer_readheader(conn->buffer);
    if (sz > 0) {
        char data[BUFFER_MAX];
        sz = databuffer_read(conn->buffer, data, sz);
        if (sz > 0) {
            char msg[MESSAGE_BUFFER_MAX];
            sprintf(msg, "forward,%d", sz);
            memcpy(msg+strlen(msg), data, sz);

            skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, strlen(msg));
        }
    }
}

void forward_accept(struct skynet_service * ctx, struct connection * conn) {
    struct gate * g = ctx->hook;
    char msg[64];
    sprintf(msg, "connect,%s", conn->remote_name);
    skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, strlen(msg));
}

void forward_close(struct skynet_service * ctx, struct connection * conn) {
    struct gate * g = ctx->hook;
    char msg[16];
    sprintf(msg, "disconnect");
    skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, strlen(msg));
}

int gate_create(struct skynet_service * ctx, int harbor, const char * args) {
    int i;
    struct gate * g = skynet_malloc(sizeof(struct gate));
    sscanf(args, "%[^','],%d,%d", g->forward, &g->listen_port, &g->connect_max);

    g->listen_fd = skynet_socket_listen(ctx, "0.0.0.0", g->listen_port, BACKLOG);
    if (g->listen_fd < 0) {
        return 1;
    }

    hashid_init(&g->hash, g->connect_max);
    g->conn = skynet_malloc(g->connect_max * sizeof(struct connection));
    memset(g->conn, 0, g->connect_max * sizeof(struct connection));
    for (i=0; i<g->connect_max; i++) {
        g->conn[i].fd = -1;
    }
    ctx->hook = g;

    skynet_logger_notice(ctx, "[gate]listen port:%d fd %d", g->listen_port, g->listen_fd);
    skynet_socket_start(ctx, g->listen_fd);
    return 0;
}

void gate_release(struct skynet_service * ctx) {
    int i;
    struct gate * g = ctx->hook;
    for (i=0; i<g->connect_max; i++) {
        struct connection * c = &g->conn[i];
        if (c->fd >= 0) {
            databuffer_free(c->buffer);
            skynet_socket_close(ctx, c->fd);
        }
    }
    if (g->listen_fd >= 0) {
        skynet_socket_close(ctx, g->listen_fd);
    }
    hashid_clear(&g->hash);
    skynet_free(g->conn);
    skynet_free(g);
}

void dispatch_socket_message(struct skynet_service * ctx, const struct skynet_socket_message * message, size_t sz) {
    struct gate * g = ctx->hook;
    switch(message->type) {
        case SKYNET_SOCKET_TYPE_DATA: {
            skynet_logger_debug(ctx, "[gate]recv data fd %d size:%d", message->id, message->ud);
            int id = hashid_lookup(&g->hash, message->id);
            if (id >= 0) {
                struct connection *c = &g->conn[id];
                if (databuffer_push(c->buffer, message->buffer, message->ud) <= 0) {
                    skynet_logger_error(ctx, "[gate]connection recv data too long fd=%d size=%d", message->id, message->ud);
                    skynet_socket_close(ctx, message->id);
                    skynet_free(message->buffer);
                } else {
                    forward_message(ctx, c);
                }
            } else {
                skynet_logger_error(ctx, "[gate]recv unknown connection data fd=%d size=%d", message->id, message->ud);
                skynet_socket_close(ctx, message->id);
                skynet_free(message->buffer);
            }
            break;
        }
        case SKYNET_SOCKET_TYPE_ACCEPT: {
            assert(g->listen_fd == message->id);
            if (hashid_full(&g->hash)) {
                skynet_logger_error(ctx, "[gate]full on accepting, alloc:%d, accepted:%d, close socket:%d", g->connect_max, g->hash.count, message->ud);
                skynet_socket_close(ctx, message->ud);
            } else {
                int id = hashid_insert(&g->hash, message->ud);
                const char * remote_name = (const char *) (message + 1);

                struct connection *c = &g->conn[id];
                c->fd = message->ud;
                c->buffer = databuffer_create(BUFFER_MAX);
                snprintf(c->remote_name, sizeof(c->remote_name)-1, "%s", remote_name);

                forward_accept(ctx, c);
                skynet_socket_start(ctx, c->fd);
                skynet_logger_debug(ctx, "[gate]accept fd=%d addr=%s", c->fd, c->remote_name);
            }
            break;
        }
        case SKYNET_SOCKET_TYPE_CLOSE:
        case SKYNET_SOCKET_TYPE_ERROR: {
            skynet_logger_debug(ctx, "[gate]close or error %d fd %d", message->type, message->id);
            int id = hashid_remove(&g->hash, message->id);
            if (id >= 0) {
                struct connection *c = &g->conn[id];
                forward_close(ctx, c);
                c->fd = -1;
                databuffer_free(c->buffer);
            }
            break;
        }
        case SKYNET_SOCKET_TYPE_CONNECT: {
            if (message->id == g->listen_fd) {
                break; // start listening
            }
            int id = hashid_lookup(&g->hash, message->id);
            if (id <= 0) {
                skynet_logger_error(ctx, "[gate]connected unknown connection %d message", message->id);
                skynet_socket_close(ctx, message->id);
            }
            break;
        }
        case SKYNET_SOCKET_TYPE_WARNING:
            skynet_logger_warn(ctx, "[gate]fd (%d) send buffer (%d)K", message->id, message->ud);
            break;
        default:
            skynet_logger_error(ctx, "[gate]recv error message type %d", message->type);
            break;
    }
}

int gate_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {
    switch(type) {
        case SERVICE_SOCKET:
            dispatch_socket_message(ctx, (const struct skynet_socket_message *)msg, (int)(sz-sizeof(struct skynet_socket_message)));
            break;
        default:
            break;
    }
    return 0;
}
