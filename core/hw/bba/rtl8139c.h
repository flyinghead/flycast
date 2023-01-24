/*
	Copyright (c) 2004 Fabrice Bellard and QEMU contributors

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include <climits>
#include <cinttypes>
#include "types.h"
#include "hw/sh4/sh4_sched.h"

typedef u8 uint8_t;
typedef u16 uint16_t;
typedef u64 uint64_t;

typedef uint64_t dma_addr_t;
typedef uint64_t hwaddr;

#ifdef _MSC_VER
#if defined(_WIN64)
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif
#endif

#define ETH_ALEN 6

#define DMA_ADDR_FMT "%" PRIx64

struct MACAddr {
    uint8_t a[6];
};

uint32_t net_crc32(const uint8_t *p, int len);

struct NetClientState {
    int link_down;
};

typedef struct NICConf {
    MACAddr macaddr;
} NICConf;

/** MemoryRegion:
 *
 * A struct representing a memory region.
 */
struct MemoryRegion {
    uint32_t size;
};

#define glue(a, b) _glue(a, b)
#define _glue(a, b) a ## b

#if defined(HOST_WORDS_BIGENDIAN)
#define be_bswap(v, size) (v)
#define le_bswap(v, size) glue(bswap, size)(v)
#define be_bswaps(v, size)
#define le_bswaps(p, size) do { *p = glue(bswap, size)(*p); } while(0)
#else
#define le_bswap(v, size) (v)
#define be_bswap(v, size) glue(bswap, size)(v)
#define le_bswaps(v, size)
#define be_bswaps(p, size) do { *p = glue(bswap, size)(*p); } while(0)
#endif

#define CPU_CONVERT(endian, size, type)\
static inline type endian ## size ## _to_cpu(type v)\
{\
    return glue(endian, _bswap)(v, size);\
}\
\
static inline type cpu_to_ ## endian ## size(type v)\
{\
    return glue(endian, _bswap)(v, size);\
}\
\
static inline void endian ## size ## _to_cpus(type *p)\
{\
    glue(endian, _bswaps)(p, size);\
}\
\
static inline void cpu_to_ ## endian ## size ## s(type *p)\
{\
    glue(endian, _bswaps)(p, size);\
}

CPU_CONVERT(le, 16, uint16_t)

CPU_CONVERT(le, 32, uint32_t)

static inline int lduw_he_p(const void *ptr)
{
    uint16_t r;
    memcpy(&r, ptr, sizeof(r));
    return r;
}
static inline void stl_he_p(void *ptr, uint32_t v)
{
    memcpy(ptr, &v, sizeof(v));
}

static inline int lduw_le_p(const void *ptr)
{
    return (uint16_t)le_bswap(lduw_he_p(ptr), 16);
}
static inline void stl_le_p(void *ptr, uint32_t v)
{
    stl_he_p(ptr, le_bswap(v, 32));
}

/*
 * PCI support
 */
static inline uint16_t pci_get_word(const uint8_t *config)
{
    return lduw_le_p(config);
}
static inline void pci_set_long(uint8_t *config, uint32_t val)
{
    stl_le_p(config, val);
}

typedef uint64_t pcibus_t;

#define PCI_DEVICE(d) ((PCIDevice *)(d))
#define RTL8139(d) ((RTL8139State *)(d));

typedef struct PCIIORegion {
    pcibus_t addr; /* current PCI mapping address. -1 means not mapped */
#define PCI_BAR_UNMAPPED (~(pcibus_t)0)
    pcibus_t size;
    uint8_t type;
} PCIIORegion;

#define PCI_ROM_SLOT 6
#define PCI_NUM_REGIONS 7

struct PCIDevice {
    /* PCI config space */
    uint8_t *config;

    /* Used to enable config checks on load. Note that writable bits are
     * never checked even if set in cmask. */
    uint8_t *cmask;

    /* Used to implement R/W bytes */
    uint8_t *wmask;

    PCIIORegion io_regions[PCI_NUM_REGIONS];
};

void pci_set_irq(PCIDevice *pci_dev, int level);
void pci_register_bar(PCIDevice *pci_dev, int region_num, uint8_t type, MemoryRegion *memory);

void pci_dma_read(PCIDevice *dev, dma_addr_t addr, void *buf, dma_addr_t len);
void pci_dma_write(PCIDevice *dev, dma_addr_t addr, const void *buf, dma_addr_t len);

#define g_malloc malloc
#define g_free free

#define PCI_VENDOR_ID_REALTEK            0x10ec
#define PCI_DEVICE_ID_REALTEK_8139       0x8139

#define FMT_PCIBUS                      PRIx64

#define PCI_HEADER_TYPE		0x0e	/* 8 bits */
#define  PCI_HEADER_TYPE_BRIDGE		1
#define  PCI_HEADER_TYPE_MULTI_FUNCTION 0x80

/*
* Base addresses specify locations in memory or I/O space.
* Decoded size can be determined by writing a value of
* 0xffffffff to the register, and reading it back.  Only
* 1 bits are decoded.
*/
#define PCI_BASE_ADDRESS_0	0x10	/* 32 bits */
#define  PCI_BASE_ADDRESS_SPACE_IO	0x01
#define  PCI_BASE_ADDRESS_SPACE_MEMORY	0x00
#define  PCI_BASE_ADDRESS_MEM_TYPE_64	0x04	/* 64 bit address */
#define PCI_ROM_ADDRESS		0x30	/* Bits 31..11 are address, 10..1 reserved */
#define  PCI_ROM_ADDRESS_ENABLE	0x01

#define PCI_CAPABILITY_LIST	0x34	/* Offset of first capability list entry */
#define PCI_INTERRUPT_PIN	0x3d	/* 8 bits */

#define PCI_ROM_ADDRESS1	0x38	/* Same as PCI_ROM_ADDRESS, but for htype 1 */

struct RTL8139State;

ssize_t qemu_send_packet(RTL8139State *s, const uint8_t *buf, int size);

void pci_rtl8139_realize(PCIDevice *dev);

uint64_t rtl8139_ioport_read(void *opaque, hwaddr addr, unsigned size);
void rtl8139_ioport_write(void *opaque, hwaddr addr, uint64_t val, unsigned size);
void rtl8139_reset(RTL8139State *s);
bool rtl8139_can_receive(RTL8139State *s);
ssize_t rtl8139_receive(RTL8139State *s, const uint8_t *buf, size_t size);

RTL8139State *rtl8139_init(NICConf *conf);
void rtl8139_destroy(RTL8139State *state);
void rtl8139_serialize(RTL8139State *state, Serializer& ser);
bool rtl8139_deserialize(RTL8139State *state, Deserializer& deser);
