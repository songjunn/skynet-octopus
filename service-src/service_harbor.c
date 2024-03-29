#include "skynet.h"
#include "skynet_socket.h"
#include "hashid.h"
#include "databuffer.h"

#define BACKLOG 32
#define CLUSTER_ADDR_MAX 32
#define SERVICE_NAME_MAX 64
#define HARBOR_CLUSTER_MAX 32
#define REMOTE_SERVICE_MAX 256

struct harbor_cluster {
    int fd;
    int port;
    int harbor_id;
    char addr[CLUSTER_ADDR_MAX];
};

struct harbor_remote_service {
    char name[SERVICE_NAME_MAX];
    int harbor_id;
};

struct harbor {
    int harbor_id;
    int listen_fd;
    int listen_port;
    int buffer_size;
    int remote_service_count;
    struct hashid hash;
    struct databuffer * buffer[HARBOR_CLUSTER_MAX];
    struct harbor_cluster hcs[HARBOR_CLUSTER_MAX];
    struct harbor_remote_service rss[REMOTE_SERVICE_MAX];
};

struct harbor_cluster * harbor_get_cluster_fd(struct harbor * h, int fd) {
    int i;
    for (i=0;i<HARBOR_CLUSTER_MAX;++i) {
        if (h->hcs[i].fd == fd) {
            return &h->hcs[i];
        }
    }
    return NULL;
}

struct harbor_cluster * harbor_get_cluster_id(struct harbor * h, int harbor_id) {
    if (harbor_id > 0 && harbor_id < HARBOR_CLUSTER_MAX) {
        return &h->hcs[harbor_id];
    }
    return NULL;
}

struct harbor_cluster * harbor_get_cluster_name(struct harbor * h, const char * name) {
    int i, harbor_id;
    for (i=0;i<h->remote_service_count;++i) {
        if (!strcmp(h->rss[i].name, name)) {
            harbor_id = h->rss[i].harbor_id;
            return &h->hcs[harbor_id];
        }
    }
    return NULL;
}

int harbor_load_service(struct skynet_service * ctx, struct harbor * h, int harbor_id, char * name) {
    if (h->remote_service_count >= REMOTE_SERVICE_MAX) {
        skynet_logger_error(ctx->handle, "[harbor]service name=%s, max count=%d", name, h->remote_service_count);
        return 1;
    }

    struct harbor_remote_service* service = &h->rss[h->remote_service_count++];
    service->harbor_id = harbor_id;
    sprintf(service->name, "%s", name);
    return 0;
}

int harbor_load_cluster(struct skynet_service * ctx, struct harbor * h, char * name) {
    int harbor_id, port;
    char * m, * p;
    char service_name[1024], cluster_addr[1024], cluster_service[1024];

    skynet_config_int(name, "harbor", &harbor_id);
    skynet_config_int(name, "port", &port);
    skynet_config_string(name, "addr", cluster_addr, sizeof(cluster_addr));
    skynet_config_string(name, "services", cluster_service, sizeof(cluster_service));

    if (harbor_id < 0 && harbor_id >= HARBOR_CLUSTER_MAX) {
        skynet_logger_error(ctx->handle, "[harbor]invalid harobr id=%d", harbor_id);
        return 1;
    }
    if (harbor_id != h->harbor_id) {
        struct harbor_cluster * cluster = &h->hcs[harbor_id];
        cluster->port = port;
        cluster->harbor_id = harbor_id;
        cluster->fd = skynet_socket_connect(ctx, cluster_addr, port);
        sprintf(cluster->addr, "%s", cluster_addr);

        skynet_logger_debug(ctx->handle, "[harbor]create cluster:%s harbor:%d %s:%d fd:%d", 
            name, cluster->harbor_id, cluster->addr, cluster->port, cluster->fd);

        p = cluster_service;
        while (m = strchr(p, ',')) {
            snprintf(service_name, m-p+1, "%s", p);
            p = m + 1;

            if (harbor_load_service(ctx, h, harbor_id, service_name)) {
                return 1;
            }
        }
        return harbor_load_service(ctx, h, harbor_id, p);
    }

    return 0;
}

