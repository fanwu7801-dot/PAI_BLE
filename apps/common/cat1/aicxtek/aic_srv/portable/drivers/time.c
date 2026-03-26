#include <aic_portable.h>
#include <aic_time.h>
#include <timer.h>
#include "app_config.h"

#if TCFG_CAT1_AICXTEK_ENABLE

#define MSEC_PER_SEC    (1000)
#define USEC_PER_MSEC   (1000)
#define NSEC_PER_USEC   (1000)
#define NSEC_PER_MSEC   (1000000)
#define NSEC_PER_SEC    (1000000000)

void aic_msec_to_abstime(long msec, struct timespec *abstime)
{
    struct timeval now;
    int ret;

    AIC_ASSERT((msec >= 0) && (abstime != NULL));

    ret = gettimeofday(&now, (struct timezone *)NULL);
    AIC_ASSERT(ret == 0);

    abstime->tv_sec = now.tv_sec + (msec / MSEC_PER_SEC);
    abstime->tv_nsec = (now.tv_usec + (msec % MSEC_PER_SEC) * USEC_PER_MSEC)
                       * NSEC_PER_USEC;
    if (abstime->tv_nsec >= NSEC_PER_SEC) {
        abstime->tv_nsec %= NSEC_PER_SEC;
        abstime->tv_sec += 1;
    }
}

void aic_msec_to_time(long msec, struct timespec *timeout)
{
    AIC_ASSERT((msec >= 0) && (timeout != NULL));

    timeout->tv_sec = msec / MSEC_PER_SEC;
    timeout->tv_nsec = (msec % MSEC_PER_SEC) * NSEC_PER_MSEC;
}

#endif /* #if TCFG_CAT1_AICXTEK_ENABLE */
