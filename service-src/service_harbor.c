#include "skynet.h"

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
	int remote_service_count;
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
		if (!strncmp(h->rss[i].name, name)) {
			harbor_id = h->rss[i].harbor_id;
			return &h->hcs[harbor_id];
		}
	}
	return NULL;
}

int load_clusters(struct skynet_service * ctx, struct harbor * h) {
	int harbor_id, port;
	char * cluster_name = NULL;
	char * service_name = NULL;
    char cluster_list[1024] = {0};
    char cluster_addr[1024] = {0};
    char cluster_service[1024] = {0};

    skynet_config_string("skynet", "cluster_list", cluster_list, 1024);

    cluster_name = strtok(cluster_list, ",");
    while (cluster_name != NULL) {
        skynet_config_int(cluster_name, "harbor", &harbor_id);
        skynet_config_int(cluster_name, "port", &port);
        skynet_config_string(cluster_name, "addr", cluster_addr, 1024);
        skynet_config_string(cluster_name, "services", cluster_service, 1024);

        if (harbor_id < 0 && harbor_id >= HARBOR_CLUSTER_MAX) {
        	skynet_logger_error(ctx, "invalid harobr id=%d", harbor_id);
        	return 1;
        }

        struct harbor_cluster * cluster = &h->hcs[harbor_id];
        cluster->port = port;
        cluster->harbor_id = harbor_id;
        cluster->fd = skynet_socket_connect(ctx, cluster_addr, port);
        memcpy(cluster->addr, cluster_addr, sizeof(cluster_addr));

        service_name = strtok(cluster_service, ",");
        while (service_name != NULL) {
        	if (h->remote_service_count >= REMOTE_SERVICE_MAX) {
	        	skynet_logger_error(ctx, "service name=%s, max count=%d", service_name, h->remote_service_count);
	        	return 1;
	        }
	        struct remote_service* service = &h->rss[h->remote_service_count++];
	        service->harbor_id = harbor_id;
	        memcpy(service->name, service_name, sizeof(service->name));

	        service_name = strtok(NULL, ",");
        }
        
        cluster_name = strtok(NULL, ",");
    }

    return 0;
}

void send_remote_message(struct skynet_service * ctx, const void * msg, size_t sz) {
	struct skynet_remote_message * rmsg = skynet_malloc(sizeof(struct skynet_remote_message));
	memcpy(rmsg, msg, sz);

	struct harbor_cluster * cluster = NULL;
	if (rmsg->handle != 0) {
		cluster = get_cluster_handle(ctx->hook, rmsg->handle);
	} else {
		cluster = get_cluster_name(ctx->hook, rmsg->name);
	}
	if (cluster == NULL) {
		skynet_logger_error(ctx, "[harbor]find cluster failed");
		return;
	}

	skynet_socket_send(ctx, cluster->fd, rmsg.data, rmsg.size);
}

void recv_remote_message(const char * message, size_t sz) {

}

bool harbor_create(struct skynet_service * ctx, int harbor, const char * args) {
	struct harbor * h = (struct harbor *)skynet_malloc(sizeof(struct harbor));

	if (load_clusters(ctx, h)) {
		return false;
	}

	ctx->hook = h;
	skynet_harbor_start(ctx);

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
			size_t size = sz - sizeof(struct skynet_socket_message);

			switch(smsg->type) {
				case SKYNET_SOCKET_TYPE_DATA:
					break;
				case SKYNET_SOCKET_TYPE_ACCEPT:
					skynet_logger_debug(ctx, "[harbor]accept fd=%d addr=%s", smsg->ud, (const char *) (smsg + 1));
					break;
				case SKYNET_SOCKET_TYPE_CLOSE:
		    	case SKYNET_SOCKET_TYPE_ERROR: {
		    		skynet_logger_debug(ctx, "[harbor]close fd=%d", smsg->id);
		    		struct harbor_cluster * cluster = get_cluster_fd(h, smsg->id);
		    		if (cluster) {
		    			cluster->fd = skynet_socket_connect(ctx, cluster->addr, cluster->port);
		    			skynet_logger_debug(ctx, "[harbor]connecting fd=%d", smsg->id);
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
