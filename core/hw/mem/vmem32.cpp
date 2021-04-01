/*
    Created on: Apr 11, 2019

	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "vmem32.h"
#include "_vmem.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/modules/mmu.h"

#include <unordered_set>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#ifdef __ANDROID__
#include <linux/ashmem.h>
#endif
#endif

#ifndef MAP_NOSYNC
#define MAP_NOSYNC       0
#endif

extern bool VramLockedWriteOffset(size_t offset);
extern std::mutex vramlist_lock;

#ifdef _WIN32
extern HANDLE mem_handle;
#else
extern int vmem_fd;
#endif

#define VMEM32_ERROR_NOT_MAPPED 0x100

static const u64 VMEM32_SIZE = 0x100000000L;
static const u64 USER_SPACE = 0x80000000L;
static const u64 AREA7_ADDRESS = 0x7C000000L;

#define VRAM_PROT_SEGMENT (1024 * 1024)	// vram protection regions are grouped by 1MB segment

static std::unordered_set<u32> vram_mapped_pages;
struct vram_lock {
	u32 start;
	u32 end;
};
static std::vector<vram_lock> vram_blocks[VRAM_SIZE_MAX / VRAM_PROT_SEGMENT];
static u8 sram_mapped_pages[USER_SPACE / PAGE_SIZE / 8];	// bit set to 1 if page is mapped

bool vmem32_inited;

// stats
//u64 vmem32_page_faults;
//u64 vmem32_flush;

static void* vmem32_map_buffer(u32 dst, u32 addrsz, u32 offset, u32 size, bool write)
{
	void* ptr;
	void* rv;

	//printf("MAP32 %08X w/ %d\n",dst,offset);
	u32 map_times = addrsz / size;
#ifdef _WIN32
	rv = MapViewOfFileEx(mem_handle, FILE_MAP_READ | (write ? FILE_MAP_WRITE : 0), 0, offset, size, &virt_ram_base[dst]);
	if (rv == NULL)
		return NULL;

	for (u32 i = 1; i < map_times; i++)
	{
		dst += size;
		ptr = MapViewOfFileEx(mem_handle, FILE_MAP_READ | (write ? FILE_MAP_WRITE : 0), 0, offset, size, &virt_ram_base[dst]);
		if (ptr == NULL)
			return NULL;
	}
#else
	u32 prot = PROT_READ | (write ? PROT_WRITE : 0);
	rv = mmap(&virt_ram_base[dst], size, prot, MAP_SHARED | MAP_NOSYNC | MAP_FIXED, vmem_fd, offset);
	if (MAP_FAILED == rv)
	{
		ERROR_LOG(VMEM, "MAP1 failed %d", errno);
		return NULL;
	}

	for (u32 i = 1; i < map_times; i++)
	{
		dst += size;
		ptr = mmap(&virt_ram_base[dst], size, prot , MAP_SHARED | MAP_NOSYNC | MAP_FIXED, vmem_fd, offset);
		if (MAP_FAILED == ptr)
		{
			ERROR_LOG(VMEM, "MAP2 failed %d", errno);
			return NULL;
		}
	}
#endif
	return rv;
}

static void vmem32_unmap_buffer(u32 start, u64 end)
{
#ifdef _WIN32
	UnmapViewOfFile(&virt_ram_base[start]);
#else
	mmap(&virt_ram_base[start], end - start, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
}

static void vmem32_protect_buffer(u32 start, u32 size)
{
	verify((start & PAGE_MASK) == 0);
#ifdef _WIN32
	DWORD old;
	VirtualProtect(virt_ram_base + start, size, PAGE_READONLY, &old);
#else
	mprotect(&virt_ram_base[start], size, PROT_READ);
#endif
}

static void vmem32_unprotect_buffer(u32 start, u32 size)
{
	verify((start & PAGE_MASK) == 0);
#ifdef _WIN32
	DWORD old;
	VirtualProtect(virt_ram_base + start, size, PAGE_READWRITE, &old);
#else
	mprotect(&virt_ram_base[start], size, PROT_READ | PROT_WRITE);
#endif
}

void vmem32_protect_vram(u32 addr, u32 size)
{
	if (!vmem32_inited)
		return;
	for (u32 page = (addr & VRAM_MASK) / VRAM_PROT_SEGMENT; page <= ((addr & VRAM_MASK) + size - 1) / VRAM_PROT_SEGMENT; page++)
	{
		vram_blocks[page].push_back({ addr, addr + size - 1 });
	}
}
void vmem32_unprotect_vram(u32 addr, u32 size)
{
	if (!vmem32_inited)
		return;
	for (u32 page = (addr & VRAM_MASK) / VRAM_PROT_SEGMENT; page <= ((addr & VRAM_MASK) + size - 1) / VRAM_PROT_SEGMENT; page++)
	{
		std::vector<vram_lock>& block_list = vram_blocks[page];
		for (auto it = block_list.begin(); it != block_list.end(); )
		{
			if (it->start >= addr && it->end < addr + size)
				it = block_list.erase(it);
			else
				it++;
		}
	}
}

static const u32 page_sizes[] = { 1024, 4 * 1024, 64 * 1024, 1024 * 1024 };

static u32 vmem32_paddr_to_offset(u32 address)
{
	u32 low_addr = address & 0x1FFFFFFF;
	switch ((address >> 26) & 7)
	{
	case 0:	// area 0
		// Aica ram
		if (low_addr >= 0x00800000 && low_addr < 0x00800000 + 0x00800000)
		{
			return ((low_addr - 0x00800000) & (ARAM_SIZE - 1)) + MAP_ARAM_START_OFFSET;
		}
		else if (low_addr >= 0x02800000 && low_addr < 0x02800000 + 0x00800000)
		{
			return low_addr - 0x02800000 + MAP_ARAM_START_OFFSET;
		}
		break;
	case 1:	// area 1
		// Vram
		if (low_addr >= 0x04000000 && low_addr < 0x04000000 + 0x01000000)
		{
			return ((low_addr - 0x04000000) & (VRAM_SIZE - 1)) + MAP_VRAM_START_OFFSET;
		}
		else if (low_addr >= 0x06000000 && low_addr < 0x06000000 + 0x01000000)
		{
			return ((low_addr - 0x06000000) & (VRAM_SIZE - 1)) + MAP_VRAM_START_OFFSET;
		}
		break;
	case 3:	// area 3
		// System ram
		if (low_addr >= 0x0C000000 && low_addr < 0x0C000000 + 0x04000000)
		{
			return ((low_addr - 0x0C000000) & (RAM_SIZE - 1)) + MAP_RAM_START_OFFSET;
		}
		break;
	//case 4:
		// TODO vram?
		//break;
	default:
		break;
	}
	// Unmapped address
	return -1;
}

static u32 vmem32_map_mmu(u32 address, bool write)
{
#ifndef NO_MMU
	u32 pa;
	const TLB_Entry *entry;
	u32 rc = mmu_full_lookup<false>(address, &entry, pa);
	if (rc == MMU_ERROR_NONE)
	{
		//0X  & User mode-> protection violation
		//if ((entry->Data.PR >> 1) == 0 && p_sh4rcb->cntx.sr.MD == 0)
		//	return MMU_ERROR_PROTECTED;

		//if (write)
		//{
		//	if ((entry->Data.PR & 1) == 0)
		//		return MMU_ERROR_PROTECTED;
		//	if (entry->Data.D == 0)
		//		return MMU_ERROR_FIRSTWRITE;
		//}
		u32 page_size = page_sizes[entry->Data.SZ1 * 2 + entry->Data.SZ0];
		if (page_size == 1024)
			return VMEM32_ERROR_NOT_MAPPED;

		u32 vpn = (entry->Address.VPN << 10) & ~(page_size - 1);
		u32 ppn = (entry->Data.PPN << 10) & ~(page_size - 1);
		u32 offset = vmem32_paddr_to_offset(ppn);
		if (offset == (u32)-1)
			return VMEM32_ERROR_NOT_MAPPED;

		bool allow_write = (entry->Data.PR & 1) != 0;
		if (offset >= MAP_VRAM_START_OFFSET && offset < MAP_VRAM_START_OFFSET + VRAM_SIZE)
		{
			// Check vram protected regions
			u32 start = offset - MAP_VRAM_START_OFFSET;
			if (!vram_mapped_pages.insert(vpn).second)
			{
				// page has been mapped already: vram locked write
				vmem32_unprotect_buffer(address & ~PAGE_MASK, PAGE_SIZE);
				u32 addr_offset = start + (address & (page_size - 1));
				VramLockedWriteOffset(addr_offset);

				return MMU_ERROR_NONE;
			}
			verify(vmem32_map_buffer(vpn, page_size, offset, page_size, allow_write) != NULL);
			u32 end = start + page_size;
			const std::vector<vram_lock>& blocks = vram_blocks[start / VRAM_PROT_SEGMENT];

			{
				std::lock_guard<std::mutex> lock(vramlist_lock);
				for (int i = blocks.size() - 1; i >= 0; i--)
				{
					if (blocks[i].start < end && blocks[i].end >= start)
					{
						u32 prot_start = std::max(start, blocks[i].start);
						u32 prot_size = std::min(end, blocks[i].end + 1) - prot_start;
						prot_size += prot_start % PAGE_SIZE;
						prot_start &= ~PAGE_MASK;
						vmem32_protect_buffer(vpn + (prot_start & (page_size - 1)), prot_size);
					}
				}
			}
		}
		else if (offset >= MAP_RAM_START_OFFSET && offset < MAP_RAM_START_OFFSET + RAM_SIZE)
		{
#if FEAT_SHREC != DYNAREC_NONE
			// Check system RAM protected pages
			u32 start = offset - MAP_RAM_START_OFFSET;

			if (bm_IsRamPageProtected(start) && allow_write)
			{
				if (sram_mapped_pages[start >> 15] & (1 << ((start >> 12) & 7)))
				{
					// Already mapped => write access
					vmem32_unprotect_buffer(address & ~PAGE_MASK, PAGE_SIZE);
					bm_RamWriteAccess(ppn);
				}
				else
				{
					sram_mapped_pages[start >> 15] |= (1 << ((start >> 12) & 7));
					verify(vmem32_map_buffer(vpn, page_size, offset, page_size, false) != NULL);
				}
			}
			else
#endif
				verify(vmem32_map_buffer(vpn, page_size, offset, page_size, allow_write) != NULL);
		}
		else
			// Not vram or system ram
			verify(vmem32_map_buffer(vpn, page_size, offset, page_size, allow_write) != NULL);

		return MMU_ERROR_NONE;
	}
#else
	u32 rc = MMU_ERROR_PROTECTED;
#endif
	return rc;
}

static u32 vmem32_map_address(u32 address, bool write)
{
	u32 area = address >> 29;
	switch (area)
	{
	case 3:	// P0/U0
		if (address >= AREA7_ADDRESS)
			// area 7: unmapped
			return VMEM32_ERROR_NOT_MAPPED;
		/* no break */
	case 0:
	case 1:
	case 2:
	case 6:	// P3
		return vmem32_map_mmu(address, write);

	default:
		break;
	}
	return VMEM32_ERROR_NOT_MAPPED;
}

