#ifndef SKYNET_MALLOC_H
#define SKYNET_MALLOC_H

#include <stddef.h>
#include <stdint.h>

void * skynet_malloc(size_t sz, const char * file, size_t line);
void * skynet_realloc(void *ptr, size_t sz, const char * file, size_t line);
void skynet_free(void *ptr);

void skynet_malloc_init();
void skynet_malloc_free();
void skynet_malloc_insert(void *ptr, size_t sz, const char * file, int line);
void skynet_malloc_remove(void *ptr);
void skynet_malloc_print();

#endif //SKYNET_MALLOC_H
