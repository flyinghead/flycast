
// Implementation of the vmem related function for POSIX-like platforms.
// There's some minimal amount of platform specific hacks to support
// Android and OSX since they are slightly different in some areas.

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>

#include "hw/mem/_vmem.h"
#include "hw/sh4/sh4_if.h"
#include "stdclass.h"

#ifndef MAP_NOSYNC
#define MAP_NOSYNC       0 //missing from linux :/ -- could be the cause of android slowness ?
#endif

#ifdef __ANDROID__
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
#endif  // #ifdef __ANDROID__

bool mem_region_lock(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	if (mprotect((u8*)start - inpage, len + inpage, PROT_READ))
		die("mprotect failed...");
	return true;
}

bool mem_region_unlock(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	if (mprotect((u8*)start - inpage, len + inpage, PROT_READ | PROT_WRITE))
		// Add some way to see why it failed? gdb> info proc mappings
		die("mprotect  failed...");
	return true;
}

bool mem_region_set_exec(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	if (mprotect((u8*)start - inpage, len + inpage, PROT_READ | PROT_WRITE | PROT_EXEC))
	{
		WARN_LOG(VMEM, "mem_region_set_exec: mprotect failed. errno %d", errno);
		return false;
	}
	return true;
}

void *mem_region_reserve(void *start, size_t len)
{
	void *p = mmap(start, len, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (p == MAP_FAILED)
	{
		perror("mmap");
		return NULL;
	}
	else
		return p;
}

bool mem_region_release(void *start, size_t len)
{
	return munmap(start, len) == 0;
}

void *mem_region_map_file(void *file_handle, void *dest, size_t len, size_t offset, bool readwrite)
{
	int flags = MAP_SHARED | MAP_NOSYNC | (dest != NULL ? MAP_FIXED : 0);
	void *p = mmap(dest, len, PROT_READ | (readwrite ? PROT_WRITE : 0), flags, (int)(uintptr_t)file_handle, offset);
	if (p == MAP_FAILED)
	{
		perror("mmap");
		return NULL;
	}
	else
		return p;
}

bool mem_region_unmap_file(void *start, size_t len)
{
	return mem_region_release(start, len);
}

// Allocates memory via a fd on shmem/ahmem or even a file on disk
static int allocate_shared_filemem(unsigned size) {
	int fd = -1;
	#if defined(__ANDROID__)
	// Use Android's specific shmem stuff.
	fd = ashmem_create_region(0, size);
	#else
		#if !defined(__APPLE__)
		fd = shm_open("/dcnzorz_mem", O_CREAT | O_EXCL | O_RDWR, S_IREAD | S_IWRITE);
		shm_unlink("/dcnzorz_mem");
		#endif

		// if shmem does not work (or using OSX) fallback to a regular file on disk
		if (fd < 0) {
			std::string path = get_writable_data_path("dcnzorz_mem");
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

int vmem_fd = -1;
static int shmem_fd2 = -1;
static void *reserved_base;
static size_t reserved_size;

// vmem_base_addr points to an address space of 512MB (or 4GB) that can be used for fast memory ops.
// In negative offsets of the pointer (up to FPCB size, usually 65/129MB) the context and jump table
// can be found. If the platform init returns error, the user is responsible for initializing the
// memory using a fallback (that is, regular mallocs and falling back to slow memory JIT).
VMemType vmem_platform_init(void **vmem_base_addr, void **sh4rcb_addr) {
	// Firt let's try to allocate the shm-backed memory
	vmem_fd = allocate_shared_filemem(RAM_SIZE_MAX + VRAM_SIZE_MAX + ARAM_SIZE_MAX);
	if (vmem_fd < 0)
		return MemTypeError;

	// Now try to allocate a contiguous piece of memory.
	VMemType rv;
#ifdef HOST_64BIT_CPU
	reserved_size = 0x100000000L + sizeof(Sh4RCB) + 0x10000;	// 4GB + context size + 64K padding
	reserved_base = mem_region_reserve(NULL, reserved_size);
	rv = MemType4GB;
#endif
	if (reserved_base == NULL)
	{
		reserved_size = 512*1024*1024 + sizeof(Sh4RCB) + ARAM_SIZE_MAX + 0x10000;
		reserved_base = mem_region_reserve(NULL, reserved_size);
		if (!reserved_base) {
			close(vmem_fd);
			return MemTypeError;
		}
		rv = MemType512MB;
	}

	// Align pointer to 64KB too, some Linaro bug (no idea but let's just be safe I guess).
	uintptr_t ptrint = (uintptr_t)reserved_base;
	ptrint = (ptrint + 0x10000 - 1) & (~0xffff);
	*sh4rcb_addr = (void*)ptrint;
	*vmem_base_addr = (void*)(ptrint + sizeof(Sh4RCB));
	const size_t fpcb_size = sizeof(((Sh4RCB *)NULL)->fpcb);
	void *sh4rcb_base_ptr  = (void*)(ptrint + fpcb_size);

	// Now map the memory for the SH4 context, do not include FPCB on purpose (paged on demand).
	mem_region_unlock(sh4rcb_base_ptr, sizeof(Sh4RCB) - fpcb_size);

	return rv;
}

// Just tries to wipe as much as possible in the relevant area.
void vmem_platform_destroy() {
	if (reserved_base != NULL)
		mem_region_release(reserved_base, reserved_size);
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
	verify(mem_region_unlock(address, size_bytes));
}

// Creates mappings to the underlying file including mirroring sections
void vmem_platform_create_mappings(const vmem_mapping *vmem_maps, unsigned nummaps) {
	for (unsigned i = 0; i < nummaps; i++) {
		// Ignore unmapped stuff, it is already reserved as PROT_NONE
		if (!vmem_maps[i].memsize)
			continue;

		// Calculate the number of mirrors
		u64 address_range_size = vmem_maps[i].end_address - vmem_maps[i].start_address;
		unsigned num_mirrors = (address_range_size) / vmem_maps[i].memsize;
		verify((address_range_size % vmem_maps[i].memsize) == 0 && num_mirrors >= 1);

		for (unsigned j = 0; j < num_mirrors; j++) {
			u64 offset = vmem_maps[i].start_address + j * vmem_maps[i].memsize;
			verify(mem_region_unmap_file(&virt_ram_base[offset], vmem_maps[i].memsize));
			verify(mem_region_map_file((void*)(uintptr_t)vmem_fd, &virt_ram_base[offset],
					vmem_maps[i].memsize, vmem_maps[i].memoffset, vmem_maps[i].allow_writes) != NULL);
		}
	}
}

// Prepares the code region for JIT operations, thus marking it as RWX
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rwx) {
	// Try to map is as RWX, this fails apparently on OSX (and perhaps other systems?)
	if (!mem_region_set_exec(code_area, size))
	{
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
	INFO_LOG(DYNAREC, "Info: Using NO_RWX mode, rx ptr: %p, rw ptr: %p, offset: %lu", ptr_rx, ptr_rw, (unsigned long)*rx_offset);

	return (ptr_rw != MAP_FAILED);
}

// Some OSes restrict cache flushing, cause why not right? :D

#if HOST_CPU == CPU_ARM64

// Code borrowed from Dolphin https://github.com/dolphin-emu/dolphin
static void Arm64_CacheFlush(void* start, void* end) {
	if (start == end)
		return;

#if defined(__APPLE__)
	// Header file says this is equivalent to: sys_icache_invalidate(start, end - start);
	sys_cache_control(kCacheFunctionPrepareForExecution, start, end - start);
#else
	// Don't rely on GCC's __clear_cache implementation, as it caches
	// icache/dcache cache line sizes, that can vary between cores on
	// big.LITTLE architectures.
	u64 addr, ctr_el0;
	static size_t icache_line_size = 0xffff, dcache_line_size = 0xffff;
	size_t isize, dsize;

	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr_el0));
	isize = 4 << ((ctr_el0 >> 0) & 0xf);
	dsize = 4 << ((ctr_el0 >> 16) & 0xf);

	// use the global minimum cache line size
	icache_line_size = isize = icache_line_size < isize ? icache_line_size : isize;
	dcache_line_size = dsize = dcache_line_size < dsize ? dcache_line_size : dsize;

	addr = (u64)start & ~(u64)(dsize - 1);
	for (; addr < (u64)end; addr += dsize)
		// use "civac" instead of "cvau", as this is the suggested workaround for
		// Cortex-A53 errata 819472, 826319, 827319 and 824069.
		__asm__ volatile("dc civac, %0" : : "r"(addr) : "memory");
	__asm__ volatile("dsb ish" : : : "memory");

	addr = (u64)start & ~(u64)(isize - 1);
	for (; addr < (u64)end; addr += isize)
		__asm__ volatile("ic ivau, %0" : : "r"(addr) : "memory");

	__asm__ volatile("dsb ish" : : : "memory");
	__asm__ volatile("isb" : : : "memory");
#endif
}


void vmem_platform_flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end) {
	Arm64_CacheFlush(dcache_start, dcache_end);

	// Dont risk it and flush and invalidate icache&dcache for both ranges just in case.
	if (icache_start != dcache_start)
		Arm64_CacheFlush(icache_start, icache_end);
}

#endif // #if HOST_CPU == CPU_ARM64

