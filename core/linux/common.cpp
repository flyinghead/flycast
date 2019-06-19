#include "types.h"
#include "cfg/cfg.h"

#if HOST_OS==OS_LINUX || HOST_OS == OS_DARWIN
#if HOST_OS == OS_DARWIN
	#define _XOPEN_SOURCE 1
	#define __USE_GNU 1
	#include <TargetConditionals.h>
#endif
#if !defined(TARGET_NACL32)
#include <poll.h>
#include <termios.h>
#endif  
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/time.h>
#if !defined(TARGET_BSD) && !defined(_ANDROID) && !defined(TARGET_IPHONE) && !defined(TARGET_NACL32) && !defined(TARGET_EMSCRIPTEN) && !defined(TARGET_OSX) && !defined(TARGET_OSX_X64)
  #include <sys/personality.h>
  #include <dlfcn.h>
#endif
#include <unistd.h>
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/mem/vmem32.h"

#include "linux/context.h"

#include "hw/sh4/dyna/ngen.h"

#if !defined(TARGET_NO_EXCEPTIONS)
bool ngen_Rewrite(unat& addr,unat retadr,unat acc);
u32* ngen_readm_fail_v2(u32* ptr,u32* regs,u32 saddr);
bool VramLockedWrite(u8* address);
bool BM_LockedWrite(u8* address);

#if HOST_OS == OS_DARWIN
void sigill_handler(int sn, siginfo_t * si, void *segfault_ctx) {
	
    rei_host_context_t ctx;
    
    context_from_segfault(&ctx, segfault_ctx);

	unat pc = (unat)ctx.pc;
	bool dyna_cde = (pc>(unat)CodeCache) && (pc<(unat)(CodeCache + CODE_SIZE + TEMP_CODE_SIZE));
	
	printf("SIGILL @ %lx -> %p was not in vram, dynacode:%d\n", pc, si->si_addr, dyna_cde);
	
	//printf("PC is used here %08X\n", pc);
    kill(getpid(), SIGABRT);
}
#endif

void fault_handler (int sn, siginfo_t * si, void *segfault_ctx)
{
	rei_host_context_t ctx;
	context_from_segfault(&ctx, segfault_ctx);

	bool dyna_cde = ((unat)CC_RX2RW(ctx.pc) > (unat)CodeCache) && ((unat)CC_RX2RW(ctx.pc) < (unat)(CodeCache + CODE_SIZE + TEMP_CODE_SIZE));

#if !defined(NO_MMU) && defined(HOST_64BIT_CPU)
#if HOST_CPU == CPU_ARM64
	u32 op = *(u32*)ctx.pc;
	bool write = (op & 0x00400000) == 0;
	u32 exception_pc = ctx.x2;
#elif HOST_CPU == CPU_X64
	bool write = false;	// TODO?
	u32 exception_pc = 0;
#endif
	if (vmem32_handle_signal(si->si_addr, write, exception_pc))
		return;
#endif
	if (bm_RamWriteAccess(si->si_addr, ctx.pc))
		return;
	if (VramLockedWrite((u8*)si->si_addr) || BM_LockedWrite((u8*)si->si_addr))
		return;
	#if FEAT_SHREC == DYNAREC_JIT
		#if HOST_CPU==CPU_ARM
			else if (dyna_cde)
			{
				ctx.pc = (u32)ngen_readm_fail_v2((u32*)ctx.pc, ctx.r, (unat)si->si_addr);

				context_to_segfault(&ctx, segfault_ctx);
			}
		#elif HOST_CPU==CPU_X86
			else if (ngen_Rewrite((unat&)ctx.pc, *(unat*)ctx.esp, ctx.eax))
			{
				//remove the call from call stack
				ctx.esp += 4;
				//restore the addr from eax to ecx so it's valid again
				ctx.ecx = ctx.eax;

				context_to_segfault(&ctx, segfault_ctx);
			}
		#elif HOST_CPU == CPU_X64
			else if (dyna_cde && ngen_Rewrite((unat&)ctx.pc, 0, 0))
			{
				context_to_segfault(&ctx, segfault_ctx);
			}
		#elif HOST_CPU == CPU_ARM64
			else if (dyna_cde && ngen_Rewrite(ctx.pc, 0, 0))
			{
				context_to_segfault(&ctx, segfault_ctx);
			}
		#else
			#error JIT: Not supported arch
		#endif
	#endif
	else
	{
		printf("SIGSEGV @ %lx -> %p was not in vram, dynacode:%d\n", ctx.pc, si->si_addr, dyna_cde);
		die("segfault");
		signal(SIGSEGV, SIG_DFL);
	}
}

