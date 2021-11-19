/*
	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "oslib/oslib.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/sh4/dyna/ngen.h"
#include "rend/TexCache.h"
#include "hw/mem/_vmem.h"
#include "hw/mem/mem_watch.h"
#include <windows.h>

static PVOID vectoredHandler;
static LONG (WINAPI *prevExceptionHandler)(EXCEPTION_POINTERS *ep);

static void readContext(const EXCEPTION_POINTERS *ep, host_context_t &context)
{
#if HOST_CPU == CPU_X86
	context.pc = ep->ContextRecord->Eip;
	context.esp = ep->ContextRecord->Esp;
	context.eax = ep->ContextRecord->Eax;
	context.ecx = ep->ContextRecord->Ecx;
#elif HOST_CPU == CPU_X64
	context.pc = ep->ContextRecord->Rip;
	context.rsp = ep->ContextRecord->Rsp;
	context.r9 = ep->ContextRecord->R9;
	context.rcx = ep->ContextRecord->Rcx;
#endif
}

static void writeContext(EXCEPTION_POINTERS *ep, const host_context_t &context)
{
#if HOST_CPU == CPU_X86
	ep->ContextRecord->Eip = context.pc;
	ep->ContextRecord->Esp = context.esp;
	ep->ContextRecord->Eax = context.eax;
	ep->ContextRecord->Ecx = context.ecx;
#elif HOST_CPU == CPU_X64
	ep->ContextRecord->Rip = context.pc;
	ep->ContextRecord->Rsp = context.rsp;
	ep->ContextRecord->R9 = context.r9;
	ep->ContextRecord->Rcx = context.rcx;
#endif
}

static LONG WINAPI exceptionHandler(EXCEPTION_POINTERS *ep)
{
	u32 dwCode = ep->ExceptionRecord->ExceptionCode;

	if (dwCode < 0x80000000u)
		// software exceptions, debug messages
		return EXCEPTION_CONTINUE_SEARCH;

	if (dwCode != EXCEPTION_ACCESS_VIOLATION)
	{
		// Call the previous unhandled exception handler (presumably Breakpad) if any and terminate
	    if (prevExceptionHandler != nullptr)
	    {
	    	LONG action = prevExceptionHandler(ep);
	    	if (action != EXCEPTION_EXECUTE_HANDLER)
	    		return action;
	    }
		RaiseFailFastException(ep->ExceptionRecord, ep->ContextRecord, 0);
    	return EXCEPTION_CONTINUE_SEARCH;
	}

	EXCEPTION_RECORD* pExceptionRecord = ep->ExceptionRecord;
	u8* address = (u8 *)pExceptionRecord->ExceptionInformation[1];

	// Ram watcher for net rollback
	if (memwatch::writeAccess(address))
		return EXCEPTION_CONTINUE_EXECUTION;
	// code protection in RAM
	if (bm_RamWriteAccess(address))
		return EXCEPTION_CONTINUE_EXECUTION;
	// texture protection in VRAM
	if (VramLockedWrite(address))
		return EXCEPTION_CONTINUE_EXECUTION;
	// FPCB jump table protection
	if (BM_LockedWrite(address))
		return EXCEPTION_CONTINUE_EXECUTION;

	host_context_t context;
	readContext(ep, context);
#if FEAT_SHREC == DYNAREC_JIT
	// fast mem access rewriting
	if (ngen_Rewrite(context, address))
	{
		writeContext(ep, context);
		return EXCEPTION_CONTINUE_EXECUTION;
	}
#endif

	ERROR_LOG(COMMON, "[GPF] PC %p unhandled access to %p", (void *)context.pc, address);
	if (prevExceptionHandler != nullptr)
		prevExceptionHandler(ep);

	RaiseFailFastException(ep->ExceptionRecord, ep->ContextRecord, 0);
	return EXCEPTION_CONTINUE_SEARCH;
}

void os_InstallFaultHandler()
{
#if defined(_WIN64) && !defined(TARGET_UWP)
	prevExceptionHandler = SetUnhandledExceptionFilter(nullptr);
	vectoredHandler = AddVectoredExceptionHandler(1, exceptionHandler);
#else
	prevExceptionHandler = SetUnhandledExceptionFilter(exceptionHandler);
	(void)vectoredHandler;
#endif
}

void os_UninstallFaultHandler()
{
#if defined(_WIN64) && !defined(TARGET_UWP)
	RemoveVectoredExceptionHandler(vectoredHandler);
#endif
	SetUnhandledExceptionFilter(prevExceptionHandler);
}

double os_GetSeconds()
{
	static double qpfd = []() {
		LARGE_INTEGER qpf;
		QueryPerformanceFrequency(&qpf);
		return 1.0 / qpf.QuadPart; }();

	LARGE_INTEGER time_now;

	QueryPerformanceCounter(&time_now);
	static LARGE_INTEGER time_now_base = time_now;

	return (time_now.QuadPart - time_now_base.QuadPart) * qpfd;
}
