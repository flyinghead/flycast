#include "types.h"


bool reios_init(u8* rom, u8* flash);

void reios_reset();

void reios_term();

void DYNACALL reios_trap(u32 op);

#define REIOS_OPCODE 0x085B