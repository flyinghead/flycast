/*
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
#include "types.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM
#ifndef _M_ARM
#include <unistd.h>
#endif
#include <array>
#include <map>

#ifdef _M_ARM
#pragma push_macro("MemoryBarrier")
#pragma push_macro("Yield")
#undef MemoryBarrier
#undef Yield
#endif

#include <aarch32/macro-assembler-aarch32.h>
using namespace vixl::aarch32;

#ifdef _M_ARM
#pragma pop_macro("MemoryBarrier")
#pragma pop_macro("Yield")
#endif

#include "hw/sh4/sh4_opcode_list.h"

#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_rom.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/dyna/ssa_regalloc.h"
#include "hw/sh4/sh4_mem.h"
#include "cfg/option.h"
#include "arm_unwind.h"
#include "oslib/virtmem.h"
#include "emulator.h"

//#define CANONICALTEST

/*

	ARM ABI
		r0~r1: scratch, params, return
		r2~r3: scratch, params
		8 regs, v6 is platform dependent
			r4~r11
		r12 is "The Intra-Procedure-call scratch register"
		r13 stack
		r14 link
		r15 pc

		Registers f0-s15 (d0-d7, q0-q3) do not need to be preserved (and can be used for passing arguments or returning results in standard procedure-call variants).
		Registers s16-s31 (d8-d15, q4-q7) must be preserved across subroutine calls;
		Registers d16-d31 (q8-q15), if present, do not need to be preserved.

	Block linking
	Reg alloc
		r0~r4: scratch
		r5,r6,r7,r9,r10,r11: allocated
		r8: sh4 cntx

	fpu reg alloc
	d8:d15, single storage

*/

#ifdef __clang__
extern "C" char *stpcpy(char *dst, char const *src)
{
	size_t src_len = strlen(src);
	return (char *)memcpy(dst, src, src_len) + src_len;
}
#endif

#define rcbOffset(x) (-sizeof(Sh4RCB) + offsetof(Sh4RCB, x))
#define ctxOffset(x) (-sizeof(Sh4Context) + offsetof(Sh4Context, x))

struct DynaRBI : RuntimeBlockInfo
{
	DynaRBI(Sh4Context& sh4ctx, Sh4CodeBuffer& codeBuffer)
	: sh4ctx(sh4ctx), codeBuffer(codeBuffer) {}
	u32 Relink() override;

	Register T_reg;
	Sh4Context& sh4ctx;
	Sh4CodeBuffer& codeBuffer;
};

static std::map<shilop, ConditionType> ccmap;
static std::map<shilop, ConditionType> ccnmap;

static const void *no_update;
static const void *intc_sched;
static const void *ngen_blockcheckfail;
static const void *ngen_LinkBlock_Generic_stub;
static const void *ngen_LinkBlock_cond_Branch_stub;
static const void *ngen_LinkBlock_cond_Next_stub;
static void (*ngen_FailedToFindBlock_)();
static void (*mainloop)(void *);
static void (*handleException)();
static void (*checkBlockFpu)();
static void (*checkBlockNoFpu)();

class Arm32Assembler;

struct arm_reg_alloc : RegAlloc<int, int, true>
{
	arm_reg_alloc(Arm32Assembler& ass) : ass(ass) {}

	void Preload(u32 reg, int nreg) override;
	void Writeback(u32 reg, int nreg) override;

	void Preload_FPU(u32 reg, int nreg) override;
	void Writeback_FPU(u32 reg, int nreg) override;

	SRegister mapFReg(const shil_param& prm, int index = 0)
	{
		return SRegister(mapf(prm, index));
	}
	Register mapReg(const shil_param& prm)
	{
		return Register(mapg(prm));
	}

private:
	Arm32Assembler& ass;
};

enum mem_op_type
{
	SZ_8,
	SZ_16,
	SZ_32I,
	SZ_32F,
	SZ_64F,
};

class Arm32Assembler : public MacroAssembler
{
	using FPBinOP = void (MacroAssembler::*)(DataType, SRegister, SRegister, SRegister);
	using FPUnOP = void (MacroAssembler::*)(DataType, SRegister, SRegister);
	using BinaryOP = void (MacroAssembler::*)(Register, Register, const Operand&);

public:
	Arm32Assembler(Sh4Context& sh4ctx, Sh4CodeBuffer& codeBuffer)
	: MacroAssembler((u8 *)codeBuffer.get(), codeBuffer.getFreeSpace(), A32), sh4ctx(sh4ctx), codeBuffer(codeBuffer), reg(*this) {}
	Arm32Assembler(Sh4Context& sh4ctx, Sh4CodeBuffer& codeBuffer, u8 *buffer, size_t size)
	: MacroAssembler(buffer, size, A32), sh4ctx(sh4ctx), codeBuffer(codeBuffer), reg(*this) {}

	void compile(RuntimeBlockInfo* block, bool force_checks, bool optimise);
	void rewrite(Register raddr, Register rt, SRegister ft, DRegister fd, bool write, bool is_sq, mem_op_type optp);
	void genMainLoop();
	void canonStart(const shil_opcode *op);
	void canonParam(const shil_opcode *op, const shil_param *par, CanonicalParamType tp);
	void canonCall(const shil_opcode *op, void *function);
	void canonFinish(const shil_opcode *op);

	void loadSh4Reg(Register Rt, u32 Sh4_Reg)
	{
		const int shRegOffs = getRegOffset((Sh4RegType)Sh4_Reg) - sizeof(Sh4Context);

		Ldr(Rt, MemOperand(r8, shRegOffs));
	}

	void storeSh4Reg(Register Rt, u32 Sh4_Reg)
	{
		const int shRegOffs = getRegOffset((Sh4RegType)Sh4_Reg) - sizeof(Sh4Context);

		Str(Rt, MemOperand(r8, shRegOffs));
	}

	void Finalize()
	{
		FinalizeCode();
		virtmem::flush_cache(GetBuffer()->GetStartAddress<void *>(), GetCursorAddress<u8 *>() - 1,
				GetBuffer()->GetStartAddress<void *>(), GetCursorAddress<u8 *>() - 1);
	}

	void jump(const void *code, ConditionType cond = al)
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(code) - GetBuffer()->GetStartAddress<uintptr_t>();
		verify((offset & 3) == 0);
		if (offset < -32 * 1024 * 1024 || offset >= 32 * 1024 * 1024)
		{
			WARN_LOG(DYNAREC, "jump offset too large: %d", offset);
			UseScratchRegisterScope scope(this);
			Register reg = scope.Acquire();
			Mov(cond, reg, (u32)code);
			Bx(cond, reg);
		}
		else
		{
			Label code_label(offset);
			B(cond, &code_label);
		}
	}

	void call(const void *code, ConditionType cond = al)
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(code) - GetBuffer()->GetStartAddress<uintptr_t>();
		verify((offset & 3) == 0);
		if (offset < -32 * 1024 * 1024 || offset >= 32 * 1024 * 1024)
		{
			WARN_LOG(DYNAREC, "call offset too large: %d", offset);
			UseScratchRegisterScope scope(this);
			Register reg = scope.Acquire();
			Mov(cond, reg, (u32)code);
			Blx(cond, reg);
		}
		else
		{
			Label code_label(offset);
			Bl(cond, &code_label);
		}
	}

	u32 relinkBlock(DynaRBI *block)
	{
		u32 start_offset = GetCursorOffset();
		switch (block->BlockType)
		{
		case BET_Cond_0:
		case BET_Cond_1:
		{
			//quick opt here:
			//peek into reg alloc, store actual sr_T register to relink_data
#ifndef CANONICALTEST
			bool last_op_sets_flags = !block->has_jcond && !block->oplist.empty() &&
					block->oplist.back().rd._reg == reg_sr_T && ccmap.count(block->oplist.back().op);
#else
			bool last_op_sets_flags = false;
#endif

			ConditionType CC = eq;

			if (last_op_sets_flags)
			{
				shilop op = block->oplist.back().op;

				if ((block->BlockType & 1) == 1)
					CC = ccmap[op];
				else
					CC = ccnmap[op];
			}
			else
			{
				if (!block->has_jcond)
				{
					if (block->T_reg.IsRegister())
					{
						mov(r4, block->T_reg);
					}
					else
					{
						INFO_LOG(DYNAREC, "SLOW COND PATH %x", block->oplist.empty() ? -1 : block->oplist.back().op);
						loadSh4Reg(r4, reg_sr_T);
					}
				}
				cmp(r4, block->BlockType & 1);
			}

			if (!mmu_enabled())
			{
				if (block->pBranchBlock)
					jump((void *)block->pBranchBlock->code, CC);
				else
					call(ngen_LinkBlock_cond_Branch_stub, CC);

				if (block->pNextBlock)
					jump((void *)block->pNextBlock->code);
				else
					call(ngen_LinkBlock_cond_Next_stub);
				nop();
				nop();
				nop();
				nop();
			}
			else
			{
				mov(Condition(CC).Negate(), r4, block->NextBlock);
				mov(CC, r4, block->BranchBlock);
				storeSh4Reg(r4, reg_nextpc);
				jump(no_update);
			}
			break;
		}

		case BET_DynamicRet:
		case BET_DynamicCall:
		case BET_DynamicJump:
			if (!mmu_enabled())
			{
				sub(r2, r8, -rcbOffset(fpcb));
				ubfx(r1, r4, 1, 24);
				ldr(pc, MemOperand(r2, r1, LSL, 2));
			}
			else
			{
				storeSh4Reg(r4, reg_nextpc);
				jump(no_update);
			}
			break;

		case BET_StaticCall:
		case BET_StaticJump:
			if (!mmu_enabled())
			{
				if (block->pBranchBlock == nullptr)
					call(ngen_LinkBlock_Generic_stub);
				else
					call((void *)block->pBranchBlock->code);
				nop();
				nop();
				nop();
			}
			else
			{
				mov(r4, block->BranchBlock);
				storeSh4Reg(r4, reg_nextpc);
				jump(no_update);
			}

			break;

		case BET_StaticIntr:
		case BET_DynamicIntr:
			if (block->BlockType == BET_StaticIntr)
				mov(r4, block->NextBlock);
			//else -> already in r4 djump !

			storeSh4Reg(r4, reg_nextpc);
			call((void *)UpdateINTC);
			loadSh4Reg(r4, reg_nextpc);
			jump(no_update);
			break;

		default:
			ERROR_LOG(DYNAREC, "Error, Relink() Block Type: %X", block->BlockType);
			verify(false);
			break;
		}
		return GetCursorOffset() - start_offset;
	}