int harbor_load_clusters(struct skynet_service * ctx, struct harbor * h) {
    char * m, * p;
    char cluster_name[1024], cluster_list[1024];

    skynet_config_string("skynet", "cluster_list", cluster_list, sizeof(cluster_list));

    p = cluster_list;
    while (m = strchr(p, ',')) {
        snprintf(cluster_name, m-p+1, "%s", p);
        p = m + 1;

        if (harbor_load_cluster(ctx, h, cluster_name)) {
            return 1;
        }
    }
    return harbor_load_cluster(ctx, h, p);
}

void harbor_forward_remote_message(struct skynet_service * ctx, const void * msg, size_t sz) {
    struct skynet_remote_message * rmsg = msg;
    struct harbor_cluster * cluster = NULL;
    int harbor_id = skynet_harbor_id(rmsg->handle);

    if (rmsg->handle != 0) {
        cluster = harbor_get_cluster_id(ctx->hook, harbor_id);
    } else {
        cluster = harbor_get_cluster_name(ctx->hook, rmsg->name);
    }
    if (cluster == NULL) {
        skynet_logger_error(ctx->handle, "[harbor]find cluster failed");
        return;
    }

    int head = skynet_remote_message_header() + rmsg->size;
    int ulen = head + sizeof(head);
    char * buffer = (char *) SKYNET_MALLOC(ulen);
    memcpy(buffer, (char *) &head, sizeof(head));
    skynet_remote_message_push(rmsg, buffer + sizeof(head), ulen);
    skynet_socket_send(ctx, cluster->fd, buffer, ulen);
    skynet_logger_debug(ctx->handle, "[harbor]send fd=%d size=%d head=%d", cluster->fd, ulen, head);
    skynet_logger_debug(ctx->handle, "[harbor]remote message forward name=%s handle=%d source=%d session=%d type=%d size=%d",
                rmsg->name, rmsg->handle, rmsg->source, rmsg->session, rmsg->type, rmsg->size);
}

void harbor_forward_local_message(struct skynet_service * ctx, struct databuffer * buffer) {
    int sz;
    struct skynet_remote_message * rmsg;

    while (sz = databuffer_readpack(buffer, &rmsg)) {
        skynet_logger_debug(ctx->handle, "[harbor]local message forward name=%s handle=%d source=%d session=%d type=%d size=%d", 
            rmsg->name, rmsg->handle, rmsg->source, rmsg->session, rmsg->type, rmsg->size);
        skynet_local_message_forward(rmsg, sz);
        databuffer_freepack(buffer);
    }
}

void harbor_cluster_reconnect(struct skynet_service * ctx, struct harbor * h, int harbor_id) {
    struct harbor_cluster * cluster = harbor_get_cluster_id(h, harbor_id);
    if (cluster) {
        cluster->fd = skynet_socket_connect(ctx, cluster->addr, cluster->port);
        skynet_logger_debug(ctx->handle, "[harbor]connecting fd=%d harbor_id=%d", cluster->fd, cluster->harbor_id);
    }
}

int harbor_create(struct skynet_service * ctx, int harbor, const char * args) {
    struct harbor * h = SKYNET_MALLOC(sizeof(struct harbor));
    h->harbor_id = harbor;
    ctx->hook = h;
    sscanf(args, "%d,%d", &h->listen_port, &h->buffer_size);
    hashid_init(&h->hash, HARBOR_CLUSTER_MAX);

    if (harbor_load_clusters(ctx, h)) {
        return 1;
    }

    h->listen_fd = skynet_socket_listen(ctx, "0.0.0.0", h->listen_port, BACKLOG);
    if (h->listen_fd < 0) {
        return 1;
    }

    skynet_harbor_start(ctx);
    skynet_socket_start(ctx, h->listen_fd);
    skynet_logger_notice(ctx->handle, "[harbor]listen port:%d fd %d", h->listen_port, h->listen_fd);

    return 0;
}

