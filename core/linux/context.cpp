#include "context.h"

#if defined(_ANDROID)
	#include <asm/sigcontext.h>
#else
	#if HOST_OS == OS_DARWIN
		#define _XOPEN_SOURCE 1
		#define __USE_GNU 1
	#endif

	#include <ucontext.h>
#endif


//////

#define MCTX(p) (((ucontext_t *)(segfault_ctx))->uc_mcontext p)
template <typename Ta, typename Tb>
void swap(Ta& a, Tb& b, bool reverse) {
	if (reverse) {
		b = a;
	}
	else {
		a = b;
	}
}

void context_segfault(rei_host_context_t* reictx, void* segfault_ctx, bool to_segfault) {

#if HOST_CPU == CPU_ARM
	#if HOST_OS == OS_LINUX
		swap(reictx->pc, MCTX(.arm_pc), to_segfault);
		u32* r =(u32*) &MCTX(.arm_r0);

		for (int i = 0; i < 15; i++)
			swap(reictx->r[i], r[i], to_segfault);

	#elif HOST_OS == OS_DARWIN
		swap(reictx->pc, MCTX(->__ss.__pc), to_segfault);

		for (int i = 0; i < 15; i++)
			swap(reictx->r[i], MCTX(->__ss.__r[i]), to_segfault);
	#else
		#error HOST_OS
	#endif
#elif HOST_CPU == CPU_X86
	#if HOST_OS == OS_LINUX
		swap(reictx->pc, MCTX(.gregs[REG_EIP]), to_segfault);
		swap(reictx->esp, MCTX(.gregs[REG_ESP]), to_segfault);
		swap(reictx->eax, MCTX(.gregs[REG_EAX]), to_segfault);
		swap(reictx->ecx, MCTX(.gregs[REG_ECX]), to_segfault);
	#elif HOST_OS == OS_DARWIN
		swap(reictx->pc, MCTX(->__ss.__eip), to_segfault);
		swap(reictx->esp, MCTX(->__ss.__esp), to_segfault);
		swap(reictx->eax, MCTX(->__ss.__eax), to_segfault);
		swap(reictx->ecx, MCTX(->__ss.__ecx), to_segfault);
	#else
		#error HOST_OS
	#endif
#elif HOST_CPU == CPU_MIPS
	swap(reictx->pc, MCTX(.pc), to_segfault);
#else
	#error Unsupported HOST_CPU
#endif
	
}

void context_from_segfault(rei_host_context_t* reictx, void* segfault_ctx) {
	context_segfault(reictx, segfault_ctx, false);
}

void context_to_segfault(rei_host_context_t* reictx, void* segfault_ctx) {
	context_segfault(reictx, segfault_ctx, true);
}