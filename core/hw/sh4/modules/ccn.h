#pragma once
#include "types.h"
#include "hw/hwreg.h"

template<u32 idx>
void CCN_QACR_write(u32 addr, u32 value);

extern u32 CCN[18];

class CCNRegisters : public RegisterBank<CCN, 18>
{
	using super = RegisterBank<CCN, 18>;

public:
	void init();
};
extern CCNRegisters ccn;
