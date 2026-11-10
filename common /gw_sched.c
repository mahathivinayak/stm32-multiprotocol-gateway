#include <stdio.h>
#include <string.h>
#include "gw_sched.h"
#include "gw_queue.h"

typedef struct {
    const char *name;
    gw_task_fn  fn;
    uint32_t    period_ms;   /* 0 = disabled */
    uint32_t    last_ms;
    uint32_t    runs;
} task_t;

static task_t tasks[GW_TASK_COUNT];

void gw_sched_register(gw_task_id_t id, const char *name, gw_task_fn fn, uint32_t period_ms)
{
    if (id >= GW_TASK_COUNT) return;
    tasks[id].name      = name;
    tasks[id].fn        = fn;
    tasks[id].period_ms = period_ms;
    tasks[id].last_ms   = 0;
    tasks[id].runs      = 0;
}

void gw_sched_run(uint32_t now_ms)
{
    for (int i = 0; i < GW_TASK_COUNT; i++) {
        task_t *t = &tasks[i];
        /* unsigned subtraction stays correct when the ms tick wraps */
        if (t->fn && t->period_ms && (uint32_t)(now_ms - t->last_ms) >= t->period_ms) {
            t->last_ms = now_ms;
            t->runs++;
            t->fn();
        }
    }
}

static task_t *find_task(const char *name)
{
    for (int i = 0; i < GW_TASK_COUNT; i++)
        if (tasks[i].name && strcmp(tasks[i].name, name) == 0) return &tasks[i];
    return NULL;
}

int gw_sched_command(const char *cmd, char *reply, size_t cap)
{
    char name[12];
    unsigned long ms;

    if (sscanf(cmd, "SCHED %11s %lu", name, &ms) == 2) {
        task_t *t = find_task(name);
        if (!t) {
            snprintf(reply, cap, "ERR no task %s", name);
            return 0;
        }
        if (ms != 0 && (ms < GW_SCHED_MIN_MS || ms > GW_SCHED_MAX_MS)) {
            snprintf(reply, cap, "ERR period %lu out of range", ms);
            return 0;
        }
        t->period_ms = (uint32_t)ms;
        snprintf(reply, cap, "OK %s %lu", name, ms);
        return 1;
    }

    if (strncmp(cmd, "STATUS", 6) == 0) {
        size_t n = 0;
        for (int i = 0; i < GW_TASK_COUNT && n < cap; i++) {
            if (!tasks[i].name) continue;
            int w = snprintf(reply + n, cap - n, "%s=%lu ",
                             tasks[i].name, (unsigned long)tasks[i].period_ms);
            if (w < 0) break;
            n += (size_t)w;
        }
        if (n < cap) snprintf(reply + n, cap - n, "drop=%lu", (unsigned long)gw_queue_dropped());
        return 1;
    }

    snprintf(reply, cap, "ERR unknown command");
    return 0;
}
