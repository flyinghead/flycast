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
#include "hw/sh4/dyna/ssa_regalloc.h"

class X86Compiler;

struct X86RegAlloc : RegAlloc<Xbyak::Operand::Code, s8>
{
	X86RegAlloc(X86Compiler *compiler) : compiler(compiler) {}

	void Preload(u32 reg, Xbyak::Operand::Code nreg) override;
	void Writeback(u32 reg, Xbyak::Operand::Code nreg) override;
	void Preload_FPU(u32 reg, s8 nreg) override;
	void Writeback_FPU(u32 reg, s8 nreg) override;

	void doAlloc(RuntimeBlockInfo* block);

	Xbyak::Reg32 MapRegister(const shil_param& param)
	{
		Xbyak::Operand::Code ereg = mapg(param);
		if (ereg == (Xbyak::Operand::Code)-1)
			die("Register not allocated");
		return Xbyak::Reg32(ereg);
	}

	Xbyak::Xmm MapXRegister(const shil_param& param)
	{
		s8 ereg = mapf(param);
		if (ereg == -1)
			die("VRegister not allocated");
		return Xbyak::Xmm(ereg);
	}

	bool IsMapped(const Xbyak::Xmm &xmm, size_t opid)
	{
		return regf_used((s8)xmm.getIdx());
	}

	X86Compiler *compiler;
};
