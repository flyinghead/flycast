/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CORE_REC_ARM64_ARM64_REGALLOC_H_
#define CORE_REC_ARM64_ARM64_REGALLOC_H_


#include "hw/sh4/dyna/regalloc.h"
#include "deps/vixl/aarch64/macro-assembler-aarch64.h"
using namespace vixl::aarch64;

enum eReg {
	W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, W12, W13, W14, W15, W16,
	W17, W18, W19, W20, W21, W22, W23, W24, W25, W26, W27, W28, W29, W30
};
enum eFReg {
	S0, S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14, S15, S16,
	S17, S18, S19, S20, S21, S22, S23, S24, S25, S26, S27, S28, S29, S30, S31
};

static eReg alloc_regs[] = { W19, W20, W21, W22, W23, W24, W25, W26, (eReg)-1 };
static eFReg alloc_fregs[] = { S8, S9, S10, S11, S12, S13, S14, S15, (eFReg)-1 };

class Arm64Assembler;

struct Arm64RegAlloc : RegAlloc<eReg, eFReg, false>		// TODO explode_spans=true (default, x86) breaks things. Why?
{
	Arm64RegAlloc(Arm64Assembler *assembler) : assembler(assembler) {}

	void DoAlloc(RuntimeBlockInfo* block)
	{
		RegAlloc::DoAlloc(block, alloc_regs, alloc_fregs);
	}

	virtual void Preload(u32 reg, eReg nreg) override;
	virtual void Writeback(u32 reg, eReg nreg) override;
	virtual void Preload_FPU(u32 reg, eFReg nreg) override;
	virtual void Writeback_FPU(u32 reg, eFReg nreg) override;

	const Register& MapRegister(const shil_param& param)
	{
		eReg ereg = mapg(param);
		if (ereg == (eReg)-1)
			die("Register not allocated");
		return Register::GetWRegFromCode(ereg);
	}

	const VRegister& MapVRegister(const shil_param& param, u32 index = 0)
	{
		eFReg ereg = mapfv(param, index);
		if (ereg == (eFReg)-1)
			die("VRegister not allocated");
		return VRegister::GetSRegFromCode(ereg);
	}

	Arm64Assembler *assembler;
};

#endif /* CORE_REC_ARM64_ARM64_REGALLOC_H_ */
