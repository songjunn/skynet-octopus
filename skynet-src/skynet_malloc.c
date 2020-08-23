#include "skynet.h"
#include "skynet_malloc.h"
#include "skynet_timer.h"
#include "skynet_rwlock.h"

#include <stdio.h>

struct memory_node {
	void * ptr;
	size_t size;
	char * file;
	int line;
	uint64_t timestamp;
	struct memory_node *next;
};

struct memory_hash {
	int cap;
	struct rwlock lock;
	struct memory_node **node;
};

static struct memory_hash *HM = 0;

void
skynet_malloc_init() {
	int hashcap, i;
	struct memory_hash *hm;

	hashcap = 1023;
	hm = skynet_malloc(sizeof(struct memory_hash));
	hm->node = skynet_malloc(hashcap * sizeof(struct memory_node *));
	memset(hm->node, 0, hashcap * sizeof(struct memory_node *));
	hm->cap = hashcap;
	rwlock_init(&hm->lock);
	HM = hm;
}

void
skynet_malloc_free() {
	struct memory_hash *hm = HM;
	struct memory_node *c, *t;
	int i;

	for (i=0;i<hm->cap;i++) {
		c = hm->node[i];
		while (c && c->ptr) {
			t = c;
			c = c->next;
			skynet_free(t->file);
			skynet_free(t);
		}
	}
	skynet_free(hm->node);
	skynet_free(hm);
}

void
skynet_malloc_insert(void *ptr, size_t sz, const char * file, int line) {
	struct memory_hash *hm = HM;
	struct memory_node *c, *n;

	size_t fsz = strlen(file)+1;
	n = skynet_malloc(sizeof(struct memory_node));
	n->ptr = ptr;
	n->size = sz;
	n->file = skynet_malloc(fsz);
	memcpy(n->file, file, fsz);
	n->line = line;
	n->timestamp = 0;
	n->next = NULL;

	rwlock_wlock(&hm->lock);	
	int index = (unsigned int)ptr % hm->cap;
	c = hm->node[index];
	if (c) {
		while (c->next) {
			c = c->next;
		}
		c->next = n;
	} else {
		hm->node[index] = n;
	}
	rwlock_wunlock(&hm->lock);
}

void
skynet_malloc_remove(void *ptr) {
	if (ptr == NULL) return;

	struct memory_hash *hm = HM;
	struct memory_node *c, *n;

	rwlock_wlock(&hm->lock);
	int index = (unsigned int)ptr % hm->cap;
	c = hm->node[index];
	assert(c);
	n = c;

	while(c->ptr != ptr) {
		n = c;
		c = c->next;
	}
	if (n == c) {
		hm->node[index] = c->next;
	} else {
		n->next = c->next;
	}
	rwlock_wunlock(&hm->lock);
	
	skynet_free(c->file);
	skynet_free(c);
}

void 
skynet_malloc_print() {
	struct memory_hash *hm = HM;
	struct memory_node *c;
	int i;

	rwlock_rlock(&hm->lock);
	for (i=0;i<hm->cap;i++) {
		c = hm->node[i];
		while (c && c->ptr) {
			fprintf(stdout, "hashmemory %s:%d %x %d %lld\n", c->file, c->line, c->ptr, c->size, c->timestamp);
			c = c->next;
		}
	}
	rwlock_runlock(&hm->lock);
}

void * 
skynet_malloc(size_t sz) {
	return malloc(sz);
}

void * 
skynet_realloc(void *ptr, size_t sz) {
	return realloc(ptr, sz);
}

void 
skynet_free(void *ptr) {
	if (ptr) free(ptr);
}
