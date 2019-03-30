#include "skynet.h"

#define CLUSTER_ADDR_MAX 32
#define SERVICE_NAME_MAX 64
#define SERVICE_LIST_MAX 128
#define HARBOR_CLUSTER_MAX 32
#define REMOTE_SERVICE_MAX 256

struct harbor_cluster {
	int fd;
	int host;
	int port;
	int harbor_id;
	char addr[CLUSTER_ADDR_MAX];
};

struct remote_service {
	char name[SERVICE_NAME_MAX];
	int harbor_id;
};

struct harbor {
	int port;
	int remote_service_count;
	char addr[CLUSTER_ADDR_MAX];
	char service_list[SERVICE_LIST_MAX];
	struct harbor_cluster hcs[HARBOR_CLUSTER_MAX];
	struct remote_service rss[REMOTE_SERVICE_MAX];
};

struct harbor_cluster * get_cluster_handle(struct harbor * h, uint32_t handle) {
	int harbor_id = skynet_harbor_id(handle);
	if (harbor_id > 0 && harbor_id < HARBOR_CLUSTER_MAX) {
		return h->hcs[harbor_id];
	}
	return NULL;
}

struct harbor_cluster * get_cluster_name(struct harbor * h, const char * name) {
	int i, harbor_id;
	for (i=0;i<remote_service_count;++i) {
		if (!strncmp(h->rss[i].name, name)) {
			harbor_id = h->rss[i].harbor_id;
			return h->hcs[harbor_id];
		}
	}
	return NULL;
}

void _send_remote_message(struct skynet_service * ctx, uint32_t source, uint32_t session, const void * msg, size_t sz) {

}

bool harbor_create(struct skynet_service * ctx, int harbor, const char * args) {
	struct harbor * instance = (struct harbor *)skynet_malloc(sizeof(struct harbor));
	sscanf(args, "%[^','],%d,%[^',']", instance->addr, &instance->port, instance->service_list);

	ctx->hook = instance;
	skynet_harbor_start(ctx);

	return true;
}

void harbor_release(struct skynet_service * ctx) {

}

bool harbor_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {
	switch (type) {
	case SERVICE_REMOTE: {
		_send_remote_message(ctx, source, session, msg, sz);
		break;
	}
	case SERVICE_SOCKET: {
		break;
	}
	default:
		break;
	}

	return true;
}
