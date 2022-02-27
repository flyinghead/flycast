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
#include "types.h"
#include "_vmem.h"
#include "hw/aica/aica_if.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/TexCache.h"
#include <array>
#include <unordered_map>

namespace memwatch
{

using PageMap = std::unordered_map<u32, std::array<u8, PAGE_SIZE>>;

template<typename T>
class Watcher
{
	bool started;
	PageMap pages;

public:
	void protect()
	{
		if (!started)
		{
			static_cast<T&>(*this).protectMem(0, 0xffffffff);
			started = true;
		}
		else
		{
			for (const auto& pair : pages)
				static_cast<T&>(*this).protectMem(pair.first, PAGE_SIZE);
		}
		pages.clear();
	}

	void reset()
	{
		started = false;
		pages.clear();
	}

	bool hit(void *addr)
	{
		u32 offset = static_cast<T&>(*this).getMemOffset(addr);
		if (offset == (u32)-1)
			return false;
		offset &= ~PAGE_MASK;
		if (pages.count(offset) > 0)
			// already saved
			return true;
		memcpy(&pages[offset][0], static_cast<T&>(*this).getMemPage(offset), PAGE_SIZE);
		static_cast<T&>(*this).unprotectMem(offset, PAGE_SIZE);
		return true;
	}

	const PageMap& getPages() {
		return pages;
	}
};

class VramWatcher : public Watcher<VramWatcher>
{
	friend class Watcher<VramWatcher>;

protected:
	void protectMem(u32 addr, u32 size)
	{
		_vmem_protect_vram(addr, std::min(VRAM_SIZE - addr, size) & ~PAGE_MASK);
	}

	void unprotectMem(u32 addr, u32 size)
	{
		_vmem_unprotect_vram(addr, std::min(VRAM_SIZE - addr, size) & ~PAGE_MASK);
	}

	u32 getMemOffset(void *p)
	{
		return _vmem_get_vram_offset(p);
	}

public:
	void *getMemPage(u32 addr)
	{
		return &vram[addr];
	}
};

class RamWatcher : public Watcher<RamWatcher>
{
	friend class Watcher<RamWatcher>;

protected:
	void protectMem(u32 addr, u32 size)
	{
		bm_LockPage(addr, std::min(RAM_SIZE - addr, size) & ~PAGE_MASK);
	}

	void unprotectMem(u32 addr, u32 size)
	{
		bm_UnlockPage(addr, std::min(RAM_SIZE - addr, size) & ~PAGE_MASK);
	}

	u32 getMemOffset(void *p)
	{
		return bm_getRamOffset(p);
	}

public:
	void *getMemPage(u32 addr)
	{
		return &mem_b[addr];
	}
};

class AicaRamWatcher : public Watcher<AicaRamWatcher>
{
	friend class Watcher<AicaRamWatcher>;

protected:
	void protectMem(u32 addr, u32 size);
	void unprotectMem(u32 addr, u32 size);
	u32 getMemOffset(void *p);

public:
	void *getMemPage(u32 addr)
	{
		return &aica_ram[addr];
	}
};

extern VramWatcher vramWatcher;
extern RamWatcher ramWatcher;
extern AicaRamWatcher aramWatcher;

inline static bool writeAccess(void *p)
{
	if (!config::GGPOEnable)
		return false;
	if (ramWatcher.hit(p))
	{
		bm_RamWriteAccess(p);
		return true;
	}
	if (vramWatcher.hit(p))
	{
		VramLockedWrite((u8 *)p);
		return true;
	}
	return aramWatcher.hit(p);
}

inline static void protect()
{
	if (!config::GGPOEnable)
		return;
	vramWatcher.protect();
	ramWatcher.protect();
	aramWatcher.protect();
}

inline static void reset()
{
	vramWatcher.reset();
	ramWatcher.reset();
	aramWatcher.reset();
}

}
