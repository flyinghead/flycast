/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 *********************************************************************/

#ifndef PICO_SUPPORT_POSIX
#define PICO_SUPPORT_POSIX

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

/*
   #define MEMORY_MEASURE
   #define TIME_PRESCALE
   #define PICO_SUPPORT_THREADING
 */
#define dbg printf

#define stack_fill_pattern(...) do {} while(0)
#define stack_count_free_words(...) do {} while(0)
#define stack_get_free_words() (0)

/* measure allocated memory */
#ifdef MEMORY_MEASURE
extern uint32_t max_mem;
extern uint32_t cur_mem;

static inline void *pico_zalloc(int x)
{
    uint32_t *ptr;
    if ((cur_mem + x) > (10 * 1024))
        return NULL;

    ptr = (uint32_t *)calloc(x + 4, 1);
    *ptr = (uint32_t)x;
    cur_mem += x;
    if (cur_mem > max_mem) {
        max_mem = cur_mem;
    }

    return (void*)(ptr + 1);
}

static inline void pico_free(void *x)
{
    uint32_t *ptr = (uint32_t*)(((uint8_t *)x) - 4);
    cur_mem -= *ptr;
    free(ptr);
}
#else
#define pico_zalloc(x) calloc(x, 1)
#define pico_free(x) free(x)
#endif

/* time prescaler */
#ifdef TIME_PRESCALE
extern int32_t prescale_time;
#endif

#if defined(PICO_SUPPORT_RTOS) || defined (PICO_SUPPORT_PTHREAD)
/* pico_ms_tick must be defined */
extern volatile uint32_t pico_ms_tick;

static inline uint32_t PICO_TIME(void)
{
    #ifdef TIME_PRESCALE
        return (pico_ms_tick / 1000) << prescale_time;
    #else
        return (pico_ms_tick / 1000);
    #endif
}

static inline uint32_t PICO_TIME_MS(void)
{
    #ifdef TIME_PRESCALE
        return pico_ms_tick << prescale_time;
    #else
        return pico_ms_tick;
    #endif
}

#else

static inline uint32_t PICO_TIME(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
  #ifdef TIME_PRESCALE
    return (prescale_time < 0) ? (uint32_t)(t.tv_sec / 1000 << (-prescale_time)) : \
           (uint32_t)(t.tv_sec / 1000 >> prescale_time);
  #else
    return (uint32_t)t.tv_sec;
  #endif
}

static inline uint32_t PICO_TIME_MS(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
  #ifdef TIME_PRESCALER
    uint32_t tmp = ((t.tv_sec * 1000) + (t.tv_usec / 1000));
    return (prescale_time < 0) ? (uint32_t)(tmp / 1000 << (-prescale_time)) : \
           (uint32_t)(tmp / 1000 >> prescale_time);
  #else
    return (uint32_t)((t.tv_sec * 1000) + (t.tv_usec / 1000));
  #endif
}
#endif

#ifdef PICO_SUPPORT_THREADING
#define PICO_SUPPORT_MUTEX
/* mutex implementations */
extern void *pico_mutex_init(void);
extern void pico_mutex_lock(void *mux);
extern void pico_mutex_unlock(void *mux);

/* semaphore implementations (only used in wrapper code) */
extern void *pico_sem_init(void);
extern void pico_sem_destroy(void *sem);
extern void pico_sem_post(void *sem);
/* returns -1 on timeout (in ms), else returns 0 */
/* if timeout < 0, the semaphore waits forever */
extern int pico_sem_wait(void *sem, int timeout);

/* thread implementations */
extern void *pico_thread_create(void *(*routine)(void *), void *arg);
#endif  /* PICO_SUPPORT_THREADING */

static inline void PICO_IDLE(void)
{
    usleep(5000);
}

#endif  /* PICO_SUPPORT_POSIX */

