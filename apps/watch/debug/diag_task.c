#include "system/includes.h"

static void diag_task(void *p)
{
    while (1) {
        printf("[DIAG] heartbeat\n");
        os_time_dly(100); // 100 ticks; adjust to ~1s depending on tick config
    }
}

void diag_task_start(void)
{
    os_task_create(diag_task, NULL, 3, 1024, 32, "diag_task");
}
