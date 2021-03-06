#include "skynet.h"
#include "skynet_malloc.h"
#include "skynet_socket.h"
#include "websocket.h"
#include "hashid.h"
#include "databuffer.h"

#define BACKLOG 32
#define MESSAGE_BUFFER_MAX 20496

struct gatews_conn {
    int fd;
    char * remote_name;
    struct databuffer * buffer;
    struct handshake * hs;
    enum wsState state;
};

struct gatews {
    int listen_fd;
    int listen_port;
    int buffer_max;
    int connect_max;
    char forward[64];
    struct hashid hash;
    struct gatews_conn * conn;
};

void conn_init(struct gatews * g, struct gatews_conn * conn, int fd, char * remote_name, size_t sz) {
    conn->fd = fd;
    conn->buffer = databuffer_create(g->buffer_max);
    conn->remote_name = skynet_malloc(sz+1);
    skynet_malloc_insert(conn->remote_name, sz+1, __FILE__, __LINE__);
    memcpy(conn->remote_name, remote_name, sz);
    conn->remote_name[sz] = '\0';
    conn->state = WS_STATE_OPENING;
    conn->hs = skynet_malloc(sizeof(struct handshake));
    skynet_malloc_insert(conn->hs, sizeof(struct handshake), __FILE__, __LINE__);
    nullHandshake(conn->hs);
}

void conn_free(struct gatews_conn * conn) {
    conn->fd = -1;
    databuffer_free(conn->buffer);
    freeHandshake(conn->hs);
    skynet_malloc_remove(conn->hs);
    skynet_malloc_remove(conn->remote_name);
    skynet_free(conn->hs);
    skynet_free(conn->remote_name);
}

void gatews_accept(struct skynet_service * ctx, struct gatews_conn * conn) {
    struct gatews * g = ctx->hook;
    char msg[64];
    sprintf(msg, "connect|%d|%s", strlen(conn->remote_name), conn->remote_name);
    skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, strlen(msg));
}

void gatews_close(struct skynet_service * ctx, struct gatews_conn * conn) {
    struct gatews * g = ctx->hook;
    char msg[16];
    sprintf(msg, "disconnect|0|");
    skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, strlen(msg));
}

