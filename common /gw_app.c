#include <stdio.h>
#include <string.h>
#include "gw_app.h"
#include "gw_port.h"
#include "gw_queue.h"
#include "gw_sched.h"
#include "gw_serialize.h"

/* A line from the Pi is a scheduling command: run it, reply to the Pi,
   and rewrite the message as "<command> => <reply>" for MQTT and the log. */
static void handle_pi_command(gw_msg_t *m)
{
    char cmd[GW_MSG_MAX_DATA + 1];
    char reply[64];

    memcpy(cmd, m->data, m->len);
    cmd[m->len] = '\0';

    gw_sched_command(cmd, reply, sizeof reply);
    gw_port_peer_write(reply, (uint32_t)strlen(reply));
    gw_port_peer_write("\n", 1);

    int n = snprintf((char *)m->data, GW_MSG_MAX_DATA, "%s => %s", cmd, reply);
    if (n < 0) n = 0;
    m->len = (uint8_t)(n >= GW_MSG_MAX_DATA ? GW_MSG_MAX_DATA - 1 : n);
}

void gw_app_init(void)
{
    gw_port_init();
    gw_sched_register(GW_TASK_TEMP,  "temp",  gw_port_task_temp,          1000);
    gw_sched_register(GW_TASK_FLASH, "flash", gw_port_task_flash_status,  5000);
    gw_sched_register(GW_TASK_CAN,   "can",   gw_port_task_can_heartbeat, 1000);
}

void gw_app_loop(void)
{
    static char line[256];
    gw_msg_t msg;

    gw_sched_run(gw_port_millis());

    while (gw_queue_pop(&msg)) {
        if (msg.src == GW_SRC_UART) handle_pi_command(&msg);
        gw_port_log_append(&msg);                           /* black box in SPI flash */
        int n = gw_serialize(&msg, line, sizeof line);
        if (n > 0) gw_port_uplink_write(line, (uint32_t)n); /* to PC -> MQTT */
    }
}
