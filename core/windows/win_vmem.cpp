#include "hw/sh4/sh4_if.h"
#include "hw/mem/addrspace.h"
#include "oslib/virtmem.h"

#include <windows.h>
#include "dynlink.h"

namespace virtmem
{

// Implementation of the vmem related function for Windows platforms.
// For now this probably does some assumptions on the CPU/platform.

// The implementation allows it to be empty (that is, to not lock memory).

bool region_lock(void *start, size_t len)
{
	DWORD old;
	if (!VirtualProtect(start, len, PAGE_READONLY, &old)) {
		ERROR_LOG(VMEM, "VirtualProtect(%p, %x, RO) failed: %d", start, (u32)len, GetLastError());
		die("VirtualProtect(ro) failed");
	}
	return true;
}

bool region_unlock(void *start, size_t len)
{
	DWORD old;
	if (!VirtualProtect(start, len, PAGE_READWRITE, &old)) {
		ERROR_LOG(VMEM, "VirtualProtect(%p, %x, RW) failed: %d", start, (u32)len, GetLastError());
		die("VirtualProtect(rw) failed");
	}
	return true;
}

static void *mem_region_reserve(void *start, size_t len)
{
	DWORD type = MEM_RESERVE;
	if (start == nullptr)
		type |= MEM_TOP_DOWN;
	return VirtualAlloc(start, len, type, PAGE_NOACCESS);
}

static bool mem_region_release(void *start, size_t len)
{
	return VirtualFree(start, 0, MEM_RELEASE);
}

HANDLE mem_handle = INVALID_HANDLE_VALUE;
static HANDLE mem_handle2 = INVALID_HANDLE_VALUE;
static char * base_alloc = NULL;

static std::vector<void *> unmapped_regions;
static std::vector<void *> mapped_regions;

// Implement vmem initialization for RAM, ARAM, VRAM and SH4 context, fpcb etc.

#ifdef TARGET_UWP
static WinLibLoader kernel32("Kernel32.dll");
static LPVOID(WINAPI *MapViewOfFileEx)(HANDLE, DWORD, DWORD, DWORD, SIZE_T, LPVOID);
#endif

// Please read the POSIX implementation for more information. On Windows this is
// rather straightforward.
bool init(void **vmem_base_addr, void **sh4rcb_addr, size_t ramSize)
{
#ifdef TARGET_UWP
	if (MapViewOfFileEx == nullptr)
	{
		MapViewOfFileEx = kernel32.getFunc("MapViewOfFileEx", MapViewOfFileEx);
		if (MapViewOfFileEx == nullptr)
			return false;
	}
#endif
	unmapped_regions.reserve(32);
	mapped_regions.reserve(32);

	// First let's try to allocate the in-memory file
	mem_handle = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, (DWORD)ramSize, 0);

	// Now allocate the actual address space (it will be 64KB aligned on windows).
	unsigned memsize = 512_MB + sizeof(Sh4RCB) + ARAM_SIZE_MAX;
	base_alloc = (char*)mem_region_reserve(NULL, memsize);

	// Calculate pointers now
	*sh4rcb_addr = &base_alloc[0];
	*vmem_base_addr = &base_alloc[sizeof(Sh4RCB)];

	VirtualFree(base_alloc, 0, MEM_RELEASE);
	// Map the SH4CB block too
	void *base_ptr = VirtualAlloc(base_alloc, sizeof(Sh4RCB), MEM_RESERVE, PAGE_NOACCESS);
	verify(base_ptr == base_alloc);
	// Map the rest of the context
	void *cntx_ptr = VirtualAlloc((u8*)p_sh4rcb + sizeof(p_sh4rcb->fpcb), sizeof(Sh4RCB) - sizeof(p_sh4rcb->fpcb), MEM_COMMIT, PAGE_READWRITE);
	verify(cntx_ptr == (u8*)p_sh4rcb + sizeof(p_sh4rcb->fpcb));

	// Reserve the rest of the memory but don't commit it
	void *ptr = VirtualAlloc(*vmem_base_addr, memsize - sizeof(Sh4RCB), MEM_RESERVE, PAGE_NOACCESS);
	verify(ptr == *vmem_base_addr);
	unmapped_regions.push_back(ptr);