private:
	Register GetParam(const shil_param& param, Register raddr = r0);
	void binaryOp(shil_opcode* op, BinaryOP dtop);
	void binaryFpOp(shil_opcode* op, FPBinOP fpop);
	void unaryFpOp(shil_opcode* op, FPUnOP fpop);
	void vmem_slowpath(Register raddr, Register rt, SRegister ft, DRegister fd, mem_op_type optp, bool read);
	Register GenMemAddr(shil_opcode* op, Register raddr = r0);
	bool readMemImmediate(RuntimeBlockInfo* block, shil_opcode* op, bool optimise);
	bool writeMemImmediate(RuntimeBlockInfo* block, shil_opcode* op, bool optimise);
	void genMmuLookup(RuntimeBlockInfo* block, const shil_opcode& op, u32 write, Register& raddr);
	void compileOp(RuntimeBlockInfo* block, shil_opcode* op, bool optimise);

	Sh4Context& sh4ctx;
	Sh4CodeBuffer& codeBuffer;
	arm_reg_alloc reg;
	struct CC_PS
	{
		CanonicalParamType type;
		const shil_param* par;
	};
	std::vector<CC_PS> CC_pars;
};

void arm_reg_alloc::Preload(u32 reg, int nreg)
{
	ass.loadSh4Reg(Register(nreg), reg);
}
void arm_reg_alloc::Writeback(u32 reg, int nreg)
{
	if (reg == reg_pc_dyn)
		// reg_pc_dyn has been stored in r4 by the jdyn op implementation
		// No need to write it back since it won't be used past the end of the block
		; //ass.Mov(r4, Register(nreg));
	else
		ass.storeSh4Reg(Register(nreg), reg);
}

void arm_reg_alloc::Preload_FPU(u32 reg, int nreg)
{
	const s32 shRegOffs = getRegOffset((Sh4RegType)reg) - sizeof(Sh4Context);

	ass.Vldr(SRegister(nreg), MemOperand(r8, shRegOffs));
}
void arm_reg_alloc::Writeback_FPU(u32 reg, int nreg)
{
	const s32 shRegOffs = getRegOffset((Sh4RegType)reg) - sizeof(Sh4Context);

	ass.Vstr(SRegister(nreg), MemOperand(r8, shRegOffs));
}

static ArmUnwindInfo unwinder;
std::map<u32, u32 *> ArmUnwindInfo::fdes;

static u32 jmp_stack;

class Arm32Dynarec : public Sh4Dynarec
{
public:
	Arm32Dynarec() {
		sh4Dynarec = this;
	}

	void init(Sh4Context& sh4ctx, Sh4CodeBuffer& codeBuffer) override;
	void reset() override;
	RuntimeBlockInfo *allocateBlock() override;
	void handleException(host_context_t &context) override;
	bool rewrite(host_context_t& context, void *faultAddress) override;

	void mainloop(void* context) override
	{
		do {
			restarting = false;
			generate_mainloop();

			::mainloop(context);
			if (restarting && !emu.restartCpu())
				restarting = false;
		} while (restarting);
	}

	void compile(RuntimeBlockInfo* block, bool smc_check, bool optimise) override {
		ass = new Arm32Assembler(*sh4ctx, *codeBuffer);
		ass->compile(block, smc_check, optimise);
		delete ass;
		ass = nullptr;
	}

	void canonStart(const shil_opcode *op) override {
		ass->canonStart(op);
	}
	void canonParam(const shil_opcode *op, const shil_param *param, CanonicalParamType paramType) override {
		ass->canonParam(op, param, paramType);
	}
	void canonCall(const shil_opcode *op, void *function)override {
		ass->canonCall(op, function);
	}
	void canonFinish(const shil_opcode *op) override {
		ass->canonFinish(op);
	}

private:
	void generate_mainloop();

	Sh4Context *sh4ctx = nullptr;
	Sh4CodeBuffer *codeBuffer = nullptr;
	bool restarting = false;
	Arm32Assembler *ass = nullptr;
};
static Arm32Dynarec instance;

u32 DynaRBI::Relink()
{
	Arm32Assembler ass(sh4ctx, codeBuffer, (u8 *)code + relink_offset, host_code_size - relink_offset);

	u32 size = ass.relinkBlock(this);

	ass.Finalize();

	return size;
}

Register Arm32Assembler::GetParam(const shil_param& param, Register raddr)
{
	if (param.is_imm())
	{
		Mov(raddr, param._imm);
		return raddr;
	}
	if (param.is_r32i())
		return reg.mapReg(param);

	die("Invalid parameter");
	return Register();
}

void Arm32Assembler::binaryOp(shil_opcode* op, BinaryOP dtop)
{
	Register rs1 = GetParam(op->rs1);
	
	Register rs2 = r1;
	if (op->rs2.is_imm())
	{
		if (ImmediateA32::IsImmediateA32(op->rs2._imm))
		{
			((*this).*dtop)(reg.mapReg(op->rd), rs1, Operand(op->rs2._imm));
			return;
		}
		Mov(rs2, op->rs2._imm);
	}
	else if (op->rs2.is_r32i())
	{
		rs2 = reg.mapReg(op->rs2);
	}
	else
	{
		ERROR_LOG(DYNAREC, "ngen_Bin ??? %d", op->rs2.type);
		verify(false);
	}

	((*this).*dtop)(reg.mapReg(op->rd), rs1, rs2);
}

void Arm32Assembler::binaryFpOp(shil_opcode* op, FPBinOP fpop)
{
	SRegister rs1 = s0;
	if (op->rs1.is_imm())
	{
		Mov(r0, op->rs1._imm);
		Vmov(rs1, r0);
	}
	else
	{
		rs1 = reg.mapFReg(op->rs1);
	}

	SRegister rs2 = s1;
	if (op->rs2.is_imm())
	{
		Mov(r0, op->rs2._imm);
		Vmov(rs2, r0);
	}
	else
	{
		rs2 = reg.mapFReg(op->rs2);
	}

	((*this).*fpop)(DataType(F32), reg.mapFReg(op->rd), rs1, rs2);
}

void Arm32Assembler::unaryFpOp(shil_opcode* op, FPUnOP fpop)
{
	((*this).*fpop)(DataType(F32), reg.mapFReg(op->rd), reg.mapFReg(op->rs1));
}

void Arm32Assembler::canonStart(const shil_opcode *op)
{ 
	CC_pars.clear();
}

void Arm32Assembler::canonParam(const shil_opcode *op, const shil_param *par, CanonicalParamType tp)
{ 
	switch(tp)
	{
		case CPT_f32rv:
#ifdef __ARM_PCS_VFP
			// -mfloat-abi=hard
			if (reg.IsAllocg(*par))
				Vmov(reg.mapReg(*par), s0);
			else if (reg.IsAllocf(*par))
				Vmov(reg.mapFReg(*par), s0);
			break;
#endif

		case CPT_u32rv:
		case CPT_u64rvL:
			if (reg.IsAllocg(*par))
				Mov(reg.mapReg(*par), r0);
			else if (reg.IsAllocf(*par))
				Vmov(reg.mapFReg(*par), r0);
			else
				die("unhandled param");
			break;

		case CPT_u64rvH:
			verify(reg.IsAllocg(*par));
			Mov(reg.mapReg(*par), r1);
			break;

		case CPT_u32:
		case CPT_ptr:
		case CPT_f32:
		case CPT_sh4ctx:
			{
				CC_PS t = { tp, par };
				CC_pars.push_back(t);
			}
			break;

		default:
			die("invalid tp");
			break;
	}
}

void Arm32Assembler::canonCall(const shil_opcode *op, void *function)
{
	Register rd = r0;
	SRegister fd = s0;

	for (int i = CC_pars.size(); i-- > 0; )
	{
		CC_PS& param = CC_pars[i];
		if (param.type == CPT_ptr)
		{
			Mov(rd, (u32)param.par->reg_ptr(sh4ctx));
		}
		else if (param.type == CPT_sh4ctx)
		{
			Mov(rd, reinterpret_cast<uintptr_t>(&sh4ctx));
		}
		else
		{
			if (param.par->is_reg())
			{
#ifdef __ARM_PCS_VFP
				// -mfloat-abi=hard
				if (param.type == CPT_f32)
				{
					if (reg.IsAllocg(*param.par))
						Vmov(fd, reg.mapReg(*param.par));
					else if (reg.IsAllocf(*param.par))
						Vmov(fd, reg.mapFReg(*param.par));
					else
						die("Must not happen!");
					continue;
				}
#endif

				if (reg.IsAllocg(*param.par))
					Mov(rd, reg.mapReg(*param.par));
				else if (reg.IsAllocf(*param.par))
					Vmov(rd, reg.mapFReg(*param.par));
				else
					die("Must not happen!");
			}
			else
			{
				verify(param.par->is_imm());
				Mov(rd, param.par->_imm);
			}
		}
		rd = Register(rd.GetCode() + 1);
		fd = SRegister(fd.GetCode() + 1);
	}
	call(function);
	for (const CC_PS& ccParam : CC_pars)
	{
		const shil_param& prm = *ccParam.par;
		if (ccParam.type == CPT_ptr && prm.count() == 2 && reg.IsAllocf(prm) && (op->rd._reg == prm._reg || op->rd2._reg == prm._reg))
		{
			// fsca rd param is a pointer to a 64-bit reg so reload the regs if allocated
			const int shRegOffs = prm.reg_nofs();
			Vldr(reg.mapFReg(prm, 0), MemOperand(r8, shRegOffs));
			Vldr(reg.mapFReg(prm, 1), MemOperand(r8, shRegOffs + 4));
		}
	}
}

