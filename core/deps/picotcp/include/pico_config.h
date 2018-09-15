/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#include "pico_defines.h"
#ifndef INCLUDE_PICO_CONFIG
#define INCLUDE_PICO_CONFIG
#ifndef __KERNEL__
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#else
#include <linux/types.h>
#endif

#if defined __IAR_SYSTEMS_ICC__ || defined ATOP
#   define PACKED_STRUCT_DEF __packed struct
#   define PEDANTIC_STRUCT_DEF __packed struct
#   define PACKED_UNION_DEF  __packed union
#   define PACKED __packed
#   define WEAK
#elif defined __WATCOMC__
#   define PACKED_STRUCT_DEF   _Packed struct
#   define PEDANTIC_STRUCT_DEF struct
#   define PACKED_UNION_DEF    _Packed union
#   define WEAK
#else
#   define PACKED_STRUCT_DEF struct __attribute__((packed))
#   define PEDANTIC_STRUCT_DEF struct
#   define PACKED_UNION_DEF  union   /* Sane compilers do not require packed unions */
#   define PACKED __attribute__((packed))
#   define WEAK __attribute__((weak))
#   ifdef __GNUC__
#       define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#       if ((GCC_VERSION >= 40800))
#           define BYTESWAP_GCC
#       endif
#   endif
#endif

#ifdef PICO_BIGENDIAN

# define PICO_IDETH_IPV4 0x0800
# define PICO_IDETH_ARP 0x0806
# define PICO_IDETH_IPV6 0x86DD

# define PICO_ARP_REQUEST 0x0001
# define PICO_ARP_REPLY   0x0002
# define PICO_ARP_HTYPE_ETH 0x0001

#define short_be(x) (x)
#define long_be(x) (x)
#define long_long_be(x) (x)

static inline uint16_t short_from(void *_p)
{
    unsigned char *p = (unsigned char *)_p;
    uint16_t r, p0, p1;
    p0 = p[0];
    p1 = p[1];
    r = (p0 << 8) + p1;
    return r;
}

static inline uint32_t long_from(void *_p)
{
    unsigned char *p = (unsigned char *)_p;
    uint32_t r, p0, p1, p2, p3;
    p0 = p[0];
    p1 = p[1];
    p2 = p[2];
    p3 = p[3];
    r = (p0 << 24) + (p1 << 16) + (p2 << 8) + p3;
    return r;
}

#else

static inline uint16_t short_from(void *_p)
{
    unsigned char *p = (unsigned char *)_p;
    uint16_t r, _p0, _p1;
    _p0 = p[0];
    _p1 = p[1];
    r = (uint16_t)((_p1 << 8u) + _p0);
    return r;
}

static inline uint32_t long_from(void *_p)
{
    unsigned char *p = (unsigned char *)_p;
    uint32_t r, _p0, _p1, _p2, _p3;
    _p0 = p[0];
    _p1 = p[1];
    _p2 = p[2];
    _p3 = p[3];
    r = (_p3 << 24) + (_p2 << 16) + (_p1 << 8) + _p0;
    return r;
}


# define PICO_IDETH_IPV4 0x0008
# define PICO_IDETH_ARP 0x0608
# define PICO_IDETH_IPV6 0xDD86

# define PICO_ARP_REQUEST 0x0100
# define PICO_ARP_REPLY   0x0200
# define PICO_ARP_HTYPE_ETH 0x0100

#   ifndef BYTESWAP_GCC
static inline uint16_t short_be(uint16_t le)
{
    return (uint16_t)(((le & 0xFFu) << 8) | ((le >> 8u) & 0xFFu));
}

