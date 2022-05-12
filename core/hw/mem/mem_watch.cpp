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
#include "mem_watch.h"

namespace memwatch
{

VramWatcher vramWatcher;
RamWatcher ramWatcher;
AicaRamWatcher aramWatcher;
ElanRamWatcher elanWatcher;

void AicaRamWatcher::protectMem(u32 addr, u32 size)
{
	size = std::min(ARAM_SIZE - addr, size) & ~PAGE_MASK;
	if (_nvmem_enabled() && _nvmem_4gb_space()) {
		mem_region_lock(virt_ram_base + 0x00800000 + addr, size);	// P0
		mem_region_lock(virt_ram_base + 0x02800000 + addr, size);// P0 - mirror
		mem_region_lock(virt_ram_base + 0x80800000 + addr, size);	// P1
		//mem_region_lock(virt_ram_base + 0x82800000 + addr, size);	// P1 - mirror
		mem_region_lock(virt_ram_base + 0xA0800000 + addr, size);	// P2
		//mem_region_lock(virt_ram_base + 0xA2800000 + addr, size);	// P2 - mirror
		if (ARAM_SIZE == 2 * 1024 * 1024) {
			mem_region_lock(virt_ram_base + 0x00A00000 + addr, size);	// P0
			mem_region_lock(virt_ram_base + 0x00C00000 + addr, size);	// P0
			mem_region_lock(virt_ram_base + 0x00E00000 + addr, size);	// P0
			mem_region_lock(virt_ram_base + 0x02A00000 + addr, size);// P0 - mirror
			mem_region_lock(virt_ram_base + 0x02C00000 + addr, size);// P0 - mirror
			mem_region_lock(virt_ram_base + 0x02E00000 + addr, size);// P0 - mirror
			mem_region_lock(virt_ram_base + 0x80A00000 + addr, size);	// P1
			mem_region_lock(virt_ram_base + 0x80C00000 + addr, size);	// P1
			mem_region_lock(virt_ram_base + 0x80E00000 + addr, size);	// P1
			mem_region_lock(virt_ram_base + 0xA0A00000 + addr, size);	// P2
			mem_region_lock(virt_ram_base + 0xA0C00000 + addr, size);	// P2
			mem_region_lock(virt_ram_base + 0xA0E00000 + addr, size);	// P2
		}
	} else {
		mem_region_lock(aica_ram.data + addr,
				std::min(aica_ram.size - addr, size));
	}
}

void AicaRamWatcher::unprotectMem(u32 addr, u32 size)
{
	size = std::min(ARAM_SIZE - addr, size) & ~PAGE_MASK;
	if (_nvmem_enabled() && _nvmem_4gb_space()) {
		mem_region_unlock(virt_ram_base + 0x00800000 + addr, size);		// P0
		mem_region_unlock(virt_ram_base + 0x02800000 + addr, size);	// P0 - mirror
		mem_region_unlock(virt_ram_base + 0x80800000 + addr, size);		// P1
		//mem_region_unlock(virt_ram_base + 0x82800000 + addr, size);	// P1 - mirror
		mem_region_unlock(virt_ram_base + 0xA0800000 + addr, size);		// P2
		//mem_region_unlock(virt_ram_base + 0xA2800000 + addr, size);	// P2 - mirror
		if (ARAM_SIZE == 2 * 1024 * 1024) {
			mem_region_unlock(virt_ram_base + 0x00A00000 + addr, size);	// P0
			mem_region_unlock(virt_ram_base + 0x00C00000 + addr, size);	// P0
			mem_region_unlock(virt_ram_base + 0x00E00000 + addr, size);	// P0
			mem_region_unlock(virt_ram_base + 0x02A00000 + addr, size);	// P0 - mirror
			mem_region_unlock(virt_ram_base + 0x02C00000 + addr, size);	// P0 - mirror
			mem_region_unlock(virt_ram_base + 0x02E00000 + addr, size);	// P0 - mirror
			mem_region_unlock(virt_ram_base + 0x80A00000 + addr, size);	// P1
			mem_region_unlock(virt_ram_base + 0x80C00000 + addr, size);	// P1
			mem_region_unlock(virt_ram_base + 0x80E00000 + addr, size);	// P1
			mem_region_unlock(virt_ram_base + 0xA0A00000 + addr, size);	// P2
			mem_region_unlock(virt_ram_base + 0xA0C00000 + addr, size);	// P2
			mem_region_unlock(virt_ram_base + 0xA0E00000 + addr, size);	// P2
		}
	} else {
		mem_region_unlock(aica_ram.data + addr,
				std::min(aica_ram.size - addr, size));
	}
}

u32 AicaRamWatcher::getMemOffset(void *p)
{
	u32 addr;
	if (_nvmem_enabled() && _nvmem_4gb_space()) {
		if ((u8*) p < virt_ram_base || (u8*) p >= virt_ram_base + 0x100000000L)
			return -1;
		addr = (u32) ((u8*) p - virt_ram_base);
		u32 area = (addr >> 29) & 7;
		if (area != 0 && area != 4 && area != 5)
			return -1;
		addr &= 0x1fffffff & ~0x02000000;
		if (addr < 0x00800000 || addr >= 0x01000000)
			return -1;
		addr &= ARAM_MASK;
	} else {
		if ((u8*) p < &aica_ram[0] || (u8*) p >= &aica_ram[ARAM_SIZE])
			return -1;
		addr = (u32) ((u8*) p - &aica_ram[0]);
	}
	return addr;
}

void ElanRamWatcher::protectMem(u32 addr, u32 size)
{
	using namespace elan;
	size = std::min(ELAN_RAM_SIZE - addr, size) & ~PAGE_MASK;
	if (_nvmem_enabled())
	{
		mem_region_lock(virt_ram_base + 0x0a000000 + addr, size);	// P0
		if (_nvmem_4gb_space())
		{
			mem_region_lock(virt_ram_base + 0x8a000000 + addr, size);	// P1
			mem_region_lock(virt_ram_base + 0xaa000000 + addr, size);	// P2
		}
	} else {
		mem_region_lock(RAM + addr, size);
	}
}

void ElanRamWatcher::unprotectMem(u32 addr, u32 size)
{
	using namespace elan;
	size = std::min(ELAN_RAM_SIZE - addr, size) & ~PAGE_MASK;
	if (_nvmem_enabled())
	{
		mem_region_unlock(virt_ram_base + 0x0a000000 + addr, size);	// P0
		if (_nvmem_4gb_space())
		{
			mem_region_unlock(virt_ram_base + 0x8a000000 + addr, size);	// P1
			mem_region_unlock(virt_ram_base + 0xaa000000 + addr, size);	// P2
		}
	} else {
		mem_region_unlock(RAM + addr, size);
	}
}

u32 ElanRamWatcher::getMemOffset(void *p)
{
	using namespace elan;
	u32 addr;
	if (_nvmem_enabled())
	{
		if ((u8 *)p < virt_ram_base || (u8 *)p >= virt_ram_base + 0x100000000L)
			return -1;
		addr = (u32)((u8 *)p - virt_ram_base);
		u32 area = (addr >> 29) & 7;
		if (area != 0 && area != 4 && area != 5) // P0, P1 or P2 only
			return -1;
		addr &= 0x1fffffff;
		if (addr < 0x0a000000 || addr >= 0x0a000000 + ELAN_RAM_SIZE)
			return -1;
		addr &= ELAN_RAM_MASK;
	} else {
		if ((u8 *)p < RAM || (u8 *)p >= &RAM[ELAN_RAM_SIZE])
			return -1;
		addr = (u32)((u8 *)p - RAM);
	}
	return addr;
}

}

