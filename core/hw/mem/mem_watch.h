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
#include "addrspace.h"
#include "hw/aica/aica_if.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/pvr/elan.h"
#include "rend/TexCache.h"
#include <unordered_map>

namespace memwatch
{

struct Page
{
	Page() {
		// don't initialize data
	}
	u8 data[PAGE_SIZE];
};
using PageMap = std::unordered_map<u32, Page>;

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
	}

	void unprotect()
	{
		static_cast<T&>(*this).unprotectMem(0, 0xffffffff);
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
	    auto rv = pages.emplace(offset, Page());
	    if (!rv.second)
	      // already saved
	      return true;
	    Page& page = rv.first->second;
	    memcpy(&page.data[0], static_cast<T&>(*this).getMemPage(offset), PAGE_SIZE);
		static_cast<T&>(*this).unprotectMem(offset, PAGE_SIZE);
		return true;
	}

	void getPages(PageMap& other)
	{
		std::swap(pages, other);
		pages = PageMap();
	}
};

class VramWatcher : public Watcher<VramWatcher>
{
	friend class Watcher<VramWatcher>;

protected:
	void protectMem(u32 addr, u32 size)
	{
		addrspace::protectVram(addr, std::min(VRAM_SIZE - addr, size) & ~PAGE_MASK);
	}

	void unprotectMem(u32 addr, u32 size)
	{
		addrspace::unprotectVram(addr, std::min(VRAM_SIZE - addr, size) & ~PAGE_MASK);
	}

	u32 getMemOffset(void *p)
	{
		return addrspace::getVramOffset(p);
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
		return &aica::aica_ram[addr];
	}
};

class ElanRamWatcher : public Watcher<ElanRamWatcher>
{
	friend class Watcher<ElanRamWatcher>;

protected:
	void protectMem(u32 addr, u32 size);
	u32 getMemOffset(void *p);

public:
	void unprotectMem(u32 addr, u32 size);
	void *getMemPage(u32 addr)
	{
		return &elan::RAM[addr];
	}
};

extern VramWatcher vramWatcher;
extern RamWatcher ramWatcher;
extern AicaRamWatcher aramWatcher;
extern ElanRamWatcher elanWatcher;

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
	if (settings.platform.isNaomi2() && elanWatcher.hit(p))
		return true;
	return aramWatcher.hit(p);
}

inline static void protect()
{
	if (!config::GGPOEnable)
		return;
	vramWatcher.protect();
	ramWatcher.protect();
	aramWatcher.protect();
	elanWatcher.protect();
}

inline static void unprotect()
{
	vramWatcher.unprotect();
	ramWatcher.unprotect();
	aramWatcher.unprotect();
	elanWatcher.unprotect();
}

inline static void reset()
{
	vramWatcher.reset();
	ramWatcher.reset();
	aramWatcher.reset();
	elanWatcher.reset();
}

}
