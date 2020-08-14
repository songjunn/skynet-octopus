#ifndef SKYNET_MALLOC_H
#define SKYNET_MALLOC_H

#include <stddef.h>

#define skynet_malloc malloc
#define skynet_calloc calloc
#define skynet_realloc realloc
#define skynet_free free

void * skynet_malloc(size_t sz);
void * skynet_realloc(void *ptr, size_t size);
void skynet_free(void *ptr);

#endif //SKYNET_MALLOC_H
