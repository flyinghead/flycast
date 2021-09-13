#pragma once
#include <switch.h>

typedef struct
{
	uint64_t regs[29];
	uint64_t pc;
} mcontext_t;

typedef struct ucontext_t
{
	mcontext_t uc_mcontext;
} ucontext_t;

typedef struct
{
	void* si_addr;
} switch_siginfo_t;
