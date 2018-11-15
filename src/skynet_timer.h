#ifndef SKYNET_TIMER_H
#define SKYNET_TIMER_H

void skynet_timer_init(void);
void skynet_updatetime(void);

uint64_t skynet_timer_now(void);
void skynet_timer_register(uint32_t handle, const void * args, size_t size, int time);

#endif