int gatews_message(struct skynet_service * ctx, struct gatews_conn * conn) {
    struct gatews * g = ctx->hook;
    struct handshake * hs = conn->hs;
    enum wsFrameType frameType;
    uint8_t *data = NULL;
    size_t readSize = 0;
    size_t dataSize = 0;
    size_t frameSize = 1024;

    if (conn->state == WS_STATE_OPENING) {
        frameType = wsParseHandshake(conn->buffer->chunk, conn->buffer->ptr, conn->hs);
    } else {
        frameType = wsParseInputFrame(conn->buffer->chunk, conn->buffer->ptr, &readSize, &data, &dataSize);
    }

    //skynet_logger_debug(ctx->handle, "[gatews]parse state=%d frameType=%d", conn->state, frameType);

    if ((frameType == WS_INCOMPLETE_FRAME && databuffer_isfull(conn->buffer)) || frameType == WS_ERROR_FRAME) {
        skynet_logger_error(ctx->handle, "[gatews]parse message error, fd=%d frameType=%d", conn->fd, frameType);
            
        if (conn->state == WS_STATE_OPENING) {
            char * frame = (char *)skynet_malloc(frameSize);
            skynet_malloc_insert(frame, frameSize, __FILE__, __LINE__);
            frameSize = sprintf(frame, "HTTP/1.1 400 Bad Request\r\n%s%s\r\n\r\n", versionField, version);
            skynet_socket_send(ctx, conn->fd, (void *)frame, frameSize);
            skynet_socket_close(ctx, conn->fd);
        } else {
            char * frame = (char *)skynet_malloc(frameSize);
            skynet_malloc_insert(frame, frameSize, __FILE__, __LINE__);
            wsMakeFrame(NULL, 0, frame, &frameSize, WS_CLOSING_FRAME);
            skynet_socket_send(ctx, conn->fd, (void *)frame, frameSize);
            skynet_socket_close(ctx, conn->fd);
        }
        return 0;
    }

    if (conn->state == WS_STATE_OPENING) {
        //assert(frameType == WS_OPENING_FRAME);
        if (frameType == WS_OPENING_FRAME) {
            char * frame = (char *)skynet_malloc(frameSize);
            skynet_malloc_insert(frame, frameSize, __FILE__, __LINE__);
            wsGetHandshakeAnswer(hs, frame, &frameSize);
            skynet_socket_send(ctx, conn->fd, (void *)frame, frameSize);
            databuffer_reset(conn->buffer);
            conn->state = WS_STATE_NORMAL;
            skynet_logger_debug(ctx->handle, "[gatews]handshake success fd=%d", conn->fd);
        }
    } else {
        if (frameType == WS_BINARY_FRAME /*|| frameType == WS_TEXT_FRAME*/) {
            if (dataSize <= g->buffer_max - 32) {
                char * msg = (char *)skynet_malloc(g->buffer_max);
                skynet_malloc_insert(msg, g->buffer_max, __FILE__, __LINE__);
                sprintf(msg, "forward|%d|", dataSize);
                int hsz = strlen(msg);
                memcpy(msg+hsz, data, dataSize);
                skynet_logger_debug(ctx->handle, "[gatews]forward data sz=%d", hsz+dataSize);
                skynet_sendname(g->forward, ctx->handle, conn->fd, SERVICE_TEXT, msg, hsz+dataSize);
                skynet_malloc_remove(msg);
                skynet_free(msg);
                return databuffer_pop(conn->buffer, readSize);
            } else {
                skynet_logger_error(ctx->handle, "[gatews]message too large, fd=%d dataSize=%d", conn->fd, dataSize);
                skynet_socket_close(ctx, conn->fd);
            }
        } else if (frameType == WS_CLOSING_FRAME) {
            if (conn->state != WS_STATE_CLOSING) {
                char * frame = (char *)skynet_malloc(frameSize);
                skynet_malloc_insert(frame, frameSize, __FILE__, __LINE__);
                wsMakeFrame(NULL, 0, frame, &frameSize, WS_CLOSING_FRAME);
                skynet_socket_send(ctx, conn->fd, (void *)frame, frameSize);
                skynet_socket_close(ctx, conn->fd);
            }
        }
    }
    return 0;
}

void gatews_dispatch_cmd(struct skynet_service * ctx, int fd, const char * msg, size_t sz) {
    int i;
    struct gatews * g = ctx->hook;
    char * command = msg;

    for (i=0;i<sz;i++) {
        if (command[i]=='|') {
            break;
        }
    }
    
    if (memcmp(command, "forward", i) == 0) {
        int size = sz-i-1;
        if (size > 0) {
            size_t frameSize = size+10;
            char * param = command+i+1;
            char * frame = (char *)skynet_malloc(frameSize);
            skynet_malloc_insert(frame, frameSize, __FILE__, __LINE__);
            wsMakeFrame(param, size, frame, &frameSize, WS_BINARY_FRAME);
            skynet_socket_send(ctx, fd, (void *)frame, frameSize);
        }
    } else if (memcmp(command, "kick", i) == 0) {
        skynet_socket_close(ctx, fd);
    }
}

