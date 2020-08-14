#include "skynet.h"
#include "skynet_alloc.h"

#include <stddef.h>

void *
skynet_malloc(size_t size) {
	void * ptr = malloc(size);
	skynet_logger_debug("[skynet]Memcheck malloc 0x%x %d", ptr, size);
	return ptr;
}

void *
skynet_realloc(void *ptr, size_t size) {
	return realloc(ptr, size);
}

void
skynet_free(void *ptr) {
	if (ptr == NULL) return;
	free(ptr);
}