void Arm32Assembler::canonFinish(const shil_opcode *op)
{ 
	CC_pars.clear(); 
}

static mem_op_type memop_type(shil_opcode* op)
{
	bool fp32 = op->rs2.is_r32f() || op->rd.is_r32f();

	if (op->size == 1)
		return SZ_8;
	else if (op->size == 2)
		return SZ_16;
	else if (op->size == 4)
		return fp32 ? SZ_32F : SZ_32I;
	else if (op->size == 8)
		return SZ_64F;

	die("Unknown op");
	return SZ_32I;
}

const u32 memop_bytes[] = { 1, 2, 4, 4, 8 };
static const void *_mem_hndl_SQ32[3][14];
static const void *_mem_hndl[2][3][14];
const void * const _mem_func[2][2] =
{
	{ (void *)addrspace::write32, (void *)addrspace::write64 },
	{ (void *)addrspace::read32, (void *)addrspace::read64 },
};

void Arm32Assembler::vmem_slowpath(Register raddr, Register rt, SRegister ft, DRegister fd, mem_op_type optp, bool read)
{
	if (!raddr.Is(r0))
		Mov(r0, raddr);

	if (!read)
	{
		if (optp <= SZ_32I)
			Mov(r1, rt);
		else if (optp == SZ_32F)
			Vmov(r1, ft);
		else if (optp == SZ_64F)
			Vmov(r2, r3, fd);
	}

	const void *funct = nullptr;

	if (optp <= SZ_32I)
		funct = _mem_hndl[read][optp][raddr.GetCode()];
	else
		funct = _mem_func[read][optp - SZ_32F];

	verify(funct != nullptr);
	call(funct);

	if (read)
	{
		if (optp <= SZ_32I)
			Mov(rt, r0);
		else if (optp == SZ_32F)
			Vmov(ft, r0);
		else if (optp == SZ_64F)
			Vmov(fd, r0, r1);
	}
}

bool Arm32Dynarec::rewrite(host_context_t& context, void *faultAddress)
{
	static constexpr struct
	{
		u32 mask;
		u32 key;
		bool read;
		mem_op_type optp;
		u32 offs;
	}
	op_table[] =
	{
		//LDRSB
		{ 0x0E500FF0, 0x001000D0, true, SZ_8, 1 },
		//LDRSH
		{ 0x0E500FF0, 0x001000F0, true, SZ_16, 1 },
		//LDR
		{ 0x0E500010, 0x06100000, true, SZ_32I, 1 },
		//VLDR.32
		{ 0x0F300F00, 0x0D100A00, true, SZ_32F, 2 },
		//VLDR.64
		{ 0x0F300F00, 0x0D100B00, true, SZ_64F, 2 },

		//
		//STRB
		{ 0x0FF00010, 0x07C00000, false, SZ_8, 1 },
		//STRH
		{ 0x0FF00FF0, 0x018000B0, false, SZ_16, 1 },
		//STR
		{ 0x0E500010, 0x06000000, false, SZ_32I, 1 },
		//VSTR.32
		{ 0x0F300F00, 0x0D000A00, false, SZ_32F, 2 },
		//VSTR.64
		{ 0x0F300F00, 0x0D000B00, false, SZ_64F, 2 },

		{ 0, 0 },
	};

	union arm_mem_op
	{
		struct
		{
			u32 Ra:4;
			u32 pad0:8;
			u32 Rt:4;
			u32 Rn:4;
			u32 pad1:2;
			u32 D:1;
			u32 pad3:1;
			u32 pad4:4;
			u32 cond:4;
		};

		u32 full;
	};

	if (codeBuffer == nullptr)
		// init() not called yet
		return false;
	if ((u8 *)context.pc < (u8 *)codeBuffer->getBase()
			|| (u8 *)context.pc >= (u8 *)codeBuffer->getBase() + codeBuffer->getSize())
		return false;
	u32 *regs = context.reg;
	arm_mem_op *ptr = (arm_mem_op *)context.pc;

	mem_op_type optp;
	bool read;
	s32 offs = -1;

	u32 fop = ptr[0].full;

	for (int i = 0; op_table[i].mask; i++)
	{
		if ((fop & op_table[i].mask) == op_table[i].key)
		{
			optp = op_table[i].optp;
			read = op_table[i].read;
			offs = op_table[i].offs;
		}
	}

	if (offs == -1)
	{
		ERROR_LOG(DYNAREC, "%08X : invalid size", fop);
		die("can't decode opcode\n");
	}

	ptr -= offs;

	Register raddr, rt;
	SRegister ft;
	DRegister fd;

	//Get used regs from opcodes ..
	
	if ((ptr[0].full & 0x0FE00070) == 0x07E00050)
	{
		//from ubfx !
		raddr = Register(ptr[0].Ra);
	}
	else if ((ptr[0].full & 0x0FE00000) == 0x03C00000)
	{
		raddr = Register(ptr[0].Rn);
	}
	else
	{
		ERROR_LOG(DYNAREC, "fail raddr %08X {@%08X}:(", ptr[0].full, regs[1]);
		die("Invalid opcode: vmem fixup\n");
	}
	//from mem op
	rt = Register(ptr[offs].Rt);
	ft = SRegister(ptr[offs].Rt * 2 + ptr[offs].D);
	fd = DRegister(ptr[offs].D * 16 + ptr[offs].Rt);

	//get some other relevant data
	u32 sh4_addr = regs[raddr.GetCode()];
	u32 fault_offs = (uintptr_t)faultAddress - regs[8];
	bool is_sq = (sh4_addr >> 26) == 0x38;

	// fault offset must always be the addr from ubfx (sanity check)
	// ignore last 2 bits zeroed to avoid sigbus errors
	verify(fault_offs == 0 || (fault_offs & ~3) == (sh4_addr & 0x1FFFFFFC));

	ass = new Arm32Assembler(*sh4ctx, *codeBuffer, (u8 *)ptr, 12);
	ass->rewrite(raddr, rt, ft, fd, !read, is_sq, optp);
	delete ass;
	ass = nullptr;
	context.pc = (size_t)ptr;

	return true;
}

void Arm32Assembler::rewrite(Register raddr, Register rt, SRegister ft, DRegister fd, bool write, bool is_sq, mem_op_type optp)
{
	if (is_sq && write && optp >= SZ_32I)
	{
		if (optp >= SZ_32F)
		{
			if (!raddr.Is(r0))
				Mov(r0, raddr);
			else
				Nop();
			raddr = r0;
		}
		switch (optp)
		{
		case SZ_32I:
			Mov(r1, rt);
			break;
		case SZ_32F:
			Vmov(r1, ft);
			break;
		case SZ_64F:
			Vmov(r2, r3, fd);
			break;
		default:
			break;
		}
		call(_mem_hndl_SQ32[optp - SZ_32I][raddr.GetCode()]);
	}
	else
	{
		//Fallback to function !

		if (optp >= SZ_32F)
		{
			if (!raddr.Is(r0))
				Mov(r0, raddr);
			else
				Nop();
		}

		if (write)
		{
			if (optp <= SZ_32I)
				Mov(r1, rt);
			else if (optp == SZ_32F)
				Vmov(r1, ft);
			else if (optp == SZ_64F)
				Vmov(r2, r3, fd);
		}

		const void *funct;

		if (optp >= SZ_32F)
			funct = _mem_func[!write][optp - SZ_32F];
		else
			funct = _mem_hndl[!write][optp][raddr.GetCode()];

		call(funct);

		if (!write)
		{
			if (optp <= SZ_32I)
				Mov(rt, r0);
			else if (optp == SZ_32F)
				Vmov(ft, r0);
			else if (optp == SZ_64F)
				Vmov(fd, r0, r1);
		}
	}

	Finalize();
}

Register Arm32Assembler::GenMemAddr(shil_opcode* op, Register raddr)
{
	if (op->rs3.is_imm())
	{
		if (ImmediateA32::IsImmediateA32(op->rs3._imm))
		{
			Add(raddr, reg.mapReg(op->rs1), op->rs3._imm);
		}
		else 
		{
			Mov(r1, op->rs3._imm);
			Add(raddr, reg.mapReg(op->rs1), r1);
		}
	}
	else if (op->rs3.is_r32i())
	{
		Add(raddr, reg.mapReg(op->rs1), reg.mapReg(op->rs3));
	}
	else if (!op->rs3.is_null())
	{
		ERROR_LOG(DYNAREC, "rs3: %08X", op->rs3.type);
		die("invalid rs3");
	}
	else if (op->rs1.is_imm())
	{
		Mov(raddr, op->rs1._imm);
	}
	else
	{
		raddr = reg.mapReg(op->rs1);
	}

	return raddr;
}

