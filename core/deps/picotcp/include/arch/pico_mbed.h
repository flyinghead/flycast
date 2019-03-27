/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   File: pico_mbed.h
   Author: Toon Peters
 *********************************************************************/

#ifndef PICO_SUPPORT_MBED
#define PICO_SUPPORT_MBED
#include <stdio.h>
#include <pico_queue.h>
/* #include "mbed.h" */
/* #include "serial_api.h" */

/* #define TIME_PRESCALE */
/* #define PICO_MEASURE_STACK */
/* #define MEMORY_MEASURE */
/*
   Debug needs initialization:
 * void serial_init       (serial_t *obj, PinName tx, PinName rx);
 * void serial_baud       (serial_t *obj, int baudrate);
 * void serial_format     (serial_t *obj, int data_bits, SerialParity parity, int stop_bits);
 */

#define dbg(...)

/*
   #define MEMORY_MEASURE
   #define JENKINS_DEBUG
 */

/* Intended for Mr. Jenkins endurance test loggings */
#ifdef JENKINS_DEBUG
#include "PicoTerm.h"
#define jenkins_dbg ptm_dbg
#endif

#ifdef PICO_MEASURE_STACK

extern int freeStack;
#define STACK_TOTAL_WORDS   1000u
#define STACK_PATTERN       (0xC0CAC01Au)

void stack_fill_pattern(void *ptr);
void stack_count_free_words(void *ptr);
int stack_get_free_words(void);
#else
#define stack_fill_pattern(...) do {} while(0)
#define stack_count_free_words(...) do {} while(0)
#define stack_get_free_words() (0)
#endif

#ifdef MEMORY_MEASURE /* in case, comment out the two defines above me. */
extern uint32_t max_mem;
extern uint32_t cur_mem;

struct mem_chunk_stats {
#ifdef MEMORY_MEASURE_ADV
    uint32_t signature;
    void *mem;
#endif
    uint32_t size;
};

static inline void *pico_zalloc(int x)
{
    struct mem_chunk_stats *stats;
    if ((cur_mem + x) > (10 * 1024))
        return NULL;

    stats = (struct mem_chunk_stats *)calloc(x + sizeof(struct mem_chunk_stats), 1);
#ifdef MEMORY_MEASURE_ADV
    stats->signature = 0xdeadbeef;
    stats->mem = ((uint8_t *)stats) + sizeof(struct mem_chunk_stats);
#endif
    stats->size = x;

    /* Intended for Mr. Jenkins endurance test loggings */
    #ifdef JENKINS_DEBUG
    if (!stats) {
        jenkins_dbg(">> OUT OF MEM\n");
        while(1) ;
        ;
    }

    #endif
    cur_mem += x;
    if (cur_mem > max_mem) {
        max_mem = cur_mem;
        /*      printf("max mem: %lu\n", max_mem); */
    }

#ifdef MEMORY_MEASURE_ADV
    return (void*)(stats->mem);
#else
    return (void*) (((uint8_t *)stats) + sizeof(struct mem_chunk_stats));
#endif
}

static inline void pico_free(void *x)
{
    struct mem_chunk_stats *stats = (struct mem_chunk_stats *) ((uint8_t *)x - sizeof(struct mem_chunk_stats));

    #ifdef JENKINS_DEBUG
    #ifdef MEMORY_MEASURE_ADV
    if ((stats->signature != 0xdeadbeef) || (x != stats->mem)) {
        jenkins_dbg(">> FREE ERROR: caller is %p\n", __builtin_return_address(0));
        while(1) ;
        ;
    }

    #endif

    #endif

    cur_mem -= stats->size;
    memset(stats, 0, sizeof(struct mem_chunk_stats));
    free(stats);
}
#else

#define pico_zalloc(x) calloc(x, 1)
#define pico_free(x) free(x)

#endif

#define PICO_SUPPORT_MUTEX
extern void *pico_mutex_init(void);
extern void pico_mutex_lock(void*);
extern void pico_mutex_unlock(void*);
extern void pico_mutex_deinit(void*);

extern uint32_t os_time;
extern pico_time local_time;
extern uint32_t last_os_time;

#ifdef TIME_PRESCALE
extern int32_t prescale_time;
#endif

#define UPDATE_LOCAL_TIME() do {local_time = local_time + ((pico_time)os_time - (pico_time)last_os_time);last_os_time = os_time;} while(0)

static inline pico_time PICO_TIME(void)
{
    UPDATE_LOCAL_TIME();
  #ifdef TIME_PRESCALE
    return (prescale_time < 0) ? (pico_time)(local_time / 1000 << (-prescale_time)) : \
           (pico_time)(local_time / 1000 >> prescale_time);
  #else
    return (pico_time)(local_time / 1000);
  #endif
}

static inline pico_time PICO_TIME_MS(void)
{
    UPDATE_LOCAL_TIME();
  #ifdef TIME_PRESCALE
    return (prescale_time < 0) ? (pico_time)(local_time << (-prescale_time)) : \
           (pico_time)(local_time >> prescale_time);
  #else
    return (pico_time)local_time;
  #endif
}

static inline void PICO_IDLE(void)
{
    /* TODO needs implementation */
}
/*
   static inline void PICO_DEBUG(const char * formatter, ... )
   {
   char buffer[256];
   char *ptr;
   va_list args;
   va_start(args, formatter);
   vsnprintf(buffer, 256, formatter, args);
   ptr = buffer;
   while(*ptr != '\0')
    serial_putc(serial_t *obj, (int) (*(ptr++)));
   va_end(args);
   //TODO implement serial_t
   }*/

#endif
