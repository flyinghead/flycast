#if defined(__SWITCH__)
#include "hw/sh4/sh4_if.h"
#include "hw/mem/addrspace.h"
#include "oslib/virtmem.h"

#include <switch.h>
#include <malloc.h>
#include <unordered_set>

namespace virtmem
{

#define siginfo_t switch_siginfo_t

// Allocated RAM
static void *ramBase;
static size_t ramSize;

// Reserved memory space
static void *reserved_base;
static size_t reserved_size;
static VirtmemReservation *virtmemReservation;

// Mapped regions
static Mapping *memMappings;
static u32 memMappingCount;

static void deleteMappings();

bool region_lock(void *start, size_t len)
{
	const size_t inpage = (uintptr_t)start & PAGE_MASK;
	len = (len + inpage + PAGE_SIZE - 1) & ~PAGE_MASK;

	Result rc;
	uintptr_t start_addr = (uintptr_t)start - inpage;
	for (uintptr_t addr = start_addr; addr < (start_addr + len); addr += PAGE_SIZE)
	{
		rc = svcSetMemoryPermission((void *)addr, PAGE_SIZE, Perm_R);
		if (R_FAILED(rc))
			ERROR_LOG(VMEM, "Failed to SetPerm Perm_R on %p len 0x%x rc 0x%x", (void*)addr, PAGE_SIZE, rc);
	}

	return true;
}

bool region_unlock(void *start, size_t len)
{
	const size_t inpage = (uintptr_t)start & PAGE_MASK;
	len = (len + inpage + PAGE_SIZE - 1) & ~PAGE_MASK;

	Result rc;
	uintptr_t start_addr = (uintptr_t)start - inpage;
	for (uintptr_t addr = start_addr; addr < (start_addr + len); addr += PAGE_SIZE)
	{
		rc = svcSetMemoryPermission((void *)addr, PAGE_SIZE, Perm_Rw);
		if (R_FAILED(rc))
			ERROR_LOG(VMEM, "Failed to SetPerm Perm_Rw on %p len 0x%x rc 0x%x", (void*)addr, PAGE_SIZE, rc);
	}

	return true;
}

// Implement vmem initialization for RAM, ARAM, VRAM and SH4 context, fpcb etc.

// vmem_base_addr points to an address space of 512MB that can be used for fast memory ops.
// In negative offsets of the pointer (up to FPCB size, usually 65/129MB) the context and jump table
// can be found. If the platform init returns error, the user is responsible for initializing the
// memory using a fallback (that is, regular mallocs and falling back to slow memory JIT).
bool init(void **vmem_base_addr, void **sh4rcb_addr, size_t ramSize)
{
	// Allocate RAM
	ramSize += sizeof(Sh4RCB);
	virtmem::ramSize = ramSize;
	ramBase = memalign(PAGE_SIZE, ramSize);
	if (ramBase == nullptr) {
		ERROR_LOG(VMEM, "memalign(%zx) failed", ramSize);
		return false;
	}

	// Reserve address space
	reserved_size = 512_MB + sizeof(Sh4RCB) + ARAM_SIZE_MAX;
	virtmemLock();
    // virtual mem
    reserved_base = virtmemFindCodeMemory(reserved_size, PAGE_SIZE);
	if (reserved_base != nullptr)
		virtmemReservation = virtmemAddReservation(reserved_base, reserved_size);
	virtmemUnlock();
	if (reserved_base == nullptr || virtmemReservation == nullptr)
	{
		ERROR_LOG(VMEM, "virtmemReserve(%zx) failed: base %p res %p", reserved_size, reserved_base, virtmemReservation);
		free(ramBase);
		return false;
	}

	constexpr size_t fpcb_size = sizeof(Sh4RCB::fpcb);
	void *sh4rcb_base_ptr  = (u8 *)reserved_base + fpcb_size;
	Handle process = envGetOwnProcessHandle();

	// Now map the memory for the SH4 context, do not include FPCB on purpose (paged on demand).
	size_t sz = sizeof(Sh4RCB) - fpcb_size;
	DEBUG_LOG(VMEM, "mapping %lx to %p size %zx", (u64)ramBase + fpcb_size, sh4rcb_base_ptr, sz);
	Result rc = svcMapProcessCodeMemory(process, (u64)sh4rcb_base_ptr, (u64)ramBase + fpcb_size, sz);
	if (R_FAILED(rc))
	{
		ERROR_LOG(VMEM, "Failed to Map Sh4RCB (%p, %p, %zx) -> %x", sh4rcb_base_ptr, (u8 *)ramBase + fpcb_size, sz, rc);
		destroy();
		return false;
	}
	rc = svcSetProcessMemoryPermission(process, (u64)sh4rcb_base_ptr, sz, Perm_Rw);
	if (R_FAILED(rc))
	{
		ERROR_LOG(VMEM, "Failed to set Sh4RCB perms (%p, %zx) -> %x", sh4rcb_base_ptr, sz, rc);
		destroy();
		return false;
	}
	*sh4rcb_addr = reserved_base;
	*vmem_base_addr = (u8 *)reserved_base + sizeof(Sh4RCB);
	NOTICE_LOG(VMEM, "virtmem::init successful");

	return true;
}

void destroy()
{
	deleteMappings();
	constexpr size_t fpcb_size = sizeof(Sh4RCB::fpcb);
	Result rc = svcUnmapProcessCodeMemory(envGetOwnProcessHandle(), (u64)reserved_base + fpcb_size,
			(u64)ramBase + fpcb_size, sizeof(Sh4RCB) - fpcb_size);
	if (R_FAILED(rc))
		ERROR_LOG(VMEM, "Failed to Unmap Sh4RCB -> %x", rc);
	if (virtmemReservation != nullptr)
	{
		virtmemLock();
		virtmemRemoveReservation(virtmemReservation);
		virtmemUnlock();
		virtmemReservation = nullptr;
	}
	free(ramBase);
	ramBase = nullptr;
	NOTICE_LOG(VMEM, "virtmem::destroy done");
}

// Flush (unmap) the FPCB array
void reset_mem(void *ptr, unsigned size)
{
	u64 offset = (u8 *)ptr - (u8 *)reserved_base;
	DEBUG_LOG(VMEM, "Unmapping %lx from %p size %x", (u64)ramBase + offset, ptr, size);
	Handle process = envGetOwnProcessHandle();
	for (; offset < size; offset += PAGE_SIZE)
	{
		svcUnmapProcessCodeMemory(process, (u64)ptr + offset, (u64)ramBase + offset, PAGE_SIZE);
		// fails if page is unmapped, which isn't an error
	}
}

// Allocates a memory page for the FPCB array
void ondemand_page(void *address, unsigned size)
{
	u64 offset = (u8 *)address - (u8 *)reserved_base;
	//DEBUG_LOG(VMEM, "Mapping %lx to %p size %x", (u64)ramBase + offset, address, size);
	Result rc = svcMapProcessCodeMemory(envGetOwnProcessHandle(), (u64)address, (u64)ramBase + offset, size);
	if (R_FAILED(rc)) {
		ERROR_LOG(VMEM, "svcMapProcessCodeMemory(%p, %lx, %x) failed: %x", address, (u64)ramBase + offset, size, rc);
		return;
	}
	rc = svcSetProcessMemoryPermission(envGetOwnProcessHandle(), (u64)address, size, Perm_Rw);
	if (R_FAILED(rc))
		ERROR_LOG(VMEM, "svcSetProcessMemoryPermission(%p, %x) failed: %x", address, size, rc);
}

static void deleteMappings()
{
	if (memMappings == nullptr)
		return;
	std::unordered_set<u64> backingAddrs;
	Handle process = envGetOwnProcessHandle();
	for (u32 i = 0; i < memMappingCount; i++)
	{
		// Ignore unmapped stuff
		if (memMappings[i].memsize == 0)
			continue;
		if (!memMappings[i].allow_writes) // don't (un)map ARAM read-only
			continue;

		u64 ramOffset = (u64)ramBase + sizeof(Sh4RCB) + memMappings[i].memoffset;
		if (backingAddrs.count(ramOffset) != 0)
			continue;
		backingAddrs.insert(ramOffset);
		u64 offset = memMappings[i].start_address + sizeof(Sh4RCB);
		Result rc = svcUnmapProcessCodeMemory(process, (u64)reserved_base + offset,
					ramOffset, memMappings[i].memsize);
		if (R_FAILED(rc))
			ERROR_LOG(VMEM, "deleteMappings: error unmapping offset %lx from %lx", memMappings[i].memoffset, offset);
	}
	delete [] memMappings;
	memMappings = nullptr;
	memMappingCount = 0;
}

// Creates mappings to the underlying ram (not including mirroring sections)
void create_mappings(const Mapping *vmem_maps, unsigned nummaps)
{
	deleteMappings();
	memMappingCount = nummaps;
	memMappings = new Mapping[nummaps];
	memcpy(memMappings, vmem_maps, nummaps * sizeof(Mapping));

	std::unordered_set<u64> backingAddrs;
	Handle process = envGetOwnProcessHandle();
	for (u32 i = 0; i < nummaps; i++)
	{
		// Ignore unmapped stuff, it is already reserved as PROT_NONE
		if (vmem_maps[i].memsize == 0)
			continue;
		if (!vmem_maps[i].allow_writes) // don't map ARAM read-only
			continue;

		u64 ramOffset = (u64)ramBase + sizeof(Sh4RCB) + vmem_maps[i].memoffset;
		if (backingAddrs.count(ramOffset) != 0)
			// already mapped once so ignore other mirrors
			continue;
		backingAddrs.insert(ramOffset);
		u64 offset = vmem_maps[i].start_address + sizeof(Sh4RCB);
		Result rc = svcMapProcessCodeMemory(process, (u64)reserved_base + offset, ramOffset, vmem_maps[i].memsize);
		if (R_FAILED(rc)) {
			ERROR_LOG(VMEM, "create_mappings: error mapping offset %lx to %lx", vmem_maps[i].memoffset, offset);
		}
		else
		{
			rc = svcSetProcessMemoryPermission(process, (u64)reserved_base + offset, vmem_maps[i].memsize, Perm_Rw);
			if (R_FAILED(rc))
				ERROR_LOG(VMEM, "svcSetProcessMemoryPermission() failed: %x", rc);
			else
				DEBUG_LOG(VMEM, "create_mappings: mapped offset %lx to %lx size %lx", vmem_maps[i].memoffset, offset, vmem_maps[i].memsize);
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
		ERROR_LOG(DYNAREC, "Failed to map jit rw block...");
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
	alignas(16) static ucontext_t u_ctx;
	u_ctx.uc_mcontext.pc = ctx->pc.x;
	for (int i = 0; i < 29; i++)
		u_ctx.uc_mcontext.regs[i] = ctx->cpu_gprs[i].x;

	siginfo_t sig_info;
	sig_info.si_addr = (void*)ctx->far.x;

	fault_handler(0, &sig_info, (void *)&u_ctx);

	alignas(16) static uint64_t savedRegisters[66];
	uint64_t *ptr = savedRegisters;
	// fpu registers (64 bits)
	for (int i = 0; i < 32; i++)
		ptr[i]  = *(u64 *)&ctx->fpu_gprs[i].d;
	// cpu gp registers
	for (int i = 0; i < 29; i++)
		ptr[i + 32]  = u_ctx.uc_mcontext.regs[i];
	// Special regs
	ptr[29 + 32] = ctx->fp.x;	// frame pointer
	ptr[30 + 32] = ctx->lr.x;	// link register
	ptr[31 + 32] = ctx->pstate;	// sprs
	ptr[32 + 32] = ctx->sp.x;	// stack pointer
	ptr[33 + 32] = u_ctx.uc_mcontext.pc; // PC

	context_switch_aarch64(ptr);
}
}
#endif	// TARGET_NO_EXCEPTIONS

#ifndef LIBRETRO
[[noreturn]] void os_DebugBreak()
{
	diagAbortWithResult(MAKERESULT(350, 1));
}
#endif
#endif	// __SWITCH__
