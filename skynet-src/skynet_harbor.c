#include "skynet.h"
#include "skynet_harbor.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

static int HARBOR_ID = ~0;
static struct skynet_service * HARBOR = 0;

void skynet_harbor_init(int harbor) {
	HARBOR_ID = harbor;
}

void skynet_harbor_start(struct skynet_service * ctx) {
	HARBOR = ctx;
}

void skynet_harbor_exit() {
	HARBOR = NULL;
}

int skynet_harbor_local_id() {
    return HARBOR_ID;
}

int skynet_harbor_id(uint32_t handle) {
    return ((handle & 0xff00) >> 8);
}

int skynet_harbor_index(uint32_t handle) {
    return handle & 0x00ff;
}

uint32_t skynet_harbor_handle(int harbor, int index) {
    return ((harbor << 8) & 0xff00) | (index & 0x00ff);
}

int skynet_harbor_isremote(uint32_t handle) {
	assert(HARBOR_ID != ~0);
	int h = skynet_harbor_id(handle);
	return h != HARBOR_ID && h !=0;
}

void skynet_harbor_sendname(const char * name, uint32_t source, uint32_t session, int type, void * data, size_t size) {
	if (HARBOR != NULL) {
		struct skynet_remote_message rmsg;
		rmsg.source = source;
		rmsg.session = session;
		rmsg.type = type;
		rmsg.size = size;
		rmsg.handle = 0;
		strncpy(rmsg.name, name, 32);
		if (data != NULL) {
	        rmsg.data = skynet_malloc(size);
	        memcpy(rmsg.data, data, size);
	    }

		skynet_send(HARBOR, source, session, SERVICE_REMOTE, &rmsg, sizeof(rmsg));
	}
}

void skynet_harbor_sendhandle(uint32_t target, uint32_t source, uint32_t session, int type, void * data, size_t size) {
	if (HARBOR != NULL && skynet_harbor_isremote(target)) {
		struct skynet_remote_message rmsg;
		rmsg.source = source;
		rmsg.session = session;
		rmsg.type = type;
		rmsg.size = size;
		rmsg.handle = target;
		if (data != NULL) {
	        rmsg.data = skynet_malloc(size);
	        memcpy(rmsg.data, data, size);
	    }

		skynet_send(HARBOR, source, session, SERVICE_REMOTE, &rmsg, sizeof(rmsg));
	}
}

size_t skynet_remote_message_header() {
	return (sizeof(struct skynet_remote_message) - sizeof(void *));
}

size_t skynet_remote_message_push(struct skynet_remote_message * rmsg, void * data, size_t size) {
	if (size >= skynet_remote_message_header() + rmsg->size) {
		memcpy(data, rmsg, skynet_remote_message_header());
		memcpy(data + skynet_remote_message_header(), rmsg->data, rmsg->size);
		skynet_free(rmsg->data);
		return skynet_remote_message_header() + rmsg->size;
	}
	return 0;
}

void skynet_local_message_forward(void * msg, size_t size) {
    struct skynet_remote_message * rmsg = msg;
    assert(skynet_remote_message_header() + rmsg->size == size);

    void * data = msg + skynet_remote_message_header();
    if (rmsg->handle > 0) {
        skynet_sendhandle(rmsg->handle, rmsg->source, rmsg->session, rmsg->type, data, rmsg->size);
    } else {
        skynet_sendname(rmsg->name, rmsg->source, rmsg->session, rmsg->type, data, rmsg->size);
    }
}
