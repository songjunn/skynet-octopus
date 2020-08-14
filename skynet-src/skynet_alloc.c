#include "skynet.h"
#include "skynet_alloc.h"

void *
skynet_malloc(size_t size) {
	void * ptr = malloc(size);
	//fprintf(stderr, "[skynet]Memcheck malloc 0x%x %d", ptr, size);
	return ptr;
}

void *
skynet_realloc(void *ptr, size_t size) {
	return realloc(ptr, size);
}

void
skynet_free(void *ptr) {
	if (ptr == NULL) return;
	//fprintf(stderr, "[skynet]Memcheck free 0x%x", ptr);
	free(ptr);
}

/*#define skynet_malloc(sz) \
	malloc(sz); \
	fprintf(stdout, "[skynet]Memcheck malloc 0x%x %d", ptr, size);*/