bool Arm32Assembler::readMemImmediate(RuntimeBlockInfo* block, shil_opcode* op, bool optimise)
{
	if (!op->rs1.is_imm())
		return false;

	void *ptr;
	bool isram;
	u32 addr;
	if (!rdv_readMemImmediate(op->rs1._imm, op->size, ptr, isram, addr, block))
		return false;
	
	mem_op_type optp = memop_type(op);
	Register rd = (optp != SZ_32F && optp != SZ_64F) ? reg.mapReg(op->rd) : r0;

	if (isram)
	{
		if (optp == SZ_32F || optp == SZ_64F)
			ptr = (void *)((uintptr_t)ptr & ~3);
		Mov(r0, (u32)ptr);
		switch(optp)
		{
		case SZ_8:
			Ldrsb(rd, MemOperand(r0));
			break;

		case SZ_16:
			Ldrsh(rd, MemOperand(r0));
			break;

		case SZ_32I:
			Ldr(rd, MemOperand(r0));
			break;

		case SZ_32F:
			Vldr(reg.mapFReg(op->rd), MemOperand(r0));
			break;

		case SZ_64F:
			if (reg.IsAllocf(op->rd))
			{
				Vldr(reg.mapFReg(op->rd, 0), MemOperand(r0));
				Vldr(reg.mapFReg(op->rd, 1), MemOperand(r0, 4));
			}
			else
			{
				Vldr(d0, MemOperand(r0));
				Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
			}
			break;
		}
	}
	else
	{
		// Not RAM
		if (optp == SZ_64F)
		{
			// Need to call the handler twice
			Mov(r0, op->rs1._imm);
			call(ptr);
			if (reg.IsAllocf(op->rd))
				Vmov(reg.mapFReg(op->rd, 0), r0);
			else
				Str(r0, MemOperand(r8, op->rd.reg_nofs()));

			Mov(r0, op->rs1._imm + 4);
			call(ptr);
			if (reg.IsAllocf(op->rd))
				Vmov(reg.mapFReg(op->rd, 1), r0);
			else
				Str(r0, MemOperand(r8, op->rd.reg_nofs() + 4));
		}
		else
		{
			Mov(r0, op->rs1._imm);
			call(ptr);

			switch(optp)
			{
			case SZ_8:
				Sxtb(r0, r0);
				break;

			case SZ_16:
				Sxth(r0, r0);
				break;

			case SZ_32I:
			case SZ_32F:
				break;

			default:
				die("Invalid size");
				break;
			}

			if (reg.IsAllocg(op->rd))
				Mov(rd, r0);
			else if (reg.IsAllocf(op->rd))
				Vmov(reg.mapFReg(op->rd), r0);
			else
				die("Unsupported");
		}
	}

	return true;
}

bool Arm32Assembler::writeMemImmediate(RuntimeBlockInfo* block, shil_opcode* op, bool optimise)
{
	if (!op->rs1.is_imm())
		return false;

	void *ptr;
	bool isram;
	u32 addr;
	if (!rdv_writeMemImmediate(op->rs1._imm, op->size, ptr, isram, addr, block))
		return false;

	mem_op_type optp = memop_type(op);
	Register rs2 = r1;
	SRegister rs2f = s0;
	if (op->rs2.is_imm())
		Mov(rs2, op->rs2._imm);
	else if (optp == SZ_32F)
		rs2f = reg.mapFReg(op->rs2);
	else if (optp != SZ_64F)
		rs2 = reg.mapReg(op->rs2);

	if (isram)
	{
		if (optp == SZ_32F || optp == SZ_64F)
			ptr = (void *)((uintptr_t)ptr & ~3);
		Mov(r0, (u32)ptr);
		switch(optp)
		{
		case SZ_8:
			Strb(rs2, MemOperand(r0));
			break;

		case SZ_16:
			Strh(rs2, MemOperand(r0));
			break;

		case SZ_32I:
			Str(rs2, MemOperand(r0));
			break;

		case SZ_32F:
			Vstr(rs2f, MemOperand(r0));
			break;

		case SZ_64F:
			if (reg.IsAllocf(op->rs2))
			{
				Vstr(reg.mapFReg(op->rs2, 0), MemOperand(r0));
				Vstr(reg.mapFReg(op->rs2, 1), MemOperand(r0, 4));
			}
			else
			{
				Vldr(d0, MemOperand(r8, op->rs2.reg_nofs()));
				Vstr(d0, MemOperand(r0));
			}
			break;

		default:
			die("Invalid size");
			break;
		}
	}
	else
	{
		if (optp == SZ_64F)
			die("SZ_64F not supported");
		Mov(r0, op->rs1._imm);
		if (optp == SZ_8)
			Uxtb(r1, rs2);
		else if (optp == SZ_16)
			Uxth(r1, rs2);
		else if (optp == SZ_32F)
			Vmov(r1, rs2f);
		else if (!rs2.Is(r1))
			Mov(r1, rs2);

		call(ptr);
	}
	return true;
}

void Arm32Assembler::genMmuLookup(RuntimeBlockInfo* block, const shil_opcode& op, u32 write, Register& raddr)
{
	if (mmu_enabled())
	{
		Label inCache;
		Label done;

		Lsr(r1, raddr, 12);
		Ldr(r1, MemOperand(r9, r1, LSL, 2));
		Cmp(r1, 0);
		B(ne, &inCache);
		if (!raddr.Is(r0))
			Mov(r0, raddr);
		Mov(r1, write);
		Mov(r2, block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0));	// pc
		call((void *)mmuDynarecLookup);
		B(&done);
		Bind(&inCache);
		And(r0, raddr, 0xFFF);
		Orr(r0, r0, r1);
		Bind(&done);
		raddr = r0;
	}
}

static void interpreter_fallback(Sh4Context *ctx, u16 op, OpCallFP *oph, u32 pc)
{
	try {
		oph(ctx, op);
	} catch (SH4ThrownException& ex) {
		if (pc & 1)
		{
			// Delay slot
			AdjustDelaySlotException(ex);
			pc--;
		}
		Do_Exception(pc, ex.expEvn);
		handleException();
	}
}

static void do_sqw_mmu_no_ex(u32 addr, Sh4Context *ctx, u32 pc)
{
	try {
		ctx->doSqWrite(addr, ctx);
	} catch (SH4ThrownException& ex) {
		if (pc & 1)
		{
			// Delay slot
			AdjustDelaySlotException(ex);
			pc--;
		}
		Do_Exception(pc, ex.expEvn);
		handleException();
	}
}