void gatews_dispatch_socket_message(struct skynet_service * ctx, const struct skynet_socket_message * message, size_t sz) {
    struct gatews * g = ctx->hook;
    switch(message->type) {
    case SKYNET_SOCKET_TYPE_DATA: {
        skynet_logger_debug(ctx->handle, "[gatews]recv data fd %d size:%d", message->id, message->ud);
        int id = hashid_lookup(&g->hash, message->id);
        if (id >= 0) {
            struct gatews_conn *c = &g->conn[id];
            if (databuffer_push(c->buffer, message->buffer, message->ud) <= 0) {
                skynet_logger_error(ctx->handle, "[gatews]connection recv data too long fd=%d size=%d", message->id, message->ud);
                skynet_socket_close(ctx, message->id);
            } else {
                while(gatews_message(ctx, c));
            }
        } else {
            skynet_logger_error(ctx->handle, "[gatews]recv unknown connection data fd=%d size=%d", message->id, message->ud);
            skynet_socket_close(ctx, message->id);
        }
        skynet_malloc_remove(message->buffer);
        skynet_free(message->buffer);
        break;
    }
    case SKYNET_SOCKET_TYPE_ACCEPT: {
        assert(g->listen_fd == message->id);
        if (hashid_full(&g->hash)) {
            skynet_logger_error(ctx->handle, "[gatews]full on accepting, alloc:%d, accepted:%d, close socket:%d", g->connect_max, g->hash.count, message->ud);
            skynet_socket_close(ctx, message->ud);
        } else {
            int id = hashid_insert(&g->hash, message->ud);
            const char * remote_name = (const char *) (message + 1);

            struct gatews_conn *c = &g->conn[id];
            conn_init(g, c, message->ud, remote_name, sz);

            gatews_accept(ctx, c);
            skynet_socket_start(ctx, c->fd);
            skynet_logger_debug(ctx->handle, "[gatews]accept fd=%d addr=%s", c->fd, c->remote_name);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_CLOSE:
    case SKYNET_SOCKET_TYPE_ERROR: {
        skynet_logger_debug(ctx->handle, "[gatews]close or error %d fd %d", message->type, message->id);
        int id = hashid_remove(&g->hash, message->id);
        if (id >= 0) {
            struct gatews_conn *c = &g->conn[id];
            gatews_close(ctx, c);
            conn_free(c);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_CONNECT: {
        if (message->id == g->listen_fd) {
            break; // start listening
        }
        int id = hashid_lookup(&g->hash, message->id);
        if (id <= 0) {
            skynet_logger_error(ctx->handle, "[gatews]connected unknown connection %d message", message->id);
            skynet_socket_close(ctx, message->id);
        }
        break;
    }
    case SKYNET_SOCKET_TYPE_WARNING:
        skynet_logger_warn(ctx->handle, "[gatews]fd (%d) send buffer (%d)K", message->id, message->ud);
        break;
    default:
        skynet_logger_error(ctx->handle, "[gatews]recv error message type %d", message->type);
        break;
    }
}

int gatews_create(struct skynet_service * ctx, int harbor, const char * args) {
    int i;
    struct gatews * g = skynet_malloc(sizeof(struct gatews));
    sscanf(args, "%[^','],%d,%d,%d", g->forward, &g->listen_port, &g->connect_max, &g->buffer_max);

    g->listen_fd = skynet_socket_listen(ctx, "0.0.0.0", g->listen_port, BACKLOG);
    if (g->listen_fd < 0) {
        return 1;
    }

    hashid_init(&g->hash, g->connect_max);
    g->conn = skynet_malloc(g->connect_max * sizeof(struct gatews_conn));
    memset(g->conn, 0, g->connect_max * sizeof(struct gatews_conn));
    for (i=0; i<g->connect_max; i++) {
        g->conn[i].fd = -1;
    }
    ctx->hook = g;

    skynet_logger_notice(ctx->handle, "[gatews]listen port:%d fd:%d", g->listen_port, g->listen_fd);
    skynet_socket_start(ctx, g->listen_fd);
    return 0;
}

void gatews_release(struct skynet_service * ctx) {
    int i;
    struct gatews * g = ctx->hook;
    for (i=0; i<g->connect_max; i++) {
        struct gatews_conn * c = &g->conn[i];
        if (c->fd >= 0) {
            skynet_socket_close(ctx, c->fd);
            conn_free(c);
        }
    }
    if (g->listen_fd >= 0) {
        skynet_socket_close(ctx, g->listen_fd);
    }
    hashid_clear(&g->hash);
    skynet_free(g->conn);
    skynet_free(g);
}

int gatews_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {
    switch(type) {
    case SERVICE_SOCKET:
        gatews_dispatch_socket_message(ctx, (const struct skynet_socket_message *)msg, (int)(sz-sizeof(struct skynet_socket_message)));
        break;
    case SERVICE_TEXT:
        gatews_dispatch_cmd(ctx, session, msg, sz);
        break;
    default:
        break;
    }
    return 0;
}
