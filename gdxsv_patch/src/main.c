typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define GDXDATA __attribute__((section("gdx.data")))
#define GDXFUNC __attribute__((section("gdx.func")))
#define GDXMAIN __attribute__((section("gdx.main")))

#include "mini-printf.h"

#define ADDR_MCS_READ 0x0c0455ba
#define BIN_OFFSET 0x80000000
#define GDX_QUEUE_SIZE 512

struct gdx_queue {
    u16 head;
    u16 tail;
    u8 buf[GDX_QUEUE_SIZE];
};

u32 GDXFUNC gdx_queue_init(struct gdx_queue *q) {
    q->head = 0;
    q->tail = 0;
}

u32 GDXFUNC gdx_queue_size(struct gdx_queue *q) {
    return (q->tail + GDX_QUEUE_SIZE - q->head) % GDX_QUEUE_SIZE;
}

u32 GDXFUNC gdx_queue_avail(struct gdx_queue *q) {
    return GDX_QUEUE_SIZE - gdx_queue_size(q) - 1;
}

void GDXFUNC gdx_queue_push(struct gdx_queue *q, u8 data) {
    q->buf[q->tail] = data;
    q->tail = (q->tail + 1) % GDX_QUEUE_SIZE;
}

u8 GDXFUNC gdx_queue_pop(struct gdx_queue *q) {
    u8 ret = q->buf[q->head];
    q->head = (q->head + 1) % GDX_QUEUE_SIZE;
    return ret;
}

GDXDATA u32 initialized = 0x10;
GDXDATA u32 print_buf_pos = 0;
GDXDATA char print_buf[1024];
struct gdx_queue gdx_rxq GDXDATA;

void GDXFUNC gdx_printf(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    print_buf_pos += mini_vsnprintf(print_buf + print_buf_pos, sizeof(print_buf) - print_buf_pos, format, arg);
    va_end(arg);
}

u32 GDXFUNC read32(u32 addr) {
    u32 *p = addr;
    return *p;
}

u16 GDXFUNC read16(u32 addr) {
    u16 *p = addr;
    return *p;
}

u8 GDXFUNC read8(u32 addr) {
    u8 *p = addr;
    return *p;
}

void GDXFUNC write32(u32 addr, u32 value) {
    u32 *p = addr;
    *p = value;
}

void GDXFUNC write16(u32 addr, u16 value) {
    u16 *p = addr;
    *p = value;
}

void GDXFUNC write8(u32 addr, u8 value) {
    u8 *p = addr;
    *p = value;
}

int GDXFUNC __attribute__((stdcall)) gdx_ReadSock(u32 sock, u8 *buf, u32 size) {
    gdx_printf("gdx_ReadSock 0x%x 0x%x %d\n", sock, buf, size);

    int org_ret = ((int (__attribute__((stdcall)) *)(u32, u8 *, u32)) ADDR_MCS_READ)(sock, buf, size); // call original read function
    gdx_printf("org %02d : ", org_ret);
    for (int i = 0; i < org_ret; i++) {
        gdx_printf("%02x", buf[i]);
    }
    gdx_printf("\n");
    // return org_ret;

    int n = gdx_queue_size(&gdx_rxq);
    gdx_printf("dc queue size:%d head:%d tail:%d\n", n, gdx_rxq.head, gdx_rxq.tail);
    gdx_printf("pop %02d : ", n);
    if (size < n) n = size;
    for (int i = 0; i < n; ++i) {
        u8 data = gdx_queue_pop(&gdx_rxq);
        gdx_printf("%02x", data);
        write8(buf + i, data);
    }
    gdx_printf("\n");

    gdx_printf("mod %02d : ", n);
    for (int i = 0; i < n; i++) {
        gdx_printf("%02x", buf[i]);
    }
    gdx_printf("\n");
    return n;
}

void GDXFUNC gdx_initialize() {
    gdx_queue_init(&gdx_rxq);
    write32(BIN_OFFSET + 0x0c046678, gdx_ReadSock);
    initialized += 1;
}

void GDXMAIN gdx_main() {
    initialized += 1;
    gdx_initialize();
}
