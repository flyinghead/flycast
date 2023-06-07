#pragma once
#include "hw/hwreg.h"

void DMAC_Ch2St();

#define DMAOR_MASK	0xFFFF8201

extern u32 DMAC[17];

class DMACRegisters : public RegisterBank<DMAC, 17>
{
	using super = RegisterBank<DMAC, 17>;

public:
	void init();
};
extern DMACRegisters dmac;
