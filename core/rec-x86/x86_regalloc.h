/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once

//#define OLD_REGALLOC

#ifdef OLD_REGALLOC
#include "hw/sh4/dyna/regalloc.h"
#else
#include "hw/sh4/dyna/ssa_regalloc.h"
#endif

class X86Compiler;

struct X86RegAlloc : RegAlloc<Xbyak::Operand::Code, s8>
{
	X86RegAlloc(X86Compiler *compiler) : compiler(compiler) {}

	virtual void Preload(u32 reg, Xbyak::Operand::Code nreg) override;
	virtual void Writeback(u32 reg, Xbyak::Operand::Code nreg) override;
	virtual void Preload_FPU(u32 reg, s8 nreg) override;
	virtual void Writeback_FPU(u32 reg, s8 nreg) override;

	void doAlloc(RuntimeBlockInfo* block);

	Xbyak::Reg32 MapRegister(const shil_param& param)
	{
		Xbyak::Operand::Code ereg = mapg(param);
		if (ereg == (Xbyak::Operand::Code)-1)
			die("Register not allocated");
		return Xbyak::Reg32(ereg);
	}

	Xbyak::Xmm MapXRegister(const shil_param& param, u32 index = 0)
	{
#ifdef OLD_REGALLOC
		s8 ereg = mapfv(param, index);
#else
		s8 ereg = mapf(param);
#endif
		if (ereg == -1)
			die("VRegister not allocated");
		return Xbyak::Xmm(ereg);
	}

	bool IsMapped(const Xbyak::Xmm &xmm, size_t opid)
	{
#ifndef OLD_REGALLOC
		return regf_used((s8)xmm.getIdx());
#else
		for (size_t sid = 0; sid < all_spans.size(); sid++)
		{
			if (all_spans[sid]->nregf == xmm.getIdx() && all_spans[sid]->contains(opid))
				return true;
		}
		return false;
#endif
	}

	X86Compiler *compiler;
};
