#ifndef SKYNET_MALLOC_H
#define SKYNET_MALLOC_H

#include "jemalloc.h"

#include <stddef.h>
#include <stdint.h>

#ifdef USE_JEMALLOC
#define skynet_malloc je_malloc
#define skynet_realloc je_realloc
#define skynet_free(ptr) if(ptr) je_free(ptr);
#else
#define skynet_malloc malloc
#define skynet_realloc realloc
#define skynet_free(ptr) if(ptr) free(ptr);
#endif

void skynet_malloc_init();
void skynet_malloc_free();
void skynet_malloc_insert(void *ptr, size_t sz, const char * file, int line);
void skynet_malloc_remove(void *ptr);
void skynet_malloc_print();

#endif //SKYNET_MALLOC_H
