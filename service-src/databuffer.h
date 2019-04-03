#ifndef SKYNET_DATABUFFER_H
#define SKYNET_DATABUFFER_H

#define BUFFER_SIZE 20480

struct databuffer {
	int size;
	char chunk[BUFFER_SIZE];
};

int databuffer_push(struct databuffer * buffer, void * data, int sz) {
	if (buffer->size + sz > BUFFER_SIZE) {
		return -1;
	}

	memcpy(buffer->chunk + buffer->size, data, sz);
	buffer->size += sz;
	return sz;
}

int databuffer_read(struct databuffer * buffer, void * data, int sz) {
	if (buffer->size < sz) {
		return -1;
	}

	memcpy(data, buffer->chunk, sz);
	memcpy(buffer->chunk, buffer->chunk + buffer->size, buffer->size);
	buffer->size -= sz;
	return sz;
}

int databuffer_pushheader(struct databuffer * buffer, void * data) {
	memcpy(buffer->chunk, data, sizeof(int));
	buffer->size += sizeof(int);
	return buffer->size;
}

int databuffer_readheader(struct databuffer * buffer) {
	if (buffer->size < sizeof(int)) {
		return -1;
	}
	return *((int *) buffer->chunk);
}

void databuffer_reset(struct databuffer * buffer) {
	buffer->size = 0;
}

#endif //SKYNET_DATABUFFER_H