void harbor_release(struct skynet_service * ctx) {
    struct harbor * h = ctx->hook;
    skynet_harbor_exit();
    hashid_clear(&h->hash);
    skynet_free(ctx->hook);
}

int harbor_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {
    struct harbor * h = ctx->hook;
    switch (type) {
    case SERVICE_REMOTE: {
        harbor_forward_remote_message(ctx, msg, sz);
        break;
    }
    case SERVICE_SOCKET: {
        const struct skynet_socket_message * smsg = (const struct skynet_socket_message *) msg;
        //size_t size = sz - sizeof(struct skynet_socket_message);

        switch(smsg->type) {
        case SKYNET_SOCKET_TYPE_DATA: {
            skynet_logger_debug(ctx->handle, "[harbor]recv fd=%d size=%d", smsg->id, smsg->ud);
            int id = hashid_lookup(&h->hash, smsg->id);
            if (id >= 0) {
                if (databuffer_push(h->buffer[id], smsg->buffer, smsg->ud) <= 0) {
                    skynet_logger_error(ctx->handle, "[harbor]connection %d recv data too long, size:%d", smsg->id, smsg->ud);
                    skynet_socket_close(ctx, smsg->id);
                }
                harbor_forward_local_message(ctx, h->buffer[id]);
                skynet_free(smsg->buffer);
            } else {
                skynet_logger_error(ctx->handle, "[harbor]recv unknown connection %d message", smsg->id);
                skynet_socket_close(ctx, smsg->id);
                skynet_free(smsg->buffer);
            }
            break;
        }
        case SKYNET_SOCKET_TYPE_ACCEPT: {
            if (hashid_full(&h->hash)) {
                skynet_socket_close(ctx, smsg->ud);
                skynet_logger_debug(ctx->handle, "[harbor]accept max fd=%d addr=%s", smsg->ud, (const char *) (smsg + 1));
            } else {
                int id = hashid_insert(&h->hash, smsg->ud);
                h->buffer[id] = databuffer_create(h->buffer_size);
                skynet_socket_start(ctx, smsg->ud);
                skynet_logger_debug(ctx->handle, "[harbor]accept fd=%d addr=%s", smsg->ud, (const char *) (smsg + 1));
            }
            break;
        }
        case SKYNET_SOCKET_TYPE_CLOSE:
        case SKYNET_SOCKET_TYPE_ERROR: {
            skynet_logger_debug(ctx->handle, "[harbor]close fd=%d", smsg->id);
            struct harbor_cluster * cluster = harbor_get_cluster_fd(h, smsg->id);
            if (cluster) {
                cluster->fd = 0;
                skynet_timer_register(ctx->handle, 0, &cluster->harbor_id, sizeof(cluster->harbor_id), 1000);
            }

            int id = hashid_remove(&h->hash, smsg->id);
            if (id >= 0) {
                databuffer_free(h->buffer[id]);
            }
            break;
        }
        case SKYNET_SOCKET_TYPE_CONNECT:
            if (smsg->id == h->listen_fd) {
                break; // start listening
            }
            skynet_logger_debug(ctx->handle, "[harbor]connected fd=%d", smsg->id);
            break;
        case SKYNET_SOCKET_TYPE_WARNING:
            skynet_logger_warn(ctx->handle, "[harbor]fd (%d) send buffer (%d)K", smsg->id, smsg->ud);
            break;
        default:
            skynet_logger_error(ctx->handle, "[harbor]recv error message type %d", smsg->type);
            break;
        }
        break;
    }
    case SERVICE_TIMER: {
        int harbor_id = *(int *) msg;
        harbor_cluster_reconnect(ctx, h, harbor_id);
        break;
    }
    default:
        break;
    }

    return 0;
}