void Arm32Assembler::compileOp(RuntimeBlockInfo* block, shil_opcode* op, bool optimise)
{
	switch(op->op)
	{
		case shop_readm:
			if (!readMemImmediate(block, op, optimise))
			{
				mem_op_type optp = memop_type(op);
				Register raddr = GenMemAddr(op);
				genMmuLookup(block, *op, 0, raddr);

				if (addrspace::virtmemEnabled()) {
					Bic(r1, raddr, optp == SZ_32F || optp == SZ_64F ? 0xE0000003 : 0xE0000000);

					switch(optp)
					{
					case SZ_8:	
						Ldrsb(reg.mapReg(op->rd), MemOperand(r1, r8));
						break;

					case SZ_16: 
						Ldrsh(reg.mapReg(op->rd), MemOperand(r1, r8));
						break;

					case SZ_32I: 
						Ldr(reg.mapReg(op->rd), MemOperand(r1, r8));
						break;

					case SZ_32F:
						Add(r1, r1, r8);	//3 opcodes, there's no [REG+REG] VLDR
						Vldr(reg.mapFReg(op->rd), MemOperand(r1));
						break;

					case SZ_64F:
						Add(r1, r1, r8);	//3 opcodes, there's no [REG+REG] VLDR
						Vldr(d0, MemOperand(r1));
						if (reg.IsAllocf(op->rd))
						{
							Vmov(r0, r1, d0);
							Vmov(reg.mapFReg(op->rd, 0), r0);
							Vmov(reg.mapFReg(op->rd, 1), r1);
							// easier to do just this but we need to use a different op than 32f to distinguish during rewrite
							//Vldr(reg.mapFReg(op->rd, 0), MemOperand(r1));
							//Vldr(reg.mapFReg(op->rd, 1), MemOperand(r1, 4));
						}
						else
						{
							Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
						}
						break;
					}
				} else {
					switch(optp)
					{
					case SZ_8:	
						vmem_slowpath(raddr, reg.mapReg(op->rd), s0, d0, optp, true);
						break;

					case SZ_16: 
						vmem_slowpath(raddr, reg.mapReg(op->rd), s0, d0, optp, true);
						break;

					case SZ_32I: 
						vmem_slowpath(raddr, reg.mapReg(op->rd), s0, d0, optp, true);
						break;

					case SZ_32F:
						vmem_slowpath(raddr, r0, reg.mapFReg(op->rd), d0, optp, true);
						break;

					case SZ_64F:
						vmem_slowpath(raddr, r0, s0, d0, optp, true);
						if (reg.IsAllocf(op->rd))
						{
							Vmov(r0, r1, d0);
							Vmov(reg.mapFReg(op->rd, 0), r0);
							Vmov(reg.mapFReg(op->rd, 1), r1);
						}
						else
						{
							Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
						}
						break;
					}
				}
			}
			break;

		case shop_writem:
			if (!writeMemImmediate(block, op, optimise))
			{
				mem_op_type optp = memop_type(op);

				Register raddr = GenMemAddr(op);
				genMmuLookup(block, *op, 1,raddr);

				Register rs2 = r2;
				SRegister rs2f = s2;

				if (optp == SZ_64F)
				{
					if (reg.IsAllocf(op->rs2))
					{
						Vmov(r2, reg.mapFReg(op->rs2, 0));
						Vmov(r3, reg.mapFReg(op->rs2, 1));
						Vmov(d0, r2, r3);
					}
					else
					{
						Vldr(d0, MemOperand(r8, op->rs2.reg_nofs()));
					}
				}
				else if (op->rs2.is_imm())
				{
					Mov(rs2, op->rs2._imm);
					if (optp == SZ_32F)
						Vmov(rs2f, rs2);
				}
				else
				{
					if (optp == SZ_32F)
						rs2f = reg.mapFReg(op->rs2);
					else
						rs2 = reg.mapReg(op->rs2);
				}
				if (addrspace::virtmemEnabled())
				{
					Bic(r1, raddr, optp == SZ_32F || optp == SZ_64F ? 0xE0000003 : 0xE0000000);

					switch(optp)
					{
					case SZ_8:
						Strb(rs2, MemOperand(r1, r8));
						break;

					case SZ_16:
						Strh(rs2, MemOperand(r1, r8));
						break;

					case SZ_32I:
						Str(rs2, MemOperand(r1, r8));
						break;

					case SZ_32F:
						Add(r1, r1, r8);	//3 opcodes: there's no [REG+REG] VLDR, also required for SQ
						Vstr(rs2f, MemOperand(r1));
						break;

					case SZ_64F:
						Add(r1, r1, r8);	//3 opcodes: there's no [REG+REG] VLDR, also required for SQ
						Vstr(d0, MemOperand(r1));
						break;
					}
				} else {
					switch(optp)
					{
					case SZ_8:
						vmem_slowpath(raddr, rs2, s0, d0, optp, false);
						break;

					case SZ_16:
						vmem_slowpath(raddr, rs2, s0, d0, optp, false);
						break;

					case SZ_32I:
						vmem_slowpath(raddr, rs2, s0, d0, optp, false);
						break;

					case SZ_32F:
						vmem_slowpath(raddr, r0, rs2f, d0, optp, false);
						break;

					case SZ_64F:
						vmem_slowpath(raddr, r0, s0, d0, optp, false);
						break;
					}
				}
			}
			break;

		//dynamic jump, r+imm32.This will be at the end of the block, but doesn't -have- to be the last opcode
		case shop_jdyn:
			verify(op->rd.is_reg() && op->rd._reg == reg_pc_dyn);
			if (op->rs2.is_imm())
			{
				Mov(r2, op->rs2.imm_value());
				Add(r4, reg.mapReg(op->rs1), r2);
			}
			else
			{
				Mov(r4, reg.mapReg(op->rs1));
			}
			break;

		case shop_mov32:
			verify(op->rd.is_r32());

			if (op->rs1.is_imm())
			{
				if (op->rd.is_r32i())
				{
					Mov(reg.mapReg(op->rd), op->rs1._imm);
				}
				else
				{
					if (op->rs1._imm==0)
					{
						//VEOR(reg.mapFReg(op->rd),reg.mapFReg(op->rd),reg.mapFReg(op->rd));
						//hum, vmov can't do 0, but can do all kind of weird small consts ... really useful ...
						//simd is slow on a9
#if 0
						Movw(r0, 0);
						Vmov(reg.mapFReg(op->rd), r0);
#else
						//1-1=0 !
						//should be slightly faster ...
						//we could get rid of the imm mov, if not for infs & co ..
						Vmov(reg.mapFReg(op->rd), 1.f);;
						Vsub(reg.mapFReg(op->rd), reg.mapFReg(op->rd), reg.mapFReg(op->rd));
#endif
					}
					else if (op->rs1._imm == 0x3F800000)
						Vmov(reg.mapFReg(op->rd), 1.f);
					else
					{
						Mov(r0, op->rs1._imm);
						Vmov(reg.mapFReg(op->rd), r0);
					}
				}
			}
			else if (op->rs1.is_r32())
			{
				u32 type = 0;

				if (reg.IsAllocf(op->rd))
					type |= 1;

				if (reg.IsAllocf(op->rs1))
					type |= 2;

				switch(type)
				{
				case 0: // reg = reg
					Mov(reg.mapReg(op->rd), reg.mapReg(op->rs1));
					break;

				case 1: // vfp = reg
					Vmov(reg.mapFReg(op->rd), reg.mapReg(op->rs1));
					break;

				case 2: // reg = vfp
					Vmov(reg.mapReg(op->rd), reg.mapFReg(op->rs1));
					break;

				case 3: // vfp = vfp
					Vmov(reg.mapFReg(op->rd), reg.mapFReg(op->rs1));
					break;
				}
			}
			else
			{
				die("Invalid mov32 size");
			}
			break;
			
		case shop_mov64:
			verify(op->rs1.is_r64f() && op->rd.is_r64f());
			if (reg.IsAllocf(op->rd))
			{
				verify(reg.IsAllocf(op->rs1));
				SRegister rd0 = reg.mapFReg(op->rd, 0);
				SRegister rs0 = reg.mapFReg(op->rs1, 0);
				SRegister rd1 = reg.mapFReg(op->rd, 1);
				SRegister rs1 = reg.mapFReg(op->rs1, 1);
				if (rd0.Is(rs1))
				{
					Vmov(s0, rd0);
					Vmov(rd0, rs0);
					Vmov(rd1, s0);
				}
				else
				{
					if (!rd0.Is(rs0))
						Vmov(rd0, rs0);
					if (!rd1.Is(rs1))
						Vmov(rd1, rs1);
				}
			}
			else
			{
				Vldr(d0, MemOperand(r8, op->rs1.reg_nofs()));
				Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
			}
			break;

		case shop_jcond:
			verify(op->rd.is_reg() && op->rd._reg == reg_pc_dyn);
			Mov(r4, reg.mapReg(op->rs1));
			break;

		case shop_ifb:
			if (op->rs1._imm) 
			{
				Mov(r1, op->rs2._imm);
				storeSh4Reg(r1, reg_nextpc);
			}

			Sub(r0, r8, sizeof(Sh4Context));
			Mov(r1, op->rs3._imm);
			if (!mmu_enabled())
			{
				call((void *)OpPtr[op->rs3._imm]);
			}
			else
			{
				Mov(r2, reinterpret_cast<uintptr_t>(*OpDesc[op->rs3._imm]->oph));	// op handler
				Mov(r3, block->vaddr + op->guest_offs - (op->delay_slot ? 1 : 0));	// pc
				call((void *)interpreter_fallback);
			}
			break;

#ifndef CANONICALTEST
		case shop_neg:
			Rsb(reg.mapReg(op->rd), reg.mapReg(op->rs1), 0);
			break;
		case shop_not:
			Mvn(reg.mapReg(op->rd), reg.mapReg(op->rs1));
			break;

		case shop_shl:
			binaryOp(op, &MacroAssembler::Lsl);
			break;
		case shop_shr:
			binaryOp(op, &MacroAssembler::Lsr);
			break;
		case shop_sar:
			binaryOp(op, &MacroAssembler::Asr);
			break;

		case shop_and:
			binaryOp(op, &MacroAssembler::And);
			break;
		case shop_or:
			binaryOp(op, &MacroAssembler::Orr);
			break;
		case shop_xor:
			binaryOp(op, &MacroAssembler::Eor);
			break;

		case shop_add:
			binaryOp(op, &MacroAssembler::Add);
			break;
		case shop_sub:
			binaryOp(op, &MacroAssembler::Sub);
			break;
		case shop_ror:
			binaryOp(op, &MacroAssembler::Ror);
			break;

		case shop_adc:
			{
				Register rs1 = GetParam(op->rs1, r1);
				Register rs2 = GetParam(op->rs2, r2);
				Register rs3 = GetParam(op->rs3, r3);

				Lsr(SetFlags, r0, rs3, 1); 					 //C=rs3, r0=0
				Adc(SetFlags, reg.mapReg(op->rd), rs1, rs2); //(C,rd)=rs1+rs2+rs3(C)
				Adc(reg.mapReg(op->rd2), r0, 0);			 //rd2=C, (or MOVCS rd2, 1)
			}
			break;

		case shop_rocr:
			{
				Register rd2 = reg.mapReg(op->rd2);
				Register rs1 = GetParam(op->rs1, r1);
				Register rs2 = GetParam(op->rs2, r2);
				if (!rd2.Is(rs1)) {
					Lsr(SetFlags, rd2, rs2, 1);	//C=rs2, rd2=0
					And(rd2, rs1, 1);     		//get new carry
				} else {
					Lsr(SetFlags, r0, rs2, 1);	//C=rs2, rd2=0
					Add(r0, rs1, 1);
				}
				Rrx(reg.mapReg(op->rd), rs1);	//RRX w/ carry :)
				if (rd2.Is(rs1))
					Mov(rd2, r0);
			}
			break;
			
		case shop_rocl:
			{
				Register rs1 = GetParam(op->rs1, r1);
				Register rs2 = GetParam(op->rs2, r2);
				Orr(SetFlags, reg.mapReg(op->rd), rs2, Operand(rs1, LSL, 1)); //(C,rd)= rs1<<1 + (|) rs2
				Mov(reg.mapReg(op->rd2), 0);								  //clear rd2 (for ADC/MOVCS)
				Adc(reg.mapReg(op->rd2), reg.mapReg(op->rd2), 0);			  //rd2=C (or MOVCS rd2, 1)
			}
			break;
			
		case shop_sbc:
			{
				Register rd2 = reg.mapReg(op->rd2);
				Register rs1 = GetParam(op->rs1, r1);
				if (rs1.Is(rd2))
				{
					Mov(r1, rs1);
					rs1 = r1;
				}
				Register rs2 = GetParam(op->rs2, r2);
				if (rs2.Is(rd2))
				{
					Mov(r2, rs2);
					rs2 = r2;
				}
				Register rs3 = GetParam(op->rs3, r3);
				Eor(rd2, rs3, 1);
				Lsr(SetFlags, rd2, rd2, 1); //C=rs3, rd2=0
				Sbc(SetFlags, reg.mapReg(op->rd), rs1, rs2);
				Mov(cc, rd2, 1);
			}
			break;
		
		case shop_negc:
			{
				Register rd2 = reg.mapReg(op->rd2);
				Register rs1 = GetParam(op->rs1, r1);
				if (rs1.Is(rd2))
				{
					Mov(r1, rs1);
					rs1 = r1;
				}
				Register rs2 = GetParam(op->rs2, r2);
				Eor(rd2, rs2, 1);
				Lsr(SetFlags, rd2, rd2, 1);						//C=rs3, rd2=0
				Sbc(SetFlags, reg.mapReg(op->rd), rd2, rs1);	// rd2 == 0
				Mov(cc, rd2, 1);
			}
			break;

		case shop_shld:
			{
				verify(!op->rs2.is_imm());
				And(SetFlags, r0, reg.mapReg(op->rs2), 0x8000001F);
				Rsb(mi, r0, r0, 0x80000020);
				Register rs1 = GetParam(op->rs1, r1);
				Lsr(mi, reg.mapReg(op->rd), rs1, r0);
				Lsl(pl, reg.mapReg(op->rd), rs1, r0);
			}		
			break;

		case shop_shad:
			{
				verify(!op->rs2.is_imm());
				And(SetFlags, r0, reg.mapReg(op->rs2), 0x8000001F);
				Rsb(mi, r0, r0, 0x80000020);
				Register rs1 = GetParam(op->rs1, r1);
				Asr(mi, reg.mapReg(op->rd), rs1, r0);
				Lsl(pl, reg.mapReg(op->rd), rs1, r0);
			}		
			break;

		case shop_sync_sr:
			//must flush: SRS, SRT, r0-r7, r0b-r7b
			call((void *)UpdateSR);
			break;

		case shop_sync_fpscr:
			Sub(r0, r8, sizeof(Sh4Context));
			call((void *)Sh4Context::UpdateFPSCR);
			break;

		case shop_test:
		case shop_seteq:
		case shop_setge:
		case shop_setgt:
		case shop_setae:
		case shop_setab:
			{
				Register rd = reg.mapReg(op->rd);
				Register rs1 = GetParam(op->rs1, r0);

				Register rs2 = r1;
				bool is_imm = false;

				if (op->rs2.is_imm())
				{
					if (!ImmediateA32::IsImmediateA32(op->rs2._imm))
						Mov(rs2, (u32)op->rs2._imm);
					else
						is_imm = true;
				}
				else if (op->rs2.is_r32i())
				{
					rs2 = reg.mapReg(op->rs2);
				}
				else
				{
					ERROR_LOG(DYNAREC, "ngen_Bin ??? %d", op->rs2.type);
					verify(false);
				}

				if (op->op == shop_test)
				{
					if (is_imm)
						Tst(rs1, op->rs2._imm);
					else
						Tst(rs1, rs2);
				}
				else
				{
					if (is_imm)
						Cmp(rs1, op->rs2._imm);
					else
						Cmp(rs1, rs2);
				}

				static const ConditionType opcls2[] = { eq, eq, ge, gt, hs, hi };

				Mov(rd, 0);
				Mov(opcls2[op->op-shop_test], rd, 1);
			}
			break;

		case shop_setpeq:
			{
				Register rs1 = GetParam(op->rs1, r1);
				Register rs2 = GetParam(op->rs2, r2);
				Eor(r1, rs1, rs2);
				Mov(reg.mapReg(op->rd), 0);
				
				Tst(r1, 0xFF000000u);
				Tst(ne, r1, 0x00FF0000u);
				Tst(ne, r1, 0x0000FF00u);
				Tst(ne, r1, 0x000000FFu);
				Mov(eq, reg.mapReg(op->rd), 1);
			}
			break;
		
		//UXTH for zero extention and/or more mul forms (for 16 and 64 bits)

		case shop_mul_u16:
			{
				Register rs2 = GetParam(op->rs2, r2);
				Uxth(r1, reg.mapReg(op->rs1));
				Uxth(r2, rs2);
				Mul(reg.mapReg(op->rd), r1, r2);
			}
			break;
		case shop_mul_s16:
			{
				Register rs2 = GetParam(op->rs2, r2);
				Sxth(r1, reg.mapReg(op->rs1));
				Sxth(r2, rs2);
				Mul(reg.mapReg(op->rd), r1, r2);
			}
			break;
		case shop_mul_i32:
			{
				Register rs2 = GetParam(op->rs2, r2);
				//x86_opcode_class opdt[]={op_movzx16to32,op_movsx16to32,op_mov32,op_mov32,op_mov32};
				//x86_opcode_class opmt[]={op_mul32,op_mul32,op_mul32,op_mul32,op_imul32};
				//only the top 32 bits are different on signed vs unsigned

				Mul(reg.mapReg(op->rd), reg.mapReg(op->rs1), rs2);
			}
			break;
		case shop_mul_u64:
			{
				Register rs2 = GetParam(op->rs2, r2);
				Umull(reg.mapReg(op->rd), reg.mapReg(op->rd2), reg.mapReg(op->rs1), rs2);
			}
			break;
		case shop_mul_s64:
			{
				Register rs2 = GetParam(op->rs2, r2);
				Smull(reg.mapReg(op->rd), reg.mapReg(op->rd2), reg.mapReg(op->rs1), rs2);
			}
			break;
	
		case shop_pref:
			{
				ConditionType cc = eq;
				if (!op->rs1.is_imm())
				{
					Lsr(r1, reg.mapReg(op->rs1), 26);
					Mov(r0, reg.mapReg(op->rs1));
					Cmp(r1, 0x38);
				}
				else
				{
					// The SSA pass has already checked that the
					// destination is a store queue so no need to check
					Mov(r0, op->rs1.imm_value());
					cc = al;
				}

				Sub(r1, r8, sizeof(Sh4Context));
				if (mmu_enabled())
				{
					Mov(r2, block->vaddr + op->guest_offs - (op->delay_slot ? 1 : 0));	// pc
					call((void *)do_sqw_mmu_no_ex, cc);
				}
				else
				{
					Ldr(r2, MemOperand(r8, ctxOffset(doSqWrite)));
					Blx(cc, r2);
				}
			}
			break;

		case shop_ext_s8:
		case shop_ext_s16:
			if (op->op == shop_ext_s8)
				Sxtb(reg.mapReg(op->rd), reg.mapReg(op->rs1));
			else
				Sxth(reg.mapReg(op->rd), reg.mapReg(op->rs1));
			break;
			
		case shop_xtrct:
			{
				Register rd = reg.mapReg(op->rd);
				Register rs1;
				if (op->rs1.is_imm()) {
					rs1 = r1;
					Mov(rs1, op->rs1._imm);
				}
				else
				{
					rs1 = reg.mapReg(op->rs1);
				}
				Register rs2;
				if (op->rs2.is_imm()) {
					rs2 = r2;
					Mov(rs2, op->rs2._imm);
				}
				else
				{
					rs2 = reg.mapReg(op->rs2);
				}
				if (rd.Is(rs1))
				{
					verify(!rd.Is(rs2));
					Lsr(rd, rs1, 16);
					Lsl(r0, rs2, 16);
				}
				else
				{
					Lsl(rd, rs2, 16);
					Lsr(r0, rs1, 16);
				}
				Orr(rd, rd, r0);
			}
			break;

		//
		// FPU
		//

		case shop_fadd:
		case shop_fsub:
		case shop_fmul:
		case shop_fdiv:
			{
				static const FPBinOP opcds[] = {
						&MacroAssembler::Vadd, &MacroAssembler::Vsub, &MacroAssembler::Vmul, &MacroAssembler::Vdiv
				};
				binaryFpOp(op, opcds[op->op - shop_fadd]);
			}
			break;

		case shop_fabs:
		case shop_fneg:
			{
				static const FPUnOP opcds[] = { &MacroAssembler::Vabs, &MacroAssembler::Vneg };
				unaryFpOp(op, opcds[op->op - shop_fabs]);
			}
			break;

		case shop_fsqrt:
			unaryFpOp(op, &MacroAssembler::Vsqrt);
			break;

		case shop_fmac:
			{
				SRegister rd = reg.mapFReg(op->rd);
				SRegister rs1 = s1;
				if (op->rs1.is_imm())
				{
					Mov(r0, op->rs1.imm_value());
					Vmov(rs1, r0);
				}
				else
					rs1 = reg.mapFReg(op->rs1);
				SRegister rs2 = s2;
				if (op->rs2.is_imm())
				{
					Mov(r1, op->rs2.imm_value());
					Vmov(rs2, r1);
				}
				else
				{
					rs2 = reg.mapFReg(op->rs2);
					if (rs2.Is(rd))
					{
						Vmov(s2, rs2);
						rs2 = s2;
					}
				}
				SRegister rs3 = s3;
				if (op->rs3.is_imm())
				{
					Mov(r2, op->rs3.imm_value());
					Vmov(rs3, r2);
				}
				else
				{
					rs3 = reg.mapFReg(op->rs3);
					if (rs3.Is(rd))
					{
						Vmov(s3, rs3);
						rs3 = s3;
					}
				}
				if (!rd.Is(rs1))
					Vmov(rd, rs1);
				Vfma(rd, rs2, rs3);
			}
			break;

		case shop_fsrra:
			Vmov(s1, 1.f);
			Vsqrt(s0, reg.mapFReg(op->rs1));
			Vdiv(reg.mapFReg(op->rd), s1, s0);
			break;

		case shop_fsetgt:
		case shop_fseteq:
#if 1
			//this is apparently much faster (tested on A9)
			Mov(reg.mapReg(op->rd), 0);
			Vcmp(reg.mapFReg(op->rs1), reg.mapFReg(op->rs2));

			Vmrs(RegisterOrAPSR_nzcv(APSR_nzcv), FPSCR);
			if (op->op == shop_fsetgt)
				Mov(gt, reg.mapReg(op->rd), 1);
			else
				Mov(eq, reg.mapReg(op->rd), 1);
#else
			if (op->op == shop_fsetgt)
				Vcgt(d0, reg.mapFReg(op->rs1), reg.mapFReg(op->rs2));
			else
				Vceq(d0, reg.mapFReg(op->rs1), reg.mapFReg(op->rs2));

			Vmov(r0, s0);
			And(reg.mapReg(op->rd), r0, 1);
#endif
			break;
			

		case shop_fsca:
			//r1: base ptr
			Mov(r1, (u32)sin_table & 0xFFFF);
			if (op->rs1.is_imm())
				Mov(r0, op->rs1._imm & 0xFFFF);
			else
				Uxth(r0, reg.mapReg(op->rs1));
			Movt(r1, (u32)sin_table >> 16);

			Add(r0, r1, Operand(r0, LSL, 3));

			if (reg.IsAllocf(op->rd))
			{
				Vldr(reg.mapFReg(op->rd, 0), MemOperand(r0));
				Vldr(reg.mapFReg(op->rd, 1), MemOperand(r0, 4));
			}
			else
			{
				Vldr(d0, MemOperand(r0));
				Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
			}
			break;
		/* fall back to the canonical implementations for better precision
		case shop_fipr:
			{
				QRegister _r1 = q0;
				QRegister _r2 = q0;

				Sub(r0, r8, -op->rs1.reg_nofs());
				if (op->rs2.reg_nofs() == op->rs1.reg_nofs())
				{
					Vldm(r0, NO_WRITE_BACK, DRegisterList(d0, 2));
				}
				else
				{
					Sub(r1, r8, -op->rs2.reg_nofs());
					Vldm(r0, NO_WRITE_BACK, DRegisterList(d0, 2));
					Vldm(r1, NO_WRITE_BACK, DRegisterList(d2, 2));
					_r2 = q1;
				}

#if 1
				//VFP
				SRegister fs2 = _r2.Is(q0) ? s0 : s4;

				Vmul(reg.mapFReg(op->rd), s0, fs2);
				Vmla(reg.mapFReg(op->rd), s1, SRegister(fs2.GetCode() + 1));
				Vmla(reg.mapFReg(op->rd), s2, SRegister(fs2.GetCode() + 2));
				Vmla(reg.mapFReg(op->rd), s3, SRegister(fs2.GetCode() + 3));
#else			
				Vmul(q0, _r1, _r2);
				Vpadd(d0, d0, d1);
				Vadd(reg.mapFReg(op->rd), f0, f1);
#endif
			}
			break;

		case shop_ftrv:
			{
				Register rdp = r1;
				Sub(r2, r8, -op->rs2.reg_nofs());
				Sub(r1, r8, -op->rs1.reg_nofs());
				if (op->rs1.reg_nofs() != op->rd.reg_nofs())
				{
					rdp = r0;
					Sub(r0, r8, -op->rd.reg_nofs());
				}
	
#if 1
				//f0,f1,f2,f3	  : vin
				//f4,f5,f6,f7     : out
				//f8,f9,f10,f11   : mtx temp
				//f12,f13,f14,f15 : mtx temp
				//(This is actually faster than using neon)

				Vldm(r2, WRITE_BACK, DRegisterList(d4, 2));
				Vldm(r1, NO_WRITE_BACK, DRegisterList(d0, 2));

				Vmul(s4, vixl::aarch32::s8, s0);
				Vmul(s5, s9, s0);
				Vmul(s6, s10, s0);
				Vmul(s7, s11, s0);
				
				Vldm(r2, WRITE_BACK, DRegisterList(d6, 2));

				Vmla(s4, s12, s1);
				Vmla(s5, s13, s1);
				Vmla(s6, s14, s1);
				Vmla(s7, s15, s1);

				Vldm(r2, WRITE_BACK, DRegisterList(d4, 2));

				Vmla(s4, vixl::aarch32::s8, s2);
				Vmla(s5, s9, s2);
				Vmla(s6, s10, s2);
				Vmla(s7, s11, s2);

				Vldm(r2, NO_WRITE_BACK, DRegisterList(d6, 2));

				Vmla(s4, s12, s3);
				Vmla(s5, s13, s3);
				Vmla(s6, s14, s3);
				Vmla(s7, s15, s3);

				Vstm(rdp, NO_WRITE_BACK, DRegisterList(d2, 2));
#else
				//this fits really nicely to NEON !
				// TODO
				Vldm(d16,r2,8);
				Vldm(d0,r1,2);

				Vmla(q2,q8,d0,0);
				Vmla(q2,q9,d0,1);
				Vmla(q2,q10,d1,0);
				Vmla(q2,q11,d1,1);
				Vstm(d4,rdp,2);
#endif
			}
			break;
			*/
		case shop_frswap:
			Sub(r0, r8, -op->rs1.reg_nofs());
			Sub(r1, r8, -op->rd.reg_nofs());
			//Assumes no FPU reg alloc here
			//frswap touches all FPU regs, so all spans should be clear here ..
			Vldm(r1, NO_WRITE_BACK, DRegisterList(d0, 8));
			Vldm(r0, NO_WRITE_BACK, DRegisterList(d8, 8));
			Vstm(r0, NO_WRITE_BACK, DRegisterList(d0, 8));
			Vstm(r1, NO_WRITE_BACK, DRegisterList(d8, 8));
			break;

		case shop_cvt_f2i_t:
			{
				SRegister from = reg.mapFReg(op->rs1);
				Register to = reg.mapReg(op->rd);
				Vcvt(S32, F32, s0, from);
				Vmov(to, s0);
				Mvn(r0, 127);
				Sub(r0, r0, 0x80000000);
				Cmp(to, r0);
				Mvn(gt, to, 0xf8000000);
				Vcmp(from, from);
				Vmrs(RegisterOrAPSR_nzcv(APSR_nzcv), FPSCR);
				Mov(ne, to, 0x80000000);
			}
			break;

		case shop_cvt_i2f_n:	// may be some difference should be made ?
		case shop_cvt_i2f_z:
			Vmov(s0, reg.mapReg(op->rs1));
			Vcvt(F32, S32, reg.mapFReg(op->rd), s0);
			break;
#endif

		default:
			shil_chf[op->op](op);
			break;
	}
}

