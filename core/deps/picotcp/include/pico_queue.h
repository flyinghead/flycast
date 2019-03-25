/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_QUEUE
#define INCLUDE_PICO_QUEUE
#include "pico_config.h"
#include "pico_frame.h"

#define Q_LIMIT 0

#ifndef NULL
#define NULL ((void *)0)
#endif

void *pico_mutex_init(void);
void pico_mutex_deinit(void *mutex);
void pico_mutex_lock(void *mutex);
int pico_mutex_lock_timeout(void *mutex, int timeout);
void pico_mutex_unlock(void *mutex);
void pico_mutex_unlock_ISR(void *mutex);

struct pico_queue {
    uint32_t frames;
    uint32_t size;
    uint32_t max_frames;
    uint32_t max_size;
    struct pico_frame *head;
    struct pico_frame *tail;
#ifdef PICO_SUPPORT_MUTEX
    void *mutex;
#endif
    uint8_t shared;
    uint16_t overhead;
};

#ifdef PICO_SUPPORT_MUTEX
#define PICOTCP_MUTEX_LOCK(x) { \
        if (x == NULL) \
            x = pico_mutex_init(); \
        pico_mutex_lock(x); \
}
#define PICOTCP_MUTEX_UNLOCK(x) pico_mutex_unlock(x)
#define PICOTCP_MUTEX_DEL(x) pico_mutex_deinit(x)

#else
#define PICOTCP_MUTEX_LOCK(x) do {} while(0)
#define PICOTCP_MUTEX_UNLOCK(x) do {} while(0)
#define PICOTCP_MUTEX_DEL(x) do {} while(0)
#endif

#ifdef PICO_SUPPORT_DEBUG_TOOLS
static void debug_q(struct pico_queue *q)
{
    struct pico_frame *p = q->head;
    dbg("%d: ", q->frames);
    while(p) {
        dbg("(%p)-->", p);
        p = p->next;
    }
    dbg("X\n");
}

#else

#define debug_q(x) do {} while(0)
#endif

static inline int32_t pico_enqueue(struct pico_queue *q, struct pico_frame *p)
{
    if ((q->max_frames) && (q->max_frames <= q->frames))
        return -1;

#if (Q_LIMIT != 0)
    if ((Q_LIMIT < p->buffer_len + q->size))
        return -1;

#endif

    if ((q->max_size) && (q->max_size < (p->buffer_len + q->size)))
        return -1;

    if (q->shared)
        PICOTCP_MUTEX_LOCK(q->mutex);

    p->next = NULL;
    if (!q->head) {
        q->head = p;
        q->tail = p;
        q->size = 0;
        q->frames = 0;
    } else {
        q->tail->next = p;
        q->tail = p;
    }

    q->size += p->buffer_len + q->overhead;
    q->frames++;
    debug_q(q);

    if (q->shared)
        PICOTCP_MUTEX_UNLOCK(q->mutex);

    return (int32_t)q->size;
}

static inline struct pico_frame *pico_dequeue(struct pico_queue *q)
{
    struct pico_frame *p = q->head;
    if (!p)
        return NULL;

    if (q->frames < 1)
        return NULL;

    if (q->shared)
        PICOTCP_MUTEX_LOCK(q->mutex);

    q->head = p->next;
    q->frames--;
    q->size -= p->buffer_len - q->overhead;
    if (q->head == NULL)
        q->tail = NULL;

    debug_q(q);

    p->next = NULL;
    if (q->shared)
        PICOTCP_MUTEX_UNLOCK(q->mutex);

    return p;
}

static inline struct pico_frame *pico_queue_peek(struct pico_queue *q)
{
    struct pico_frame *p = q->head;
    if (q->frames < 1)
        return NULL;

    debug_q(q);
    return p;
}

static inline void pico_queue_deinit(struct pico_queue *q)
{
    if (q->shared) {
        PICOTCP_MUTEX_DEL(q->mutex);
    }
}

static inline void pico_queue_empty(struct pico_queue *q)
{
    struct pico_frame *p = pico_dequeue(q);
    while(p) {
        pico_frame_discard(p);
        p = pico_dequeue(q);
    }
}

static inline void pico_queue_protect(struct pico_queue *q)
{
    q->shared = 1;
}

#endif
