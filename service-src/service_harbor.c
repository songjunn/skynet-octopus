#include "skynet.h"

bool harbor_create(struct skynet_service * ctx, int harbor, const char * args) {

	skynet_harbor_start(ctx);

	return true;
}

void harbor_release(struct skynet_service * ctx) {

}

bool harbor_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, const void * msg, size_t sz) {

}
