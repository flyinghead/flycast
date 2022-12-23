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
#pragma once

#include <xbyak/xbyak.h>
#include "hw/sh4/dyna/ssa_regalloc.h"

#ifdef _WIN32
static Xbyak::Operand::Code alloc_regs[] = { Xbyak::Operand::RBX, Xbyak::Operand::RBP, Xbyak::Operand::RDI, Xbyak::Operand::RSI,
		Xbyak::Operand::R12, Xbyak::Operand::R13, Xbyak::Operand::R14, Xbyak::Operand::R15, (Xbyak::Operand::Code)-1 };
static s8 alloc_fregs[] = { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1 };          // XMM6 to XMM15 are callee-saved in Windows
#define ALLOC_F64 true
#else
static Xbyak::Operand::Code alloc_regs[] = { Xbyak::Operand::RBX, Xbyak::Operand::RBP, Xbyak::Operand::R12, Xbyak::Operand::R13,
		Xbyak::Operand::R14, Xbyak::Operand::R15, (Xbyak::Operand::Code)-1 };
static s8 alloc_fregs[] = { 8, 9, 10, 11, -1 };		// XMM8-11
// all xmm registers are caller-saved on linux
#define ALLOC_F64 false
#endif

class BlockCompiler;

struct X64RegAlloc : RegAlloc<Xbyak::Operand::Code, s8, ALLOC_F64>
{
	X64RegAlloc(BlockCompiler *compiler) : compiler(compiler) {}

	void DoAlloc(RuntimeBlockInfo* block)
	{
		RegAlloc::DoAlloc(block, alloc_regs, alloc_fregs);
	}

	void Preload(u32 reg, Xbyak::Operand::Code nreg) override;
	void Writeback(u32 reg, Xbyak::Operand::Code nreg) override;
	void Preload_FPU(u32 reg, s8 nreg) override;
	void Writeback_FPU(u32 reg, s8 nreg) override;

	Xbyak::Reg32 MapRegister(const shil_param& param)
	{
		Xbyak::Operand::Code ereg = mapg(param);
		if (ereg == (Xbyak::Operand::Code)-1)
			die("Register not allocated");
		return Xbyak::Reg32(ereg);
	}

	Xbyak::Xmm MapXRegister(const shil_param& param, int index = 0)
	{
		s8 ereg = mapf(param, index);
		if (ereg == -1)
			die("VRegister not allocated");
		return Xbyak::Xmm(ereg);
	}

	bool IsMapped(const Xbyak::Xmm &xmm, size_t opid)
	{
		return regf_used((s8)xmm.getIdx());
	}

	BlockCompiler *compiler;
};
