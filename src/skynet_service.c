#include "skynet.h"
#include "skynet_service.h"
#include "skynet_server.h"
#include "skynet_mq.h"
#include "skynet_atomic.h"
#include "skynet_spinlock.h"

#include <dlfcn.h>
#include <stdio.h>

#define MAX_MODULE_TYPE 32

struct services {
	int count;
	const char * path;
	struct spinlock lock;
	struct skynet_service m[MAX_MODULE_TYPE];
};

static struct services * M = NULL;

bool _open(struct skynet_service * service, const char * path, const char * name) {
	size_t path_size = strlen(path);
	size_t name_size = strlen(name);

	int sz = path_size + name_size + 8;
	char fullpath[sz];
	sprintf(fullpath, "%s/lib%s.so", path, name);

	service->module = dlopen(fullpath, RTLD_NOW | RTLD_GLOBAL);
	if (service->module == NULL) {
		skynet_logger_error(NULL, "try open %s failed : %s", fullpath, dlerror());
		return false;
	}

	char tmp[name_size + 10]; // create/release/callback , longest name is callback (8)
	memcpy(tmp, name, name_size);

	strcpy(tmp+name_size, "_create");
	service->create = (service_dl_create) dlsym(service->module, tmp);
	if (service->create == NULL) {
		goto failed;
	}

	strcpy(tmp+name_size, "_release");
	service->release = (service_dl_release) dlsym(service->module, tmp);
	if (service->release == NULL) {
		goto failed;
	}

	strcpy(tmp+name_size, "_callback");
	service->cb = (service_dl_callback) dlsym(service->module, tmp);
	if (service->cb == NULL) {
		goto failed;
	}

	return true;

failed:
	skynet_logger_error(NULL, "%s sym %s failed : %s", fullpath, tmp, dlerror());
	dlclose(service->module);
	return false;
}

struct skynet_service * _query(const char * name) {
	int i;
	for (i=0;i<M->count;i++) {
		if (strcmp(M->m[i].name,name)==0) {
			return &M->m[i];
		}
	}
	return NULL;
}

uint32_t _handle(int index) {
	return index & 0xffff;
}

struct skynet_service * _load(const char * name, const char * module) {
	struct skynet_service * result = _query(name);
	if (result)
		return result;

	SPIN_LOCK(M)

	result = _query(name); // double check

	if (result == NULL && M->count < MAX_MODULE_TYPE) {
		int index = M->count;
		if (_open(&M->m[index], M->path, module)) {
			M->m[index].name = skynet_strdup(name);
			M->m[index].handle = _handle(index);
			M->count ++;
			result = &M->m[index];
		}
	}

	SPIN_UNLOCK(M)

	return result;
}

struct skynet_service * skynet_service_create(const char * name, int harbor, const char * module, const char * args, int concurrent) {
	struct skynet_service * ctx = _load(name, module);
	if (ctx == NULL)
		return NULL;

	ctx->ref = 1;
	ctx->queue = skynet_mq_create(ctx->handle, concurrent);

	if (ctx->create(ctx, harbor, args)) {
		skynet_globalmq_push(ctx->queue);
		skynet_logger_notice(NULL, "create service %s success handle:%d args:%s", name, ctx->handle, args);
		return ctx;
	} else {
		skynet_logger_error(NULL, "create service %s failed args:%s", name, args);
		skynet_service_release(ctx);
		return NULL;
	}
}

struct skynet_service * skynet_service_insert(struct skynet_service * ctx, int harbor, const char * args, int concurrent) {
	SPIN_LOCK(M)
	int index = M->count;
	if (index >= MAX_MODULE_TYPE) {
		SPIN_UNLOCK(M)
		skynet_logger_error(NULL, "create service %s failed, becasue of services's count %d", ctx->name, index);
		return NULL;
	}

	M->m[index].name = skynet_strdup(ctx->name);
	M->m[index].handle = _handle(index);
	M->m[index].create = ctx->create;
	M->m[index].release = ctx->release;
	M->m[index].cb = ctx->cb;
	M->count ++;
	SPIN_UNLOCK(M)

	ctx = &M->m[index];
	ctx->ref = 1;
	ctx->queue = skynet_mq_create(ctx->handle, concurrent);

	if (ctx->create(ctx, harbor, args)) {
		skynet_globalmq_push(ctx->queue);
		skynet_logger_notice(NULL, "create service %s success handle:%d args:%s", ctx->name, ctx->handle, args);
		return ctx;
	} else {
		skynet_service_release(ctx);
		skynet_logger_error(NULL, "create service %s failed args:%s", ctx->name, args);
		return NULL;
	}
}

void skynet_service_release(struct skynet_service * ctx) {
	if (ATOM_DEC(&ctx->ref) == 0) {
		skynet_logger_notice(NULL, "release service %s success handle:%d", ctx->name, ctx->handle);
		ctx->release(ctx);
		if (ctx->module) dlclose(ctx->module);
		if (ctx->name) skynet_free(ctx->name);
		if (ctx->queue) skynet_mq_release(ctx->queue);
	}
}

void skynet_service_releaseall() {
	int i;
	for (i=M->count-1; i>=0; i--) { // release by desc, except logger service
		struct skynet_service * ctx = &M->m[i];
		while (skynet_service_message_dispatch(ctx) == 0);
		skynet_service_release(ctx);
	}
}

struct skynet_service * skynet_service_grab(uint32_t handle) {
	struct skynet_service * result = NULL;

	SPIN_LOCK(M)

	if (handle >= 0 && handle < (uint32_t)M->count) {
		result = &M->m[handle];
	}

	SPIN_UNLOCK(M)

	return result;
}

struct skynet_service * skynet_service_query(const char * name) {
	return _query(name);
}

uint32_t skynet_service_handle(struct skynet_service *ctx) {
	return ctx->handle;
}

void skynet_service_init(const char *path) {
	struct services * m = skynet_malloc(sizeof(*m));
	m->count = 0;
	m->path = skynet_strdup(path);

	SPIN_INIT(m)

	M = m;
}

void skynet_service_sendmsg(struct skynet_service * context, struct skynet_message * message) {
	skynet_mq_push(context->queue, message);
}

int skynet_service_pushmsg(uint32_t handle, struct skynet_message * message) {
	struct skynet_service * ctx = skynet_service_grab(handle);
	if (ctx == NULL) {
		return -1;
	}
	skynet_mq_push(ctx->queue, message);
	return 0;
}
