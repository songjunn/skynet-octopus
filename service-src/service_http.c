#include "skynet.h"
#include "skynet_socket.h"
#include "hashid.h"
#include "http_proxy.h"

#define BACKLOG 32
#define BUFFER_MAX 20480
#define MESSAGE_BUFFER_MAX 20496

struct http_connection {
    int fd;
    char * remote_name;
    struct http_proxy * buffer;
};

struct http {
    int listen_fd;
    int listen_port;
    int connect_max;
    char forward[64];
    struct hashid hash;
    struct http_connection * conn;
};

void http_accept(struct skynet_service * ctx, struct http_connection * conn) {

}

void http_close(struct skynet_service * ctx, struct http_connection * conn) {

}

void http_message(struct skynet_service * ctx, struct http_connection * conn) {
    struct http * g = ctx->hook;
    struct http_proxy * buf = conn->buffer;

    char msg[MESSAGE_BUFFER_MAX];
    sprintf(msg, "http|%d|", buf->url_ptr);
    int hsz = strlen(msg);
    memcpy(msg+hsz, buf->url, buf->url_ptr);

    skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, hsz+buf->url_ptr);
    proxy_reset(buf);
}

void http_dispatch_cmd(struct skynet_service * ctx, const char * msg, size_t sz) {
    int i;
    char * command = msg;

    for (i=0;i<sz;i++) {
        if (command[i]=='|') {
            break;
        }
    }
    if (i >= sz || i+1 >= sz) {
        return;
    }

    char * param = command+i+1;
    if (memcmp(command, "response", i) == 0) {
        char * fdstr = strchr(param, '|');
        if (fdstr == NULL) {
            return;
        }
        int size = sz-(fdstr-command)-1;
        if (size > 0) {
            int fd = atoi(param);
            char * buffer = (char *)skynet_malloc(size);
            memcpy(buffer, fdstr+1, size);
            skynet_socket_send(ctx, fd, (void *)buffer, size);
        }
    } else if (memcmp(command, "request", i) == 0) {

    }
}

void http_dispatch_socket_message(struct skynet_service * ctx, const struct skynet_socket_message * message, size_t sz) {
    struct http * g = ctx->hook;
    switch(message->type) {
    case SKYNET_SOCKET_TYPE_DATA: {
        skynet_logger_debug(ctx->handle, "[http]recv data fd %d size:%d", message->id, message->ud);
        int id = hashid_lookup(&g->hash, message->id);
        if (id >= 0) {
            struct http_connection *c = &g->conn[id];
            int nparsed = proxy_parse(c->buffer, message->buffer, message->ud);
            if (nparsed < 0) {
                skynet_logger_error(ctx->handle, "[http]connection recv data too long fd=%d size=%d", message->id, message->ud);
                skynet_socket_close(ctx, message->id);
                skynet_free(message->buffer);
            } else if (nparsed > 0) {
                http_message(ctx, c);
            }
        } else {
            skynet_logger_error(ctx->handle, "[http]recv unknown connection data fd=%d size=%d", message->id, message->ud);
            skynet_socket_close(ctx, message->id);
            skynet_free(message->buffer);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_ACCEPT: {
        assert(g->listen_fd == message->id);
        if (hashid_full(&g->hash)) {
            skynet_logger_error(ctx->handle, "[http]full on accepting, alloc:%d, accepted:%d, close socket:%d", g->connect_max, g->hash.count, message->ud);
            skynet_socket_close(ctx, message->ud);
        } else {
            int id = hashid_insert(&g->hash, message->ud);
            const char * remote_name = (const char *) (message + 1);

            struct http_connection *c = &g->conn[id];
            c->fd = message->ud;
            c->buffer = proxy_create(HTTP_REQUEST, message->ud, remote_name, sz);
            c->remote_name = skynet_malloc(sz+1);
            memcpy(c->remote_name, remote_name, sz);
            c->remote_name[sz] = '\0';

            http_accept(ctx, c);
            skynet_socket_start(ctx, c->fd);
            skynet_logger_debug(ctx->handle, "[http]accept fd=%d addr=%s", c->fd, c->remote_name);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_CLOSE:
    case SKYNET_SOCKET_TYPE_ERROR: {
        skynet_logger_debug(ctx->handle, "[http]close or error %d fd %d", message->type, message->id);
        int id = hashid_remove(&g->hash, message->id);
        if (id >= 0) {
            struct http_connection *c = &g->conn[id];
            http_close(ctx, c);
            c->fd = -1;
            proxy_destroy(c->buffer);
            skynet_free(c->remote_name);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_CONNECT: {
        if (message->id == g->listen_fd) {
            break; // start listening
        }
        int id = hashid_lookup(&g->hash, message->id);
        if (id <= 0) {
            skynet_logger_error(ctx->handle, "[http]connected unknown connection %d message", message->id);
            skynet_socket_close(ctx, message->id);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_WARNING:
        skynet_logger_warn(ctx->handle, "[http]fd (%d) send buffer (%d)K", message->id, message->ud);
        break;
    default:
        skynet_logger_error(ctx->handle, "[http]recv error message type %d", message->type);
        break;
    }
}

int http_create(struct skynet_service * ctx, int harbor, const char * args) {
    int i;
    struct http * g = skynet_malloc(sizeof(struct http));
    sscanf(args, "%[^','],%d,%d", g->forward, &g->listen_port, &g->connect_max);

    g->listen_fd = skynet_socket_listen(ctx, "0.0.0.0", g->listen_port, BACKLOG);
    if (g->listen_fd < 0) {
        return 1;
    }

    hashid_init(&g->hash, g->connect_max);
    g->conn = skynet_malloc(g->connect_max * sizeof(struct http_connection));
    memset(g->conn, 0, g->connect_max * sizeof(struct http_connection));
    for (i=0; i<g->connect_max; i++) {
        g->conn[i].fd = -1;
    }
    ctx->hook = g;

    skynet_logger_notice(ctx->handle, "[http]listen port:%d fd:%d", g->listen_port, g->listen_fd);
    skynet_socket_start(ctx, g->listen_fd);
    return 0;
}

void http_release(struct skynet_service * ctx) {
    int i;
    struct http * g = ctx->hook;
    for (i=0; i<g->connect_max; i++) {
        struct http_connection * c = &g->conn[i];
        if (c->fd >= 0) {
            skynet_free(c->remote_name);
            proxy_destroy(c->buffer);
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

int http_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {
    switch(type) {
    case SERVICE_SOCKET:
        http_dispatch_socket_message(ctx, (const struct skynet_socket_message *)msg, (int)(sz-sizeof(struct skynet_socket_message)));
        break;
    case SERVICE_TEXT:
        http_dispatch_cmd(ctx, msg, sz);
        break;
    default:
        break;
    }
    return 0;
}
