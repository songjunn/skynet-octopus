#ifndef SKYNET_DATABUFFER_H
#define SKYNET_DATABUFFER_H

#include "skynet.h"

struct databuffer {
	int sz;
	int ptr;
	char * chunk;
};

struct databuffer * databuffer_create(int sz) {
	struct databuffer * buffer = skynet_malloc(sizeof(struct databuffer));
	buffer->sz = sz;
	buffer->ptr = 0;
	buffer->chunk = skynet_malloc(sizeof(char) * sz);
	return buffer;
}

void databuffer_free(struct databuffer * buffer) {
	skynet_free(buffer->chunk);
	skynet_free(buffer);
}

int databuffer_push(struct databuffer * buffer, void * data, int sz) {
	if (buffer->ptr + sz > buffer->sz) {
		return -1;
	}

	memcpy(buffer->chunk + buffer->ptr, data, sz);
	buffer->ptr += sz;
	return sz;
}

char * databuffer_read(struct databuffer * buffer, int ptr) {
	if (buffer->ptr <= ptr) {
		return NULL;
	}
	return buffer->chunk + ptr;
}

int databuffer_pop(struct databuffer * buffer, int sz) {
	assert(buffer->ptr >= sz);
	buffer->ptr -= sz;

	//may overlap
	//memcpy(buffer->chunk, buffer->chunk + sz, buffer->ptr);
	int c = 0;
	while (c < buffer->ptr) {
		*(buffer->chunk + c) = *(buffer->chunk + sz + c);
		++c;
	}
	return buffer->ptr;
}

int databuffer_isfull(struct databuffer * buffer) {
	return buffer->ptr >= buffer->sz;
}

void databuffer_reset(struct databuffer * buffer) {
	buffer->ptr = 0;
}

//for default package
#define PACKHEAD(buffer) (sizeof(int))

int databuffer_readpack(struct databuffer * buffer, char ** data) {
	int hsz = PACKHEAD(buffer);
	if (buffer->ptr < hsz) {
		return 0;
	}
	int sz = *((int *) buffer->chunk);
	if (sz > buffer->ptr - hsz) {
		return 0;
	}
	*data = databuffer_read(buffer, hsz);
	return sz;
}

void databuffer_freepack(struct databuffer * buffer) {
	int sz = *((int *) buffer->chunk);
    databuffer_pop(buffer, sz + PACKHEAD(buffer));
}

#endif //SKYNET_DATABUFFER_H
