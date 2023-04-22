#pragma once
#include "types.h"

struct host_context_t {
#if HOST_CPU != CPU_GENERIC
	unat pc;
#endif

#if HOST_CPU == CPU_X86
	u32 eax;
	u32 ecx;
	u32 esp;
#elif HOST_CPU == CPU_X64
	u64 rsp;
	u64 r9;
#ifdef _WIN32
	u64 rcx;
#else
	u64 rdi;
#endif
#elif HOST_CPU == CPU_ARM
	u32 reg[15];
#elif HOST_CPU == CPU_ARM64
	u64 x0;
#endif
};
