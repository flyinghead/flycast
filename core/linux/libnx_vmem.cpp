#if defined(__SWITCH__)
#include "hw/sh4/sh4_if.h"
#include "hw/mem/addrspace.h"
#include "oslib/virtmem.h"

#include <switch.h>
#include <malloc.h>

namespace virtmem
{

#define siginfo_t switch_siginfo_t

using mem_handle_t = uintptr_t;
static mem_handle_t vmem_fd = -1;
//static mem_handle_t vmem_fd_page = -1;
static mem_handle_t vmem_fd_codememory = -1;

static void *reserved_base;
static size_t reserved_size;
static VirtmemReservation *virtmemReservation;

bool region_lock(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	len += inpage;
	size_t inlen = len & PAGE_MASK;
	if (inlen)
		len = (len + PAGE_SIZE) & ~(PAGE_SIZE-1);

	Result rc;
	uintptr_t start_addr = (uintptr_t)start - inpage;
	for (uintptr_t addr = start_addr; addr < (start_addr + len); addr += PAGE_SIZE)
	{
		rc = svcSetMemoryPermission((void*)addr, PAGE_SIZE, Perm_R);
		if (R_FAILED(rc))
			WARN_LOG(VMEM, "Failed to SetPerm Perm_R on %p len 0x%x rc 0x%x", (void*)addr, PAGE_SIZE, rc);
	}

	return true;
}

bool region_unlock(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	len += inpage;
	size_t inlen = len & PAGE_MASK;
	if (inlen)
		len = (len + PAGE_SIZE) & ~(PAGE_SIZE-1);

	Result rc;
	uintptr_t start_addr = (uintptr_t)start - inpage;
	for (uintptr_t addr = start_addr; addr < (start_addr + len); addr += PAGE_SIZE)
	{
		rc = svcSetMemoryPermission((void*)addr, PAGE_SIZE, Perm_Rw);
		if (R_FAILED(rc))
			WARN_LOG(VMEM, "Failed to SetPerm Perm_Rw on %p len 0x%x rc 0x%x", (void*)addr, PAGE_SIZE, rc);
	}

	return true;
}

/*
static bool region_set_exec(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;

	svcSetMemoryPermission((void*)((uintptr_t)start - inpage), len + inpage, Perm_R); // *shrugs*

	return true;
}

static void *region_reserve(void *start, size_t len)
{
	virtmemLock();
	void *p = virtmemFindAslr(len, 0);
	if (p != nullptr)
		virtmemReservation = virtmemAddReservation(p, len);
	virtmemUnlock();
	return p;
}
*/

static bool region_release(void *start, size_t len)
{
	if (virtmemReservation != nullptr)
	{
		virtmemLock();
		virtmemRemoveReservation(virtmemReservation);
		virtmemUnlock();
		virtmemReservation = nullptr;
	}
	return true;
}

static void *region_map_file(void *file_handle, void *dest, size_t len, size_t offset, bool readwrite)
{
	Result rc = svcMapProcessMemory(dest, envGetOwnProcessHandle(), (u64)(vmem_fd_codememory + offset), len);
	if (R_FAILED(rc))
		WARN_LOG(VMEM, "Fatal error creating the view... base: %p offset: 0x%zx size: 0x%zx src: %p err: 0x%x",
				(void*)vmem_fd, offset, len, (void*)(vmem_fd_codememory + offset), rc);
	else
		INFO_LOG(VMEM, "Created the view... base: %p offset: 0x%zx size: 0x%zx src: %p err: 0x%x",
				(void*)vmem_fd, offset, len, (void*)(vmem_fd_codememory + offset), rc);

	return dest;
}

static bool region_unmap_file(void *start, size_t len)
{
	return region_release(start, len);
}

/*
// Allocates memory via a fd on shmem/ahmem or even a file on disk
static mem_handle_t allocate_shared_filemem(unsigned size)
{
	void* mem = memalign(0x1000, size);
	return (uintptr_t)mem;
}
*/

// Implement vmem initialization for RAM, ARAM, VRAM and SH4 context, fpcb etc.

// vmem_base_addr points to an address space of 512MB that can be used for fast memory ops.
// In negative offsets of the pointer (up to FPCB size, usually 65/129MB) the context and jump table
// can be found. If the platform init returns error, the user is responsible for initializing the
// memory using a fallback (that is, regular mallocs and falling back to slow memory JIT).
bool init(void **vmem_base_addr, void **sh4rcb_addr, size_t ramSize)
{
	return false;
#if 0
	const unsigned size_aligned = ((RAM_SIZE_MAX + VRAM_SIZE_MAX + ARAM_SIZE_MAX + PAGE_SIZE) & (~(PAGE_SIZE-1)));
	vmem_fd_page = allocate_shared_filemem(size_aligned);
	if (vmem_fd_page < 0)
		return false;

	vmem_fd_codememory = (uintptr_t)virtmemReserve(size_aligned);

	if (R_FAILED(svcMapProcessCodeMemory(envGetOwnProcessHandle(), (u64) vmem_fd_codememory, (u64) vmem_fd_page, size_aligned)))
		WARN_LOG(VMEM, "Failed to Map memory (platform_int)...");

	if (R_FAILED(svcSetProcessMemoryPermission(envGetOwnProcessHandle(), vmem_fd_codememory, size_aligned, Perm_Rx)))
		WARN_LOG(VMEM, "Failed to set perms (platform_int)...");

	// Now try to allocate a contiguous piece of memory.
	if (reserved_base == NULL)
	{
		reserved_size = 512*1024*1024 + sizeof(Sh4RCB) + ARAM_SIZE_MAX + 0x10000;
		reserved_base = region_reserve(NULL, reserved_size);
		if (!reserved_base)
			return false;
	}

	*sh4rcb_addr = reserved_base;
	*vmem_base_addr = (char *)reserved_base + sizeof(Sh4RCB);
	const size_t fpcb_size = sizeof(((Sh4RCB *)NULL)->fpcb);
	void *sh4rcb_base_ptr  = (char *)reserved_base + fpcb_size;

	// Now map the memory for the SH4 context, do not include FPCB on purpose (paged on demand).
	region_unlock(sh4rcb_base_ptr, sizeof(Sh4RCB) - fpcb_size);

	return true;
#endif
}

// Just tries to wipe as much as possible in the relevant area.
void destroy()
{
	if (reserved_base != NULL)
		region_release(reserved_base, reserved_size);
}

// Resets a chunk of memory by deleting its data and setting its protection back.
void reset_mem(void *ptr, unsigned size_bytes) {
	svcSetMemoryPermission(ptr, size_bytes, Perm_None);
}

// Allocates a bunch of memory (page aligned and page-sized)
void ondemand_page(void *address, unsigned size_bytes) {
	bool rc = region_unlock(address, size_bytes);
	verify(rc);
}

// Creates mappings to the underlying file including mirroring sections
void create_mappings(const Mapping *vmem_maps, unsigned nummaps)
{
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
			bool rc = region_unmap_file(&addrspace::ram_base[offset], vmem_maps[i].memsize);
			verify(rc);
			void *p = region_map_file((void*)(uintptr_t)vmem_fd, &addrspace::ram_base[offset],
					vmem_maps[i].memsize, vmem_maps[i].memoffset, vmem_maps[i].allow_writes);
			verify(p != nullptr);
		}
	}
}

