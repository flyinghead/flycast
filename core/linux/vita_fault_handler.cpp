#include <kubridge.h>

#include "hw/sh4/dyna/blockmanager.h"

#include "oslib/host_context.h"

#include "hw/sh4/dyna/ngen.h"
#include "rend/TexCache.h"
#include "hw/mem/_vmem.h"
#include "hw/mem/mem_watch.h"

static KuKernelAbortHandler nextHandler;

void context_to_segfault(host_context_t *hostctx, void *segfault_ctx)
{
	KuKernelAbortContext *abortContext = reinterpret_cast<KuKernelAbortContext *>(segfault_ctx);
	for (int i = 0; i < 15; i++)
		(&abortContext->r0)[i] = hostctx->reg[i];

	abortContext->pc = hostctx->pc;
}
void context_from_segfault(host_context_t *hostctx, void *segfault_ctx)
{
	KuKernelAbortContext *abortContext = reinterpret_cast<KuKernelAbortContext *>(segfault_ctx);
	for (int i = 0; i < 15; i++)
		hostctx->reg[i] = (&abortContext->r0)[i];

	hostctx->pc = abortContext->pc;
}

void abortHandler(KuKernelAbortContext *abortContext)
{
	// Ram watcher for net rollback
	if (memwatch::writeAccess((u8 *)abortContext->FAR))
		return;
	// code protection in RAM
	if (bm_RamWriteAccess((u8*)abortContext->FAR))
		return;
	// texture protection in VRAM
	if (VramLockedWrite((u8*)abortContext->FAR))
		return;
	// FPCB jump table protection
	if (BM_LockedWrite((u8*)abortContext->FAR))
		return;

#if FEAT_SHREC == DYNAREC_JIT
	// fast mem access rewriting
	host_context_t ctx;
	context_from_segfault(&ctx, abortContext);
	bool dyna_cde = ((unat)CC_RX2RW(ctx.pc) >= (unat)CodeCache) && ((unat)CC_RX2RW(ctx.pc) < (unat)(CodeCache + CODE_SIZE + TEMP_CODE_SIZE));

	if (dyna_cde && ngen_Rewrite(ctx, (void *)abortContext->FAR))
	{
		context_to_segfault(&ctx, abortContext);
		return;
	}
#endif
	ERROR_LOG(COMMON, "SIGSEGV @ %p -> %p was not in vram, dynacode:%d", (void *)ctx.pc, (void *)abortContext->FAR, dyna_cde);
	// abortContext->pc = abortContext->lr - 4;
	if (nextHandler != nullptr)
		nextHandler(abortContext);
	else
		die("segfault");
}

void os_InstallFaultHandler()
{
	DEBUG_LOG(VMEM, "In the abort handler");
	verify(kuKernelRegisterAbortHandler(abortHandler, &nextHandler, NULL) == 0);
}
void os_UninstallFaultHandler()
{
	kuKernelRegisterAbortHandler(nextHandler, NULL, NULL);
}