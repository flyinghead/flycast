#include "context.h"

#if defined(_ANDROID)
#include <asm/sigcontext.h>
#else
#include <ucontext.h>
#endif


//////

#define MCTX(p) (((ucontext_t *)(segfault_ctx))->uc_mcontext.p)
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
	swap(reictx->pc, MCTX(arm_pc), to_segfault);
#elif HOST_CPU == CPU_X86
	swap(reictx->pc, MCTX(gregs[REG_EIP]), to_segfault);
	swap(reictx->esp, MCTX(gregs[REG_ESP]), to_segfault);
	swap(reictx->eax, MCTX(gregs[REG_EAX]), to_segfault);
	swap(reictx->ecx, MCTX(gregs[REG_ECX]), to_segfault);
#elif HOST_CPU == CPU_MIPS
	swap(reictx->pc, MCTX(pc), to_segfault);
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