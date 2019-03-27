/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef _INCLUDE_PICO_GCC
#define _INCLUDE_PICO_GCC

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pico_constants.h"

/* #define TIME_PRESCALE */

/* monotonically increasing tick,
 * typically incremented every millisecond in a systick interrupt */
extern volatile unsigned int pico_ms_tick;

#define dbg(...)

#ifdef PICO_SUPPORT_PTHREAD
    #define PICO_SUPPORT_MUTEX
#endif

#ifdef PICO_SUPPORT_RTOS
    #define PICO_SUPPORT_MUTEX

extern void *pico_mutex_init(void);
extern void pico_mutex_lock(void*);
extern void pico_mutex_unlock(void*);
extern void *pvPortMalloc( size_t xSize );
extern void vPortFree( void *pv );

    #define pico_free(x) vPortFree(x)
    #define free(x)      vPortFree(x)

static inline void *pico_zalloc(size_t size)
{
    void *ptr = pvPortMalloc(size);

    if(ptr)
        memset(ptr, 0u, size);

    return ptr;
}

/* time prescaler */
#ifdef TIME_PRESCALE
extern int32_t prescale_time;
#endif

static inline pico_time PICO_TIME_MS()
{
    #ifdef TIME_PRESCALE
        return pico_ms_tick << prescale_time;
    #else
        return pico_ms_tick;
    #endif
}

static inline pico_time PICO_TIME()
{
    #ifdef TIME_PRESCALE
        return (pico_ms_tick / 1000) << prescale_time;
    #else
        return (pico_ms_tick / 1000);
    #endif
}

static inline void PICO_IDLE(void)
{
    pico_time now = PICO_TIME_MS();
    while(now == PICO_TIME_MS()) ;
}

#else /* NO RTOS SUPPORT */

    #ifdef MEM_MEAS
/* These functions should be implemented elsewhere */
extern void *memmeas_zalloc(size_t size);
extern void memmeas_free(void *);
        #define pico_free(x)    memmeas_free(x)
        #define pico_zalloc(x)  memmeas_zalloc(x)
    #else
/* Use plain C-lib malloc and free */
        #define pico_free(x) free(x)
static inline void *pico_zalloc(size_t size)
{
    void *ptr = malloc(size);
    if(ptr)
        memset(ptr, 0u, size);

    return ptr;
}
    #endif

static inline pico_time PICO_TIME_MS(void)
{
    return (pico_time)pico_ms_tick;
}

static inline pico_time PICO_TIME(void)
{
    return (pico_time)(PICO_TIME_MS() / 1000);
}

static inline void PICO_IDLE(void)
{
    unsigned int now = pico_ms_tick;
    while(now == pico_ms_tick) ;
}

#endif /* IFNDEF RTOS */

#endif  /* PICO_GCC */

