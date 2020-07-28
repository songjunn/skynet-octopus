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

int databuffer_read(struct databuffer * buffer, void * data, int bsz) {
	int sz = bsz + sizeof(int);
	if (buffer->ptr < sz) {
		return -1;
	}

	memcpy(data, buffer->chunk + sizeof(int), bsz);
	memcpy(buffer->chunk, buffer->chunk + sz, buffer->ptr - sz);
	buffer->ptr -= sz;
	return bsz;
}

int databuffer_readint(struct databuffer * buffer) {
	if (buffer->ptr < sizeof(int)) {
		return -1;
	}
	return *((int *) buffer->chunk);
}

int databuffer_readpack(struct databuffer * buffer, char ** data) {
	if (buffer->ptr < sizeof(int)) {
		return 0;
	}
	int sz = *((int *) buffer->chunk);
	if (sz < buffer->ptr - sizeof(int)) {
		return 0;
	}
	data = buffer->chunk + sizeof(int);
	return sz;
}

void databuffer_freepack(struct databuffer * buffer, int sz) {
	assert(buffer->ptr >= sz + sizeof(int));
	buffer->ptr -= sz + sizeof(int);
	memcpy(buffer->chunk, buffer->chunk + sz, buffer->ptr);
}

void databuffer_reset(struct databuffer * buffer) {
	buffer->ptr = 0;
}

#endif //SKYNET_DATABUFFER_H
