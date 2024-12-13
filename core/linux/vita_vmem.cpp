// Implementation of the vmem related function for the PS Vita
// Based on posix_vmem.cpp, requires forked kubridge kernel module:
// https://github.com/bythos14/kubridge/tree/exceptions_mprotect

#if defined(__vita__)
#include <vitasdk.h>
#include <kubridge.h>

#include "hw/mem/addrspace.h"
#include "oslib/virtmem.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/sh4/sh4_if.h"
#include "hw/pvr/elan.h"
#include "stdclass.h"
#include "types.h"

#define ALIGN(addr, align) (((uintptr_t)addr + (align - 1)) & ~(align - 1))

extern bool is_standalone;

namespace virtmem
{

bool region_lock(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	verify(kuKernelMemProtect((u8 *)start - inpage, len + inpage, KU_KERNEL_PROT_READ) == 0);
	return true;
}

bool region_unlock(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	verify(kuKernelMemProtect((u8 *)start - inpage, len + inpage, KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE) == 0);
	return true;
}

bool region_set_exec(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	kuKernelMemProtect((u8 *)start - inpage, len + inpage, KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE | KU_KERNEL_PROT_EXEC);

	return true;
}

// Implement vmem initialization for RAM, ARAM, VRAM and SH4 context, fpcb etc.
// The function supports allocating 512MB or 4GB addr spaces.
static void *reserved_base;
static size_t reserved_size;
static SceUID reserved_block = -1;
static SceUID vmem_block = -1;

// vmem_base_addr points to an address space of 512MB (or 4GB) that can be used for fast memory ops.
// In negative offsets of the pointer (up to FPCB size, usually 65/129MB) the context and jump table
// can be found. If the platform init returns error, the user is responsible for initializing the
// memory using a fallback (that is, regular mallocs and falling back to slow memory JIT).
bool init(void **vmem_base_addr, void **sh4rcb_addr, size_t ramSize)
{

	// Now try to allocate a contiguous piece of memory.
	reserved_size = 512 * 1024 * 1024 + ALIGN(sizeof(Sh4RCB), PAGE_SIZE) + ARAM_SIZE_MAX;
	reserved_base = nullptr;
	reserved_block = kuKernelMemReserve(&reserved_base, reserved_size, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW);
	verify(reserved_block >= 0);

	vmem_block = sceKernelAllocMemBlock("vmem_backing_block", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, ALIGN(ramSize, PAGE_SIZE), NULL);
	verify(vmem_block >= 0);

	uintptr_t ptrint = (uintptr_t)reserved_base;
	*sh4rcb_addr = (void *)ptrint;
	*vmem_base_addr = (void *)(ptrint + sizeof(Sh4RCB));
	const size_t fpcb_size = sizeof(((Sh4RCB *)NULL)->fpcb);

	// Now commit the memory for the SH4RCB
	verify(kuKernelMemCommit(*sh4rcb_addr, sizeof(Sh4RCB), KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE, NULL) == 0);
	// Set the fpcb to be non-accessible so that it can be configured upon use.
	verify(kuKernelMemProtect(*sh4rcb_addr, fpcb_size, KU_KERNEL_PROT_NONE) == 0);

	return true;
}

// Just tries to wipe as much as possible in the relevant area.
void destroy()
{
	if (reserved_block != -1)
		sceKernelFreeMemBlock(reserved_block);
	if (vmem_block != -1)
		sceKernelFreeMemBlock(vmem_block);
}

// Resets a chunk of memory by deleting its data and setting its protection back.
void reset_mem(void *ptr, unsigned size_bytes)
{
	// Mark them as non accessible.
	bm_vmem_pagefill((void **)ptr, size_bytes); // TODO: Figure out why this call is needed
	verify(kuKernelMemProtect(ptr, size_bytes, KU_KERNEL_PROT_NONE) == 0);
}

// Allocates a bunch of memory (page aligned and page-sized)
void ondemand_page(void *address, unsigned size_bytes)
{
	verify(region_unlock(address, size_bytes));
}

// Creates mappings to the underlying file including mirroring sections
void create_mappings(const Mapping *vmem_maps, unsigned nummaps)
{
	KuKernelMemCommitOpt opt;
	opt.size = sizeof(opt);
	opt.attr = 0x1;
	opt.baseBlock = vmem_block;

	for (unsigned i = 0; i < nummaps; i++)
	{
		// Ignore unmapped stuff, it is already reserved as PROT_NONE
		if (!vmem_maps[i].memsize)
			continue;

		// Calculate the number of mirrors
		u64 address_range_size = vmem_maps[i].end_address - vmem_maps[i].start_address;
		unsigned num_mirrors = (address_range_size) / vmem_maps[i].memsize;
		verify((address_range_size % vmem_maps[i].memsize) == 0 && num_mirrors >= 1);

		for (unsigned j = 0; j < num_mirrors; j++)
		{
			u64 offset = vmem_maps[i].start_address + j * vmem_maps[i].memsize;
			opt.baseOffset = vmem_maps[i].memoffset;
			verify(kuKernelMemCommit(&addrspace::ram_base[offset], vmem_maps[i].memsize, KU_KERNEL_PROT_READ | (vmem_maps[i].allow_writes ? KU_KERNEL_PROT_WRITE : 0), &opt) == 0);
		}
	}
}

// Prepares the code region for JIT operations, thus marking it as RWX
bool prepare_jit_block(void *code_area, unsigned size, void **code_area_rwx)
{
	if (code_area)
	{
		verify(kuKernelMemProtect(code_area, size, KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_WRITE | KU_KERNEL_PROT_READ) == 0);
		*code_area_rwx = code_area;
	}
	return true;
}

void release_jit_block(void *code_area, size_t size)
{

}

// Use two addr spaces: need to remap something twice, therefore use allocate_shared_filemem()
bool prepare_jit_block(void *code_area, unsigned size, void **code_area_rw, ptrdiff_t *rx_offset)
{
	return false; // Unimplemented
}

void jit_set_exec(void *code, size_t size, bool enable)
{

}

void flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end)
{
	kuKernelFlushCaches(icache_start, (u8 *)icache_end - (u8 *)icache_start + 1);
}

}
#endif // #if defined(__vita__)
