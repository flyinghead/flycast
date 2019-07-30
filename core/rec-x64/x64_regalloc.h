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

#ifndef CORE_REC_X64_X64_REGALLOC_H_
#define CORE_REC_X64_X64_REGALLOC_H_

//#define OLD_REGALLOC

#include "deps/xbyak/xbyak.h"
#ifdef OLD_REGALLOC
#include "hw/sh4/dyna/regalloc.h"
#else
#include "hw/sh4/dyna/ssa_regalloc.h"
#endif

#ifdef _WIN32
static Xbyak::Operand::Code alloc_regs[] = { Xbyak::Operand::RBX, Xbyak::Operand::RBP, Xbyak::Operand::RDI, Xbyak::Operand::RSI,
		Xbyak::Operand::R12, Xbyak::Operand::R13, Xbyak::Operand::R14, Xbyak::Operand::R15, (Xbyak::Operand::Code)-1 };
static s8 alloc_fregs[] = { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1 };          // XMM6 to XMM15 are callee-saved in Windows
#else
static Xbyak::Operand::Code alloc_regs[] = { Xbyak::Operand::RBX, Xbyak::Operand::RBP, Xbyak::Operand::R12, Xbyak::Operand::R13,
		Xbyak::Operand::R14, Xbyak::Operand::R15, (Xbyak::Operand::Code)-1 };
static s8 alloc_fregs[] = { 8, 9, 10, 11, -1 };		// XMM8-11
#endif

class BlockCompiler;

struct X64RegAlloc : RegAlloc<Xbyak::Operand::Code, s8,
#ifdef EXPLODE_SPANS
														true
#else
														false
#endif
															 >
{
	X64RegAlloc(BlockCompiler *compiler) : compiler(compiler) {}

	void DoAlloc(RuntimeBlockInfo* block)
	{
		RegAlloc::DoAlloc(block, alloc_regs, alloc_fregs);
	}

	virtual void Preload(u32 reg, Xbyak::Operand::Code nreg) override;
	virtual void Writeback(u32 reg, Xbyak::Operand::Code nreg) override;
	virtual void Preload_FPU(u32 reg, s8 nreg) override;
	virtual void Writeback_FPU(u32 reg, s8 nreg) override;

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

	BlockCompiler *compiler;
};

#endif /* CORE_REC_X64_X64_REGALLOC_H_ */
