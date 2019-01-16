#include "types.h"


struct rei_host_context_t {
#if HOST_CPU != CPU_GENERIC
	unat pc;
#endif

#if HOST_CPU == CPU_X86
	u32 eax;
	u32 ecx;
	u32 esp;
#elif HOST_CPU == CPU_ARM
	u32 r[15];
#endif
};

void context_from_segfault(rei_host_context_t* reictx, void* segfault_ctx);
void context_to_segfault(rei_host_context_t* reictx, void* segfault_ctx);
