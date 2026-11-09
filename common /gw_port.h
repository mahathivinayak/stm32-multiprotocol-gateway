#ifndef GW_PORT_H
#define GW_PORT_H
#include <stdint.h>
#include "gw_msg.h"

/* Hardware abstraction: the portable core (queue, scheduler, serializer, app)
   only calls these. Each board provides its own implementation:
   boards/f103-bluepill -> gw_port_stm32f1.c, boards/f407-discovery -> gw_port_stm32f4.c */
void     gw_port_init(void);
uint32_t gw_port_millis(void);
void     gw_port_enter_critical(void);
void     gw_port_exit_critical(void);

void     gw_port_uplink_write(const char *buf, uint32_t len);  /* to PC -> MQTT       */
void     gw_port_peer_write(const char *buf, uint32_t len);    /* to Raspberry Pi     */
void     gw_port_log_append(const gw_msg_t *m);                /* SPI flash black box */

/* Periodic tasks, run by gw_sched at periods the Pi can change */
void gw_port_task_temp(void);
void gw_port_task_flash_status(void);
void gw_port_task_can_heartbeat(void);

#endif
