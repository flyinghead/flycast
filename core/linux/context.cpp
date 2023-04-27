#include "oslib/host_context.h"

#if defined(__ANDROID__)
	#include <asm/sigcontext.h>
#else
	#if defined(__APPLE__)
		#define _XOPEN_SOURCE 1
		#define __USE_GNU 1
	#endif

	#if !defined(TARGET_NO_EXCEPTIONS) && !defined(__OpenBSD__)
		#include <ucontext.h>
	#endif

	#if defined(__OpenBSD__)
		#include <signal.h>
	#endif

#endif


//////

#if defined(__OpenBSD__)
#define MCTX(p) (((ucontext_t *)(segfault_ctx)) p)
#else
#define MCTX(p) (((ucontext_t *)(segfault_ctx))->uc_mcontext p)
#endif
template <bool ToSegfault, typename Tctx, typename Tseg>
static void bicopy(Tctx& ctx, Tseg& seg)
{
	static_assert(sizeof(Tctx) == sizeof(Tseg), "Invalid assignment");
	if (ToSegfault)
		seg = (Tseg)ctx;
	else
		ctx = (Tctx)seg;
}

template<bool ToSegfault>
static void context_segfault(host_context_t* hostctx, void* segfault_ctx)
{
#if !defined(TARGET_NO_EXCEPTIONS)
#if HOST_CPU == CPU_ARM
	#if defined(__FreeBSD__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(.__gregs[_REG_PC]));

		for (int i = 0; i < 15; i++)
			bicopy<ToSegfault>(hostctx->reg[i], MCTX(.__gregs[i]));
	#elif defined(__unix__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(.arm_pc));
		u32* reg =(u32*) &MCTX(.arm_r0);

		for (int i = 0; i < 15; i++)
			bicopy<ToSegfault>(hostctx->reg[i], reg[i]);

	#elif defined(__APPLE__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(->__ss.__pc));

		for (int i = 0; i < 15; i++)
			bicopy<ToSegfault>(hostctx->reg[i], MCTX(->__ss.__r[i]));
	#else
		#error "Unsupported OS"
	#endif
#elif HOST_CPU == CPU_ARM64
	#if defined(__APPLE__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(->__ss.__pc));
		bicopy<ToSegfault>(hostctx->x0, MCTX(->__ss.__x[0]));
 	#else
 		bicopy<ToSegfault>(hostctx->pc, MCTX(.pc));
 		bicopy<ToSegfault>(hostctx->x0, MCTX(.regs[0]));
 	#endif
#elif HOST_CPU == CPU_X86
	#if defined(__FreeBSD__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(.mc_eip));
		bicopy<ToSegfault>(hostctx->esp, MCTX(.mc_esp));
		bicopy<ToSegfault>(hostctx->eax, MCTX(.mc_eax));
		bicopy<ToSegfault>(hostctx->ecx, MCTX(.mc_ecx));
	#elif defined(__unix__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(.gregs[REG_EIP]));
		bicopy<ToSegfault>(hostctx->esp, MCTX(.gregs[REG_ESP]));
		bicopy<ToSegfault>(hostctx->eax, MCTX(.gregs[REG_EAX]));
		bicopy<ToSegfault>(hostctx->ecx, MCTX(.gregs[REG_ECX]));
	#elif defined(__APPLE__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(->__ss.__eip));
		bicopy<ToSegfault>(hostctx->esp, MCTX(->__ss.__esp));
		bicopy<ToSegfault>(hostctx->eax, MCTX(->__ss.__eax));
		bicopy<ToSegfault>(hostctx->ecx, MCTX(->__ss.__ecx));
	#else
		#error "Unsupported OS"
	#endif
#elif HOST_CPU == CPU_X64
	#if defined(__FreeBSD__) || defined(__DragonFly__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(.mc_rip));
	#elif defined(__OpenBSD__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(->sc_rip));
		bicopy<ToSegfault>(hostctx->rsp, MCTX(->sc_rsp));
		bicopy<ToSegfault>(hostctx->r9, MCTX(->sc_r9));
		bicopy<ToSegfault>(hostctx->rdi, MCTX(->sc_rdi));
	#elif defined(__NetBSD__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(.__gregs[_REG_RIP]));
		bicopy<ToSegfault>(hostctx->rsp, MCTX(.__gregs[_REG_RSP]));
		bicopy<ToSegfault>(hostctx->r9, MCTX(.__gregs[_REG_R9]));
		bicopy<ToSegfault>(hostctx->rdi, MCTX(.__gregs[_REG_RDI]));
	#elif defined(__unix__)
		bicopy<ToSegfault>(hostctx->pc, MCTX(.gregs[REG_RIP]));
		bicopy<ToSegfault>(hostctx->rsp, MCTX(.gregs[REG_RSP]));
		bicopy<ToSegfault>(hostctx->r9, MCTX(.gregs[REG_R9]));
		bicopy<ToSegfault>(hostctx->rdi, MCTX(.gregs[REG_RDI]));
    #elif defined(__APPLE__)
        bicopy<ToSegfault>(hostctx->pc, MCTX(->__ss.__rip));
		bicopy<ToSegfault>(hostctx->rsp, MCTX(->__ss.__rsp));
		bicopy<ToSegfault>(hostctx->r9, MCTX(->__ss.__r9));
		bicopy<ToSegfault>(hostctx->rdi, MCTX(->__ss.__rdi));
    #else
        #error "Unsupported OS"
	#endif
#elif HOST_CPU == CPU_MIPS
	bicopy<ToSegfault>(hostctx->pc, MCTX(.pc));
#elif HOST_CPU == CPU_GENERIC
    //nothing!
#else
	#error Unsupported HOST_CPU
#endif
	#endif
	
}

void context_from_segfault(host_context_t* hostctx, void* segfault_ctx) {
	context_segfault<false>(hostctx, segfault_ctx);
}

void context_to_segfault(host_context_t* hostctx, void* segfault_ctx) {
	context_segfault<true>(hostctx, segfault_ctx);
}