// Prepares the code region for JIT operations, thus marking it as RWX
bool prepare_jit_block(void *code_area, size_t size, void **code_area_rwx)
{
	die("Not supported in libnx");

	return false;
}

void release_jit_block(void *code_area, size_t size)
{
	die("Not supported in libnx");
}

// Use two addr spaces: need to remap something twice, therefore use allocate_shared_filemem()
bool prepare_jit_block(void *code_area, size_t size, void **code_area_rw, ptrdiff_t *rx_offset)
{
	const size_t size_aligned = ((size + PAGE_SIZE) & (~(PAGE_SIZE-1)));

	virtmemLock();
	void* ptr_rw = virtmemFindAslr(size_aligned, 0);
	bool failure = ptr_rw == nullptr
			|| R_FAILED(svcMapProcessMemory(ptr_rw, envGetOwnProcessHandle(), (u64)code_area, size_aligned));
	virtmemUnlock();
	if (failure)
	{
		WARN_LOG(DYNAREC, "Failed to map jit rw block...");
		return false;
	}

	*code_area_rw = ptr_rw;
	*rx_offset = (char*)code_area - (char*)ptr_rw;
	INFO_LOG(DYNAREC, "Info: Using NO_RWX mode, rx ptr: %p, rw ptr: %p, offset: %ld\n", code_area, ptr_rw, (long)*rx_offset);

	return true;
}

