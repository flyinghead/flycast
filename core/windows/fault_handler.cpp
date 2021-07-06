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
#include "fault_handler.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/sh4/dyna/ngen.h"

bool VramLockedWrite(u8* address);
bool BM_LockedWrite(u8* address);

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

LONG exceptionHandler(EXCEPTION_POINTERS *ep)
{
	u32 dwCode = ep->ExceptionRecord->ExceptionCode;

	if (dwCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	EXCEPTION_RECORD* pExceptionRecord = ep->ExceptionRecord;
	u8* address = (u8 *)pExceptionRecord->ExceptionInformation[1];

	//printf("[EXC] During access to : 0x%X\n", address);

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
    os_DebugBreak();

	return EXCEPTION_CONTINUE_SEARCH;
}

#ifdef _WIN64

typedef union _UNWIND_CODE {
	struct {
		u8 CodeOffset;
		u8 UnwindOp : 4;
		u8 OpInfo : 4;
	};
	USHORT FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
	u8 Version : 3;
	u8 Flags : 5;
	u8 SizeOfProlog;
	u8 CountOfCodes;
	u8 FrameRegister : 4;
	u8 FrameOffset : 4;
	//ULONG ExceptionHandler;
	UNWIND_CODE UnwindCode[1];
	/*  UNWIND_CODE MoreUnwindCode[((CountOfCodes + 1) & ~1) - 1];
	*   union {
	*       OPTIONAL ULONG ExceptionHandler;
	*       OPTIONAL ULONG FunctionEntry;
	*   };
	*   OPTIONAL ULONG ExceptionData[]; */
} UNWIND_INFO, *PUNWIND_INFO;

static RUNTIME_FUNCTION Table[1];
static _UNWIND_INFO unwind_info[1];

PRUNTIME_FUNCTION
seh_callback(
_In_ DWORD64 ControlPc,
_In_opt_ PVOID Context
) {
	unwind_info[0].Version = 1;
	unwind_info[0].Flags = UNW_FLAG_UHANDLER;
	/* We don't use the unwinding info so fill the structure with 0 values.  */
	unwind_info[0].SizeOfProlog = 0;
	unwind_info[0].CountOfCodes = 0;
	unwind_info[0].FrameOffset = 0;
	unwind_info[0].FrameRegister = 0;
	/* Add the exception handler.  */

//		unwind_info[0].ExceptionHandler =
	//	(DWORD)((u8 *)__gnat_SEH_error_handler - CodeCache);
	/* Set its scope to the entire program.  */
	Table[0].BeginAddress = 0;// (CodeCache - (u8*)__ImageBase);
	Table[0].EndAddress = /*(CodeCache - (u8*)__ImageBase) +*/ CODE_SIZE + TEMP_CODE_SIZE;
	Table[0].UnwindData = (DWORD)((u8 *)unwind_info - CodeCache);
    INFO_LOG(COMMON, "TABLE CALLBACK");
	//for (;;);
	return Table;
}

void setup_seh()
{
	/* Get the base of the module.  */
	//u8* __ImageBase = (u8*)GetModuleHandle(NULL);
	/* Current version is always 1 and we are registering an
	exception handler.  */
	unwind_info[0].Version = 1;
	unwind_info[0].Flags = UNW_FLAG_NHANDLER;
	/* We don't use the unwinding info so fill the structure with 0 values.  */
	unwind_info[0].SizeOfProlog = 0;
	unwind_info[0].CountOfCodes = 1;
	unwind_info[0].FrameOffset = 0;
	unwind_info[0].FrameRegister = 0;
	/* Add the exception handler.  */

	unwind_info[0].UnwindCode[0].CodeOffset = 0;
	unwind_info[0].UnwindCode[0].UnwindOp = 2;// UWOP_ALLOC_SMALL;
	unwind_info[0].UnwindCode[0].OpInfo = 0x20 / 8;

	//unwind_info[0].ExceptionHandler =
		//(DWORD)((u8 *)__gnat_SEH_error_handler - CodeCache);
	/* Set its scope to the entire program.  */
	Table[0].BeginAddress = 0;// (CodeCache - (u8*)__ImageBase);
	Table[0].EndAddress = /*(CodeCache - (u8*)__ImageBase) +*/ CODE_SIZE + TEMP_CODE_SIZE;
	Table[0].UnwindData = (DWORD)((u8 *)unwind_info - CodeCache);
	/* Register the unwind information.  */
	RtlAddFunctionTable(Table, 1, (DWORD64)CodeCache);

	//verify(RtlInstallFunctionTableCallback((unat)CodeCache | 0x3, (DWORD64)CodeCache, CODE_SIZE + TEMP_CODE_SIZE, seh_callback, 0, 0));
}
#endif

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