void install_fault_handler(void)
{
	struct sigaction act, segv_oact;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = fault_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act, &segv_oact);
#if HOST_OS == OS_DARWIN
    //this is broken on osx/ios/mach in general
    sigaction(SIGBUS, &act, &segv_oact);
    
    act.sa_sigaction = sigill_handler;
    sigaction(SIGILL, &act, &segv_oact);
#endif
}
#else  // !defined(TARGET_NO_EXCEPTIONS)
// No exceptions/nvmem dummy handlers.
void install_fault_handler(void) {}
#endif // !defined(TARGET_NO_EXCEPTIONS)

#include <errno.h>

double os_GetSeconds()
{
	timeval a;
	gettimeofday (&a,0);
	static u64 tvs_base=a.tv_sec;
	return a.tv_sec-tvs_base+a.tv_usec/1000000.0;
}

#if TARGET_IPHONE
void os_DebugBreak() {
    __asm__("trap");
}
#elif HOST_OS != OS_LINUX
void os_DebugBreak()
{
	__builtin_trap();
}
#endif

void enable_runfast()
{
	#if HOST_CPU==CPU_ARM && !defined(ARMCC)
	static const unsigned int x = 0x04086060;
	static const unsigned int y = 0x03000000;
	int r;
	asm volatile (
		"fmrx	%0, fpscr			\n\t"	//r0 = FPSCR
		"and	%0, %0, %1			\n\t"	//r0 = r0 & 0x04086060
		"orr	%0, %0, %2			\n\t"	//r0 = r0 | 0x03000000
		"fmxr	fpscr, %0			\n\t"	//FPSCR = r0
		: "=r"(r)
		: "r"(x), "r"(y)
	);

	printf("ARM VFP-Run Fast (NFP) enabled !\n");
	#endif
}

void linux_fix_personality() {
        #if !defined(TARGET_BSD) && !defined(_ANDROID) && !defined(TARGET_OS_MAC) && !defined(TARGET_NACL32) && !defined(TARGET_EMSCRIPTEN)
          printf("Personality: %08X\n", personality(0xFFFFFFFF));
          personality(~READ_IMPLIES_EXEC & personality(0xFFFFFFFF));
          printf("Updated personality: %08X\n", personality(0xFFFFFFFF));
        #endif
}

void linux_rpi2_init() {
#if !defined(TARGET_BSD) && !defined(_ANDROID) && !defined(TARGET_NACL32) && !defined(TARGET_EMSCRIPTEN) && defined(TARGET_VIDEOCORE)
	void* handle;
	void (*rpi_bcm_init)(void);

	handle = dlopen("libbcm_host.so", RTLD_LAZY);
	
	if (handle) {
		printf("found libbcm_host\n");
		*(void**) (&rpi_bcm_init) = dlsym(handle, "bcm_host_init");
		if (rpi_bcm_init) {
			printf("rpi2: bcm_init\n");
			rpi_bcm_init();
		}
	}
#endif
}

void common_linux_setup()
{
	linux_fix_personality();
	linux_rpi2_init();

	enable_runfast();
	install_fault_handler();
	signal(SIGINT, exit);
	
	settings.profile.run_counts=0;
	
	printf("Linux paging: %ld %08X %08X\n",sysconf(_SC_PAGESIZE),PAGE_SIZE,PAGE_MASK);
	verify(PAGE_MASK==(sysconf(_SC_PAGESIZE)-1));
}
#endif