#if !defined(NO_MMU) && defined(HOST_64BIT_CPU)
// returns:
//  0 if the fault address isn't handled by the mmu
//  1 if the fault was handled and the access should be reattempted
// -1 if an sh4 exception has been thrown
int vmem32_handle_signal(void *fault_addr, bool write, u32 exception_pc)
{
	if (!vmem32_inited || (u8*)fault_addr < virt_ram_base || (u8*)fault_addr >= virt_ram_base + VMEM32_SIZE)
		return 0;
	//vmem32_page_faults++;
	u32 guest_addr = (u8*)fault_addr - virt_ram_base;
	u32 rv = vmem32_map_address(guest_addr, write);
	DEBUG_LOG(VMEM, "vmem32_handle_signal handled signal %s @ %p -> %08x rv=%d", write ? "W" : "R", fault_addr, guest_addr, rv);
	if (rv == MMU_ERROR_NONE)
		return 1;
	if (rv == VMEM32_ERROR_NOT_MAPPED)
		return 0;
#if HOST_CPU == CPU_ARM64
	p_sh4rcb->cntx.pc = exception_pc;
#else
	p_sh4rcb->cntx.pc = p_sh4rcb->cntx.exception_pc;
#endif
	DoMMUException(guest_addr, rv, write ? MMU_TT_DWRITE : MMU_TT_DREAD);

	return -1;
}
#endif

void vmem32_flush_mmu()
{
	//vmem32_flush++;
	vram_mapped_pages.clear();
	memset(sram_mapped_pages, 0, sizeof(sram_mapped_pages));
	vmem32_unmap_buffer(0, USER_SPACE);
	// TODO flush P3?
}

bool vmem32_init()
{
#ifdef _WIN32
	return false;
#else
	if (config::DisableVmem32 || !_nvmem_4gb_space())
		return false;
	vmem32_inited = true;
	vmem32_flush_mmu();
	return true;
#endif
}

void vmem32_term()
{
	if (vmem32_inited)
	{
		vmem32_inited = false;
		vmem32_flush_mmu();
	}
}

