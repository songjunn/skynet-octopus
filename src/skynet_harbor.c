#include "skynet.h"
#include "skynet_harbor.h"
#include "skynet_service.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

static int HARBOR = ~0;
static struct skynet_service * REMOTE = 0;

void skynet_harbor_init(int harbor) {
	HARBOR = harbor;
}

void skynet_harbor_start(struct skynet_service * ctx) {
	REMOTE = ctx;
}

void skynet_harbor_exit() {
	struct skynet_service * ctx = REMOTE;
	REMOTE = NULL;
	if (ctx) {
		skynet_service_release(ctx);
	}
}

void skynet_harbor_sendname(const char * name, uint32_t source, uint32_t session, int type, void * data, size_t size) {
	struct skynet_remote_message rmsg;
	rmsg.data = data;
	rmsg.size = size;
	sprintf(rmsg.name, "%s", name);
	skynet_send(REMOTE, source, session, type, &rmsg, sizeof(rmsg));
}

void skynet_harbor_sendhandle(uint32_t target, uint32_t source, uint32_t session, int type, void * data, size_t size) {
	if (skynet_harbor_isremote(target)) {
		struct skynet_remote_message rmsg;
		rmsg.handle = target;
		rmsg.data = data;
		rmsg.size = size;
		skynet_send(REMOTE, source, session, type, &rmsg, sizeof(rmsg));
	}
}

int skynet_harbor_id(uint32_t handle) {
    return (handle & 0xff00 >> 16);
}

int skynet_harbor_index(uint32_t handle) {
    return handle & 0x00ff;
}

uint32_t skynet_harbor_handle(int harbor, int index) {
    return (harbor & 0xff00) | (index & 0x00ff);
}

int skynet_harbor_isremote(uint32_t handle) {
	assert(HARBOR != ~0);
	int h = skynet_harbor_id(handle);
	return h != HARBOR && h !=0;
}
