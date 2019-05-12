
// Implementation of the vmem related function for POSIX-like platforms.
// There's some minimal amount of platform specific hacks to support
// Android and OSX since they are slightly different in some areas.

// This implements the VLockedMemory interface, as defined in _vmem.h
// The implementation allows it to be empty (that is, to not lock memory).

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "hw/mem/_vmem.h"
#include "stdclass.h"

#ifndef MAP_NOSYNC
#define MAP_NOSYNC       0 //missing from linux :/ -- could be the cause of android slowness ?
#endif

#ifdef _ANDROID
	#include <linux/ashmem.h>
	#ifndef ASHMEM_DEVICE
	#define ASHMEM_DEVICE "/dev/ashmem"
	#undef PAGE_MASK
	#define PAGE_MASK (PAGE_SIZE-1)
#else
	#define PAGE_SIZE 4096
	#define PAGE_MASK (PAGE_SIZE-1)
#endif

// Android specific ashmem-device stuff for creating shared memory regions
int ashmem_create_region(const char *name, size_t size) {
	int fd = open(ASHMEM_DEVICE, O_RDWR);
	if (fd < 0)
		return -1;

	if (ioctl(fd, ASHMEM_SET_SIZE, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}
#endif  // #ifdef _ANDROID

void VLockedMemory::LockRegion(unsigned offset, unsigned size_bytes) {
	size_t inpage = offset & PAGE_MASK;
	if (mprotect(&data[offset - inpage], size_bytes + inpage, PROT_READ)) {
		die("mprotect failed ..\n");
	}
}

void VLockedMemory::UnLockRegion(unsigned offset, unsigned size_bytes) {
	size_t inpage = offset & PAGE_MASK;
	if (mprotect(&data[offset - inpage], size_bytes + inpage, PROT_READ|PROT_WRITE)) {
		// Add some way to see why it failed? gdb> info proc mappings
		die("mprotect  failed ..\n");
	}
}

// Allocates memory via a fd on shmem/ahmem or even a file on disk
static int allocate_shared_filemem(unsigned size) {
	int fd = -1;
	#if defined(_ANDROID)
	// Use Android's specific shmem stuff.
	fd = ashmem_create_region(0, size);
	#else
		#if HOST_OS != OS_DARWIN
		fd = shm_open("/dcnzorz_mem", O_CREAT | O_EXCL | O_RDWR, S_IREAD | S_IWRITE);
		shm_unlink("/dcnzorz_mem");
		#endif

		// if shmem does not work (or using OSX) fallback to a regular file on disk
		if (fd < 0) {
			string path = get_writable_data_path("/dcnzorz_mem");
			fd = open(path.c_str(), O_CREAT|O_RDWR|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
			unlink(path.c_str());
		}
		// If we can't open the file, fallback to slow mem.
		if (fd < 0)
			return -1;

		// Finally make the file as big as we need!
		if (ftruncate(fd, size)) {
			// Can't get as much memory as needed, fallback.
			close(fd);
			return -1;
		}
	#endif

	return fd;
}

// Implement vmem initialization for RAM, ARAM, VRAM and SH4 context, fpcb etc.
// The function supports allocating 512MB or 4GB addr spaces.

static int shmem_fd = -1, shmem_fd2 = -1;

// vmem_base_addr points to an address space of 512MB (or 4GB) that can be used for fast memory ops.
// In negative offsets of the pointer (up to FPCB size, usually 65/129MB) the context and jump table
// can be found. If the platform init returns error, the user is responsible for initializing the
// memory using a fallback (that is, regular mallocs and falling back to slow memory JIT).
VMemType vmem_platform_init(void **vmem_base_addr, void **sh4rcb_addr) {
	// Firt let's try to allocate the shm-backed memory
	shmem_fd = allocate_shared_filemem(RAM_SIZE_MAX + VRAM_SIZE_MAX + ARAM_SIZE_MAX);
	if (shmem_fd < 0)
		return MemTypeError;

	// Now try to allocate a contiguous piece of memory.
	unsigned memsize = 512*1024*1024 + sizeof(Sh4RCB) + ARAM_SIZE_MAX + 0x10000;
	void *first_ptr = mmap(0, memsize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (!first_ptr) {
		close(shmem_fd);
		return MemTypeError;
	}

	// Align pointer to 64KB too, some Linaro bug (no idea but let's just be safe I guess).
	uintptr_t ptrint = (uintptr_t)first_ptr;
	ptrint = (ptrint + 0x10000 - 1) & (~0xffff);
	*sh4rcb_addr = (void*)ptrint;
	*vmem_base_addr = (void*)(ptrint + sizeof(Sh4RCB));
	void *sh4rcb_base_ptr  = (void*)(ptrint + FPCB_SIZE);

	// Now map the memory for the SH4 context, do not include FPCB on purpose (paged on demand).
	mprotect(sh4rcb_base_ptr, sizeof(Sh4RCB) - FPCB_SIZE, PROT_READ | PROT_WRITE);

	return MemType512MB;
}

// Just tries to wipe as much as possible in the relevant area.
void vmem_platform_destroy() {
	munmap(virt_ram_base, 0x20000000);
}

// Resets a chunk of memory by deleting its data and setting its protection back.
void vmem_platform_reset_mem(void *ptr, unsigned size_bytes) {
	// Mark them as non accessible.
	mprotect(ptr, size_bytes, PROT_NONE);
	// Tell the kernel to flush'em all (FIXME: perhaps unmap+mmap 'd be better?)
	madvise(ptr, size_bytes, MADV_DONTNEED);
	#if defined(MADV_REMOVE)
	madvise(ptr, size_bytes, MADV_REMOVE);
	#elif defined(MADV_FREE)
	madvise(ptr, size_bytes, MADV_FREE);
	#endif
}

// Allocates a bunch of memory (page aligned and page-sized)
void vmem_platform_ondemand_page(void *address, unsigned size_bytes) {
	verify(!mprotect(address, size_bytes, PROT_READ | PROT_WRITE));
}

// Creates mappings to the underlying file including mirroring sections
void vmem_platform_create_mappings(const vmem_mapping *vmem_maps, unsigned nummaps) {
	for (unsigned i = 0; i < nummaps; i++) {
		// Ignore unmapped stuff, it is already reserved as PROT_NONE
		if (!vmem_maps[i].memsize)
			continue;

		// Calculate the number of mirrors
		unsigned address_range_size = vmem_maps[i].end_address - vmem_maps[i].start_address;
		unsigned num_mirrors = (address_range_size) / vmem_maps[i].memsize;
		int protection = vmem_maps[i].allow_writes ? (PROT_READ | PROT_WRITE) : PROT_READ;
		verify((address_range_size % vmem_maps[i].memsize) == 0 && num_mirrors >= 1);

		for (unsigned j = 0; j < num_mirrors; j++) {
			unsigned offset = vmem_maps[i].start_address + j * vmem_maps[i].memsize;
			verify(!munmap(&virt_ram_base[offset], vmem_maps[i].memsize));
			verify(MAP_FAILED != mmap(&virt_ram_base[offset], vmem_maps[i].memsize, protection,
			                          MAP_SHARED | MAP_NOSYNC | MAP_FIXED, shmem_fd, vmem_maps[i].memoffset));
			// ??? (mprotect(rv,size,prot)!=0)
		}
	}
}

// Prepares the code region for JIT operations, thus marking it as RWX
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rwx) {
	// Try to map is as RWX, this fails apparently on OSX (and perhaps other systems?)
	if (mprotect(code_area, size, PROT_READ | PROT_WRITE | PROT_EXEC)) {
		// Well it failed, use another approach, unmap the memory area and remap it back.
		// Seems it works well on Darwin according to reicast code :P
		munmap(code_area, size);
		void *ret_ptr = mmap(code_area, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE | MAP_ANON, 0, 0);
		// Ensure it's the area we requested
		if (ret_ptr != code_area)
			return false;   // Couldn't remap it? Perhaps RWX is disabled? This should never happen in any supported Unix platform.
	}

	// Pointer location should be same:
	*code_area_rwx = code_area;
	return true;
}

// Use two addr spaces: need to remap something twice, therefore use allocate_shared_filemem()
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rw, uintptr_t *rx_offset) {
	shmem_fd2 = allocate_shared_filemem(size);
	if (shmem_fd2 < 0)
		return false;

	// Need to unmap the section we are about to use (it might be already unmapped but nevertheless...)
	munmap(code_area, size);

	// Map the RX bits on the code_area, for proximity, as usual.
	void *ptr_rx = mmap(code_area, size, PROT_READ | PROT_EXEC,
	                    MAP_SHARED | MAP_NOSYNC | MAP_FIXED, shmem_fd2, 0);
	if (ptr_rx != code_area)
		return false;

	// Now remap the same memory as RW in some location we don't really care at all.
	void *ptr_rw = mmap(NULL, size, PROT_READ | PROT_WRITE,
	                    MAP_SHARED | MAP_NOSYNC, shmem_fd2, 0);

	*code_area_rw = ptr_rw;
	*rx_offset = (char*)ptr_rx - (char*)ptr_rw;
	printf("Info: Using NO_RWX mode, rx ptr: %p, rw ptr: %p, offset: %p\n", ptr_rx, ptr_rw, *rx_offset);

	return (ptr_rw != MAP_FAILED);
}


