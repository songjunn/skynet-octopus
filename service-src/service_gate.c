#include "skynet.h"
#include "skynet_socket.h"
#include "hashid.h"
#include "databuffer.h"

#define BACKLOG 32
#define BUFFER_MAX 20480
#define MESSAGE_BUFFER_MAX 20496

struct gate_connection {
    int fd;
    char * remote_name;
    struct databuffer * buffer;
};

struct gate {
    int listen_fd;
    int listen_port;
    int connect_max;
    char forward[64];
    struct hashid hash;
    struct gate_connection * conn;
};

void gate_accept(struct skynet_service * ctx, struct gate_connection * conn) {
    struct gate * g = ctx->hook;
    char msg[64];
    size_t sz = 
    sprintf(msg, "connect|%d|%s", strlen(conn->remote_name), conn->remote_name);
    skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, strlen(msg));
}

void gate_close(struct skynet_service * ctx, struct gate_connection * conn) {
    struct gate * g = ctx->hook;
    char msg[16];
    sprintf(msg, "disconnect|0|");
    skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, strlen(msg));
}

void gate_message(struct skynet_service * ctx, struct gate_connection * conn) {
    int sz;
    char * data;
    struct gate * g = ctx->hook;

    while (sz = databuffer_readpack(conn->buffer, &data)) {
        char msg[MESSAGE_BUFFER_MAX];
        sprintf(msg, "forward|%d|", sz);
        int hsz = strlen(msg);
        memcpy(msg+hsz, data, sz);

        skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, hsz+sz);
        databuffer_freepack(conn->buffer);
    }
}

void gate_dispatch_cmd(struct skynet_service * ctx, int fd, const char * msg, size_t sz) {
    int i;
    struct gate * g = ctx->hook;
    char * command = msg;

    for (i=0;i<sz;i++) {
        if (command[i]=='|') {
            break;
        }
    }
    
    if (memcmp(command, "forward", i) == 0) {
        int size = sz-i-1;
        if (size > 0) {
            char * param = command+i+1;
            char * buffer = (char *)skynet_malloc(size+sizeof(size));
            memcpy(buffer, (char *)&size, sizeof(size));
            memcpy(buffer+sizeof(size), param, size);
            skynet_socket_send(ctx, fd, (void *)buffer, size+sizeof(size));
            skynet_logger_debug(ctx->handle, "[gate]send data fd %d size:%d", fd, size+sizeof(size));
        }
    } else if (memcmp(command, "kick", i) == 0) {
        skynet_socket_close(ctx, fd);
    } else if (memcmp(command, "connect", i) == 0) {
        char * param = command+i+1;
        char * portstr = strchr(param, "|");
        if (portstr == NULL) {
            return;
        }
        int size = sz-(portstr-command)-1;
        if (size > 0) {
            int port = atoi(param);
            char addr[size+1];
            snprintf(addr, size, "%s", portstr+1);
            skynet_socket_connect(ctx, addr, port);
        }
    }
}

void gate_dispatch_socket_message(struct skynet_service * ctx, const struct skynet_socket_message * message, size_t sz) {
    struct gate * g = ctx->hook;
    switch(message->type) {
    case SKYNET_SOCKET_TYPE_DATA: {
        skynet_logger_debug(ctx->handle, "[gate]recv data fd %d size:%d", message->id, message->ud);
        int id = hashid_lookup(&g->hash, message->id);
        if (id >= 0) {
            struct gate_connection *c = &g->conn[id];
            if (databuffer_push(c->buffer, message->buffer, message->ud) <= 0) {
                skynet_logger_error(ctx->handle, "[gate]connection recv data too long fd=%d size=%d", message->id, message->ud);
                skynet_socket_close(ctx, message->id);
                skynet_free(message->buffer);
            } else {
                gate_message(ctx, c);
            }
        } else {
            skynet_logger_error(ctx->handle, "[gate]recv unknown connection data fd=%d size=%d", message->id, message->ud);
            skynet_socket_close(ctx, message->id);
            skynet_free(message->buffer);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_ACCEPT: {
        assert(g->listen_fd == message->id);
        if (hashid_full(&g->hash)) {
            skynet_logger_error(ctx->handle, "[gate]full on accepting, alloc:%d, accepted:%d, close socket:%d", g->connect_max, g->hash.count, message->ud);
            skynet_socket_close(ctx, message->ud);
        } else {
            int id = hashid_insert(&g->hash, message->ud);
            const char * remote_name = (const char *) (message + 1);

            struct gate_connection *c = &g->conn[id];
            c->fd = message->ud;
            c->buffer = databuffer_create(BUFFER_MAX);
            c->remote_name = skynet_malloc(sz+1);
            memcpy(c->remote_name, remote_name, sz);
            c->remote_name[sz] = '\0';

            gate_accept(ctx, c);
            skynet_socket_start(ctx, c->fd);
            skynet_logger_debug(ctx->handle, "[gate]accept fd=%d addr=%s", c->fd, c->remote_name);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_CLOSE:
    case SKYNET_SOCKET_TYPE_ERROR: {
        skynet_logger_debug(ctx->handle, "[gate]close or error %d fd %d", message->type, message->id);
        int id = hashid_remove(&g->hash, message->id);
        if (id >= 0) {
            struct gate_connection *c = &g->conn[id];
            gate_close(ctx, c);
            c->fd = -1;
            skynet_free(c->remote_name);
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
            skynet_logger_error(ctx->handle, "[gate]connected unknown connection %d message", message->id);
            skynet_socket_close(ctx, message->id);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_WARNING:
        skynet_logger_warn(ctx->handle, "[gate]fd (%d) send buffer (%d)K", message->id, message->ud);
        break;
    default:
        skynet_logger_error(ctx->handle, "[gate]recv error message type %d", message->type);
        break;
    }
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
    g->conn = skynet_malloc(g->connect_max * sizeof(struct gate_connection));
    memset(g->conn, 0, g->connect_max * sizeof(struct gate_connection));
    for (i=0; i<g->connect_max; i++) {
        g->conn[i].fd = -1;
    }
    ctx->hook = g;

    skynet_logger_notice(ctx->handle, "[gate]listen port:%d fd:%d", g->listen_port, g->listen_fd);
    skynet_socket_start(ctx, g->listen_fd);
    return 0;
}

void gate_release(struct skynet_service * ctx) {
    int i;
    struct gate * g = ctx->hook;
    for (i=0; i<g->connect_max; i++) {
        struct gate_connection * c = &g->conn[i];
        if (c->fd >= 0) {
            skynet_free(c->remote_name);
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

int gate_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {
    switch(type) {
    case SERVICE_SOCKET:
        gate_dispatch_socket_message(ctx, (const struct skynet_socket_message *)msg, (int)(sz-sizeof(struct skynet_socket_message)));
        break;
    case SERVICE_TEXT:
        gate_dispatch_cmd(ctx, session, msg, sz);
        break;
    default:
        break;
    }
    return 0;
}