void release_jit_block(void *code_area1, void *code_area2, size_t size)
{
	const size_t size_aligned = ((size + PAGE_SIZE) & (~(PAGE_SIZE-1)));
	virtmemLock();
	svcUnmapProcessMemory(code_area2, envGetOwnProcessHandle(), (u64)code_area1, size_aligned);
	virtmemUnlock();
}

} // namespace virtmem

#ifndef TARGET_NO_EXCEPTIONS

#include <ucontext.h>
void fault_handler(int sn, siginfo_t * si, void *segfault_ctx);

extern "C"
{
alignas(16) u8 __nx_exception_stack[0x1000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void context_switch_aarch64(void* context);

void __libnx_exception_handler(ThreadExceptionDump *ctx)
{
   mcontext_t m_ctx;

   m_ctx.pc = ctx->pc.x;

   for(int i=0; i<29; i++)
   {
      // printf("X%d: %p\n", i, ctx->cpu_gprs[i].x);
      m_ctx.regs[i] = ctx->cpu_gprs[i].x;
   }

   /*
   printf("PC: %p\n", ctx->pc.x);
   printf("FP: %p\n", ctx->fp.x);
   printf("LR: %p\n", ctx->lr.x);
   printf("SP: %p\n", ctx->sp.x);
   */

   ucontext_t u_ctx;
   u_ctx.uc_mcontext = m_ctx;

   siginfo_t sig_info;

   sig_info.si_addr = (void*)ctx->far.x;

   fault_handler(0, &sig_info, (void*) &u_ctx);

   uint64_t handle[64] = { 0 };

   uint64_t *ptr = (uint64_t*)handle;
   ptr[0]  = m_ctx.regs[0]; /* x0 0  */
   ptr[1]  = m_ctx.regs[1]; /* x1 8 */
   ptr[2]  = m_ctx.regs[2]; /* x2 16 */
   ptr[3]  = m_ctx.regs[3]; /* x3 24 */
   ptr[4]  = m_ctx.regs[4]; /* x4 32 */
   ptr[5]  = m_ctx.regs[5]; /* x5 40 */
   ptr[6]  = m_ctx.regs[6]; /* x6 48 */
   ptr[7]  = m_ctx.regs[7]; /* x7 56 */
   /* Non-volatiles.  */
   ptr[8]  = m_ctx.regs[8]; /* x8 64 */
   ptr[9]  = m_ctx.regs[9]; /* x9 72 */
   ptr[10]  = m_ctx.regs[10]; /* x10 80 */
   ptr[11]  = m_ctx.regs[11]; /* x11 88 */
   ptr[12]  = m_ctx.regs[12]; /* x12 96 */
   ptr[13]  = m_ctx.regs[13]; /* x13 104 */
   ptr[14]  = m_ctx.regs[14]; /* x14 112 */
   ptr[15]  = m_ctx.regs[15]; /* x15 120 */
   ptr[16]  = m_ctx.regs[16]; /* x16 128 */
   ptr[17]  = m_ctx.regs[17]; /* x17 136 */
   ptr[18]  = m_ctx.regs[18]; /* x18 144 */
   ptr[19]  = m_ctx.regs[19]; /* x19 152 */
   ptr[20] = m_ctx.regs[20]; /* x20 160 */
   ptr[21] = m_ctx.regs[21]; /* x21 168 */
   ptr[22] = m_ctx.regs[22]; /* x22 176 */
   ptr[23] = m_ctx.regs[23]; /* x23 184 */
   ptr[24] = m_ctx.regs[24]; /* x24 192 */
   ptr[25] = m_ctx.regs[25]; /* x25 200 */
   ptr[26] = m_ctx.regs[26]; /* x26 208 */
   ptr[27] = m_ctx.regs[27]; /* x27 216 */
   ptr[28] = m_ctx.regs[28]; /* x28 224 */
   /* Special regs */
   ptr[29] = ctx->fp.x; /* frame pointer 232 */
   ptr[30] = ctx->lr.x; /* link register 240 */
   ptr[31] = ctx->sp.x; /* stack pointer 248 */
   ptr[32] = (uintptr_t)ctx->pc.x; /* PC 256 */

   context_switch_aarch64(ptr);
}
}
#endif	// TARGET_NO_EXCEPTIONS
#endif	// __SWITCH__
