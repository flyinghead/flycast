#pragma once
#include "types.h"

namespace aica
{

template<typename T> T readRegInternal(u32 addr);
template<typename T> void writeRegInternal(u32 addr, T data);

void initMem();
void termMem();

alignas(4) extern u8 aica_reg[0x8000];

}
