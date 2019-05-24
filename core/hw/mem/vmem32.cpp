/*
 * vmem32.cpp
 *
 *  Created on: Apr 11, 2019
 *      Author: Flyinghead
 */
#include <unordered_set>
#include "build.h"
#include "vmem32.h"
#include "_vmem.h"

#if HOST_OS == OS_WINDOWS
#include <Windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>
#include <errno.h>
#ifdef _ANDROID
#include <linux/ashmem.h>
#endif
#endif

#ifndef MAP_NOSYNC
#define MAP_NOSYNC       0
#endif

#include "types.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/modules/mmu.h"

extern bool VramLockedWriteOffset(size_t offset);
extern cMutex vramlist_lock;

#if HOST_OS == OS_WINDOWS
extern HANDLE mem_handle;
#else
extern int vmem_fd;
#endif

#define VMEM32_ERROR_NOT_MAPPED 0x100

static const u64 VMEM32_SIZE = 0x100000000L;
static const u64 KERNEL_SPACE = 0x80000000L;
static const u64 AREA7_ADDRESS = 0x7C000000L;

#define VRAM_PROT_SEGMENT (1024 * 1024)	// vram protection regions are grouped by 1MB segment

static std::unordered_set<u32> vram_mapped_pages;
static std::vector<vram_block*> vram_blocks[VRAM_SIZE / VRAM_PROT_SEGMENT];

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
#if HOST_OS == OS_WINDOWS
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
		printf("MAP1 failed %d\n", errno);
		return NULL;
	}

	for (u32 i = 1; i < map_times; i++)
	{
		dst += size;
		ptr = mmap(&virt_ram_base[dst], size, prot , MAP_SHARED | MAP_NOSYNC | MAP_FIXED, vmem_fd, offset);
		if (MAP_FAILED == ptr)
		{
			printf("MAP2 failed %d\n", errno);
			return NULL;
		}
	}
#endif
	return rv;
}

static void vmem32_unmap_buffer(u32 start, u64 end)
{
#if HOST_OS == OS_WINDOWS
	UnmapViewOfFile(&virt_ram_base[start]);
#else
	mmap(&virt_ram_base[start], end - start, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
}

static void vmem32_protect_buffer(u32 start, u32 size)
{
	verify((start & PAGE_MASK) == 0);
#if HOST_OS == OS_WINDOWS
	DWORD old;
	VirtualProtect(virt_ram_base + start, size, PAGE_READONLY, &old);
#else
	mprotect(&virt_ram_base[start], size, PROT_READ);
#endif
}

static void vmem32_unprotect_buffer(u32 start, u32 size)
{
	verify((start & PAGE_MASK) == 0);
#if HOST_OS == OS_WINDOWS
	DWORD old;
	VirtualProtect(virt_ram_base + start, size, PAGE_READWRITE, &old);
#else
	mprotect(&virt_ram_base[start], size, PROT_READ | PROT_WRITE);
#endif
}

void vmem32_protect_vram(vram_block *block)
{
	if (!vmem32_inited)
		return;
	for (int i = block->start / VRAM_PROT_SEGMENT; i <= block->end / VRAM_PROT_SEGMENT; i++)
	{
		vram_blocks[i].push_back(block);
	}
}
void vmem32_unprotect_vram(vram_block *block)
{
	if (!vmem32_inited)
		return;
	for (int page = block->start / VRAM_PROT_SEGMENT; page <= block->end / VRAM_PROT_SEGMENT; page++)
	{
		for (int i = 0; i < vram_blocks[page].size(); i++)
			if (vram_blocks[page][i] == block)
			{
				vram_blocks[page].erase(vram_blocks[page].begin() + i);
				break;
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
		if (offset == -1)
			return VMEM32_ERROR_NOT_MAPPED;

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
			verify(vmem32_map_buffer(vpn, page_size, offset, page_size, (entry->Data.PR & 1) != 0) != NULL);
			u32 end = start + page_size;
			const vector<vram_block *>& blocks = vram_blocks[start / VRAM_PROT_SEGMENT];

			vramlist_lock.Lock();
			for (int i = blocks.size() - 1; i >= 0; i--)
			{
				if (blocks[i]->start < end && blocks[i]->end >= start)
				{
					u32 prot_start = max(start, blocks[i]->start);
					u32 prot_size = min(end, blocks[i]->end + 1) - prot_start;
					prot_size += prot_start % PAGE_SIZE;
					prot_start &= ~PAGE_MASK;
					vmem32_protect_buffer(vpn + (prot_start & (page_size - 1)), prot_size);
				}
			}
			vramlist_lock.Unlock();
		}
		else
			// Not vram
			verify(vmem32_map_buffer(vpn, page_size, offset, page_size, (entry->Data.PR & 1) != 0) != NULL);

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
bool vmem32_handle_signal(void *fault_addr, bool write)
{
	if (!vmem32_inited || (u8*)fault_addr < virt_ram_base || (u8*)fault_addr >= virt_ram_base + VMEM32_SIZE)
		return false;
	//vmem32_page_faults++;
	u32 guest_addr = (u8*)fault_addr - virt_ram_base;
	u32 rv = vmem32_map_address(guest_addr, write);
	//printf("vmem32_handle_signal handled signal %s @ %p -> %08x rv=%d\n", write ? "W" : "R", fault_addr, guest_addr, rv);
	if (rv == MMU_ERROR_NONE)
		return true;
	if (rv == VMEM32_ERROR_NOT_MAPPED)
		return false;
	p_sh4rcb->cntx.pc = p_sh4rcb->cntx.exception_pc;
	DoMMUException(guest_addr, rv, write ? MMU_TT_DWRITE : MMU_TT_DREAD);
	ngen_HandleException();
	// not reached
	return true;
}
#endif

void vmem32_flush_mmu()
{
	//vmem32_flush++;
	vram_mapped_pages.clear();
	vmem32_unmap_buffer(0, KERNEL_SPACE);
	// TODO flush P3?
}

bool vmem32_init()
{
#if HOST_OS == OS_WINDOWS
	return false;
#else
	if (settings.dynarec.disable_vmem32 || !_nvmem_4gb_space())
		return false;

	vmem32_inited = true;
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