static inline uint32_t long_be(uint32_t le)
{
    uint8_t *b = (uint8_t *)&le;
    uint32_t be = 0;
    uint32_t b0, b1, b2;
    b0 = b[0];
    b1 = b[1];
    b2 = b[2];
    be = b[3] + (b2 << 8) + (b1 << 16) + (b0 << 24);
    return be;
}
static inline uint64_t long_long_be(uint64_t le)
{
    uint8_t *b = (uint8_t *)&le;
    uint64_t be = 0;
    uint64_t b0, b1, b2, b3, b4, b5, b6;
    b0 = b[0];
    b1 = b[1];
    b2 = b[2];
    b3 = b[3];
    b4 = b[4];
    b5 = b[5];
    b6 = b[6];
    be = b[7] + (b6 << 8) + (b5 << 16) + (b4 << 24) + (b3 << 32) + (b2 << 40) + (b1 << 48) + (b0 << 56);
    return be;
}
#   else
/*
   extern uint32_t __builtin_bswap32(uint32_t);
   extern uint16_t __builtin_bswap16(uint16_t);
   extern uint64_t __builtin_bswap64(uint64_t);
 */

static inline uint32_t long_be(uint32_t le)
{
    return (uint32_t)__builtin_bswap32(le);
}

static inline uint16_t short_be(uint16_t le)
{
    return (uint16_t)__builtin_bswap16(le);
}

static inline uint64_t long_long_be(uint64_t le)
{
    return (uint64_t)__builtin_bswap64(le);
}

#   endif /* BYTESWAP_GCC */
#endif

/* Mockables */
#if defined UNIT_TEST
#   define MOCKABLE __attribute__((weak))
#else
#   define MOCKABLE
#endif

#include "pico_constants.h"
#include "pico_mm.h"

#define IGNORE_PARAMETER(x)  ((void)x)

#define PICO_MEM_DEFAULT_SLAB_SIZE 1600
#define PICO_MEM_PAGE_SIZE 4096
#define PICO_MEM_PAGE_LIFETIME 100
#define PICO_MIN_HEAP_SIZE 600
#define PICO_MIN_SLAB_SIZE 1200
#define PICO_MAX_SLAB_SIZE 1600
#define PICO_MEM_MINIMUM_OBJECT_SIZE 4

/*** *** *** *** *** *** ***
 *** PLATFORM SPECIFIC   ***
 *** *** *** *** *** *** ***/
#if defined PICO_PORT_CUSTOM
# include "pico_port.h"
#elif defined CORTEX_M4_HARDFLOAT
# include "arch/pico_cortex_m.h"
#elif defined CORTEX_M4_SOFTFLOAT
# include "arch/pico_cortex_m.h"
#elif defined CORTEX_M3
# include "arch/pico_cortex_m.h"
#elif defined CORTEX_M0
# include "arch/pico_cortex_m.h"
#elif defined DOS_WATCOM
# include "arch/pico_dos.h"
#elif defined PIC24
# include "arch/pico_pic24.h"
#elif defined PIC32
# include "arch/pico_pic32.h"
#elif defined MSP430
# include "arch/pico_msp430.h"
#elif defined MBED_TEST
# include "arch/pico_mbed.h"
#elif defined AVR
# include "arch/pico_avr.h"
#elif defined ARM9
# include "arch/pico_arm9.h"
#elif defined ESP8266
# include "arch/pico_esp8266.h"
#elif defined ATSAMD21J18
# include "arch/pico_atsamd21j18.h"
#elif defined MT7681
# include "arch/pico_generic_gcc.h"
#elif defined FAULTY
# include "../test/pico_faulty.h"
#elif defined ARCHNONE
# include "arch/pico_none.h"
#elif defined GENERIC
# include "arch/pico_generic_gcc.h"
#elif defined __KERNEL__
# include "arch/pico_linux.h"
/* #elif defined ... */
#else
# include "arch/pico_posix.h"
#endif

#ifdef PICO_SUPPORT_MM
#define PICO_ZALLOC(x) pico_mem_zalloc(x)
#define PICO_FREE(x) pico_mem_free(x)
#else
#define PICO_ZALLOC(x) pico_zalloc(x)
#define PICO_FREE(x) pico_free(x)
#endif  /* PICO_SUPPORT_MM */

#endif
