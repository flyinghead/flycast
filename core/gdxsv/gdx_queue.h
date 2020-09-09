#pragma once

#include "types.h"

static const int GDX_QUEUE_SIZE = 1024;

enum {
    GDXRPC_TCP_OPEN = 1,
    GDXRPC_TCP_CLOSE = 2,
};

struct gdx_rpc_t {
    u32 request;
    u32 response;

    u32 param1;
    u32 param2;
    u32 param3;
    u32 param4;
    u8 name1[128];
    u8 name2[128];
};

struct gdx_queue {
    u16 head;
    u16 tail;
    u8 buf[GDX_QUEUE_SIZE];
};

void gdx_queue_init(struct gdx_queue *q) {
    q->head = 0;
    q->tail = 0;
}

u32 gdx_queue_size(struct gdx_queue *q) {
    return (q->tail + GDX_QUEUE_SIZE - q->head) % GDX_QUEUE_SIZE;
}

u32 gdx_queue_avail(struct gdx_queue *q) {
    return GDX_QUEUE_SIZE - gdx_queue_size(q) - 1;
}

void gdx_queue_push(struct gdx_queue *q, u8 data) {
    q->buf[q->tail] = data;
    q->tail = (q->tail + 1) % GDX_QUEUE_SIZE;
}

u8 gdx_queue_pop(struct gdx_queue *q) {
    u8 ret = q->buf[q->head];
    q->head = (q->head + 1) % GDX_QUEUE_SIZE;
    return ret;
}