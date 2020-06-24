#include "skynet.h"
#include "skynet_service.h"
#include "skynet_server.h"
#include "skynet_mq.h"
#include "skynet_harbor.h"
#include "skynet_atomic.h"
#include "skynet_rwlock.h"

#include <dlfcn.h>
#include <stdio.h>

#define DEFAULT_SLOT_SIZE 4
#define INDEX_MASK 0xffffff

struct service_name {
	char * name;
	uint32_t handle;
};

struct service_storage {
	const char * path;
	struct rwlock lock;

	int slot_size;
	int slot_index;
	struct skynet_service ** slot;

	int name_cap;
	int name_count;
	struct service_name * name;
};

static struct service_storage * M = NULL;

int _load(struct skynet_service * service, const char * path, const char * name) {
	size_t path_size = strlen(path);
	size_t name_size = strlen(name);

	int sz = path_size + name_size + 8;
	char fullpath[sz];
	sprintf(fullpath, "%s/lib%s.so", path, name);

	service->module = dlopen(fullpath, RTLD_NOW | RTLD_GLOBAL);
	if (service->module == NULL) {
		skynet_logger_error(0, "[skynet]try open %s failed : %s", fullpath, dlerror());
		return 1;
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

	return 0;

failed:
	skynet_logger_error(0, "[skynet]%s sym %s failed : %s", fullpath, tmp, dlerror());
	dlclose(service->module);
	return 1;
}

struct skynet_service * _create(int harbor, const char * name, const char * module) {
	int i;
	struct skynet_service * result;
	struct service_storage * m = M;

	rwlock_wlock(&m->lock);

	for (;;) {
		uint32_t index = m->slot_index;
		for (i=0; i<m->slot_size; i++,index++) {
			if (index > INDEX_MASK) {
				// 0 is reserved
				index = 1;
			}
			int hash = index & (m->slot_size-1);
			if (m->slot[hash] == NULL) {
				result = skynet_malloc(sizeof(struct skynet_service));
				result->closing = 0;
				result->name = skynet_strdup(name);
				result->handle = skynet_harbor_handle(harbor, hash);
				_load(result, m->path, module);
				
				m->slot[hash] = result;
				m->slot_index = index + 1;

				rwlock_wunlock(&m->lock);

				return result;
			}
		}
		assert((m->slot_size*2 - 1) <= INDEX_MASK);
		struct skynet_service ** new_slot = skynet_malloc(m->slot_size * 2 * sizeof(struct skynet_service *));
		memset(new_slot, 0, m->slot_size * 2 * sizeof(struct skynet_service *));
		for (i=0; i<m->slot_size; i++) {
			int hash = skynet_service_handle(m->slot[i]) & (m->slot_size * 2 - 1);
			assert(new_slot[hash] == NULL);
			new_slot[hash] = m->slot[i];
		}
		skynet_free(m->slot);
		m->slot = new_slot;
		m->slot_size *= 2;
	}
}

void skynet_service_init(const char *path) {
	struct service_storage * m = skynet_malloc(sizeof(*m));
	m->path = skynet_strdup(path);
	m->slot_size = DEFAULT_SLOT_SIZE;
	m->slot_index = 0;
	m->slot = skynet_malloc(m->slot_size * sizeof(struct skynet_service *));
	memset(m->slot, 0, m->slot_size * sizeof(struct skynet_service *));

	m->name_cap = DEFAULT_SLOT_SIZE/2;
	m->name_count = 0;
	m->name = skynet_malloc(m->name_cap * sizeof(struct service_name));

	rwlock_init(&m->lock);

	M = m;
}

uint32_t skynet_service_handle(struct skynet_service *ctx) {
	return ctx->handle;
}

uint32_t skynet_service_create(const char * name, int harbor, const char * module, const char * args, int concurrent) {
	struct skynet_service * ctx = _create(harbor, name, module);
	if (ctx == NULL) {
		return NULL;
	}

	ctx->queue = skynet_mq_create(ctx->handle, concurrent);

	if (!ctx->create(ctx, harbor, args)) {
		skynet_globalmq_push(ctx->queue);
		skynet_logger_notice(0, "[skynet]create service %s success handle:%d args:%s", name, ctx->handle, args);
		return ctx->handle;
	} else {
		skynet_logger_error(0, "[skynet]create service %s failed args:%s", name, args);
		skynet_service_release(ctx);
		return 0;
	}
}

void skynet_service_close(uint32_t handle) {
	struct skynet_service * ctx = skynet_service_find(handle);
	if (ctx) {
		ctx->closing = 1;
	}
}

void skynet_service_release(struct skynet_service * ctx) {
	skynet_logger_notice(0, "[skynet]release service %s success handle:%d", ctx->name, ctx->handle);

	struct service_storage * m = M;
	int hash = skynet_harbor_index(ctx->handle);
	assert(hash >= 0 && hash < m->slot_size);
	rwlock_wlock(&m->lock);
	m->slot[hash] = NULL;
	rwlock_wunlock(&m->lock);

	ctx->release(ctx);
	if (ctx->module) dlclose(ctx->module);
	if (ctx->name) skynet_free(ctx->name);
	if (ctx->queue) skynet_mq_release(ctx->queue);
}

void skynet_service_releaseall() {
	int i;
	struct service_storage * m = M;

	for (i=m->slot_size-1; i>=0; i--) { // release by desc
		struct skynet_service * ctx = m->slot[i];
		if (ctx && ctx->handle > 0) {
			ctx->closing = 1;
			while (skynet_service_message_dispatch(ctx) == 0);
			skynet_service_release(ctx);
		}
	}
}

static void _insert_name_before(struct service_storage *s, char *name, uint32_t handle, int before) {
	if (s->name_count >= s->name_cap) {
		s->name_cap *= 2;
		assert(s->name_cap <= s->slot_size);
		struct service_name * n = skynet_malloc(s->name_cap * sizeof(struct service_name));
		int i;
		for (i=0;i<before;i++) {
			n[i] = s->name[i];
		}
		for (i=before;i<s->name_count;i++) {
			n[i+1] = s->name[i];
		}
		skynet_free(s->name);
		s->name = n;
	} else {
		int i;
		for (i=s->name_count;i>before;i--) {
			s->name[i] = s->name[i-1];
		}
	}
	s->name[before].name = name;
	s->name[before].handle = handle;
	s->name_count ++;
}

static const char * _insert_name(struct service_storage *s, const char * name, uint32_t handle) {
	int begin = 0;
	int end = s->name_count - 1;
	while (begin<=end) {
		int mid = (begin+end)/2;
		struct service_name *n = &s->name[mid];
		int c = strcmp(n->name, name);
		if (c==0) {
			return NULL;
		}
		if (c<0) {
			begin = mid + 1;
		} else {
			end = mid - 1;
		}
	}
	char * result = skynet_strdup(name);

	_insert_name_before(s, result, handle, begin);

	return result;
}

uint32_t _find_name(struct service_storage *m, const char * name) {
	uint32_t handle = 0;
	
	int begin = 0;
	int end = m->name_count - 1;
	while (begin<=end) {
		int mid = (begin+end)/2;
		struct service_name *n = &m->name[mid];
		int c = strcmp(n->name, name);
		if (c==0) {
			handle = n->handle;
			break;
		}
		if (c<0) {
			begin = mid + 1;
		} else {
			end = mid - 1;
		}
	}

	return handle;
}

const char * skynet_handle_namehandle(uint32_t handle, const char *name) {
	rwlock_wlock(&M->lock);

	const char * ret = _insert_name(M, name, handle);

	rwlock_wunlock(&M->lock);

	return ret;
}

struct skynet_service * skynet_service_find(uint32_t handle) {
	struct service_storage * m = M;
	struct skynet_service * result = NULL;

	rwlock_rlock(&m->lock);

	int hash = skynet_harbor_index(handle);
	if (hash >= 0 && hash < m->slot_size) {
		struct skynet_service * ctx = m->slot[hash];
		if (ctx && ctx->handle == handle && ctx->closing == 0) {
			result = ctx;
		}
	}

	rwlock_runlock(&m->lock);

	return result;
}

struct skynet_service * skynet_service_findname(const char * name) {
	struct service_storage * m = M;
	struct skynet_service * result = NULL;

	rwlock_rlock(&m->lock);

	uint32_t handle = _find_name(m, name);
	int hash = skynet_harbor_index(handle);
	if (hash >= 0 && hash < m->slot_size) {
		struct skynet_service * ctx = m->slot[hash];
		if (ctx && ctx->handle == handle && ctx->closing == 0) {
			result = ctx;
		}
	}

	rwlock_runlock(&m->lock);

	return result;
}

void skynet_service_sendmsg(struct skynet_service * context, struct skynet_message * message) {
	skynet_mq_push(context->queue, message);
}

int skynet_service_pushmsg(uint32_t handle, struct skynet_message * message) {
	struct skynet_service * ctx = skynet_service_find(handle);
	if (ctx == NULL) {
		return -1;
	}
	skynet_mq_push(ctx->queue, message);
	return 0;
}
