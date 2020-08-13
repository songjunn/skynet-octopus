#ifndef SKYNET_MESSAGE_QUEUE_H
#define SKYNET_MESSAGE_QUEUE_H

#include <stdlib.h>
#include <stdint.h>

struct skynet_message {
	int type;
	void * data;
	size_t size;
	uint32_t source;
	uint32_t session;
};

struct message_queue;

void skynet_globalmq_push(struct message_queue * queue);
struct message_queue * skynet_globalmq_pop(void);

struct message_queue * skynet_mq_create(uint32_t handle, int concurrent);
void skynet_mq_release(struct message_queue *q);

int skynet_mq_inglobal(struct message_queue *);
uint32_t skynet_mq_handle(struct message_queue *);

// 0 for success
int skynet_mq_pop(struct message_queue *q, struct skynet_message *message);
void skynet_mq_push(struct message_queue *q, struct skynet_message *message);

int skynet_mq_length(struct message_queue *q);
int skynet_mq_overload(struct message_queue *q);
int skynet_mq_concurrent(struct message_queue *q);

void skynet_mq_init();

#endif
