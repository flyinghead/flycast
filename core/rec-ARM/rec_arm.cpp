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
#include <unistd.h>
#include <array>
#include <map>

#include <aarch32/macro-assembler-aarch32.h>
using namespace vixl::aarch32;

#include "hw/sh4/sh4_opcode_list.h"

#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_rom.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/dyna/ssa_regalloc.h"
#include "hw/sh4/sh4_mem.h"
#include "cfg/option.h"

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
		r5,r6,r7,r10,r11: allocated
		r8: sh4 cntx
		r9: cycle counter

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

#undef do_sqw_nommu
#define rcbOffset(x) (-sizeof(Sh4RCB) + offsetof(Sh4RCB, x))

struct DynaRBI: RuntimeBlockInfo
{
	virtual u32 Relink();
	virtual void Relocate(void* dst) { }
	Register T_reg;
};

using FPBinOP = void (MacroAssembler::*)(DataType, SRegister, SRegister, SRegister);
using FPUnOP = void (MacroAssembler::*)(DataType, SRegister, SRegister);
using BinaryOP = void (MacroAssembler::*)(Register, Register, const Operand&);

class Arm32Assembler : public MacroAssembler
{
public:
	Arm32Assembler() = default;
	Arm32Assembler(u8 *buffer, size_t size) : MacroAssembler(buffer, size, A32) {}

	void Finalize() {
		FinalizeCode();
		vmem_platform_flush_cache(GetBuffer()->GetStartAddress<void *>(), GetCursorAddress<u8 *>() - 1,
				GetBuffer()->GetStartAddress<void *>(), GetCursorAddress<u8 *>() - 1);
	}
};

static Arm32Assembler ass;

static void loadSh4Reg(Register Rt, u32 Sh4_Reg)
{
	const int shRegOffs = (u8*)GetRegPtr(Sh4_Reg) - (u8*)&p_sh4rcb->cntx - sizeof(Sh4cntx);

	ass.Ldr(Rt, MemOperand(r8, shRegOffs));
}

static void storeSh4Reg(Register Rt, u32 Sh4_Reg)
{
	const int shRegOffs = (u8*)GetRegPtr(Sh4_Reg) - (u8*)&p_sh4rcb->cntx - sizeof(Sh4cntx);

	ass.Str(Rt, MemOperand(r8, shRegOffs));
}

const int alloc_regs[] = { 5, 6, 7, 10, 11, -1 };
const int alloc_fpu[] = { 16, 17, 18, 19, 20, 21, 22, 23,
				24, 25, 26, 27, 28, 29, 30, 31, -1 };

struct arm_reg_alloc: RegAlloc<int, int>
{
	void Preload(u32 reg, int nreg) override
	{
		loadSh4Reg(Register(nreg), reg);
	}
	void Writeback(u32 reg, int nreg) override
	{
		if (reg == reg_pc_dyn)
			// reg_pc_dyn has been stored in r4 by the jdyn op implementation
			// No need to write it back since it won't be used past the end of the block
			; //ass.Mov(r4, Register(nreg));
		else
			storeSh4Reg(Register(nreg), reg);
	}

	void Preload_FPU(u32 reg, int nreg) override
	{
		const s32 shRegOffs = (u8*)GetRegPtr(reg) - (u8*)&p_sh4rcb->cntx - sizeof(Sh4cntx);

		ass.Vldr(SRegister(nreg), MemOperand(r8, shRegOffs));
	}
	void Writeback_FPU(u32 reg, int nreg) override
	{
		const s32 shRegOffs = (u8*)GetRegPtr(reg) - (u8*)&p_sh4rcb->cntx - sizeof(Sh4cntx);

		ass.Vstr(SRegister(nreg), MemOperand(r8, shRegOffs));
	}

	SRegister mapFReg(const shil_param& prm)
	{
		return SRegister(mapf(prm));
	}
	Register mapReg(const shil_param& prm)
	{
		return Register(mapg(prm));
	}
};

static arm_reg_alloc reg;

static const void *no_update;
static const void *intc_sched;
static const void *ngen_blockcheckfail;
static const void *ngen_LinkBlock_Generic_stub;
static const void *ngen_LinkBlock_cond_Branch_stub;
static const void *ngen_LinkBlock_cond_Next_stub;
static void (*ngen_FailedToFindBlock_)();
static void (*mainloop)(void *);
static void (*handleException)();

static void generate_mainloop();

static std::map<shilop, ConditionType> ccmap;
static std::map<shilop, ConditionType> ccnmap;
static bool restarting;
static u32 jmp_stack;

void ngen_mainloop(void* context)
{
	do {
		restarting = false;
		generate_mainloop();

		mainloop(context);
		if (restarting)
			p_sh4rcb->cntx.CpuRunning = 1;
	} while (restarting);
}

static void jump(const void *code, ConditionType cond = al)
{
	ptrdiff_t offset = reinterpret_cast<uintptr_t>(code) - ass.GetBuffer()->GetStartAddress<uintptr_t>();
	verify((offset & 3) == 0);
	if (offset < -32 * 1024 * 1024 || offset >= 32 * 1024 * 1024)
	{
		WARN_LOG(DYNAREC, "jump offset too large: %d", offset);
		UseScratchRegisterScope scope(&ass);
		Register reg = scope.Acquire();
		ass.Mov(cond, reg, (u32)code);
		ass.Bx(cond, reg);
	}
	else
	{
		Label code_label(offset);
		ass.B(cond, &code_label);
	}
}

static void call(const void *code, ConditionType cond = al)
{
	ptrdiff_t offset = reinterpret_cast<uintptr_t>(code) - ass.GetBuffer()->GetStartAddress<uintptr_t>();
	verify((offset & 3) == 0);
	if (offset < -32 * 1024 * 1024 || offset >= 32 * 1024 * 1024)
	{
		WARN_LOG(DYNAREC, "call offset too large: %d", offset);
		UseScratchRegisterScope scope(&ass);
		Register reg = scope.Acquire();
		ass.Mov(cond, reg, (u32)code);
		ass.Blx(cond, reg);
	}
	else
	{
		Label code_label(offset);
		ass.Bl(cond, &code_label);
	}
}

static u32 relinkBlock(DynaRBI *block)
{
	u32 start_offset = ass.GetCursorOffset();
	switch(block->BlockType)
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
					ass.Mov(r4, block->T_reg);
				}
				else
				{
					INFO_LOG(DYNAREC, "SLOW COND PATH %x", block->oplist.empty() ? -1 : block->oplist.back().op);
					loadSh4Reg(r4, reg_sr_T);
				}
			}
			ass.Cmp(r4, block->BlockType & 1);
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
			ass.Nop();
			ass.Nop();
			ass.Nop();
			ass.Nop();
		}
		else
		{
			ass.Mov(Condition(CC).Negate(), r4, block->NextBlock);
			ass.Mov(CC, r4, block->BranchBlock);
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
			ass.Sub(r2, r8, -rcbOffset(fpcb));
			ass.Ubfx(r1, r4, 1, 24);
			ass.Ldr(pc, MemOperand(r2, r1, LSL, 2));
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
			ass.Nop();
			ass.Nop();
			ass.Nop();
		}
		else
		{
			ass.Mov(r4, block->BranchBlock);
			storeSh4Reg(r4, reg_nextpc);
			jump(no_update);
		}

		break;

	case BET_StaticIntr:
	case BET_DynamicIntr:
		if (block->BlockType == BET_StaticIntr)
			ass.Mov(r4, block->NextBlock);
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
	return ass.GetCursorOffset() - start_offset;
}

u32 DynaRBI::Relink()
{
	ass = Arm32Assembler((u8 *)code + relink_offset, host_code_size - relink_offset);

	u32 size = relinkBlock(this);

	ass.Finalize();

	return size;
}

