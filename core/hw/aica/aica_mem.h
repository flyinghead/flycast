#pragma once
#include "types.h"

u32 libAICA_ReadReg(u32 addr,u32 size);
void libAICA_WriteReg(u32 addr,u32 data,u32 size);

void init_mem();
void term_mem();

alignas(4) extern u8 aica_reg[0x8000];

