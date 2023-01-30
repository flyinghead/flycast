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
#include "oslib/virtmem.h"

namespace memwatch
{

VramWatcher vramWatcher;
RamWatcher ramWatcher;
AicaRamWatcher aramWatcher;
ElanRamWatcher elanWatcher;

void AicaRamWatcher::protectMem(u32 addr, u32 size)
{
	size = std::min(ARAM_SIZE - addr, size) & ~PAGE_MASK;
	virtmem::region_lock(&aica::aica_ram[addr], size);
}

void AicaRamWatcher::unprotectMem(u32 addr, u32 size)
{
	size = std::min(ARAM_SIZE - addr, size) & ~PAGE_MASK;
	virtmem::region_unlock(&aica::aica_ram[addr], size);
}

u32 AicaRamWatcher::getMemOffset(void *p)
{
	if ((u8 *)p < &aica::aica_ram[0] || (u8 *)p >= &aica::aica_ram[ARAM_SIZE])
		return -1;
	return (u32)((u8 *)p - &aica::aica_ram[0]);
}

void ElanRamWatcher::protectMem(u32 addr, u32 size)
{
	using namespace elan;
	if (ERAM_SIZE != 0)
	{
		size = std::min(ERAM_SIZE - addr, size) & ~PAGE_MASK;
		virtmem::region_lock(RAM + addr, size);
	}
}

void ElanRamWatcher::unprotectMem(u32 addr, u32 size)
{
	using namespace elan;
	if (ERAM_SIZE != 0)
	{
		size = std::min(ERAM_SIZE - addr, size) & ~PAGE_MASK;
		virtmem::region_unlock(RAM + addr, size);
	}
}

u32 ElanRamWatcher::getMemOffset(void *p)
{
	using namespace elan;
	if ((u8 *)p < RAM || (u8 *)p >= &RAM[ERAM_SIZE])
		return -1;
	return (u32)((u8 *)p - RAM);
}

}

