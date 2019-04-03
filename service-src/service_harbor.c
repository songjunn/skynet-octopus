#include "skynet.h"

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

struct remote_service {
    char name[SERVICE_NAME_MAX];
    int harbor_id;
};

struct harbor {
    int harbor_id;
    int listen_fd;
    int listen_port;
    int remote_service_count;
    struct hashid hash;
    struct databuffer buffer[HARBOR_CLUSTER_MAX];
    struct harbor_cluster hcs[HARBOR_CLUSTER_MAX];
    struct remote_service rss[REMOTE_SERVICE_MAX];
};

struct harbor_cluster * get_cluster_fd(struct harbor * h, int fd) {
    int i;
    for (i=0;i<HARBOR_CLUSTER_MAX;++i) {
        if (h->hcs[i].fd == fd) {
            return &h->hcs[i];
        }
    }
    return NULL;
}

struct harbor_cluster * get_cluster_handle(struct harbor * h, uint32_t handle) {
    int harbor_id = skynet_harbor_id(handle);
    if (harbor_id > 0 && harbor_id < HARBOR_CLUSTER_MAX) {
        return &h->hcs[harbor_id];
    }
    return NULL;
}

struct harbor_cluster * get_cluster_name(struct harbor * h, const char * name) {
    int i, harbor_id;
    for (i=0;i<h->remote_service_count;++i) {
        if (!strcmp(h->rss[i].name, name)) {
            harbor_id = h->rss[i].harbor_id;
            return &h->hcs[harbor_id];
        }
    }
    return NULL;
}

int load_service(struct skynet_service * ctx, struct harbor * h, int harbor_id, char * name) {
    if (h->remote_service_count >= REMOTE_SERVICE_MAX) {
        skynet_logger_error(ctx, "[harbor]service name=%s, max count=%d", name, h->remote_service_count);
        return 1;
    }

    struct remote_service* service = &h->rss[h->remote_service_count++];
    service->harbor_id = harbor_id;
    sprintf(service->name, "%s", name);
    return 0;
}

int load_cluster(struct skynet_service * ctx, struct harbor * h, char * name) {
    int harbor_id, port;
    char * m;
    char * service_name = skynet_malloc(1024);
    char * cluster_addr = skynet_malloc(1024);
    char * cluster_service = skynet_malloc(1024);

    skynet_config_int(name, "harbor", &harbor_id);
    skynet_config_int(name, "port", &port);
    skynet_config_string(name, "addr", cluster_addr, 1024);
    skynet_config_string(name, "services", cluster_service, 1024);

    if (harbor_id < 0 && harbor_id >= HARBOR_CLUSTER_MAX) {
        skynet_logger_error(ctx, "[harbor]invalid harobr id=%d", harbor_id);
        return 1;
    }
    if (harbor_id != h->harbor_id) {
        struct harbor_cluster * cluster = &h->hcs[harbor_id];
        cluster->port = port;
        cluster->harbor_id = harbor_id;
        cluster->fd = skynet_socket_connect(ctx, cluster_addr, port);
        sprintf(cluster->addr, "%s", cluster_addr);

        skynet_logger_debug(ctx, "[harbor]create cluster:%s harbor:%d %s:%d fd:%d", 
            name, cluster->harbor_id, cluster->addr, cluster->port, cluster->fd);

        while (m = strchr(cluster_service, ',')) {
            snprintf(service_name, m-cluster_service+1, "%s", cluster_service);
            cluster_service = m + 1;

            if (load_service(ctx, h, harbor_id, service_name)) {
                return 1;
            }
        }
        return load_service(ctx, h, harbor_id, cluster_service);
    }

    return 0;
}

int load_clusters(struct skynet_service * ctx, struct harbor * h) {
    char * p;
    char * cluster_name = skynet_malloc(1024);
    char * cluster_list = skynet_malloc(1024);

    skynet_config_string("skynet", "cluster_list", cluster_list, 1024);

    while (p = strchr(cluster_list, ',')) {
        snprintf(cluster_name, p-cluster_list+1, "%s", cluster_list);
        cluster_list = p + 1;

        if (load_cluster(ctx, h, cluster_name)) {
            return 1;
        }
    }
    return load_cluster(ctx, h, cluster_list);
}

void send_remote_message(struct skynet_service * ctx, const void * msg, size_t sz) {
    struct skynet_remote_message rmsg;
    memcpy(&rmsg, msg, sz);

    struct harbor_cluster * cluster = NULL;
    if (rmsg.handle != 0) {
        cluster = get_cluster_handle(ctx->hook, rmsg.handle);
    } else {
        cluster = get_cluster_name(ctx->hook, rmsg.name);
    }
    if (cluster == NULL) {
        skynet_logger_error(ctx, "[harbor]find cluster failed");
        return;
    }

    uint32_t idx = 0;
    uint32_t ulen = sizeof(rmsg) - sizeof(rmsg.data) + rmsg.size + sizeof(uint32_t);
    char * buffer = (char *)skynet_malloc(ulen);

    memcpy(buffer+idx, (char *)&ulen, sizeof(ulen));
    idx += sizeof(ulen);

    memcpy(buffer+idx, (char *)rmsg.name, sizeof(rmsg.name));
    idx += sizeof(rmsg.name);

    memcpy(buffer+idx, (char *)&rmsg.handle, sizeof(rmsg.handle));
    idx += sizeof(rmsg.handle);

    memcpy(buffer+idx, (char *)&rmsg.source, sizeof(rmsg.source));
    idx += sizeof(rmsg.source);

    memcpy(buffer+idx, (char *)&rmsg.session, sizeof(rmsg.session));
    idx += sizeof(rmsg.session);

    memcpy(buffer+idx, (char *)&rmsg.type, sizeof(rmsg.type));
    idx += sizeof(rmsg.type);

    memcpy(buffer+idx, (char *)&rmsg.size, sizeof(rmsg.size));
    idx += sizeof(rmsg.size);

    memcpy(buffer+idx, (char *)rmsg.data, rmsg.size);
    idx += rmsg.size;

    assert(ulen == idx);
    skynet_socket_send(ctx, cluster->fd, buffer, ulen);
}

