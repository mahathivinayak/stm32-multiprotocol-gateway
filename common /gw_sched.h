#ifndef GW_SCHED_H
#define GW_SCHED_H
#include <stddef.h>
#include <stdint.h>

typedef enum { GW_TASK_TEMP = 0, GW_TASK_FLASH, GW_TASK_CAN, GW_TASK_COUNT } gw_task_id_t;
typedef void (*gw_task_fn)(void);

#define GW_SCHED_MIN_MS 100u
#define GW_SCHED_MAX_MS 60000u

void gw_sched_register(gw_task_id_t id, const char *name, gw_task_fn fn, uint32_t period_ms);
void gw_sched_run(uint32_t now_ms);

/* Handles "SCHED <task> <ms>" (0 = disable) and "STATUS". Returns 1 on success. */
int  gw_sched_command(const char *cmd, char *reply, size_t cap);

#endif