static Register GetParam(const shil_param& param, Register raddr = r0)
{
	if (param.is_imm())
	{
		ass.Mov(raddr, param._imm);
		return raddr;
	}
	if (param.is_r32i())
		return reg.mapReg(param);

	die("Invalid parameter");
	return Register();
}

static void ngen_Binary(shil_opcode* op, BinaryOP dtop)
{
	Register rs1 = GetParam(op->rs1);
	
	Register rs2 = r1;
	if (op->rs2.is_imm())
	{
		if (ImmediateA32::IsImmediateA32(op->rs2._imm))
		{
			(ass.*dtop)(reg.mapReg(op->rd), rs1, Operand(op->rs2._imm));
			return;
		}
		ass.Mov(rs2, op->rs2._imm);
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

	(ass.*dtop)(reg.mapReg(op->rd), rs1, rs2);
}

static void ngen_fp_bin(shil_opcode* op, FPBinOP fpop)
{
	SRegister rs1 = s0;
	if (op->rs1.is_imm())
	{
		ass.Mov(r0, op->rs1._imm);
		ass.Vmov(rs1, r0);
	}
	else
	{
		rs1 = reg.mapFReg(op->rs1);
	}

	SRegister rs2 = s1;
	if (op->rs2.is_imm())
	{
		ass.Mov(r0, op->rs2._imm);
		ass.Vmov(rs2, r0);
	}
	else
	{
		rs2 = reg.mapFReg(op->rs2);
	}

	(ass.*fpop)(DataType(F32), reg.mapFReg(op->rd), rs1, rs2);
}

static void ngen_fp_una(shil_opcode* op, FPUnOP fpop)
{
	(ass.*fpop)(DataType(F32), reg.mapFReg(op->rd), reg.mapFReg(op->rs1));
}

struct CC_PS
{
	CanonicalParamType type;
	shil_param* par;
};
static std::vector<CC_PS> CC_pars;

void ngen_CC_Start(shil_opcode* op) 
{ 
	CC_pars.clear();
}

void ngen_CC_Param(shil_opcode* op,shil_param* par,CanonicalParamType tp) 
{ 
	switch(tp)
	{
		case CPT_f32rv:
#ifdef __ARM_PCS_VFP
			// -mfloat-abi=hard
			if (reg.IsAllocg(*par))
				ass.Vmov(reg.mapReg(*par), s0);
			else if (reg.IsAllocf(*par))
				ass.Vmov(reg.mapFReg(*par), s0);
			break;
#endif

		case CPT_u32rv:
		case CPT_u64rvL:
			if (reg.IsAllocg(*par))
				ass.Mov(reg.mapReg(*par), r0);
			else if (reg.IsAllocf(*par))
				ass.Vmov(reg.mapFReg(*par), r0);
			else
				die("unhandled param");
			break;

		case CPT_u64rvH:
			verify(reg.IsAllocg(*par));
			ass.Mov(reg.mapReg(*par), r1);
			break;

		case CPT_u32:
		case CPT_ptr:
		case CPT_f32:
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

void ngen_CC_Call(shil_opcode* op, void* function)
{
	Register rd = r0;
	SRegister fd = s0;

	for (int i = CC_pars.size(); i-- > 0; )
	{
		CC_PS& param = CC_pars[i];
		if (param.type == CPT_ptr)
		{
			ass.Mov(rd, (u32)param.par->reg_ptr());
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
						ass.Vmov(fd, reg.mapReg(*param.par));
					else if (reg.IsAllocf(*param.par))
						ass.Vmov(fd, reg.mapFReg(*param.par));
					else
						die("Must not happen!");
					continue;
				}
#endif

				if (reg.IsAllocg(*param.par))
					ass.Mov(rd, reg.mapReg(*param.par));
				else if (reg.IsAllocf(*param.par))
					ass.Vmov(rd, reg.mapFReg(*param.par));
				else
					die("Must not happen!");
			}
			else
			{
				verify(param.par->is_imm());
				ass.Mov(rd, param.par->_imm);
			}
		}
		rd = Register(rd.GetCode() + 1);
		fd = SRegister(fd.GetCode() + 1);
	}
	call(function);
}

void ngen_CC_Finish(shil_opcode* op) 
{ 
	CC_pars.clear(); 
}

enum mem_op_type
{
	SZ_8,
	SZ_16,
	SZ_32I,
	SZ_32F,
	SZ_64F,
};

static mem_op_type memop_type(shil_opcode* op)
{
	int sz = op->flags & 0x7f;
	bool fp32 = op->rs2.is_r32f() || op->rd.is_r32f();

	if (sz == 1)
		return SZ_8;
	else if (sz == 2)
		return SZ_16;
	else if (sz == 4)
		return fp32 ? SZ_32F : SZ_32I;
	else if (sz == 8)
		return SZ_64F;

	die("Unknown op");
	return SZ_32I;
}

const u32 memop_bytes[] = { 1, 2, 4, 4, 8 };
static const void *_mem_hndl_SQ32[3][14];
static const void *_mem_hndl[2][3][14];
const void * const _mem_func[2][2] =
{
	{ (void *)_vmem_WriteMem32, (void *)_vmem_WriteMem64 },
	{ (void *)_vmem_ReadMem32, (void *)_vmem_ReadMem64 },
};

const struct
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

static void vmem_slowpath(Register raddr, Register rt, SRegister ft, DRegister fd, mem_op_type optp, bool read)
{
	if (!raddr.Is(r0))
		ass.Mov(r0, raddr);

	if (!read)
	{
		if (optp <= SZ_32I)
			ass.Mov(r1, rt);
		else if (optp == SZ_32F)
			ass.Vmov(r1, ft);
		else if (optp == SZ_64F)
			ass.Vmov(r2, r3, fd);
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
			ass.Mov(rt, r0);
		else if (optp == SZ_32F)
			ass.Vmov(ft, r0);
		else if (optp == SZ_64F)
			ass.Vmov(fd, r0, r1);
	}
}

bool ngen_Rewrite(host_context_t &context, void *faultAddress)
{
	u32 *regs = context.reg;
	arm_mem_op *ptr = (arm_mem_op *)context.pc;

	mem_op_type optp;
	u32 read;
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

	ass = Arm32Assembler((u8 *)ptr, 12);

	//fault offset must always be the addr from ubfx (sanity check)
	verify(fault_offs == 0 || fault_offs == (sh4_addr & 0x1FFFFFFF));

	if (is_sq && !read && optp >= SZ_32I)
	{
		if (optp >= SZ_32F)
		{
			if (!raddr.Is(r0))
				ass.Mov(r0, raddr);
			else
				ass.Nop();
			raddr = r0;
		}
		switch (optp)
		{
		case SZ_32I:
			ass.Mov(r1, rt);
			break;
		case SZ_32F:
			ass.Vmov(r1, ft);
			break;
		case SZ_64F:
			ass.Vmov(r2, r3, fd);
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
				ass.Mov(r0, raddr);
			else
				ass.Nop();
		}

		if (!read)
		{
			if (optp <= SZ_32I)
				ass.Mov(r1, rt);
			else if (optp == SZ_32F)
				ass.Vmov(r1, ft);
			else if (optp == SZ_64F)
				ass.Vmov(r2, r3, fd);
		}

		const void *funct = nullptr;

		if (offs == 1)
			funct = _mem_hndl[read][optp][raddr.GetCode()];
		else if (optp >= SZ_32F)
			funct = _mem_func[read][optp - SZ_32F];

		verify(funct != nullptr);
		call(funct);

		if (read)
		{
			if (optp <= SZ_32I)
				ass.Mov(rt, r0);
			else if (optp == SZ_32F)
				ass.Vmov(ft, r0);
			else if (optp == SZ_64F)
				ass.Vmov(fd, r0, r1);
		}
	}

	ass.Finalize();
	context.pc = (size_t)ptr;

	return true;
}

static Register GenMemAddr(shil_opcode* op, Register raddr = r0)
{
	if (op->rs3.is_imm())
	{
		if (ImmediateA32::IsImmediateA32(op->rs3._imm))
		{
			ass.Add(raddr, reg.mapReg(op->rs1), op->rs3._imm);
		}
		else 
		{
			ass.Mov(r1, op->rs3._imm);
			ass.Add(raddr, reg.mapReg(op->rs1), r1);
		}
	}
	else if (op->rs3.is_r32i())
	{
		ass.Add(raddr, reg.mapReg(op->rs1), reg.mapReg(op->rs3));
	}
	else if (!op->rs3.is_null())
	{
		ERROR_LOG(DYNAREC, "rs3: %08X", op->rs3.type);
		die("invalid rs3");
	}
	else if (op->rs1.is_imm())
	{
		ass.Mov(raddr, op->rs1._imm);
	}
	else
	{
		raddr = reg.mapReg(op->rs1);
	}

	return raddr;
}

static bool ngen_readm_immediate(RuntimeBlockInfo* block, shil_opcode* op, bool optimise)
{
	if (!op->rs1.is_imm())
		return false;

	u32 size = op->flags & 0x7f;
	u32 addr = op->rs1._imm;
	if (mmu_enabled() && mmu_is_translated(addr, size))
	{
		if ((addr >> 12) != (block->vaddr >> 12) && ((addr >> 12) != ((block->vaddr + block->guest_opcodes * 2 - 1) >> 12)))
			// When full mmu is on, only consider addresses in the same 4k page
			return false;
		u32 paddr;
		u32 rv;
		switch (size)
		{
		case 1:
			rv = mmu_data_translation<MMU_TT_DREAD, u8>(addr, paddr);
			break;
		case 2:
			rv = mmu_data_translation<MMU_TT_DREAD, u16>(addr, paddr);
			break;
		case 4:
		case 8:
			rv = mmu_data_translation<MMU_TT_DREAD, u32>(addr, paddr);
			break;
		default:
			rv = 0;
			die("Invalid immediate size");
			break;
		}
		if (rv != MMU_ERROR_NONE)
			return false;
		addr = paddr;
	}

	mem_op_type optp = memop_type(op);
	bool isram = false;
	void* ptr = _vmem_read_const(addr, isram, std::min(4u, memop_bytes[optp]));

	Register rd = (optp != SZ_32F && optp != SZ_64F) ? reg.mapReg(op->rd) : r0;

	if (isram)
	{
		ass.Mov(r0, (u32)ptr);
		switch(optp)
		{
		case SZ_8:
			ass.Ldrsb(rd, MemOperand(r0));
			break;

		case SZ_16:
			ass.Ldrsh(rd, MemOperand(r0));
			break;

		case SZ_32I:
			ass.Ldr(rd, MemOperand(r0));
			break;

		case SZ_32F:
			ass.Vldr(reg.mapFReg(op->rd), MemOperand(r0));
			break;

		case SZ_64F:
			ass.Vldr(d0, MemOperand(r0));
			ass.Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
			break;
		}
	}
	else
	{
		// Not RAM
		if (optp == SZ_64F)
		{
			verify(!reg.IsAllocAny(op->rd));
			// Need to call the handler twice
			ass.Mov(r0, op->rs1._imm);
			call(ptr);
			ass.Str(r0, MemOperand(r8, op->rd.reg_nofs()));

			ass.Mov(r0, op->rs1._imm + 4);
			call(ptr);
			ass.Str(r0, MemOperand(r8, op->rd.reg_nofs() + 4));
		}
		else
		{
			ass.Mov(r0, op->rs1._imm);
			call(ptr);

			switch(optp)
			{
			case SZ_8:
				ass.Sxtb(r0, r0);
				break;

			case SZ_16:
				ass.Sxth(r0, r0);
				break;

			case SZ_32I:
			case SZ_32F:
				break;

			default:
				die("Invalid size");
				break;
			}

			if (reg.IsAllocg(op->rd))
				ass.Mov(rd, r0);
			else if (reg.IsAllocf(op->rd))
				ass.Vmov(reg.mapFReg(op->rd), r0);
			else
				die("Unsupported");
		}
	}

	return true;
}

static bool ngen_writemem_immediate(RuntimeBlockInfo* block, shil_opcode* op, bool optimise)
{
	if (!op->rs1.is_imm())
		return false;

	u32 size = op->flags & 0x7f;
	u32 addr = op->rs1._imm;
	if (mmu_enabled() && mmu_is_translated(addr, size))
	{
		if ((addr >> 12) != (block->vaddr >> 12) && ((addr >> 12) != ((block->vaddr + block->guest_opcodes * 2 - 1) >> 12)))
			// When full mmu is on, only consider addresses in the same 4k page
			return false;
		u32 paddr;
		u32 rv;
		switch (size)
		{
		case 1:
			rv = mmu_data_translation<MMU_TT_DWRITE, u8>(addr, paddr);
			break;
		case 2:
			rv = mmu_data_translation<MMU_TT_DWRITE, u16>(addr, paddr);
			break;
		case 4:
		case 8:
			rv = mmu_data_translation<MMU_TT_DWRITE, u32>(addr, paddr);
			break;
		default:
			rv = 0;
			die("Invalid immediate size");
			break;
		}
		if (rv != MMU_ERROR_NONE)
			return false;
		addr = paddr;
	}

	mem_op_type optp = memop_type(op);
	bool isram = false;
	void* ptr = _vmem_write_const(addr, isram, std::min(4u, memop_bytes[optp]));

	Register rs2 = r1;
	SRegister rs2f = s0;
	if (op->rs2.is_imm())
		ass.Mov(rs2, op->rs2._imm);
	else if (optp == SZ_32F)
		rs2f = reg.mapFReg(op->rs2);
	else if (optp != SZ_64F)
		rs2 = reg.mapReg(op->rs2);

	if (isram)
	{
		ass.Mov(r0, (u32)ptr);
		switch(optp)
		{
		case SZ_8:
			ass.Strb(rs2, MemOperand(r0));
			break;

		case SZ_16:
			ass.Strh(rs2, MemOperand(r0));
			break;

		case SZ_32I:
			ass.Str(rs2, MemOperand(r0));
			break;

		case SZ_32F:
			ass.Vstr(rs2f, MemOperand(r0));
			break;

		case SZ_64F:
			ass.Vldr(d0, MemOperand(r8, op->rs2.reg_nofs()));
			ass.Vstr(d0, MemOperand(r0));
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
		ass.Mov(r0, op->rs1._imm);
		if (optp == SZ_32F)
			ass.Vmov(r1, rs2f);
		else if (!rs2.Is(r1))
			ass.Mov(r1, rs2);

		call(ptr);
	}
	return true;
}

static void genMmuLookup(RuntimeBlockInfo* block, const shil_opcode& op, u32 write, Register& raddr)
{
	if (mmu_enabled())
	{
		Label inCache;
		Label done;

		ass.Lsr(r1, raddr, 12);
		ass.Ldr(r1, MemOperand(r9, r1, LSL, 2));
		ass.Cmp(r1, 0);
		ass.B(ne, &inCache);
		if (!raddr.Is(r0))
			ass.Mov(r0, raddr);
		ass.Mov(r1, write);
		ass.Mov(r2, block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0));	// pc
		call((void *)mmuDynarecLookup);
		ass.B(&done);
		ass.Bind(&inCache);
		ass.And(r0, raddr, 0xFFF);
		ass.Orr(r0, r0, r1);
		ass.Bind(&done);
		raddr = r0;
	}
}

static void interpreter_fallback(u16 op, OpCallFP *oph, u32 pc)
{
	try {
		oph(op);
	} catch (SH4ThrownException& ex) {
		if (pc & 1)
		{
			// Delay slot
			AdjustDelaySlotException(ex);
			pc--;
		}
		Do_Exception(pc, ex.expEvn, ex.callVect);
		handleException();
	}
}

static void do_sqw_mmu_no_ex(u32 addr, u32 pc)
{
	try {
		do_sqw_mmu(addr);
	} catch (SH4ThrownException& ex) {
		if (pc & 1)
		{
			// Delay slot
			AdjustDelaySlotException(ex);
			pc--;
		}
		Do_Exception(pc, ex.expEvn, ex.callVect);
		handleException();
	}
}

static void ngen_compile_opcode(RuntimeBlockInfo* block, shil_opcode* op, bool optimise)
{
	switch(op->op)
	{
		case shop_readm:
			if (!ngen_readm_immediate(block, op, optimise))
			{
				mem_op_type optp = memop_type(op);
				Register raddr = GenMemAddr(op);
				genMmuLookup(block, *op, 0, raddr);

				if (_nvmem_enabled()) {
					ass.Bic(r1, raddr, 0xE0000000);

					switch(optp)
					{
					case SZ_8:	
						ass.Ldrsb(reg.mapReg(op->rd), MemOperand(r1, r8));
						break;

					case SZ_16: 
						ass.Ldrsh(reg.mapReg(op->rd), MemOperand(r1, r8));
						break;

					case SZ_32I: 
						ass.Ldr(reg.mapReg(op->rd), MemOperand(r1, r8));
						break;

					case SZ_32F:
						ass.Add(r1, r1, r8);	//3 opcodes, there's no [REG+REG] VLDR
						ass.Vldr(reg.mapFReg(op->rd), MemOperand(r1));
						break;

					case SZ_64F:
						ass.Add(r1, r1, r8);	//3 opcodes, there's no [REG+REG] VLDR
						ass.Vldr(d0, MemOperand(r1));	//TODO: use reg alloc

						ass.Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
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
						ass.Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
						break;
					}
				}
			}
			break;

		case shop_writem:
			if (!ngen_writemem_immediate(block, op, optimise))
			{
				mem_op_type optp = memop_type(op);

				Register raddr = GenMemAddr(op);
				genMmuLookup(block, *op, 1,raddr);

				Register rs2 = r2;
				SRegister rs2f = s2;

				//TODO: use reg alloc
				if (optp == SZ_64F)
					ass.Vldr(d0, MemOperand(r8, op->rs2.reg_nofs()));
				else if (op->rs2.is_imm())
				{
					ass.Mov(rs2, op->rs2._imm);
					if (optp == SZ_32F)
						ass.Vmov(rs2f, rs2);
				}
				else
				{
					if (optp == SZ_32F)
						rs2f = reg.mapFReg(op->rs2);
					else
						rs2 = reg.mapReg(op->rs2);
				}
				if (_nvmem_enabled())
				{
					ass.Bic(r1, raddr, 0xE0000000);

					switch(optp)
					{
					case SZ_8:
						ass.Strb(rs2, MemOperand(r1, r8));
						break;

					case SZ_16:
						ass.Strh(rs2, MemOperand(r1, r8));
						break;

					case SZ_32I:
						ass.Str(rs2, MemOperand(r1, r8));
						break;

					case SZ_32F:
						ass.Add(r1, r1, r8);	//3 opcodes: there's no [REG+REG] VLDR, also required for SQ
						ass.Vstr(rs2f, MemOperand(r1));
						break;

					case SZ_64F:
						ass.Add(r1, r1, r8);	//3 opcodes: there's no [REG+REG] VLDR, also required for SQ
						ass.Vstr(d0, MemOperand(r1));	//TODO: use reg alloc
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
				ass.Mov(r2, op->rs2.imm_value());
				ass.Add(r4, reg.mapReg(op->rs1), r2);
			}
			else
			{
				ass.Mov(r4, reg.mapReg(op->rs1));
			}
			break;

		case shop_mov32:
			verify(op->rd.is_r32());

			if (op->rs1.is_imm())
			{
				if (op->rd.is_r32i())
				{
					ass.Mov(reg.mapReg(op->rd), op->rs1._imm);
				}
				else
				{
					if (op->rs1._imm==0)
					{
						//VEOR(reg.mapFReg(op->rd),reg.mapFReg(op->rd),reg.mapFReg(op->rd));
						//hum, vmov can't do 0, but can do all kind of weird small consts ... really useful ...
						//simd is slow on a9
#if 0
						ass.Movw(r0, 0);
						ass.Vmov(reg.mapFReg(op->rd), r0);
#else
						//1-1=0 !
						//should be slightly faster ...
						//we could get rid of the imm mov, if not for infs & co ..
						ass.Vmov(reg.mapFReg(op->rd), 1.f);;
						ass.Vsub(reg.mapFReg(op->rd), reg.mapFReg(op->rd), reg.mapFReg(op->rd));
#endif
					}
					else if (op->rs1._imm == 0x3F800000)
						ass.Vmov(reg.mapFReg(op->rd), 1.f);
					else
					{
						ass.Mov(r0, op->rs1._imm);
						ass.Vmov(reg.mapFReg(op->rd), r0);
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
					ass.Mov(reg.mapReg(op->rd), reg.mapReg(op->rs1));
					break;

				case 1: // vfp = reg
					ass.Vmov(reg.mapFReg(op->rd), reg.mapReg(op->rs1));
					break;

				case 2: // reg = vfp
					ass.Vmov(reg.mapReg(op->rd), reg.mapFReg(op->rs1));
					break;

				case 3: // vfp = vfp
					ass.Vmov(reg.mapFReg(op->rd), reg.mapFReg(op->rs1));
					break;
				}
			}
			else
			{
				die("Invalid mov32 size");
			}
			break;
			
		case shop_mov64:
			verify(op->rs1.is_r64() && op->rd.is_r64());
			ass.Vldr(d0, MemOperand(r8, op->rs1.reg_nofs()));
			ass.Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
			break;

		case shop_jcond:
			verify(op->rd.is_reg() && op->rd._reg == reg_pc_dyn);
			ass.Mov(r4, reg.mapReg(op->rs1));
			break;

		case shop_ifb:
			if (op->rs1._imm) 
			{
				ass.Mov(r1, op->rs2._imm);
				storeSh4Reg(r1, reg_nextpc);
			}

			ass.Mov(r0, op->rs3._imm);
			if (!mmu_enabled())
			{
				call((void *)OpPtr[op->rs3._imm]);
			}
			else
			{
				ass.Mov(r1, reinterpret_cast<uintptr_t>(*OpDesc[op->rs3._imm]->oph));	// op handler
				ass.Mov(r2, block->vaddr + op->guest_offs - (op->delay_slot ? 1 : 0));	// pc
				call((void *)interpreter_fallback);
			}
			break;

#ifndef CANONICALTEST
		case shop_neg:
			ass.Rsb(reg.mapReg(op->rd), reg.mapReg(op->rs1), 0);
			break;
		case shop_not:
			ass.Mvn(reg.mapReg(op->rd), reg.mapReg(op->rs1));
			break;

		case shop_shl:
			ngen_Binary(op, &MacroAssembler::Lsl);
			break;
		case shop_shr:
			ngen_Binary(op, &MacroAssembler::Lsr);
			break;
		case shop_sar:
			ngen_Binary(op, &MacroAssembler::Asr);
			break;

		case shop_and:
			ngen_Binary(op, &MacroAssembler::And);
			break;
		case shop_or:
			ngen_Binary(op, &MacroAssembler::Orr);
			break;
		case shop_xor:
			ngen_Binary(op, &MacroAssembler::Eor);
			break;

		case shop_add:
			ngen_Binary(op, &MacroAssembler::Add);
			break;
		case shop_sub:
			ngen_Binary(op, &MacroAssembler::Sub);
			break;
		case shop_ror:
			ngen_Binary(op, &MacroAssembler::Ror);
			break;

		case shop_adc:
			{
				Register rs1 = GetParam(op->rs1, r1);
				Register rs2 = GetParam(op->rs2, r2);
				Register rs3 = GetParam(op->rs3, r3);

				ass.Lsr(SetFlags, r0, rs3, 1); 					 //C=rs3, r0=0
				ass.Adc(SetFlags, reg.mapReg(op->rd), rs1, rs2); //(C,rd)=rs1+rs2+rs3(C)
				ass.Adc(reg.mapReg(op->rd2), r0, 0);			 //rd2=C, (or MOVCS rd2, 1)
			}
			break;

		case shop_rocr:
			{
				Register rd2 = reg.mapReg(op->rd2);
				Register rs1 = GetParam(op->rs1, r1);
				Register rs2 = GetParam(op->rs2, r2);
				if (!rd2.Is(rs1)) {
					ass.Lsr(SetFlags, rd2, rs2, 1);	//C=rs2, rd2=0
					ass.And(rd2, rs1, 1);     		//get new carry
				} else {
					ass.Lsr(SetFlags, r0, rs2, 1);	//C=rs2, rd2=0
					ass.Add(r0, rs1, 1);
				}
				ass.Rrx(reg.mapReg(op->rd), rs1);	//RRX w/ carry :)
				if (rd2.Is(rs1))
					ass.Mov(rd2, r0);
			}
			break;
			
		case shop_rocl:
			{
				Register rs1 = GetParam(op->rs1, r1);
				Register rs2 = GetParam(op->rs2, r2);
				ass.Orr(SetFlags, reg.mapReg(op->rd), rs2, Operand(rs1, LSL, 1)); //(C,rd)= rs1<<1 + (|) rs2
				ass.Mov(reg.mapReg(op->rd2), 0);								  //clear rd2 (for ADC/MOVCS)
				ass.Adc(reg.mapReg(op->rd2), reg.mapReg(op->rd2), 0);			  //rd2=C (or MOVCS rd2, 1)
			}
			break;
			
		case shop_sbc:
			{
				Register rd2 = reg.mapReg(op->rd2);
				Register rs1 = GetParam(op->rs1, r1);
				if (rs1.Is(rd2))
				{
					ass.Mov(r1, rs1);
					rs1 = r1;
				}
				Register rs2 = GetParam(op->rs2, r2);
				if (rs2.Is(rd2))
				{
					ass.Mov(r2, rs2);
					rs2 = r2;
				}
				Register rs3 = GetParam(op->rs3, r3);
				ass.Eor(rd2, rs3, 1);
				ass.Lsr(SetFlags, rd2, rd2, 1); //C=rs3, rd2=0
				ass.Sbc(SetFlags, reg.mapReg(op->rd), rs1, rs2);
				ass.Mov(cc, rd2, 1);
			}
			break;
		
		case shop_negc:
			{
				Register rd2 = reg.mapReg(op->rd2);
				Register rs1 = GetParam(op->rs1, r1);
				if (rs1.Is(rd2))
				{
					ass.Mov(r1, rs1);
					rs1 = r1;
				}
				Register rs2 = GetParam(op->rs2, r2);
				ass.Eor(rd2, rs2, 1);
				ass.Lsr(SetFlags, rd2, rd2, 1);						//C=rs3, rd2=0
				ass.Sbc(SetFlags, reg.mapReg(op->rd), rd2, rs1);	// rd2 == 0
				ass.Mov(cc, rd2, 1);
			}
			break;

		case shop_shld:
			{
				verify(!op->rs2.is_imm());
				ass.And(SetFlags, r0, reg.mapReg(op->rs2), 0x8000001F);
				ass.Rsb(mi, r0, r0, 0x80000020);
				Register rs1 = GetParam(op->rs1, r1);
				ass.Lsr(mi, reg.mapReg(op->rd), rs1, r0);
				ass.Lsl(pl, reg.mapReg(op->rd), rs1, r0);
			}		
			break;

		case shop_shad:
			{
				verify(!op->rs2.is_imm());
				ass.And(SetFlags, r0, reg.mapReg(op->rs2), 0x8000001F);
				ass.Rsb(mi, r0, r0, 0x80000020);
				Register rs1 = GetParam(op->rs1, r1);
				ass.Asr(mi, reg.mapReg(op->rd), rs1, r0);
				ass.Lsl(pl, reg.mapReg(op->rd), rs1, r0);
			}		
			break;

		case shop_sync_sr:
			//must flush: SRS, SRT, r0-r7, r0b-r7b
			call((void *)UpdateSR);
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
						ass.Mov(rs2, (u32)op->rs2._imm);
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
						ass.Tst(rs1, op->rs2._imm);
					else
						ass.Tst(rs1, rs2);
				}
				else
				{
					if (is_imm)
						ass.Cmp(rs1, op->rs2._imm);
					else
						ass.Cmp(rs1, rs2);
				}

				static const ConditionType opcls2[] = { eq, eq, ge, gt, hs, hi };

				ass.Mov(rd, 0);
				ass.Mov(opcls2[op->op-shop_test], rd, 1);
			}
			break;

		case shop_setpeq:
			{
				Register rs1 = GetParam(op->rs1, r1);
				Register rs2 = GetParam(op->rs2, r2);
				ass.Eor(r1, rs1, rs2);
				ass.Mov(reg.mapReg(op->rd), 0);
				
				ass.Tst(r1, 0xFF000000u);
				ass.Tst(ne, r1, 0x00FF0000u);
				ass.Tst(ne, r1, 0x0000FF00u);
				ass.Tst(ne, r1, 0x000000FFu);
				ass.Mov(eq, reg.mapReg(op->rd), 1);
			}
			break;
		
		//UXTH for zero extention and/or more mul forms (for 16 and 64 bits)

		case shop_mul_u16:
			{
				Register rs2 = GetParam(op->rs2, r2);
				ass.Uxth(r1, reg.mapReg(op->rs1));
				ass.Uxth(r2, rs2);
				ass.Mul(reg.mapReg(op->rd), r1, r2);
			}
			break;
		case shop_mul_s16:
			{
				Register rs2 = GetParam(op->rs2, r2);
				ass.Sxth(r1, reg.mapReg(op->rs1));
				ass.Sxth(r2, rs2);
				ass.Mul(reg.mapReg(op->rd), r1, r2);
			}
			break;
		case shop_mul_i32:
			{
				Register rs2 = GetParam(op->rs2, r2);
				//x86_opcode_class opdt[]={op_movzx16to32,op_movsx16to32,op_mov32,op_mov32,op_mov32};
				//x86_opcode_class opmt[]={op_mul32,op_mul32,op_mul32,op_mul32,op_imul32};
				//only the top 32 bits are different on signed vs unsigned

				ass.Mul(reg.mapReg(op->rd), reg.mapReg(op->rs1), rs2);
			}
			break;
		case shop_mul_u64:
			{
				Register rs2 = GetParam(op->rs2, r2);
				ass.Umull(reg.mapReg(op->rd), reg.mapReg(op->rd2), reg.mapReg(op->rs1), rs2);
			}
			break;
		case shop_mul_s64:
			{
				Register rs2 = GetParam(op->rs2, r2);
				ass.Smull(reg.mapReg(op->rd), reg.mapReg(op->rd2), reg.mapReg(op->rs1), rs2);
			}
			break;
	
		case shop_pref:
			{
				ConditionType cc = eq;
				if (!op->rs1.is_imm())
				{
					ass.Lsr(r1, reg.mapReg(op->rs1), 26);
					ass.Mov(r0, reg.mapReg(op->rs1));
					ass.Cmp(r1, 0x38);
				}
				else
				{
					// The SSA pass has already checked that the
					// destination is a store queue so no need to check
					ass.Mov(r0, op->rs1.imm_value());
					cc = al;
				}

				if (mmu_enabled())
				{
					ass.Mov(r1, block->vaddr + op->guest_offs - (op->delay_slot ? 1 : 0));	// pc
					call((void *)do_sqw_mmu_no_ex, cc);
				}
				else
				{
					if (CCN_MMUCR.AT)
					{
						call((void *)do_sqw_mmu, cc);
					}
					else
					{
						ass.Ldr(r2, MemOperand(r8, rcbOffset(do_sqw_nommu)));
						ass.Sub(r1, r8, -rcbOffset(sq_buffer));
						ass.Blx(cc, r2);
					}
				}
			}
			break;

		case shop_ext_s8:
		case shop_ext_s16:
			if (op->op == shop_ext_s8)
				ass.Sxtb(reg.mapReg(op->rd), reg.mapReg(op->rs1));
			else
				ass.Sxth(reg.mapReg(op->rd), reg.mapReg(op->rs1));
			break;
			
		case shop_xtrct:
			{
				Register rd = reg.mapReg(op->rd);
				Register rs1 = reg.mapReg(op->rs1);
				Register rs2 = reg.mapReg(op->rs2);
				if (rd.Is(rs1))
				{
					verify(!rd.Is(rs2));
					ass.Lsr(rd, rs1, 16);
					ass.Lsl(r0, rs2, 16);
				}
				else
				{
					ass.Lsl(rd, rs2, 16);
					ass.Lsr(r0, rs1, 16);
				}
				ass.Orr(rd, rd, r0);
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
				ngen_fp_bin(op, opcds[op->op - shop_fadd]);
			}
			break;

		case shop_fabs:
		case shop_fneg:
			{
				static const FPUnOP opcds[] = { &MacroAssembler::Vabs, &MacroAssembler::Vneg };
				ngen_fp_una(op, opcds[op->op - shop_fabs]);
			}
			break;

		case shop_fsqrt:
			ngen_fp_una(op, &MacroAssembler::Vsqrt);
			break;

		
		case shop_fmac:
			{
				SRegister rd = reg.mapFReg(op->rd);
				SRegister rs1 = s1;
				if (op->rs1.is_imm())
				{
					ass.Mov(r0, op->rs1.imm_value());
					ass.Vmov(rs1, r0);
				}
				else
					rs1 = reg.mapFReg(op->rs1);
				SRegister rs2 = s2;
				if (op->rs2.is_imm())
				{
					ass.Mov(r1, op->rs2.imm_value());
					ass.Vmov(rs2, r1);
				}
				else
				{
					rs2 = reg.mapFReg(op->rs2);
					if (rs2.Is(rd))
					{
						ass.Vmov(s2, rs2);
						rs2 = s2;
					}
				}
				SRegister rs3 = s3;
				if (op->rs3.is_imm())
				{
					ass.Mov(r2, op->rs3.imm_value());
					ass.Vmov(rs3, r2);
				}
				else
				{
					rs3 = reg.mapFReg(op->rs3);
					if (rs3.Is(rd))
					{
						ass.Vmov(s3, rs3);
						rs3 = s3;
					}
				}
				if (!rd.Is(rs1))
					ass.Vmov(rd, rs1);
				ass.Vmla(rd, rs2, rs3);
			}
			break;

		case shop_fsrra:
			ass.Vmov(s1, 1.f);
			ass.Vsqrt(s0, reg.mapFReg(op->rs1));
			ass.Vdiv(reg.mapFReg(op->rd), s1, s0);
			break;

		case shop_fsetgt:
		case shop_fseteq:
#if 1
			//this is apparently much faster (tested on A9)
			ass.Mov(reg.mapReg(op->rd), 0);
			ass.Vcmp(reg.mapFReg(op->rs1), reg.mapFReg(op->rs2));

			ass.Vmrs(RegisterOrAPSR_nzcv(APSR_nzcv), FPSCR);
			if (op->op == shop_fsetgt)
				ass.Mov(gt, reg.mapReg(op->rd), 1);
			else
				ass.Mov(eq, reg.mapReg(op->rd), 1);
#else
			if (op->op == shop_fsetgt)
				ass.Vcgt(d0, reg.mapFReg(op->rs1), reg.mapFReg(op->rs2));
			else
				ass.Vceq(d0, reg.mapFReg(op->rs1), reg.mapFReg(op->rs2));

			ass.Vmov(r0, s0);
			ass.And(reg.mapReg(op->rd), r0, 1);
#endif
			break;
			

		case shop_fsca:
			//r1: base ptr
			ass.Mov(r1, (u32)sin_table & 0xFFFF);
			ass.Uxth(r0, reg.mapReg(op->rs1));
			ass.Movt(r1, (u32)sin_table >> 16);

			ass.Add(r0, r1, Operand(r0, LSL, 3));

			ass.Vldr(d0, MemOperand(r0));
			ass.Vstr(d0, MemOperand(r8, op->rd.reg_nofs()));
			break;

		case shop_fipr:
			{
				
				QRegister _r1 = q0;
				QRegister _r2 = q0;

				ass.Sub(r0, r8, op->rs1.reg_aofs());
				if (op->rs2.reg_aofs() == op->rs1.reg_aofs())
				{
					ass.Vldm(r0, NO_WRITE_BACK, DRegisterList(d0, 2));
				}
				else
				{
					ass.Sub(r1, r8, op->rs2.reg_aofs());
					ass.Vldm(r0, NO_WRITE_BACK, DRegisterList(d0, 2));
					ass.Vldm(r1, NO_WRITE_BACK, DRegisterList(d2, 2));
					_r2 = q1;
				}

#if 1
				//VFP
				SRegister fs2 = _r2.Is(q0) ? s0 : s4;

				ass.Vmul(reg.mapFReg(op->rd), s0, fs2);
				ass.Vmla(reg.mapFReg(op->rd), s1, SRegister(fs2.GetCode() + 1));
				ass.Vmla(reg.mapFReg(op->rd), s2, SRegister(fs2.GetCode() + 2));
				ass.Vmla(reg.mapFReg(op->rd), s3, SRegister(fs2.GetCode() + 3));
#else			
				ass.Vmul(q0, _r1, _r2);
				ass.Vpadd(d0, d0, d1);
				ass.Vadd(reg.mapFReg(op->rd), f0, f1);
#endif
			}
			break;

		case shop_ftrv:
			{
				Register rdp = r1;
				ass.Sub(r2, r8, op->rs2.reg_aofs());
				ass.Sub(r1, r8, op->rs1.reg_aofs());
				if (op->rs1.reg_aofs() != op->rd.reg_aofs())
				{
					rdp = r0;
					ass.Sub(r0, r8, op->rd.reg_aofs());
				}
	
#if 1
				//f0,f1,f2,f3	  : vin
				//f4,f5,f6,f7     : out
				//f8,f9,f10,f11   : mtx temp
				//f12,f13,f14,f15 : mtx temp
				//(This is actually faster than using neon)

				ass.Vldm(r2, WRITE_BACK, DRegisterList(d4, 2));
				ass.Vldm(r1, NO_WRITE_BACK, DRegisterList(d0, 2));

				ass.Vmul(s4, vixl::aarch32::s8, s0);
				ass.Vmul(s5, s9, s0);
				ass.Vmul(s6, s10, s0);
				ass.Vmul(s7, s11, s0);
				
				ass.Vldm(r2, WRITE_BACK, DRegisterList(d6, 2));

				ass.Vmla(s4, s12, s1);
				ass.Vmla(s5, s13, s1);
				ass.Vmla(s6, s14, s1);
				ass.Vmla(s7, s15, s1);

				ass.Vldm(r2, WRITE_BACK, DRegisterList(d4, 2));

				ass.Vmla(s4, vixl::aarch32::s8, s2);
				ass.Vmla(s5, s9, s2);
				ass.Vmla(s6, s10, s2);
				ass.Vmla(s7, s11, s2);

				ass.Vldm(r2, NO_WRITE_BACK, DRegisterList(d6, 2));

				ass.Vmla(s4, s12, s3);
				ass.Vmla(s5, s13, s3);
				ass.Vmla(s6, s14, s3);
				ass.Vmla(s7, s15, s3);

				ass.Vstm(rdp, NO_WRITE_BACK, DRegisterList(d2, 2));
#else
				//this fits really nicely to NEON !
				// TODO
				ass.Vldm(d16,r2,8);
				ass.Vldm(d0,r1,2);

				ass.Vmla(q2,q8,d0,0);
				ass.Vmla(q2,q9,d0,1);
				ass.Vmla(q2,q10,d1,0);
				ass.Vmla(q2,q11,d1,1);
				ass.Vstm(d4,rdp,2);
#endif
			}
			break;

		case shop_frswap:
			ass.Sub(r0, r8, op->rs1.reg_aofs());
			ass.Sub(r1, r8, op->rd.reg_aofs());
			//Assumes no FPU reg alloc here
			//frswap touches all FPU regs, so all spans should be clear here ..
			ass.Vldm(r1, NO_WRITE_BACK, DRegisterList(d0, 8));
			ass.Vldm(r0, NO_WRITE_BACK, DRegisterList(d8, 8));
			ass.Vstm(r0, NO_WRITE_BACK, DRegisterList(d0, 8));
			ass.Vstm(r1, NO_WRITE_BACK, DRegisterList(d8, 8));
			break;

		case shop_cvt_f2i_t:
			ass.Vcvt(S32, F32, s0, reg.mapFReg(op->rs1));
			ass.Vmov(reg.mapReg(op->rd), s0);
			break;

		case shop_cvt_i2f_n:	// may be some difference should be made ?
		case shop_cvt_i2f_z:
			ass.Vmov(s0, reg.mapReg(op->rs1));
			ass.Vcvt(F32, S32, reg.mapFReg(op->rd), s0);
			break;
#endif

		default:
			shil_chf[op->op](op);
			break;
	}
}


void ngen_Compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
{
	ass = Arm32Assembler((u8 *)emit_GetCCPtr(), emit_FreeSpace());

	block->code = (DynarecCodeEntryPtr)emit_GetCCPtr();

	//reg alloc
	reg.DoAlloc(block, alloc_regs, alloc_fpu);

	u8* blk_start = ass.GetCursorAddress<u8 *>();

	//pre-load the first reg alloc operations, for better efficiency ..
	if (!block->oplist.empty())
		reg.OpBegin(&block->oplist[0], 0);

	// block checks
	if (force_checks || mmu_enabled())
	{
		u32 addr = block->addr;
		ass.Mov(r0, addr);
		if (mmu_enabled())
		{
			loadSh4Reg(r2, reg_nextpc);
			ass.Mov(r1, block->vaddr);
			ass.Cmp(r2, r1);
			jump(ngen_blockcheckfail, ne);
		}

		if (force_checks)
		{
			s32 sz = block->sh4_code_size;
			while (sz > 0)
			{
				if (sz > 2)
				{
					u32* ptr = (u32*)GetMemPtr(addr, 4);
					if (ptr != nullptr)
					{
						ass.Mov(r2, (u32)ptr);
						ass.Ldr(r2, MemOperand(r2));
						ass.Mov(r1, *ptr);
						ass.Cmp(r1, r2);

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
						ass.Mov(r2, (u32)ptr);
						ass.Ldrh(r2, MemOperand(r2));
						ass.Mov(r1, *ptr);
						ass.Cmp(r1, r2);

						jump(ngen_blockcheckfail, ne);
					}
					addr += 2;
					sz -= 2;
				}
			}
		}
		if (mmu_enabled() && block->has_fpu_op)
		{
			Label fpu_enabled;
			loadSh4Reg(r1, reg_sr_status);
			ass.Tst(r1, 1 << 15);		// test SR.FD bit
			ass.B(eq, &fpu_enabled);

			ass.Mov(r0, block->vaddr);	// pc
			ass.Mov(r1, 0x800);			// event
			ass.Mov(r2, 0x100);			// vector
			call((void *)Do_Exception);
			loadSh4Reg(r4, reg_nextpc);
			jump(no_update);

			ass.Bind(&fpu_enabled);
		}
	}

	//scheduler
	u32 cyc = block->guest_cycles;
	if (!ImmediateA32::IsImmediateA32(cyc))
		cyc &= ~3;
	if (!mmu_enabled())
	{
		ass.Sub(SetFlags, r9, r9, cyc);
	}
	else
	{
		ass.Ldr(r0, MemOperand(r8, rcbOffset(cntx.cycle_counter)));
		ass.Sub(SetFlags, r0, r0, cyc);
		ass.Str(r0, MemOperand(r8, rcbOffset(cntx.cycle_counter)));
		// FIXME condition?
		ass.Mov(r4, block->vaddr);
		storeSh4Reg(r4, reg_nextpc);
	}
	call(intc_sched, le);

	//compile the block's opcodes
	shil_opcode* op;
	for (size_t i = 0; i < block->oplist.size(); i++)
	{
		op = &block->oplist[i];
		
		op->host_offs = ass.GetCursorOffset();

		if (i != 0)
			reg.OpBegin(op, i);

		ngen_compile_opcode(block, op, optimise);

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

	block->relink_offset = ass.GetCursorOffset();
	block->relink_data = 0;

	relinkBlock((DynaRBI *)block);
	ass.Finalize();
	emit_Skip(ass.GetCursorOffset());

	u8* pEnd = ass.GetCursorAddress<u8 *>();
	//blk_start might not be the same, due to profiling counters ..
	block->host_opcodes = (pEnd - blk_start) / 4;

	//host code size needs to cover the entire range of the block
	block->host_code_size = pEnd - (u8*)block->code;
}

void ngen_ResetBlocks()
{
	INFO_LOG(DYNAREC, "ngen_ResetBlocks()");
	mainloop = nullptr;

	if (p_sh4rcb->cntx.CpuRunning)
	{
		// Force the dynarec out of mainloop() to regenerate it
		p_sh4rcb->cntx.CpuRunning = 0;
		restarting = true;
	}
	else
		generate_mainloop();
}

static void generate_mainloop()
{
	if (mainloop != nullptr)
		return;

	INFO_LOG(DYNAREC, "Generating main loop");
	ass = Arm32Assembler((u8 *)emit_GetCCPtr(), emit_FreeSpace());

	// Stubs
	Label ngen_LinkBlock_Shared_stub;
// ngen_LinkBlock_Generic_stub
	ngen_LinkBlock_Generic_stub = ass.GetCursorAddress<const void *>();
	ass.Mov(r1,r4);		// djump/pc -> in case we need it ..
	ass.B(&ngen_LinkBlock_Shared_stub);
// ngen_LinkBlock_cond_Branch_stub
	ngen_LinkBlock_cond_Branch_stub = ass.GetCursorAddress<const void *>();
	ass.Mov(r1, 1);
	ass.B(&ngen_LinkBlock_Shared_stub);
// ngen_LinkBlock_cond_Next_stub
	ngen_LinkBlock_cond_Next_stub = ass.GetCursorAddress<const void *>();
	ass.Mov(r1, 0);
	ass.B(&ngen_LinkBlock_Shared_stub);
// ngen_LinkBlock_Shared_stub
	ass.Bind(&ngen_LinkBlock_Shared_stub);
	ass.Mov(r0, lr);
	ass.Sub(r0, r0, 4);		// go before the call
	call((void *)rdv_LinkBlock);
	ass.Bx(r0);
// ngen_FailedToFindBlock_
	ngen_FailedToFindBlock_ = ass.GetCursorAddress<void (*)()>();
	if (mmu_enabled())
	{
		call((void *)rdv_FailedToFindBlock_pc);
	}
	else
	{
		ass.Mov(r0, r4);
		call((void *)rdv_FailedToFindBlock);
	}
	ass.Bx(r0);
// ngen_blockcheckfail
	ngen_blockcheckfail = ass.GetCursorAddress<const void *>();
	call((void *)rdv_BlockCheckFail);
	if (mmu_enabled())
	{
		Label jumpblockLabel;
		ass.Cmp(r0, 0);
		ass.B(ne, &jumpblockLabel);
		loadSh4Reg(r0, reg_nextpc);
		call((void *)bm_GetCodeByVAddr);
		ass.Bind(&jumpblockLabel);
	}
	ass.Bx(r0);

	// Main loop
	Label no_updateLabel;
// ngen_mainloop:
	mainloop = ass.GetCursorAddress<void (*)(void *)>();
	RegisterList savedRegisters = RegisterList::Union(
			RegisterList(r4, r5, r6, r7),
			RegisterList(r8, r9, r10, r11),
			RegisterList(r12, lr));
	{
		UseScratchRegisterScope scope(&ass);
		scope.ExcludeAll();
		ass.Push(savedRegisters);
	}
	Label longjumpLabel;
	if (!mmu_enabled())
	{
		// r8: context
		ass.Mov(r8, r0);
		// r9: cycle counter
		ass.Ldr(r9, MemOperand(r0, rcbOffset(cntx.cycle_counter)));
	}
	else
	{
		ass.Sub(sp, sp, 4);
		ass.Push(r0);									// push context

		ass.Mov(r0, reinterpret_cast<uintptr_t>(&jmp_stack));
		ass.Mov(r1, sp);
		ass.Str(r1, MemOperand(r0));

		ass.Bind(&longjumpLabel);

		ass.Ldr(r8, MemOperand(sp));					// r8: context
		ass.Mov(r9, (uintptr_t)mmuAddressLUT);			// r9: mmu LUT
	}
	ass.Ldr(r4, MemOperand(r8, rcbOffset(cntx.pc)));	// r4: pc
	ass.B(&no_updateLabel);								// Go to mainloop !
	// this code is here for fall-through behavior of do_iter
	Label do_iter;
	Label cleanup;
// intc_sched:
	intc_sched = ass.GetCursorAddress<const void *>();
	if (!mmu_enabled())
		ass.Add(r9, r9, SH4_TIMESLICE);
	else
	{
		ass.Ldr(r0, MemOperand(r8, rcbOffset(cntx.cycle_counter)));
		ass.Add(r0, r0, SH4_TIMESLICE);
		ass.Str(r0, MemOperand(r8, rcbOffset(cntx.cycle_counter)));
	}
	ass.Mov(r4, lr);
	call((void *)UpdateSystem);
	ass.Mov(lr, r4);
	ass.Cmp(r0, 0);
	ass.B(ne, &do_iter);
	ass.Ldr(r0, MemOperand(r8, rcbOffset(cntx.CpuRunning)));
	ass.Cmp(r0, 0);
	ass.Bx(ne, lr);
	// do_iter:
	ass.Bind(&do_iter);
	ass.Mov(r0, r4);
	call((void *)rdv_DoInterrupts);
	ass.Mov(r4, r0);

// no_update:
	no_update = ass.GetCursorAddress<const void *>();
	ass.Bind(&no_updateLabel);
	// next_pc _MUST_ be on r4
	ass.Ldr(r0, MemOperand(r8, rcbOffset(cntx.CpuRunning)));
	ass.Cmp(r0, 0);
	ass.B(eq, &cleanup);

	if (!mmu_enabled())
	{
		ass.Sub(r2, r8, -rcbOffset(fpcb));
		ass.Ubfx(r1, r4, 1, 24);	// 24+1 bits: 32 MB
									// RAM wraps around so if actual RAM size is 16MB, we won't overflow
		ass.Ldr(pc, MemOperand(r2, r1, LSL, 2));
	}
	else
	{
		ass.Mov(r0, r4);
		call((void *)bm_GetCodeByVAddr);
		ass.Bx(r0);
	}

// cleanup:
	ass.Bind(&cleanup);
	if (mmu_enabled())
		ass.Add(sp, sp, 8);	// pop context & alignment
	else
		ass.Str(r9, MemOperand(r8, rcbOffset(cntx.cycle_counter)));
	{
		UseScratchRegisterScope scope(&ass);
		scope.ExcludeAll();
		ass.Pop(savedRegisters);
	}
	ass.Bx(lr);

	// Exception handler
	handleException = ass.GetCursorAddress<void (*)()>();
	if (mmu_enabled())
	{
		ass.Mov(r0, reinterpret_cast<uintptr_t>(&jmp_stack));
		ass.Ldr(r1, MemOperand(r0));
		ass.Mov(sp, r1);
		ass.B(&longjumpLabel);
	}

    // Memory handlers
    for (int s=0;s<6;s++)
	{
		const void* fn=s==0?(void*)_vmem_ReadMem8SX32:
				 s==1?(void*)_vmem_ReadMem16SX32:
				 s==2?(void*)_vmem_ReadMem32:
				 s==3?(void*)_vmem_WriteMem8:
				 s==4?(void*)_vmem_WriteMem16:
				 s==5?(void*)_vmem_WriteMem32:
				 0;

		bool read=s<=2;

		//r0 to r13
		for (int i=0;i<=13;i++)
		{
			if (i==1 || i ==2 || i == 3 || i == 4 || i==12 || i==13)
				continue;

			const void *v;
			if (i == 0)
				v = fn;
			else
			{
				v = ass.GetCursorAddress<const void *>();
				ass.Mov(r0, Register(i));
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

			_mem_hndl_SQ32[optp - SZ_32I][reg] = ass.GetCursorAddress<const void *>();

			if (optp == SZ_64F)
			{
				ass.Lsr(r1, r0, 26);
				ass.Cmp(r1, 0x38);
				ass.And(r1, r0, 0x3F);
				ass.Add(r1, r1, r8);
				jump((void *)&_vmem_WriteMem64, ne);
				ass.Strd(r2, r3, MemOperand(r1, rcbOffset(sq_buffer)));
			}
			else
			{
				ass.And(r3, Register(reg), 0x3F);
				ass.Lsr(r2, Register(reg), 26);
				ass.Add(r3, r3, r8);
				ass.Cmp(r2, 0x38);
				if (reg != 0)
					ass.Mov(ne, r0, Register(reg));
				jump((void *)&_vmem_WriteMem32, ne);
				ass.Str(r1, MemOperand(r3, rcbOffset(sq_buffer)));
			}
			ass.Bx(lr);
		}
	}
	ass.Finalize();
	emit_Skip(ass.GetBuffer()->GetSizeInBytes());

    ngen_FailedToFindBlock = ngen_FailedToFindBlock_;

	INFO_LOG(DYNAREC, "readm helpers: up to %p", ass.GetCursorAddress<void *>());
}

void ngen_init()
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
}

void ngen_HandleException(host_context_t &context)
{
	context.pc = (uintptr_t)handleException;
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	generate_mainloop(); // FIXME why is this needed?
	return new DynaRBI();
};
#endif
