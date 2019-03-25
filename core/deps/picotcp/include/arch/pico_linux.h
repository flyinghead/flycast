#ifndef PICO_SUPPORT_LINUX
#define PICO_SUPPORT_LINUX

#include "linux/types.h"
#include "linux/mm.h"
#include "linux/slab.h"
#include "linux/jiffies.h"

#define dbg printk

#define pico_zalloc(x) kcalloc(x, 1, GFP_ATOMIC) /* All allocations are GFP_ATOMIC for now */
#define pico_free(x) kfree(x)


static inline unsigned long PICO_TIME(void)
{
    return (unsigned long)(jiffies_to_msecs(jiffies) / 1000);
}

static inline unsigned long PICO_TIME_MS(void)
{
    return (unsigned long)jiffies_to_msecs(jiffies);
}

static inline void PICO_IDLE(void)
{
    unsigned long now = jiffies;
    while (now == jiffies) {
        ;
    }
}

#endif
