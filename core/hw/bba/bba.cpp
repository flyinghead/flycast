/*
	Copyright 2021 flyinghead

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
#include "bba.h"
#include "rtl8139c.h"
#include "hw/holly/holly_intc.h"
#include "network/picoppp.h"
#include "serialize.h"

static RTL8139State *rtl8139device;

// 1400 - 1600 GAPSPCI bridge registers
// 1600 - 1700 standard PCI config
// 1700 - 1800 rtl8139c I/O ports
// 840000 - 847FFF RAM
#define	GAPSPCI_REGS			0x001400
#define	GAPSPCI_REGS_SIZE		   0x200
#define GAPSPCI_PCI_CONFIG		0x001600
#define GAPSPCI_PCI_CONFIG_SIZE	   0x100
#define GAPSPCI_RTL_REGS		0x001700
#define GAPSPCI_RTL_REGS_SIZE	   0x100
#define GAPSPCI_RAM_BASE		0x840000
#define GAPSPCI_RAM_BASE_MASK	0xFF0000
#define GAPSPCI_RAM_SIZE		  0x8000
#define GAPSPCI_RAM_MASK (GAPSPCI_RAM_SIZE - 1)

// GAPSPCI registers
#define GAPS_INT_ENABLE 0x14
#define GAPS_RESET      0x18
#define GAPS_DMA_BASE   0x28
#define GAPS_DMA_OFFSET 0x2c

static u8 GAPS_ram[GAPSPCI_RAM_SIZE];
static u8 GAPS_regs[GAPSPCI_REGS_SIZE];
static u32 dmaOffset;
static bool interruptPending;

static void setInterrupt()
{
	if (interruptPending && GAPS_regs[GAPS_INT_ENABLE] != 0)
		asic_RaiseInterrupt(holly_EXP_PCI);
	else
		asic_CancelInterrupt(holly_EXP_PCI);
}

void pci_set_irq(PCIDevice *pci_dev, int level)
{
	interruptPending = level != 0;
	setInterrupt();
}

void bba_Init()
{
	NICConf nicConf = { 0xc, 0xa, 0xf, 0xe, 0, 0 };
	rtl8139device = rtl8139_init(&nicConf);
	pci_rtl8139_realize(PCI_DEVICE(rtl8139device));

	memset(&GAPS_regs[0], 0, sizeof(GAPS_regs));
	memcpy(&GAPS_regs[0], "GAPSPCI_BRIDGE_2", 0x10);
	memcpy(&GAPS_regs[0x1c], "SEGA", 4);
}

void bba_Term()
{
	if (rtl8139device != nullptr)
	{
		stop_pico();
		rtl8139_destroy(rtl8139device);
		rtl8139device = nullptr;
	}
}

void bba_Reset(bool hard)
{
	if (hard)
	{
		bba_Term();
		bba_Init();
	}
}

u32 bba_ReadMem(u32 addr, u32 sz)
{
	u32 data = 0;
	if ((addr & GAPSPCI_RAM_BASE_MASK) == GAPSPCI_RAM_BASE)
	{
		if (addr & 0x8000)
			// G2 DMA access
			addr += dmaOffset;
		addr &= GAPSPCI_RAM_MASK;
		if (addr + sz > GAPSPCI_RAM_SIZE)
		{
			// wrap around
			memcpy(&data, &GAPS_ram[addr], GAPSPCI_RAM_SIZE - addr);
			memcpy((u8 *)&data + (GAPSPCI_RAM_SIZE - addr), &GAPS_ram[0], sz - (GAPSPCI_RAM_SIZE - addr));
		}
		else
			memcpy(&data, &GAPS_ram[addr], sz);

		return data;
	}
	DEBUG_LOG(NETWORK, "bba_ReadMem<%d> %06x", sz, addr);

	switch (addr & 0xFFFF00)
	{
	case GAPSPCI_REGS:
	case GAPSPCI_REGS + 0x100:
		addr &= GAPSPCI_REGS_SIZE - 1;
		memcpy(&data, &GAPS_regs[addr], sz);
		if (addr == GAPS_RESET)
			data &= 0xFF;
		break;

	case GAPSPCI_PCI_CONFIG:
		memcpy(&data, &PCI_DEVICE(rtl8139device)->config[addr & (GAPSPCI_PCI_CONFIG_SIZE - 1)], sz);
		DEBUG_LOG(NETWORK, "pcidev->config(r%d) %02x %x", sz, addr & (GAPSPCI_PCI_CONFIG_SIZE - 1), data);
		break;

	case GAPSPCI_RTL_REGS:
		return rtl8139_ioport_read(rtl8139device, addr & (GAPSPCI_RTL_REGS_SIZE - 1), sz);

	default:
		INFO_LOG(NETWORK, "bba_ReadMem<%d> address %x unknown", sz, addr);
		data = -1;
		break;
	}

	return data;
}

void bba_WriteMem(u32 addr, u32 data, u32 sz)
{
	if ((addr & GAPSPCI_RAM_BASE_MASK) == GAPSPCI_RAM_BASE)
	{
		if (addr & 0x8000)
			// G2 DMA access
			addr += dmaOffset;
		addr &= GAPSPCI_RAM_MASK;
		if (addr + sz > GAPSPCI_RAM_SIZE)
		{
			// wrap around
			memcpy(&GAPS_ram[addr], &data, GAPSPCI_RAM_SIZE - addr);
			memcpy(&GAPS_ram[0], (u8 *)&data + (GAPSPCI_RAM_SIZE - addr), sz - (GAPSPCI_RAM_SIZE - addr));
		}
		else
			memcpy(&GAPS_ram[addr], &data, sz);

		return;
	}
	DEBUG_LOG(NETWORK, "bba_WriteMem<%d> %06x = %x", sz, addr, data);

	switch (addr & 0xFFFF00)
	{
	case GAPSPCI_REGS:
	case GAPSPCI_REGS + 0x100:
		addr &= GAPSPCI_REGS_SIZE - 1;
		memcpy(&GAPS_regs[addr], &data, sz);
		switch(addr)
		{
		case GAPS_INT_ENABLE:
			setInterrupt();
			break;

		case GAPS_RESET:
			if (data & 1)
			{
				DEBUG_LOG(NETWORK, "GAPS reset");
				rtl8139_reset(rtl8139device);
				start_pico();
			}
			break;

		case GAPS_DMA_OFFSET:
			dmaOffset = data & GAPSPCI_RAM_MASK;
			break;
		}
		break;

	case GAPSPCI_PCI_CONFIG:
		DEBUG_LOG(NETWORK, "pcidev->config(w%d) %02x %x", sz, addr & (GAPSPCI_PCI_CONFIG_SIZE - 1), data);
		//pci_default_write_config(pcidev, addr & 0xFF, data, sz);
		break;

	case GAPSPCI_RTL_REGS:
		rtl8139_ioport_write(rtl8139device, addr & (GAPSPCI_RTL_REGS_SIZE - 1), data, sz);
		break;

	default:
		INFO_LOG(NETWORK, "bba_WriteMem<%d> address %x unknown (data %x)", sz, addr, data);
		break;
	}
}

ssize_t qemu_send_packet(RTL8139State *s, const uint8_t *buf, int size)
{
	pico_receive_eth_frame(buf, size);

	return size;
}

int pico_send_eth_frame(const u8 *data, u32 len)
{
	if (!rtl8139_can_receive(rtl8139device))
		return 0;
	rtl8139_receive(rtl8139device, data, len);

	return 1;
}

void pci_dma_read(PCIDevice *dev, dma_addr_t addr, void *buf, dma_addr_t len)
{
	memcpy(buf, &GAPS_ram[addr & GAPSPCI_RAM_MASK], len);
}

void pci_dma_write(PCIDevice *dev, dma_addr_t addr, const void *buf, dma_addr_t len)
{
	memcpy(&GAPS_ram[addr & GAPSPCI_RAM_MASK], buf, len);
}

void bba_Serialize(Serializer& ser)
{
	ser << GAPS_regs;
	ser << GAPS_ram;
	ser << dmaOffset;
	ser << interruptPending;
	rtl8139_serialize(rtl8139device, ser);
}

void bba_Deserialize(Deserializer& deser)
{
	deser >> GAPS_regs;
	deser >> GAPS_ram;
	deser >> dmaOffset;
	deser >> interruptPending;
    // returns true if the receiver is enabled and the network stack must be started
    if (rtl8139_deserialize(rtl8139device, deser))
        start_pico();
}

#define POLYNOMIAL_BE 0x04c11db6

uint32_t net_crc32(const uint8_t *p, int len)
{
    uint32_t crc;
    int carry, i, j;
    uint8_t b;

    crc = 0xffffffff;
    for (i = 0; i < len; i++) {
        b = *p++;
        for (j = 0; j < 8; j++) {
            carry = ((crc & 0x80000000L) ? 1 : 0) ^ (b & 0x01);
            crc <<= 1;
            b >>= 1;
            if (carry) {
                crc = ((crc ^ POLYNOMIAL_BE) | carry);
            }
        }
    }

    return crc;
}

/*
 * QEMU PCI bus manager
 *
 * Copyright (c) 2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
static inline bool is_power_of_2(uint64_t value)
{
    if (!value) {
        return false;
    }

    return !(value & (value - 1));
}

int pci_bar(PCIDevice *d, int reg)
{
    uint8_t type;

    if (reg != PCI_ROM_SLOT)
        return PCI_BASE_ADDRESS_0 + reg * 4;

    type = d->config[PCI_HEADER_TYPE] & ~PCI_HEADER_TYPE_MULTI_FUNCTION;
    return type == PCI_HEADER_TYPE_BRIDGE ? PCI_ROM_ADDRESS1 : PCI_ROM_ADDRESS;
}

void pci_register_bar(PCIDevice *pci_dev, int region_num,
                      uint8_t type, MemoryRegion *memory)
{
    PCIIORegion *r;
    uint32_t addr; /* offset in pci config space */
    uint64_t wmask;
    pcibus_t size = memory->size;

    verify(region_num >= 0);
    verify(region_num < PCI_NUM_REGIONS);
    verify(is_power_of_2(size));

    r = &pci_dev->io_regions[region_num];
    r->addr = PCI_BAR_UNMAPPED;
    r->size = size;
    r->type = type;

    wmask = ~(size - 1);
    if (region_num == PCI_ROM_SLOT) {
        /* ROM enable bit is writable */
        wmask |= PCI_ROM_ADDRESS_ENABLE;
    }

    addr = pci_bar(pci_dev, region_num);
    pci_set_long(pci_dev->config + addr, type);

    pci_set_long(pci_dev->wmask + addr, wmask & 0xffffffff);
    pci_set_long(pci_dev->cmask + addr, 0xffffffff);
}
