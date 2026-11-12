#pragma once
#include "types.h"

namespace aica
{

template<typename T> T readRegInternal(u32 addr);
template<typename T> void writeRegInternal(u32 addr, T data);

void initMem();
void termMem();

constexpr u32 AICA_REG_SIZE = 0x8000;
extern u8 *aica_reg;	// registers of the active chip (see aica_chip.h)
void selectChipRegs(int chip);

}
