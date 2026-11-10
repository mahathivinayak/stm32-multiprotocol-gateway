#include "gw_queue.h"
#include "gw_port.h"

static gw_msg_t q[GW_QUEUE_LEN];
static volatile uint32_t head, tail, dropped;

int gw_queue_push(const gw_msg_t *m)
{
    int ok = 0;
    gw_port_enter_critical();            /* producers: UART ISR, CAN ISR, main loop */
    uint32_t next = (head + 1u) % GW_QUEUE_LEN;
    if (next != tail) {
        q[head] = *m;
        head = next;
        ok = 1;
    } else {
        dropped++;
    }
    gw_port_exit_critical();
    return ok;
}

int gw_queue_pop(gw_msg_t *m)
{
    int ok = 0;
    gw_port_enter_critical();
    if (tail != head) {
        *m = q[tail];
        tail = (tail + 1u) % GW_QUEUE_LEN;
        ok = 1;
    }
    gw_port_exit_critical();
    return ok;
}

uint32_t gw_queue_dropped(void) { return dropped; }