	return true;
}

// Just tries to wipe as much as possible in the relevant area.
void destroy() {
	VirtualFree(base_alloc, 0, MEM_RELEASE);
	CloseHandle(mem_handle);
}

// Resets a chunk of memory by deleting its data and setting its protection back.
void reset_mem(void *ptr, unsigned size_bytes) {
	VirtualFree(ptr, size_bytes, MEM_DECOMMIT);
}

// Allocates a bunch of memory (page aligned and page-sized)
void ondemand_page(void *address, unsigned size_bytes) {
	void *p = VirtualAlloc(address, size_bytes, MEM_COMMIT, PAGE_READWRITE);
	verify(p != nullptr);
}

/// Creates mappings to the underlying file including mirroring sections
void create_mappings(const Mapping *vmem_maps, unsigned nummaps) {
	// Since this is tricky to get right in Windows (in posix one can just unmap sections and remap later)
	// we unmap the whole thing only to remap it later.
	// Unmap the whole section
	for (void *p : mapped_regions)
		UnmapViewOfFile(p);
	mapped_regions.clear();
	for (void *p : unmapped_regions)
		mem_region_release(p, 0);
	unmapped_regions.clear();

	for (unsigned i = 0; i < nummaps; i++)
	{
		size_t address_range_size = vmem_maps[i].end_address - vmem_maps[i].start_address;
		DWORD protection = vmem_maps[i].allow_writes ? (FILE_MAP_READ | FILE_MAP_WRITE) : FILE_MAP_READ;

		if (!vmem_maps[i].memsize) {
			// Unmapped stuff goes with a protected area or memory. Prevent anything from allocating here
			void *ptr = VirtualAlloc(&addrspace::ram_base[vmem_maps[i].start_address], address_range_size, MEM_RESERVE, PAGE_NOACCESS);
			verify(ptr == &addrspace::ram_base[vmem_maps[i].start_address]);
			unmapped_regions.push_back(ptr);
		}
		else {
			// Calculate the number of mirrors
			unsigned num_mirrors = (unsigned)(address_range_size / vmem_maps[i].memsize);
			verify((address_range_size % vmem_maps[i].memsize) == 0 && num_mirrors >= 1);

			// Remap the views one by one
			for (unsigned j = 0; j < num_mirrors; j++) {
				size_t offset = vmem_maps[i].start_address + j * vmem_maps[i].memsize;

				void *ptr = MapViewOfFileEx(mem_handle, protection, 0, (DWORD)vmem_maps[i].memoffset,
				                    vmem_maps[i].memsize, &addrspace::ram_base[offset]);
				verify(ptr == &addrspace::ram_base[offset]);
				mapped_regions.push_back(ptr);
			}
		}
	}
}

template<typename Mapper>
static void *prepare_jit_block_template(size_t size, Mapper mapper)
{
	// Several issues on Windows: can't protect arbitrary pages due to (I guess) the way
	// kernel tracks mappings, so only stuff that has been allocated with VirtualAlloc can be
	// protected (the entire allocation IIUC).

	// Strategy: Allocate a new region. Protect it properly.
	// More issues: the area should be "close" to the .text stuff so that code gen works.
	// Remember that on x64 we have 4 byte jump/load offset immediates, no issues on x86 :D

	// Take this function addr as reference.
	uintptr_t base_addr = reinterpret_cast<uintptr_t>(&init) & ~0xFFFFF;

	// Probably safe to assume reicast code is <200MB (today seems to be <16MB on every platform I've seen).
	for (uintptr_t i = 0; i < 1800_MB; i += 1_MB) {  // Some arbitrary step size.
		uintptr_t try_addr_above = base_addr + i;
		uintptr_t try_addr_below = base_addr - i;

		// We need to make sure there's no address wrap around the end of the addrspace (meaning: int overflow).
		if (try_addr_above != 0 && try_addr_above > base_addr) {
			void *ptr = mapper((void*)try_addr_above, size);
			if (ptr)
				return ptr;
		}
		if (try_addr_below != 0 && try_addr_below < base_addr) {
			void *ptr = mapper((void*)try_addr_below, size);
			if (ptr)
				return ptr;
		}
	}
	return NULL;
}

