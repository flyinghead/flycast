#include "hw/mem/_vmem.h"
#include "hw/sh4/sh4_if.h"

#include <windows.h>

// Implementation of the vmem related function for Windows platforms.
// For now this probably does some assumptions on the CPU/platform.

// The implementation allows it to be empty (that is, to not lock memory).

bool mem_region_lock(void *start, size_t len)
{
	DWORD old;
	if (!VirtualProtect(start, len, PAGE_READONLY, &old))
		die("VirtualProtect failed ..\n");
	return true;
}

bool mem_region_unlock(void *start, size_t len)
{
	DWORD old;
	if (!VirtualProtect(start, len, PAGE_READWRITE, &old))
		die("VirtualProtect failed ..\n");
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
// The function supports allocating 512MB or 4GB addr spaces.

// Please read the POSIX implementation for more information. On Windows this is
// rather straightforward.
VMemType vmem_platform_init(void **vmem_base_addr, void **sh4rcb_addr)
{
#ifdef TARGET_UWP
	return MemTypeError;
#endif
	unmapped_regions.reserve(32);
	mapped_regions.reserve(32);

	// First let's try to allocate the in-memory file
	mem_handle = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, RAM_SIZE_MAX + VRAM_SIZE_MAX + ARAM_SIZE_MAX, 0);

	// Now allocate the actual address space (it will be 64KB aligned on windows).
	unsigned memsize = 512*1024*1024 + sizeof(Sh4RCB) + ARAM_SIZE_MAX;
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

	return MemType512MB;
}

// Just tries to wipe as much as possible in the relevant area.
void vmem_platform_destroy() {
	VirtualFree(base_alloc, 0, MEM_RELEASE);
	CloseHandle(mem_handle);
}

// Resets a chunk of memory by deleting its data and setting its protection back.
void vmem_platform_reset_mem(void *ptr, unsigned size_bytes) {
	VirtualFree(ptr, size_bytes, MEM_DECOMMIT);
}

// Allocates a bunch of memory (page aligned and page-sized)
void vmem_platform_ondemand_page(void *address, unsigned size_bytes) {
	verify(VirtualAlloc(address, size_bytes, MEM_COMMIT, PAGE_READWRITE) != NULL);
}

/// Creates mappings to the underlying file including mirroring sections
void vmem_platform_create_mappings(const vmem_mapping *vmem_maps, unsigned nummaps) {
	// Since this is tricky to get right in Windows (in posix one can just unmap sections and remap later)
	// we unmap the whole thing only to remap it later.
#ifndef TARGET_UWP
	// Unmap the whole section
	for (void *p : mapped_regions)
		UnmapViewOfFile(p);
	mapped_regions.clear();
	for (void *p : unmapped_regions)
		mem_region_release(p, 0);
	unmapped_regions.clear();

	for (unsigned i = 0; i < nummaps; i++) {
		unsigned address_range_size = vmem_maps[i].end_address - vmem_maps[i].start_address;
		DWORD protection = vmem_maps[i].allow_writes ? (FILE_MAP_READ | FILE_MAP_WRITE) : FILE_MAP_READ;

		if (!vmem_maps[i].memsize) {
			// Unmapped stuff goes with a protected area or memory. Prevent anything from allocating here
			void *ptr = VirtualAlloc(&virt_ram_base[vmem_maps[i].start_address], address_range_size, MEM_RESERVE, PAGE_NOACCESS);
			verify(ptr == &virt_ram_base[vmem_maps[i].start_address]);
			unmapped_regions.push_back(ptr);
		}
		else {
			// Calculate the number of mirrors
			unsigned num_mirrors = (address_range_size) / vmem_maps[i].memsize;
			verify((address_range_size % vmem_maps[i].memsize) == 0 && num_mirrors >= 1);

			// Remap the views one by one
			for (unsigned j = 0; j < num_mirrors; j++) {
				unsigned offset = vmem_maps[i].start_address + j * vmem_maps[i].memsize;

				void *ptr = MapViewOfFileEx(mem_handle, protection, 0, vmem_maps[i].memoffset,
				                    vmem_maps[i].memsize, &virt_ram_base[offset]);
				verify(ptr == &virt_ram_base[offset]);
				mapped_regions.push_back(ptr);
			}
		}
	}
#endif
}

typedef void* (*mapper_fn) (void *addr, unsigned size);

// This is a templated function since it's used twice
static void* vmem_platform_prepare_jit_block_template(void *code_area, unsigned size, mapper_fn mapper) {
	// Several issues on Windows: can't protect arbitrary pages due to (I guess) the way
	// kernel tracks mappings, so only stuff that has been allocated with VirtualAlloc can be
	// protected (the entire allocation IIUC).

	// Strategy: ignore code_area and allocate a new one. Protect it properly.
	// More issues: the area should be "close" to the .text stuff so that code gen works.
	// Remember that on x64 we have 4 byte jump/load offset immediates, no issues on x86 :D

	// Take this function addr as reference.
	uintptr_t base_addr = reinterpret_cast<uintptr_t>(&vmem_platform_init) & ~0xFFFFF;

	// Probably safe to assume reicast code is <200MB (today seems to be <16MB on every platform I've seen).
	for (uintptr_t i = 0; i < 1800 * 1024 * 1024; i += 1024 * 1024) {  // Some arbitrary step size.
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

static void* mem_alloc(void *addr, unsigned size)
{
#ifdef TARGET_UWP
	// rwx is not allowed. Need to switch between r-x and rw-
	return VirtualAlloc(addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
	return VirtualAlloc(addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#endif
}

// Prepares the code region for JIT operations, thus marking it as RWX
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rwx) {
	// Get the RWX page close to the code_area
	void *ptr = vmem_platform_prepare_jit_block_template(code_area, size, &mem_alloc);
	if (!ptr)
		return false;

	*code_area_rwx = ptr;
	INFO_LOG(DYNAREC, "Found code area at %p, not too far away from %p", *code_area_rwx, &vmem_platform_init);

	// We should have found some area in the addrspace, after all size is ~tens of megabytes.
	// Pages are already RWX, all done
	return true;
}


static void* mem_file_map(void *addr, unsigned size)
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
bool vmem_platform_prepare_jit_block(void* code_area, unsigned size, void** code_area_rw, ptrdiff_t* rx_offset)
{
	mem_handle2 = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_EXECUTE_READWRITE, 0, size, 0);

	// Get the RX page close to the code_area
	void* ptr_rx = vmem_platform_prepare_jit_block_template(code_area, size, &mem_file_map);
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

void vmem_platform_jit_set_exec(void* code, size_t size, bool enable)
{
#ifdef TARGET_UWP
	DWORD old;
	if (!VirtualProtect(code, size, enable ? PAGE_EXECUTE_READ : PAGE_READWRITE, &old))
		die("VirtualProtect failed");
#endif
}