void Arm32Assembler::compile(RuntimeBlockInfo* block, bool force_checks, bool optimise)
{
	block->code = (DynarecCodeEntryPtr)codeBuffer.get();

	//reg alloc
	static constexpr int alloc_regs[] = { 5, 6, 7, 9, 10, 11, -1 };
	static constexpr int alloc_regs_mmu[] = { 5, 6, 7, 10, 11, -1 };
	static constexpr int alloc_fpu[] = { 16, 17, 18, 19, 20, 21, 22, 23,
					24, 25, 26, 27, 28, 29, 30, 31, -1 };
	reg.DoAlloc(block, mmu_enabled() ? alloc_regs_mmu : alloc_regs, alloc_fpu);

	u8* blk_start = GetCursorAddress<u8 *>();

	//pre-load the first reg alloc operations, for better efficiency ..
	if (!block->oplist.empty())
		reg.OpBegin(&block->oplist[0], 0);

	// block checks
	if (mmu_enabled())
	{
		Mov(r0, block->vaddr);
		Mov(r1, block->addr);
		if (block->has_fpu_op)
			call((void *)checkBlockFpu);
		else
			call((void *)checkBlockNoFpu);
	}
	if (force_checks)
	{
		u32 addr = block->addr;
		Mov(r0, addr);

		s32 sz = block->sh4_code_size;
		while (sz > 0)
		{
			if (sz > 2)
			{
				u32* ptr = (u32*)GetMemPtr(addr, 4);
				if (ptr != nullptr)
				{
					Mov(r2, (u32)ptr);
					Ldr(r2, MemOperand(r2));
					Mov(r1, *ptr);
					Cmp(r1, r2);

					jump(ngen_blockcheckfail, ne);
				}
				addr += 4;
				sz -= 4;
			}
			else
			{
				u16* ptr = (u16 *)GetMemPtr(addr, 2);
				if (ptr != nullptr)
				{
					Mov(r2, (u32)ptr);
					Ldrh(r2, MemOperand(r2));
					Mov(r1, *ptr);
					Cmp(r1, r2);

					jump(ngen_blockcheckfail, ne);
				}
				addr += 2;
				sz -= 2;
			}
		}
	}

	//scheduler
	Ldr(r1, MemOperand(r8, ctxOffset(cycle_counter)));
	Cmp(r1, 0);
	Label cyclesRemaining;
	B(pl, &cyclesRemaining);
	Mov(r0, block->vaddr);
	call(intc_sched);
	Mov(r1, r0);
	Bind(&cyclesRemaining);
	const u32 cycles = block->guest_cycles;
	if (!ImmediateA32::IsImmediateA32(cycles))
	{
		Sub(r1, r1, cycles & ~3);
		Sub(r1, r1, cycles & 3);
	}
	else
	{
		Sub(r1, r1, cycles);
	}
	Str(r1, MemOperand(r8, ctxOffset(cycle_counter)));

	//compile the block's opcodes
	shil_opcode* op;
	for (size_t i = 0; i < block->oplist.size(); i++)
	{
		op = &block->oplist[i];
		
		op->host_offs = GetCursorOffset();

		if (i != 0)
			reg.OpBegin(op, i);

		compileOp(block, op, optimise);

		reg.OpEnd(op);
	}
	if (block->BlockType == BET_Cond_0 || block->BlockType == BET_Cond_1)
	{
		// Store the arm reg containing sr.T in the block
		// This will be used when the block in (re)linked
		const shil_param param = shil_param(reg_sr_T);
		if (reg.IsAllocg(param))
			((DynaRBI *)block)->T_reg = reg.mapReg(param);
		else
			((DynaRBI *)block)->T_reg = Register();
	}
	reg.Cleanup();

	//Relink written bytes must be added to the count !

	block->relink_offset = GetCursorOffset();
	block->relink_data = 0;

	relinkBlock((DynaRBI *)block);
	Finalize();
	codeBuffer.advance(GetCursorOffset());

	u8* pEnd = GetCursorAddress<u8 *>();
	//blk_start might not be the same, due to profiling counters ..
	block->host_opcodes = (pEnd - blk_start) / 4;

	//host code size needs to cover the entire range of the block
	block->host_code_size = pEnd - (u8*)block->code;
}

