#include <aic_portable.h>
#include <timer.h>
#include "app_config.h"

#if TCFG_CAT1_AICXTEK_ENABLE

extern void udelay(u32 us);

void aic_udelay(u32 usec)
{
    udelay(usec);
}
void aic_msleep(u32 time_ms)
{
    udelay(time_ms * 1000);
}

#endif /* #if TCFG_CAT1_AICXTEK_ENABLE */