void recv_remote_message(struct skynet_service * ctx, struct databuffer * buffer) {
    int sz = databuffer_readheader(buffer->chunk);
    if (sz > 0) {
        struct skynet_remote_message rmsg;
        sz = databuffer_read(buffer->chunk, &rmsg, sz);
        if (sz > 0) {
            if (rmsg.handle > 0) {
                skynet_sendhandle(rmsg.handle, rmsg.source, rmsg.session, rmsg.type, rmsg.data, rmsg.size);
            } else {
                skynet_sendname(rmsg.name, rmsg.source, rmsg.session, rmsg.type, rmsg.data, rmsg.size);
            }
        }
    }
}

bool harbor_create(struct skynet_service * ctx, int harbor, const char * args) {
    struct harbor * h = (struct harbor *) skynet_malloc(sizeof(struct harbor));
    h->harbor_id = harbor;
    ctx->hook = h;
    sscanf(args, "%d", &h->listen_port);
    hashid_init(&h->hash, HARBOR_CLUSTER_MAX);

    if (load_clusters(ctx, h)) {
        return false;
    }

    h->listen_fd = skynet_socket_listen(ctx, "0.0.0.0", h->listen_port, BACKLOG);
    if (h->listen_fd < 0) {
        return false;
    }

    skynet_harbor_start(ctx);
    skynet_socket_start(ctx, h->listen_fd);
    skynet_logger_notice(ctx, "[harbor]listen port:%d fd %d", h->listen_port, h->listen_fd);

    return true;
}

void harbor_release(struct skynet_service * ctx) {
    skynet_harbor_exit();
    skynet_free(ctx->hook);
}

bool harbor_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {
    struct harbor * h = ctx->hook;
    switch (type) {
        case SERVICE_REMOTE: {
            send_remote_message(ctx, msg, sz);
            break;
        }
        case SERVICE_SOCKET: {
            const struct skynet_socket_message * smsg = (const struct skynet_socket_message *) msg;
            //size_t size = sz - sizeof(struct skynet_socket_message);

            switch(smsg->type) {
                case SKYNET_SOCKET_TYPE_DATA: {
                    skynet_logger_debug(ctx, "[harbor]recv fd=%d size=%d", smsg->id, smsg->ud);
                    int id = hashid_lookup(&h->hash, smsg->id);
                    if (id >= 0) {
                        if (databuffer_push(&h->buffer[id], smsg->buffer, smsg->ud) <= 0) {
                            skynet_logger_error(ctx, "[harbor]connection %d recv data too long, size:%d", smsg->id, smsg->ud);
                            skynet_socket_close(ctx, smsg->id);
                            skynet_free(smsg->buffer);
                        }
                        recv_remote_message(ctx, &h->buffer[id]);
                    } else {
                        skynet_logger_error(ctx, "[harbor]recv unknown connection %d message", smsg->id);
                        skynet_socket_close(ctx, smsg->id);
                        skynet_free(smsg->buffer);
                    }
                    break;
                }
                case SKYNET_SOCKET_TYPE_ACCEPT: {
                    if (hashid_full(&h->hash)) {
                        skynet_socket_close(ctx, smsg->ud);
                        skynet_logger_debug(ctx, "[harbor]accept max fd=%d addr=%s", smsg->ud, (const char *) (smsg + 1));
                    } else {
                        hashid_insert(&h->hash, smsg->ud);
                        skynet_socket_start(ctx, smsg->ud);
                        skynet_logger_debug(ctx, "[harbor]accept fd=%d addr=%s", smsg->ud, (const char *) (smsg + 1));
                    }
                    break;
                }
                case SKYNET_SOCKET_TYPE_CLOSE:
                case SKYNET_SOCKET_TYPE_ERROR: {
                    skynet_logger_debug(ctx, "[harbor]close fd=%d", smsg->id);
                    struct harbor_cluster * cluster = get_cluster_fd(h, smsg->id);
                    if (cluster) {
                        cluster->fd = skynet_socket_connect(ctx, cluster->addr, cluster->port);
                        skynet_logger_debug(ctx, "[harbor]connecting fd=%d harbor_id=%d", smsg->id, cluster->harbor_id);
                    }

                    int id = hashid_lookup(&h->hash, smsg->id);
                    if (id >= 0) {
                        databuffer_reset(&h->buffer[id]);
                    } else {
                        skynet_logger_error(ctx, "[harbor]close unknown connection %d", smsg->id);
                    }
                    break;
                }
                case SKYNET_SOCKET_TYPE_CONNECT:
                    skynet_logger_debug(ctx, "[harbor]connected fd=%d", smsg->id);
                    break;
                case SKYNET_SOCKET_TYPE_WARNING:
                    skynet_logger_warn(ctx, "[harbor]fd (%d) send buffer (%d)K", smsg->id, smsg->ud);
                    break;
                default:
                    skynet_logger_error(ctx, "[harbor]recv error message type %d", smsg->type);
                    break;
            }
            break;
        }
        default:
            break;
    }

    return true;
}