void Arm32Dynarec::reset()
{
	INFO_LOG(DYNAREC, "Arm32Dynarec::reset");
	::mainloop = nullptr;
	unwinder.clear();

	if (sh4ctx->CpuRunning)
	{
		// Force the dynarec out of mainloop() to regenerate it
		sh4ctx->CpuRunning = 0;
		restarting = true;
	}
	else
		generate_mainloop();
}

void Arm32Dynarec::generate_mainloop()
{
	if (::mainloop != nullptr)
		return;

	INFO_LOG(DYNAREC, "Generating main loop");
	Arm32Assembler ass(*sh4ctx, *codeBuffer);

	ass.genMainLoop();
}

void Arm32Assembler::genMainLoop()
{
	unwinder.start(GetCursorAddress<void *>());
	// Stubs
	Label ngen_LinkBlock_Shared_stub;
// ngen_LinkBlock_Generic_stub
	ngen_LinkBlock_Generic_stub = GetCursorAddress<const void *>();
	Mov(r1,r4);		// djump/pc -> in case we need it ..
	B(&ngen_LinkBlock_Shared_stub);
// ngen_LinkBlock_cond_Branch_stub
	ngen_LinkBlock_cond_Branch_stub = GetCursorAddress<const void *>();
	Mov(r1, 1);
	B(&ngen_LinkBlock_Shared_stub);
// ngen_LinkBlock_cond_Next_stub
	ngen_LinkBlock_cond_Next_stub = GetCursorAddress<const void *>();
	Mov(r1, 0);
	B(&ngen_LinkBlock_Shared_stub);
// ngen_LinkBlock_Shared_stub
	Bind(&ngen_LinkBlock_Shared_stub);
	Mov(r0, lr);
	Sub(r0, r0, 4);		// go before the call
	call((void *)rdv_LinkBlock);
	Bx(r0);
// ngen_FailedToFindBlock_
	ngen_FailedToFindBlock_ = GetCursorAddress<void (*)()>();
	if (mmu_enabled())
	{
		call((void *)rdv_FailedToFindBlock_pc);
	}
	else
	{
		Mov(r0, r4);
		call((void *)rdv_FailedToFindBlock);
	}
	Bx(r0);
// ngen_blockcheckfail
	ngen_blockcheckfail = GetCursorAddress<const void *>();
	call((void *)rdv_BlockCheckFail);
	if (mmu_enabled())
	{
		Label jumpblockLabel;
		Cmp(r0, 0);
		B(ne, &jumpblockLabel);
		loadSh4Reg(r0, reg_nextpc);
		call((void *)bm_GetCodeByVAddr);
		Bind(&jumpblockLabel);
	}
	Bx(r0);

	// Main loop
	Label no_updateLabel;
// mainloop:
	::mainloop = GetCursorAddress<void (*)(void *)>();
	RegisterList savedRegisters = RegisterList::Union(
			RegisterList(r4, r5, r6, r7),
			RegisterList(r8, r9, r10, r11),
			RegisterList(r12, lr));
	{
		UseScratchRegisterScope scope(this);
		scope.ExcludeAll();
		Push(savedRegisters);
	}
	unwinder.allocStack(0, 40);
	unwinder.saveReg(0, r4, 40);
	unwinder.saveReg(0, r5, 36);
	unwinder.saveReg(0, r6, 32);
	unwinder.saveReg(0, r7, 28);
	unwinder.saveReg(0, r8, 24);
	unwinder.saveReg(0, r9, 20);
	unwinder.saveReg(0, r10, 16);
	unwinder.saveReg(0, r11, 12);
	unwinder.saveReg(0, r12, 8);
	unwinder.saveReg(0, lr, 4);
	Label longjumpLabel;
	if (!mmu_enabled())
	{
		// r8: context
		Mov(r8, r0);
	}
	else
	{
		Sub(sp, sp, 4);
		unwinder.allocStack(0, 8);
		Push(r0);									// push context
		unwinder.saveReg(0, r4, 4);

		Mov(r0, reinterpret_cast<uintptr_t>(&jmp_stack));
		Mov(r1, sp);
		Str(r1, MemOperand(r0));

		Bind(&longjumpLabel);

		Ldr(r8, MemOperand(sp));					// r8: context
		Mov(r9, (uintptr_t)mmuAddressLUT);			// r9: mmu LUT
	}
	Ldr(r4, MemOperand(r8, ctxOffset(pc)));			// r4: pc
	B(&no_updateLabel);								// Go to mainloop !
	// this code is here for fall-through behavior of do_iter
	Label do_iter;
	Label cleanup;
// intc_sched: r0 is pc, r1 is cycle_counter
	intc_sched = GetCursorAddress<const void *>();
	Add(r1, r1, SH4_TIMESLICE);
	Str(r1, MemOperand(r8, ctxOffset(cycle_counter)));
	Str(r0, MemOperand(r8, ctxOffset(pc)));
	Ldr(r0, MemOperand(r8, ctxOffset(CpuRunning)));
	Cmp(r0, 0);
	B(eq, &cleanup);
	Mov(r4, lr);
	call((void *)UpdateSystem_INTC);
	Cmp(r0, 0);
	B(ne, &do_iter);
	Mov(lr, r4);
	Ldr(r0, MemOperand(r8, ctxOffset(cycle_counter)));
	Bx(lr);
// do_iter:
	Bind(&do_iter);
	Ldr(r4, MemOperand(r8, ctxOffset(pc)));

// no_update:
	no_update = GetCursorAddress<const void *>();
	Bind(&no_updateLabel);
	// next_pc _MUST_ be on r4
	Ldr(r0, MemOperand(r8, ctxOffset(CpuRunning)));
	Cmp(r0, 0);
	B(eq, &cleanup);

	if (!mmu_enabled())
	{
		Sub(r2, r8, -rcbOffset(fpcb));
		Ubfx(r1, r4, 1, 24);	// 24+1 bits: 32 MB
									// RAM wraps around so if actual RAM size is 16MB, we won't overflow
		Ldr(pc, MemOperand(r2, r1, LSL, 2));
	}
	else
	{
		Mov(r0, r4);
		call((void *)bm_GetCodeByVAddr);
		Bx(r0);
	}

// cleanup:
	Bind(&cleanup);
	if (mmu_enabled())
		Add(sp, sp, 8);	// pop context & alignment
	{
		UseScratchRegisterScope scope(this);
		scope.ExcludeAll();
		Pop(savedRegisters);
	}
	Bx(lr);

	// Exception handler
	handleException = GetCursorAddress<void (*)()>();
	if (mmu_enabled())
	{
		Mov(r0, reinterpret_cast<uintptr_t>(&jmp_stack));
		Ldr(r1, MemOperand(r0));
		Mov(sp, r1);
		B(&longjumpLabel);
	}

	// MMU Check block (with fpu)
	// r0: vaddr, r1: addr
	checkBlockFpu = GetCursorAddress<void (*)()>();
	Label fpu_enabled;
	loadSh4Reg(r2, reg_sr_status);
	Tst(r2, 1 << 15);		// test SR.FD bit
	B(eq, &fpu_enabled);
	Mov(r1, Sh4Ex_FpuDisabled);	// exception code
	call((void *)Do_Exception);
	loadSh4Reg(r4, reg_nextpc);
	B(&no_updateLabel);
	Bind(&fpu_enabled);
	// fallthrough

	// MMU Check block (no fpu)
	// r0: vaddr, r1: addr
	checkBlockNoFpu = GetCursorAddress<void (*)()>();
	loadSh4Reg(r2, reg_nextpc);
	Cmp(r2, r0);
	Mov(r0, r1);
	jump(ngen_blockcheckfail, ne);
	Bx(lr);

    // Memory handlers
    for (int s=0;s<6;s++)
	{
		const void* fn=s==0?(void*)addrspace::read8SX32:
				 s==1?(void*)addrspace::read16SX32:
				 s==2?(void*)addrspace::read32:
				 s==3?(void*)addrspace::write8:
				 s==4?(void*)addrspace::write16:
				 s==5?(void*)addrspace::write32:
				 0;

		bool read=s<=2;

		//r0 to r13
		for (int i=0;i<=13;i++)
		{
			if (i==1 || i ==2 || i == 3 || i == 4 || i==12 || i==13)
				continue;

			const void *v;
			if (i == 0 && s != 3 && s != 4) {
				v = fn;
			}
			else
			{
				v = GetCursorAddress<const void *>();
				Mov(r0, Register(i));
				if (s == 3)
					Uxtb(r1, r1);
				else if (s == 4)
					Uxth(r1, r1);
				jump(fn);
			}

			_mem_hndl[read][s % 3][i] = v;
		}
	}

	for (int optp = SZ_32I; optp <= SZ_64F; optp++)
	{
		//r0 to r13
		for (int reg = 0; reg <= 13; reg++)
		{
			if (reg == 1 || reg == 2 || reg == 3 || reg == 4 || reg == 12 || reg == 13)
				continue;
			if (optp != SZ_32I && reg != 0)
				continue;

			_mem_hndl_SQ32[optp - SZ_32I][reg] = GetCursorAddress<const void *>();

			if (optp == SZ_64F)
			{
				Lsr(r1, r0, 26);
				Cmp(r1, 0x38);
				And(r1, r0, 0x3F);
				Add(r1, r1, r8);
				jump((void *)&addrspace::write64, ne);
				Strd(r2, r3, MemOperand(r1, getRegOffset(reg_sq_buffer) - sizeof(Sh4Context)));
			}
			else
			{
				And(r3, Register(reg), 0x3F);
				Lsr(r2, Register(reg), 26);
				Add(r3, r3, r8);
				Cmp(r2, 0x38);
				if (reg != 0)
					Mov(ne, r0, Register(reg));
				jump((void *)&addrspace::write32, ne);
				Str(r1, MemOperand(r3, getRegOffset(reg_sq_buffer) - sizeof(Sh4Context)));
			}
			Bx(lr);
		}
	}
	Finalize();
	codeBuffer.advance(GetBuffer()->GetSizeInBytes());

	size_t unwindSize = unwinder.end(codeBuffer.getSize() - 128);
	verify(unwindSize <= 128);

    rdv_SetFailedToFindBlockHandler(ngen_FailedToFindBlock_);

	INFO_LOG(DYNAREC, "readm helpers: up to %p", GetCursorAddress<void *>());
}

void Arm32Dynarec::init(Sh4Context& sh4ctx, Sh4CodeBuffer& codeBuffer)
{
	INFO_LOG(DYNAREC, "Initializing the ARM32 dynarec");

	ccmap[shop_test] = eq;
	ccnmap[shop_test] = ne;

	ccmap[shop_seteq] = eq;
	ccnmap[shop_seteq] = ne;
	
	ccmap[shop_setge] = ge;
	ccnmap[shop_setge] = lt;
	
	ccmap[shop_setgt] = gt;
	ccnmap[shop_setgt] = le;

	ccmap[shop_setae] = hs;
	ccnmap[shop_setae] = lo;

	ccmap[shop_setab] = hi;
	ccnmap[shop_setab] = ls;

	this->sh4ctx = &sh4ctx;
	this->codeBuffer = &codeBuffer;
}

void Arm32Dynarec::handleException(host_context_t &context)
{
	context.pc = (uintptr_t)::handleException;
}

RuntimeBlockInfo* Arm32Dynarec::allocateBlock()
{
	generate_mainloop(); // FIXME why is this needed?
	return new DynaRBI(*sh4ctx, *codeBuffer);
};
#endif
