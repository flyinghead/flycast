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

#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>

#include "types.h"
#include "hw/sh4/dyna/ngen.h"
#include "x86_regalloc.h"
#include "rec-x64/xbyak_base.h"

class X86Compiler : public BaseXbyakRec<X86Compiler, false>
{
public:
	using BaseCompiler = BaseXbyakRec<X86Compiler, false>;

	X86Compiler() : BaseCompiler(), regalloc(this) { }
	X86Compiler(u8 *code_ptr) : BaseCompiler(code_ptr), regalloc(this) { }

	void compile(RuntimeBlockInfo* block, bool force_checks, bool optimise);

	void ngen_CC_Start(const shil_opcode& op)
	{
		CC_stackSize = 0;
	}
	void ngen_CC_param(const shil_opcode& op, const shil_param& param, CanonicalParamType tp);
	void ngen_CC_Call(const shil_opcode& op, void* function)
	{
		genCallCdecl((void (*)())function);
	}
	void ngen_CC_Finish(const shil_opcode &op);

	void regPreload(u32 reg, Xbyak::Operand::Code nreg)
	{
		DEBUG_LOG(DYNAREC, "RegPreload reg %d -> %s", reg, Xbyak::Reg32(nreg).toString());
		mov(Xbyak::Reg32(nreg), dword[GetRegPtr(reg)]);
	}
	void regWriteback(u32 reg, Xbyak::Operand::Code nreg)
	{
		DEBUG_LOG(DYNAREC, "RegWriteback reg %d <- %s", reg, Xbyak::Reg32(nreg).toString());
		mov(dword[GetRegPtr(reg)], Xbyak::Reg32(nreg));
	}
	void regPreload_FPU(u32 reg, s8 nreg)
	{
		DEBUG_LOG(DYNAREC, "RegPreload_FPU reg %d -> xmm%d", reg, nreg);
		movss(Xbyak::Xmm(nreg), dword[GetRegPtr(reg)]);
	}
	void regWriteback_FPU(u32 reg, s8 nreg)
	{
		DEBUG_LOG(DYNAREC, "RegWriteback_FPU reg %d <- xmm%d", reg, nreg);
		movss(dword[GetRegPtr(reg)], Xbyak::Xmm(nreg));
	}

	void genMainloop();
	u32 relinkBlock(RuntimeBlockInfo *block);
	bool rewriteMemAccess(host_context_t &context);

	static void (*handleException)();

private:
	void genOpcode(RuntimeBlockInfo *block, bool optimise, shil_opcode& op);

	bool genReadMemImmediate(const shil_opcode& op, RuntimeBlockInfo *block);
	bool genWriteMemImmediate(const shil_opcode& op, RuntimeBlockInfo *block);
	void genMemHandlers();
	void alignStack(int amount);
	void genMmuLookup(RuntimeBlockInfo* block, const shil_opcode& op, u32 write);

	void checkBlock(bool smc_checks, RuntimeBlockInfo *block);
	void freezeXMM();
	void thawXMM();

	template<class Ret, class... Params>
	void genCallCdecl(Ret (*function)(Params...))
	{
		freezeXMM();
		call((void *)function);
		thawXMM();
	}

	template<class Ret, class... Params>
	void genCall(Ret (DYNACALL *function)(Params...))
	{
		genCallCdecl((Ret (*)(Params...))function);
	}

	X86RegAlloc regalloc;
	size_t current_opid;
	u32 CC_stackSize;

	friend class BaseXbyakRec<X86Compiler, false>;
};
