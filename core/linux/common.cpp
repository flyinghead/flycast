#include "types.h"

#if defined(__unix__) || defined(__APPLE__) || defined(__SWITCH__)
#if defined(__APPLE__)
	#define _XOPEN_SOURCE 1
	#define __USE_GNU 1
	#include <TargetConditionals.h>
#endif
#include <csignal>
#include <sys/time.h>
#if defined(__linux__) && !defined(__ANDROID__)
  #include <sys/personality.h>
#endif
#include <unistd.h>
#ifdef __linux__
#include <pthread.h>
#endif

#include "oslib/host_context.h"

#include "hw/sh4/dyna/ngen.h"
#include "rend/TexCache.h"
#include "hw/mem/addrspace.h"
#include "hw/mem/mem_watch.h"
#include "emulator.h"

#ifdef __SWITCH__
#include <ucontext.h>
extern "C" char __start__;
#define siginfo_t switch_siginfo_t
#endif // __SWITCH__

#if !defined(TARGET_NO_EXCEPTIONS)

void context_from_segfault(host_context_t* hctx, void* segfault_ctx);
void context_to_segfault(host_context_t* hctx, void* segfault_ctx);

#ifndef __SWITCH__
static struct sigaction next_segv_handler;
#endif
#if defined(__APPLE__)
static struct sigaction next_bus_handler;
#endif

void fault_handler(int sn, siginfo_t * si, void *segfault_ctx)
{
	// Ram watcher for net rollback
	if (memwatch::writeAccess(si->si_addr))
		return;
	// code protection in RAM
	if (bm_RamWriteAccess(si->si_addr))
		return;
	// texture protection in VRAM
	if (VramLockedWrite((u8*)si->si_addr))
		return;
	// FPCB jump table protection
	if (addrspace::bm_lockedWrite((u8*)si->si_addr))
		return;

#if FEAT_SHREC == DYNAREC_JIT
	// fast mem access rewriting
	host_context_t ctx;
	context_from_segfault(&ctx, segfault_ctx);
	if (sh4Dynarec->rewrite(ctx, si->si_addr))
	{
		context_to_segfault(&ctx, segfault_ctx);
		return;
	}

	ERROR_LOG(COMMON, "SIGSEGV @ %p invalid access to %p", (void *)ctx.pc, si->si_addr);
#endif

#ifdef __SWITCH__
	MemoryInfo meminfo;
	u32 pageinfo;
	svcQueryMemory(&meminfo, &pageinfo, (u64)&__start__);
	ERROR_LOG(COMMON, ".text base: %p -> offset: %lx", (void*)meminfo.addr, ctx.pc - meminfo.addr);
#else
	if (next_segv_handler.sa_sigaction != nullptr)
		next_segv_handler.sa_sigaction(sn, si, segfault_ctx);
	else
#endif // !__SWITCH__
		die("segfault");
}
#undef HOST_CTX_READY

void os_InstallFaultHandler()
{
#ifndef __SWITCH__
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = fault_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act, &next_segv_handler);
#endif
#if defined(__APPLE__)
    //this is broken on osx/ios/mach in general
    sigaction(SIGBUS, &act, &next_bus_handler);
#endif
}

void os_UninstallFaultHandler()
{
#ifndef __SWITCH__
	sigaction(SIGSEGV, &next_segv_handler, nullptr);
#endif
#if defined(__APPLE__)
	sigaction(SIGBUS, &next_bus_handler, nullptr);
#endif
}

#else  // !defined(TARGET_NO_EXCEPTIONS)

void os_InstallFaultHandler()
{
}
void os_UninstallFaultHandler()
{
}
#endif // !defined(TARGET_NO_EXCEPTIONS)

#if !defined(__unix__) && !defined(LIBRETRO) && !defined(__SWITCH__)
[[noreturn]] void os_DebugBreak()
{
	__builtin_trap();
}
#endif

// RunFast mode is the combination of the following conditions:
// * the VFP11 coprocessor is in flush-to-zero mode
// * the VFP11 coprocessor is in default NaN mode
// * all exception enable bits are cleared.
static void enable_runfast()
{
#if HOST_CPU == CPU_ARM && !defined(ARMCC)
	static const unsigned int x = 0x04086060;	// reset and disable FP exceptions, flush-to-zero, default NaN mode
	static const unsigned int y = 0x03000000;	// round to zero
	int r;
	asm volatile (
		"fmrx	%0, fpscr			\n\t"	//r0 = FPSCR
		"and	%0, %0, %1			\n\t"	//r0 = r0 & 0x04086060
		"orr	%0, %0, %2			\n\t"	//r0 = r0 | 0x03000000
		"fmxr	fpscr, %0			\n\t"	//FPSCR = r0
		: "=r"(r)
		: "r"(x), "r"(y)
	);

	DEBUG_LOG(BOOT, "ARM VFP-Run Fast (NFP) enabled !");
#endif
}

// Some old CPUs lack the NX (no exec) flag so READ_IMPLIES_EXEC is set by default on these platforms.
// However resetting the flag isn't going to magically change the way the CPU works. So I wonder how useful this is.
// It's not needed on modern 64-bit architectures anyway.
static void linux_fix_personality()
{
#if defined(__linux__) && !defined(__ANDROID__) && (HOST_CPU == CPU_X86 || HOST_CPU == CPU_ARM)
	DEBUG_LOG(BOOT, "Personality: %08X", personality(0xFFFFFFFF));
	personality(~READ_IMPLIES_EXEC & personality(0xFFFFFFFF));
	DEBUG_LOG(BOOT, "Updated personality: %08X", personality(0xFFFFFFFF));
#endif
}

#if defined(__unix__) && !defined(LIBRETRO) && !defined(__ANDROID__)
static void sigintHandler(int)
{
	dc_exit();
}
#endif

void common_linux_setup()
{
	linux_fix_personality();

	enable_runfast();
	os_InstallFaultHandler();
#if defined(__unix__) && !defined(LIBRETRO) && !defined(__ANDROID__)
	// exit cleanly on ^C
	signal(SIGINT, sigintHandler);
#endif
	
	DEBUG_LOG(BOOT, "Linux paging: %ld %08X %08X", sysconf(_SC_PAGESIZE), PAGE_SIZE, PAGE_MASK);
	verify(PAGE_MASK==(sysconf(_SC_PAGESIZE)-1));
}

#ifndef __APPLE__

void os_SetThreadName(const char *name)
{
#ifdef __linux__
	if (strlen(name) > 16)
	{
		static char tmp[17];
		strncpy(tmp, name, 16);
		name = tmp;
	}
	pthread_setname_np(pthread_self(), name);
#endif
}

#endif

#endif	// __unix__ or __APPLE__ or __SWITCH__
