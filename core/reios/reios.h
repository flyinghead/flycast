#include "types.h"


bool reios_init(u8* rom, u8* flash);

void reios_reset();

void reios_term();

void DYNACALL reios_trap(u32 op);

const char* reios_locate_ip(void);
bool reios_locate_bootfile(const char* bootfile);
extern char reios_product_number[17];

#define REIOS_OPCODE 0x085B
