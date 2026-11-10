#ifndef GW_QUEUE_H
#define GW_QUEUE_H
#include "gw_msg.h"

#define GW_QUEUE_LEN 16

int      gw_queue_push(const gw_msg_t *m);   /* safe from ISR; 0 if full */
int      gw_queue_pop(gw_msg_t *m);          /* 1 if a message was returned */
uint32_t gw_queue_dropped(void);

#endif
