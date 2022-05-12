#pragma once
#include "types.h"

template<typename T> T aicaReadReg(u32 addr);
template<typename T> void aicaWriteReg(u32 addr, T data);

void init_mem();
void term_mem();

alignas(4) extern u8 aica_reg[0x8000];

