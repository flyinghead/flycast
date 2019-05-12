
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <windowsx.h>

#include "hw/mem/_vmem.h"

// Implementation of the vmem related function for Windows platforms.
// For now this probably does some assumptions on the CPU/platform.

// This implements the VLockedMemory interface, as defined in _vmem.h
// The implementation allows it to be empty (that is, to not lock memory).

void VLockedMemory::LockRegion(unsigned offset, unsigned size) {
	//verify(offset + size < this->size && size != 0);
	DWORD old;
	VirtualProtect(&data[offset], size, PAGE_READONLY, &old);
}

void VLockedMemory::UnLockRegion(unsigned offset, unsigned size) {
	//verify(offset + size <= this->size && size != 0);
	DWORD old;
	VirtualProtect(&data[offset], size, PAGE_READWRITE, &old);
}

static HANDLE mem_handle = INVALID_HANDLE_VALUE;
static char * base_alloc = NULL;

// Implement vmem initialization for RAM, ARAM, VRAM and SH4 context, fpcb etc.
// The function supports allocating 512MB or 4GB addr spaces.

// Plase read the POSIX implementation for more information. On Windows this is
// rather straightforward.
VMemType vmem_platform_init(void **vmem_base_addr, void **sh4rcb_addr) {
	// Firt let's try to allocate the in-memory file
	mem_handle = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, RAM_SIZE_MAX + VRAM_SIZE_MAX + ARAM_SIZE_MAX, 0);

	// Now allocate the actual address space (it will be 64KB aligned on windows).
	unsigned memsize = 512*1024*1024 + sizeof(Sh4RCB) + ARAM_SIZE_MAX;
	base_alloc = (char*)VirtualAlloc(0, memsize, MEM_RESERVE, PAGE_NOACCESS);

	// Calculate pointers now
	*sh4rcb_addr = &base_alloc[0];
	*vmem_base_addr = &base_alloc[sizeof(Sh4RCB)];

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
	verify(VirtualAlloc(address, size_bytes, MEM_COMMIT, PAGE_READWRITE));
}

/// Creates mappings to the underlying file including mirroring sections
void vmem_platform_create_mappings(const vmem_mapping *vmem_maps, unsigned nummaps) {
	// Since this is tricky to get right in Windows (in posix one can just unmap sections and remap later)
	// we unmap the whole thing only to remap it later.

	// Unmap the whole section
	VirtualFree(base_alloc, 0, MEM_RELEASE);

	// Map the SH4CB block too
	void *base_ptr = VirtualAlloc(base_alloc, sizeof(Sh4RCB), MEM_RESERVE, PAGE_NOACCESS);
	verify(base_ptr == base_alloc);
	void *cntx_ptr = VirtualAlloc((u8*)p_sh4rcb + sizeof(p_sh4rcb->fpcb), sizeof(Sh4RCB) - sizeof(p_sh4rcb->fpcb), MEM_COMMIT, PAGE_READWRITE);
	verify(cntx_ptr == (u8*)p_sh4rcb + sizeof(p_sh4rcb->fpcb));

	for (unsigned i = 0; i < nummaps; i++) {
		unsigned address_range_size = vmem_maps[i].end_address - vmem_maps[i].start_address;
		DWORD protection = vmem_maps[i].allow_writes ? (FILE_MAP_READ | FILE_MAP_WRITE) : FILE_MAP_READ;

		if (!vmem_maps[i].memsize) {
			// Unmapped stuff goes with a protected area or memory. Prevent anything from allocating here
			void *ptr = VirtualAlloc(&virt_ram_base[vmem_maps[i].start_address], address_range_size, MEM_RESERVE, PAGE_NOACCESS);
			verify(ptr == &virt_ram_base[vmem_maps[i].start_address]);
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
			}
		}
	}
}

