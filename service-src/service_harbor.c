#include "skynet.h"

bool harbor_create(struct skynet_service * ctx, int harbor, const char * args) {
	h->ctx = ctx;
	int harbor_id = 0;
	uint32_t slave = 0;
	sscanf(args,"%d %u", &harbor_id, &slave);
	if (slave == 0) {
		return 1;
	}
	h->id = harbor_id;
	h->slave = slave;
	skynet_callback(ctx, h, mainloop);
	skynet_harbor_start(ctx);

	struct skynet_service * ctx = skynet_malloc(sizeof(*ctx));
    ctx->name = skynet_strdup("logger");
    ctx->create = logger_create;
    ctx->release = logger_release;
    ctx->cb = logger_callback;

    skynet_service_insert(ctx, harbor, filename, 0);
    skynet_free(ctx->name);
    skynet_free(ctx);

	return true;
}

void harbor_release(struct skynet_service * ctx) {

}

bool harbor_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {

}