static void* mem_alloc(void *addr, size_t size)
{
#ifdef TARGET_UWP
	// rwx is not allowed. Need to switch between r-x and rw-
	return VirtualAlloc(addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
	return VirtualAlloc(addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#endif
}

// Prepares the code region for JIT operations, thus marking it as RWX
bool prepare_jit_block(void *, size_t size, void **code_area_rwx)
{
	// Get the RWX page close to the code_area
	void *ptr = prepare_jit_block_template(size, mem_alloc);
	if (!ptr)
		return false;

	*code_area_rwx = ptr;
	INFO_LOG(DYNAREC, "Found code area at %p, not too far away from %p", *code_area_rwx, &init);

	// We should have found some area in the addrspace, after all size is ~tens of megabytes.
	// Pages are already RWX, all done
	return true;
}

void release_jit_block(void *code_area, size_t)
{
	VirtualFree(code_area, 0, MEM_RELEASE);
}

static void* mem_file_map(void *addr, size_t size)
{
	// Maps the entire file at the specified addr.
	void *ptr = VirtualAlloc(addr, size, MEM_RESERVE, PAGE_NOACCESS);
	if (!ptr)
		return NULL;
	VirtualFree(ptr, 0, MEM_RELEASE);
	if (ptr != addr)
		return NULL;

#ifndef TARGET_UWP
	return MapViewOfFileEx(mem_handle2, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, size, addr);
#else
	return MapViewOfFileFromApp(mem_handle2, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, size);
#endif
}

// Use two addr spaces: need to remap something twice, therefore use CreateFileMapping()
bool prepare_jit_block(void *, size_t size, void** code_area_rw, ptrdiff_t* rx_offset)
{
	mem_handle2 = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_EXECUTE_READWRITE, 0, (DWORD)size, 0);

	// Get the RX page close to the code_area
	void* ptr_rx = prepare_jit_block_template(size, mem_file_map);
	if (!ptr_rx)
		return false;

	// Ok now we just remap the RW segment at any position (dont' care).
#ifndef TARGET_UWP
	void* ptr_rw = MapViewOfFileEx(mem_handle2, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size, NULL);
#else
	void* ptr_rw = MapViewOfFileFromApp(mem_handle2, FILE_MAP_READ | FILE_MAP_WRITE, 0, size);
#endif

	*code_area_rw = ptr_rw;
	*rx_offset = (char*)ptr_rx - (char*)ptr_rw;
	INFO_LOG(DYNAREC, "Info: Using NO_RWX mode, rx ptr: %p, rw ptr: %p, offset: %lu", ptr_rx, ptr_rw, (unsigned long)*rx_offset);

	return (ptr_rw != NULL);
}

void release_jit_block(void *code_area1, void *code_area2, size_t)
{
	UnmapViewOfFile(code_area1);
	UnmapViewOfFile(code_area2);
	// FIXME the same handle is used for all allocations, and thus leaks.
	// And the last opened handle is closed multiple times.
	// But windows doesn't need separate RW and RX areas except perhaps UWP
	// instead of switching back and forth between RX and RW
	CloseHandle(mem_handle2);
}

void jit_set_exec(void* code, size_t size, bool enable)
{
#ifdef TARGET_UWP
	DWORD old;
	if (!VirtualProtect(code, size, enable ? PAGE_EXECUTE_READ : PAGE_READWRITE, &old)) {
		ERROR_LOG(VMEM, "VirtualProtect(%p, %x, %s) failed: %d", code, (u32)size, enable ? "RX" : "RW", GetLastError());
		die("VirtualProtect(rx/rw) failed");
	}
#endif
}

#if HOST_CPU == CPU_ARM64 || HOST_CPU == CPU_ARM
static void Arm_Arm64_CacheFlush(void *start, void *end) {
	if (start == end) {
		return;
	}

	FlushInstructionCache(GetCurrentProcess(), start, (uintptr_t)end - (uintptr_t)start);
}

void flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end) {
	Arm_Arm64_CacheFlush(dcache_start, dcache_end);

	// Dont risk it and flush and invalidate icache&dcache for both ranges just in case.
	if (icache_start != dcache_start) {
		Arm_Arm64_CacheFlush(icache_start, icache_end);
	}
}
#endif

}	// namespace virtmem
