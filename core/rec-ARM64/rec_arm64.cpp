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

#include "types.h"

#if FEAT_SHREC == DYNAREC_JIT

#include <unistd.h>
#include <map>
#include <setjmp.h>

#include "deps/vixl/aarch64/macro-assembler-aarch64.h"
using namespace vixl::aarch64;

//#define EXPLODE_SPANS

#include "hw/sh4/sh4_opcode_list.h"

#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_rom.h"
#include "hw/mem/vmem32.h"
#include "arm64_regalloc.h"

#undef do_sqw_nommu

extern "C" void ngen_blockcheckfail(u32 pc);
extern "C" void ngen_LinkBlock_Generic_stub();
extern "C" void ngen_LinkBlock_cond_Branch_stub();
extern "C" void ngen_LinkBlock_cond_Next_stub();
extern "C" void ngen_FailedToFindBlock_mmu();
extern "C" void ngen_FailedToFindBlock_nommu();
extern void vmem_platform_flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end);
static void generate_mainloop();

u32 mem_writes, mem_reads;
u32 mem_rewrites_w, mem_rewrites_r;

struct DynaRBI : RuntimeBlockInfo
{
	virtual u32 Relink() override;

	virtual void Relocate(void* dst) override {
		verify(false);
	}
};

double host_cpu_time;
u64 guest_cpu_cycles;
static jmp_buf jmp_env;
static u32 cycle_counter;

static void (*mainloop)(void *context);
static void (*arm64_intc_sched)();
static void (*arm64_no_update)();

#ifdef PROFILING
#include <time.h>

static clock_t slice_start;
extern "C"
{
static __attribute((used)) void start_slice()
{
	slice_start = clock();
}
static __attribute((used)) void end_slice()
{
	host_cpu_time += (double)(clock() - slice_start) / CLOCKS_PER_SEC;
}
}
#endif

__asm__
(
		".hidden ngen_LinkBlock_cond_Branch_stub	\n\t"
		".globl ngen_LinkBlock_cond_Branch_stub		\n\t"
	"ngen_LinkBlock_cond_Branch_stub:		\n\t"
		"mov w1, #1							\n\t"
		"b ngen_LinkBlock_Shared_stub		\n"

		".hidden ngen_LinkBlock_cond_Next_stub	\n\t"
		".globl ngen_LinkBlock_cond_Next_stub	\n\t"
	"ngen_LinkBlock_cond_Next_stub:			\n\t"
		"mov w1, #0							\n\t"
		"b ngen_LinkBlock_Shared_stub		\n"

		".hidden ngen_LinkBlock_Generic_stub	\n\t"
		".globl ngen_LinkBlock_Generic_stub	\n\t"
	"ngen_LinkBlock_Generic_stub:			\n\t"
		"mov w1, w29						\n\t"	// djump/pc -> in case we need it ..
		//"b ngen_LinkBlock_Shared_stub		\n"

		".hidden ngen_LinkBlock_Shared_stub	\n\t"
		".globl ngen_LinkBlock_Shared_stub	\n\t"
	"ngen_LinkBlock_Shared_stub:			\n\t"
		"mov x0, lr							\n\t"
		"sub x0, x0, #4						\n\t"	// go before the call
		"bl rdv_LinkBlock					\n\t"   // returns an RX addr
		"br x0								\n"

		".hidden ngen_FailedToFindBlock_nommu	\n\t"
		".globl ngen_FailedToFindBlock_nommu	\n\t"
	"ngen_FailedToFindBlock_nommu:			\n\t"
		"mov w0, w29						\n\t"
		"bl rdv_FailedToFindBlock			\n\t"
		"br x0								\n"

		".hidden ngen_FailedToFindBlock_mmu	\n\t"
		".globl ngen_FailedToFindBlock_mmu	\n\t"
	"ngen_FailedToFindBlock_mmu:			\n\t"
		"bl rdv_FailedToFindBlock_pc		\n\t"
		"br x0								\n"

		".hidden ngen_blockcheckfail		\n\t"
		".globl ngen_blockcheckfail			\n\t"
	"ngen_blockcheckfail:					\n\t"
		"bl rdv_BlockCheckFail				\n\t"
		"br x0								\n"
);

static bool restarting;

void ngen_mainloop(void* v_cntx)
{
	do {
		restarting = false;
		generate_mainloop();

		mainloop(v_cntx);
		if (restarting)
			p_sh4rcb->cntx.CpuRunning = 1;
	} while (restarting);
}

void ngen_init()
{
	printf("Initializing the ARM64 dynarec\n");
	ngen_FailedToFindBlock = &ngen_FailedToFindBlock_nommu;
}

void ngen_ResetBlocks()
{
	mainloop = NULL;
	if (mmu_enabled())
		ngen_FailedToFindBlock = &ngen_FailedToFindBlock_mmu;
	else
		ngen_FailedToFindBlock = &ngen_FailedToFindBlock_nommu;
	if (p_sh4rcb->cntx.CpuRunning)
	{
		// Force the dynarec out of mainloop() to regenerate it
		p_sh4rcb->cntx.CpuRunning = 0;
		restarting = true;
	}
}

void ngen_GetFeatures(ngen_features* dst)
{
	dst->InterpreterFallback = false;
	dst->OnlyDynamicEnds = false;
}

template<typename T>
static T ReadMemNoEx(u32 addr, u32 pc)
{
#ifndef NO_MMU
	u32 ex;
	T rv = mmu_ReadMemNoEx<T>(addr, &ex);
	if (ex)
	{
		if (pc & 1)
			spc = pc - 1;
		else
			spc = pc;
		longjmp(jmp_env, 1);
	}
	return rv;
#else
	return (T)0;	// not used
#endif
}

template<typename T>
static void WriteMemNoEx(u32 addr, T data, u32 pc)
{
#ifndef NO_MMU
	u32 ex = mmu_WriteMemNoEx<T>(addr, data);
	if (ex)
	{
		if (pc & 1)
			spc = pc - 1;
		else
			spc = pc;
		longjmp(jmp_env, 1);
	}
#endif
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
		longjmp(jmp_env, 1);
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
		longjmp(jmp_env, 1);
	}
}

class Arm64Assembler : public MacroAssembler
{
	typedef void (MacroAssembler::*Arm64Op_RRO)(const Register&, const Register&, const Operand&);
	typedef void (MacroAssembler::*Arm64Op_RROF)(const Register&, const Register&, const Operand&, enum FlagsUpdate);

public:
	Arm64Assembler() : Arm64Assembler(emit_GetCCPtr())
	{
	}
	Arm64Assembler(void *buffer) : MacroAssembler((u8 *)buffer, emit_FreeSpace()), regalloc(this)
	{
		call_regs.push_back(&w0);
		call_regs.push_back(&w1);
		call_regs.push_back(&w2);
		call_regs.push_back(&w3);
		call_regs.push_back(&w4);
		call_regs.push_back(&w5);
		call_regs.push_back(&w6);
		call_regs.push_back(&w7);

		call_regs64.push_back(&x0);
		call_regs64.push_back(&x1);
		call_regs64.push_back(&x2);
		call_regs64.push_back(&x3);
		call_regs64.push_back(&x4);
		call_regs64.push_back(&x5);
		call_regs64.push_back(&x6);
		call_regs64.push_back(&x7);

		call_fregs.push_back(&s0);
		call_fregs.push_back(&s1);
		call_fregs.push_back(&s2);
		call_fregs.push_back(&s3);
		call_fregs.push_back(&s4);
		call_fregs.push_back(&s5);
		call_fregs.push_back(&s6);
		call_fregs.push_back(&s7);
	}

	void ngen_BinaryOp_RRO(shil_opcode* op, Arm64Op_RRO arm_op, Arm64Op_RROF arm_op2)
	{
		Operand op3 = Operand(0);
		if (op->rs2.is_imm())
		{
			op3 = Operand(op->rs2._imm);
		}
		else if (op->rs2.is_r32i())
		{
			op3 = Operand(regalloc.MapRegister(op->rs2));
		}
		if (arm_op != NULL)
			((*this).*arm_op)(regalloc.MapRegister(op->rd), regalloc.MapRegister(op->rs1), op3);
		else
			((*this).*arm_op2)(regalloc.MapRegister(op->rd), regalloc.MapRegister(op->rs1), op3, LeaveFlags);
	}

	const Register& GenMemAddr(const shil_opcode& op, const Register* raddr = NULL)
	{
		const Register* ret_reg = raddr == NULL ? &w0 : raddr;

		if (op.rs3.is_imm())
		{
			if (regalloc.IsAllocg(op.rs1))
				Add(*ret_reg, regalloc.MapRegister(op.rs1), op.rs3._imm);
			else
			{
				Ldr(*ret_reg, sh4_context_mem_operand(op.rs1.reg_ptr()));
				Add(*ret_reg, *ret_reg, op.rs3._imm);
			}
		}
		else if (op.rs3.is_r32i())
		{
			if (regalloc.IsAllocg(op.rs1) && regalloc.IsAllocg(op.rs3))
				Add(*ret_reg, regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs3));
			else
			{
				Ldr(*ret_reg, sh4_context_mem_operand(op.rs1.reg_ptr()));
				Ldr(w8, sh4_context_mem_operand(op.rs3.reg_ptr()));
				Add(*ret_reg, *ret_reg, w8);
			}
		}
		else if (!op.rs3.is_null())
		{
			die("invalid rs3");
		}
		else if (op.rs1.is_reg())
		{
			if (regalloc.IsAllocg(op.rs1))
			{
				if (raddr == NULL)
					ret_reg = &regalloc.MapRegister(op.rs1);
				else
					Mov(*ret_reg, regalloc.MapRegister(op.rs1));
			}
			else
			{
				Ldr(*ret_reg, sh4_context_mem_operand(op.rs1.reg_ptr()));
			}
		}
		else
		{
			verify(op.rs1.is_imm());
			Mov(*ret_reg, op.rs1._imm);
		}

		return *ret_reg;
	}

	void ngen_Compile(RuntimeBlockInfo* block, SmcCheckEnum smc_checks, bool reset, bool staging, bool optimise)
	{
		//printf("REC-ARM64 compiling %08x\n", block->addr);
#ifdef PROFILING
		SaveFramePointer();
#endif
		this->block = block;
		
		CheckBlock(smc_checks, block);

		// run register allocator
		regalloc.DoAlloc(block);

		// scheduler
		if (mmu_enabled())
		{
			Mov(x1, reinterpret_cast<uintptr_t>(&cycle_counter));
			Ldr(w0, MemOperand(x1));
			Subs(w0, w0, block->guest_cycles);
			Str(w0, MemOperand(x1));
		}
		else
		{
			Subs(w27, w27, block->guest_cycles);
		}
		Label cycles_remaining;
		B(&cycles_remaining, pl);
		GenCall(*arm64_intc_sched);
		Bind(&cycles_remaining);

#ifdef PROFILING
		Ldr(x11, (uintptr_t)&guest_cpu_cycles);
		Ldr(x0, MemOperand(x11));
		Add(x0, x0, block->guest_cycles);
		Str(x0, MemOperand(x11));
#endif
		for (size_t i = 0; i < block->oplist.size(); i++)
		{
			shil_opcode& op  = block->oplist[i];
			regalloc.OpBegin(&op, i);

			switch (op.op)
			{
			case shop_ifb:	// Interpreter fallback
				if (op.rs1._imm)	// if NeedPC()
				{
					Mov(w10, op.rs2._imm);
					Str(w10, sh4_context_mem_operand(&next_pc));
				}
				Mov(*call_regs[0], op.rs3._imm);

				if (!mmu_enabled())
				{
					GenCallRuntime(OpDesc[op.rs3._imm]->oph);
				}
				else
				{
					Mov(*call_regs64[1], reinterpret_cast<uintptr_t>(*OpDesc[op.rs3._imm]->oph));	// op handler
					Mov(*call_regs[2], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

					GenCallRuntime(interpreter_fallback);
				}

				break;

			case shop_jcond:
			case shop_jdyn:
				if (op.rs2.is_imm())
					Add(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), op.rs2._imm);
				else
					Mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				// Save it for the branching at the end of the block
				Mov(w29, regalloc.MapRegister(op.rd));
				break;

			case shop_mov32:
				verify(op.rd.is_reg());
				verify(op.rs1.is_reg() || op.rs1.is_imm());

				if (regalloc.IsAllocf(op.rd))
				{
					if (op.rs1.is_imm())
						Fmov(regalloc.MapVRegister(op.rd), (float&)op.rs1._imm);
					else if (regalloc.IsAllocf(op.rs1))
						Fmov(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs1));
					else
						Fmov(regalloc.MapVRegister(op.rd), regalloc.MapRegister(op.rs1));
				}
				else
				{
					if (op.rs1.is_imm())
						Mov(regalloc.MapRegister(op.rd), op.rs1._imm);
					else if (regalloc.IsAllocg(op.rs1))
						Mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
					else
						Fmov(regalloc.MapRegister(op.rd), regalloc.MapVRegister(op.rs1));
				}
				break;

			case shop_mov64:
				verify(op.rd.is_reg());
				verify(op.rs1.is_reg() || op.rs1.is_imm());

#ifdef EXPLODE_SPANS
				Fmov(regalloc.MapVRegister(op.rd, 0), regalloc.MapVRegister(op.rs1, 0));
				Fmov(regalloc.MapVRegister(op.rd, 1), regalloc.MapVRegister(op.rs1, 1));
#else
				shil_param_to_host_reg(op.rs1, x15);
				host_reg_to_shil_param(op.rd, x15);
#endif
				break;

			case shop_readm:
				GenReadMemory(op, i, optimise);
				break;

			case shop_writem:
				GenWriteMemory(op, i, optimise);
				break;

			case shop_sync_sr:
				GenCallRuntime(UpdateSR);
				break;
			case shop_sync_fpscr:
				GenCallRuntime(UpdateFPSCR);
				break;

			case shop_swaplb:
				Mov(w9, Operand(regalloc.MapRegister(op.rs1), LSR, 16));
				Rev16(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				Bfi(regalloc.MapRegister(op.rd), w9, 16, 16);
				break;

			case shop_neg:
				Neg(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				break;
			case shop_not:
				Mvn(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				break;

			case shop_and:
				ngen_BinaryOp_RRO(&op, &MacroAssembler::And, NULL);
				break;
			case shop_or:
				ngen_BinaryOp_RRO(&op, &MacroAssembler::Orr, NULL);
				break;
			case shop_xor:
				ngen_BinaryOp_RRO(&op, &MacroAssembler::Eor, NULL);
				break;
			case shop_add:
				ngen_BinaryOp_RRO(&op, NULL, &MacroAssembler::Add);
				break;
			case shop_sub:
				ngen_BinaryOp_RRO(&op, NULL, &MacroAssembler::Sub);
				break;
			case shop_shl:
				if (op.rs2.is_imm())
					Lsl(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), op.rs2._imm);
				else if (op.rs2.is_reg())
					Lsl(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
				break;
			case shop_shr:
				if (op.rs2.is_imm())
					Lsr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), op.rs2._imm);
				else if (op.rs2.is_reg())
					Lsr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
				break;
			case shop_sar:
				if (op.rs2.is_imm())
					Asr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), op.rs2._imm);
				else if (op.rs2.is_reg())
					Asr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
				break;
			case shop_ror:
				if (op.rs2.is_imm())
					Ror(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), op.rs2._imm);
				else if (op.rs2.is_reg())
					Ror(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
				break;

			case shop_adc:
				Cmp(regalloc.MapRegister(op.rs3), 1);	// C = rs3
				Adcs(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2)); // (C,rd)=rs1+rs2+rs3(C)
				Cset(regalloc.MapRegister(op.rd2), cs);	// rd2 = C
				break;
			case shop_sbc:
				Cmp(wzr, regalloc.MapRegister(op.rs3));	// C = ~rs3
				Sbcs(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2)); // (C,rd) = rs1 - rs2 - ~rs3(C)
				Cset(regalloc.MapRegister(op.rd2), cc);	// rd2 = ~C
				break;
			case shop_negc:
				Cmp(wzr, regalloc.MapRegister(op.rs2));	// C = ~rs2
				Sbcs(regalloc.MapRegister(op.rd), wzr, regalloc.MapRegister(op.rs1)); // (C,rd) = 0 - rs1 - ~rs2(C)
				Cset(regalloc.MapRegister(op.rd2), cc);	// rd2 = ~C
				break;

			case shop_rocr:
				Ubfx(w0, regalloc.MapRegister(op.rs1), 0, 1);										// w0 = rs1[0] (new C)
				Mov(regalloc.MapRegister(op.rd), Operand(regalloc.MapRegister(op.rs1), LSR, 1));	// rd = rs1 >> 1
				Bfi(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs2), 31, 1);				// rd |= C << 31
				Mov(regalloc.MapRegister(op.rd2), w0);												// rd2 = w0 (new C)
				break;
			case shop_rocl:
				Tst(regalloc.MapRegister(op.rs1), 0x80000000);	// Z = ~rs1[31]
				Orr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs2), Operand(regalloc.MapRegister(op.rs1), LSL, 1)); // rd = rs1 << 1 | rs2(C)
				Cset(regalloc.MapRegister(op.rd2), ne);			// rd2 = ~Z(C)
				break;

			case shop_shld:
			case shop_shad:
				{
					Label positive_shift, negative_shift, end;
					Tbz(regalloc.MapRegister(op.rs2), 31, &positive_shift);
					Cmn(regalloc.MapRegister(op.rs2), 32);
					B(&negative_shift, ne);
					if (op.op == shop_shld)
						// Logical shift
						Lsr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), 31);
					else
						// Arithmetic shift
						Asr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), 31);
					B(&end);

					Bind(&positive_shift);
					Lsl(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
					B(&end);

					Bind(&negative_shift);
					Neg(w1, regalloc.MapRegister(op.rs2));
					if (op.op == shop_shld)
						// Logical shift
						Lsr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), w1);
					else
						// Arithmetic shift
						Asr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), w1);
					Bind(&end);
				}
				break;

			case shop_test:
			case shop_seteq:
			case shop_setge:
			case shop_setgt:
			case shop_setae:
			case shop_setab:
				{
					if (op.op == shop_test)
					{
						if (op.rs2.is_imm())
							Tst(regalloc.MapRegister(op.rs1), op.rs2._imm);
						else
							Tst(regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
					}
					else
					{
						if (op.rs2.is_imm())
							Cmp(regalloc.MapRegister(op.rs1), op.rs2._imm);
						else
							Cmp(regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
					}

					static const Condition shop_conditions[] = { eq, eq, ge, gt, hs, hi };

					Cset(regalloc.MapRegister(op.rd), shop_conditions[op.op - shop_test]);
				}
				break;
			case shop_setpeq:
				Eor(w1, regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));

				Mov(regalloc.MapRegister(op.rd), wzr);
				Mov(w2, wzr);	// wzr not supported by csinc (?!)
				Tst(w1, 0xFF000000);
				Csinc(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), w2, ne);
				Tst(w1, 0x00FF0000);
				Csinc(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), w2, ne);
				Tst(w1, 0x0000FF00);
				Csinc(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), w2, ne);
				Tst(w1, 0x000000FF);
				Csinc(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), w2, ne);
				break;

			case shop_mul_u16:
				Uxth(w10, regalloc.MapRegister(op.rs1));
				Uxth(w11, regalloc.MapRegister(op.rs2));
				Mul(regalloc.MapRegister(op.rd), w10, w11);
				break;
			case shop_mul_s16:
				Sxth(w10, regalloc.MapRegister(op.rs1));
				Sxth(w11, regalloc.MapRegister(op.rs2));
				Mul(regalloc.MapRegister(op.rd), w10, w11);
				break;
			case shop_mul_i32:
				Mul(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
				break;
			case shop_mul_u64:
			case shop_mul_s64:
				{
					const Register& rd_xreg = Register::GetXRegFromCode(regalloc.MapRegister(op.rd).GetCode());
					if (op.op == shop_mul_u64)
						Umull(rd_xreg, regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
					else
						Smull(rd_xreg, regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
					const Register& rd2_xreg = Register::GetXRegFromCode(regalloc.MapRegister(op.rd2).GetCode());
					Lsr(rd2_xreg, rd_xreg, 32);
				}
				break;

			case shop_pref:
				{
					if (regalloc.IsAllocg(op.rs1))
						Lsr(w1, regalloc.MapRegister(op.rs1), 26);
					else
					{
						Ldr(w0, sh4_context_mem_operand(op.rs1.reg_ptr()));
						Lsr(w1, w0, 26);
					}
					Cmp(w1, 0x38);
					Label not_sqw;
					B(&not_sqw, ne);
					if (regalloc.IsAllocg(op.rs1))
						Mov(w0, regalloc.MapRegister(op.rs1));

					if (mmu_enabled())
					{
						Mov(*call_regs[1], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

						GenCallRuntime(do_sqw_mmu_no_ex);
					}
					else
					{
						if (CCN_MMUCR.AT)
						{
							Ldr(x9, reinterpret_cast<uintptr_t>(&do_sqw_mmu));
						}
						else
						{
							Sub(x9, x28, offsetof(Sh4RCB, cntx) - offsetof(Sh4RCB, do_sqw_nommu));
							Ldr(x9, MemOperand(x9));
							Sub(x1, x28, offsetof(Sh4RCB, cntx) - offsetof(Sh4RCB, sq_buffer));
						}
						Blr(x9);
					}
					Bind(&not_sqw);
				}
				break;

			case shop_ext_s8:
				Sxtb(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				break;
			case shop_ext_s16:
				Sxth(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				break;

			case shop_xtrct:
				Lsr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), 16);
				Lsl(w0, regalloc.MapRegister(op.rs2), 16);
				Orr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), w0);
				break;

			//
			// FPU
			//

			case shop_fadd:
				Fadd(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs1), regalloc.MapVRegister(op.rs2));
				break;
			case shop_fsub:
				Fsub(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs1), regalloc.MapVRegister(op.rs2));
				break;
			case shop_fmul:
				Fmul(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs1), regalloc.MapVRegister(op.rs2));
				break;
			case shop_fdiv:
				Fdiv(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs1), regalloc.MapVRegister(op.rs2));
				break;

			case shop_fabs:
				Fabs(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs1));
				break;
			case shop_fneg:
				Fneg(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs1));
				break;
			case shop_fsqrt:
				Fsqrt(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs1));
				break;

			case shop_fmac:
				Fmadd(regalloc.MapVRegister(op.rd), regalloc.MapVRegister(op.rs3), regalloc.MapVRegister(op.rs2), regalloc.MapVRegister(op.rs1));
				break;

			case shop_fsrra:
				Fsqrt(s0, regalloc.MapVRegister(op.rs1));
				Fmov(s1, 1.f);
				Fdiv(regalloc.MapVRegister(op.rd), s1, s0);
				break;

			case shop_fsetgt:
			case shop_fseteq:
				Fcmp(regalloc.MapVRegister(op.rs1), regalloc.MapVRegister(op.rs2));
				Cset(regalloc.MapRegister(op.rd), op.op == shop_fsetgt ? gt : eq);
				break;

			case shop_fsca:
				Mov(x1, reinterpret_cast<uintptr_t>(&sin_table));
				Add(x1, x1, Operand(regalloc.MapRegister(op.rs1), UXTH, 3));
#ifdef EXPLODE_SPANS
				Ldr(regalloc.MapVRegister(op.rd, 0), MemOperand(x1, 4, PostIndex));
				Ldr(regalloc.MapVRegister(op.rd, 1), MemOperand(x1));
#else
				Ldr(x2, MemOperand(x1));
				Str(x2, sh4_context_mem_operand(op.rd.reg_ptr()));
#endif
				break;

			case shop_fipr:
				Add(x9, x28, sh4_context_mem_operand(op.rs1.reg_ptr()).GetOffset());
				Ld1(v0.V4S(), MemOperand(x9));
				if (op.rs1._reg != op.rs2._reg)
				{
					Add(x9, x28, sh4_context_mem_operand(op.rs2.reg_ptr()).GetOffset());
					Ld1(v1.V4S(), MemOperand(x9));
					Fmul(v0.V4S(), v0.V4S(), v1.V4S());
				}
				else
					Fmul(v0.V4S(), v0.V4S(), v0.V4S());
				Faddp(v1.V4S(), v0.V4S(), v0.V4S());
				Faddp(regalloc.MapVRegister(op.rd), v1.V2S());
				break;

			case shop_ftrv:
				Add(x9, x28, sh4_context_mem_operand(op.rs1.reg_ptr()).GetOffset());
				Ld1(v0.V4S(), MemOperand(x9));
				Add(x9, x28, sh4_context_mem_operand(op.rs2.reg_ptr()).GetOffset());
				Ld1(v1.V4S(), MemOperand(x9, 16, PostIndex));
				Ld1(v2.V4S(), MemOperand(x9, 16, PostIndex));
				Ld1(v3.V4S(), MemOperand(x9, 16, PostIndex));
				Ld1(v4.V4S(), MemOperand(x9, 16, PostIndex));
				Fmul(v5.V4S(), v1.V4S(), s0, 0);
				Fmla(v5.V4S(), v2.V4S(), s0, 1);
				Fmla(v5.V4S(), v3.V4S(), s0, 2);
				Fmla(v5.V4S(), v4.V4S(), s0, 3);
				Add(x9, x28, sh4_context_mem_operand(op.rd.reg_ptr()).GetOffset());
				St1(v5.V4S(), MemOperand(x9));
				break;

			case shop_frswap:
				Add(x9, x28, sh4_context_mem_operand(op.rs1.reg_ptr()).GetOffset());
				Add(x10, x28, sh4_context_mem_operand(op.rd.reg_ptr()).GetOffset());
				Ld4(v0.V2D(), v1.V2D(), v2.V2D(), v3.V2D(), MemOperand(x9));
				Ld4(v4.V2D(), v5.V2D(), v6.V2D(), v7.V2D(), MemOperand(x10));
				St4(v4.V2D(), v5.V2D(), v6.V2D(), v7.V2D(), MemOperand(x9));
				St4(v0.V2D(), v1.V2D(), v2.V2D(), v3.V2D(), MemOperand(x10));
				break;

			case shop_cvt_f2i_t:
				Fcvtzs(regalloc.MapRegister(op.rd), regalloc.MapVRegister(op.rs1));
				break;
			case shop_cvt_i2f_n:
			case shop_cvt_i2f_z:
				Scvtf(regalloc.MapVRegister(op.rd), regalloc.MapRegister(op.rs1));
				break;

			default:
				shil_chf[op.op](&op);
				break;
			}
			regalloc.OpEnd(&op);
		}
		regalloc.Cleanup();

		block->relink_offset = (u32)GetBuffer()->GetCursorOffset();
		block->relink_data = 0;

		RelinkBlock(block);

		Finalize();
	}

	void ngen_CC_Start(shil_opcode* op)
	{
		CC_pars.clear();
	}

	void ngen_CC_Param(shil_opcode& op, shil_param& prm, CanonicalParamType tp)
	{
		switch (tp)
		{

		case CPT_u32:
		case CPT_ptr:
		case CPT_f32:
		{
			CC_PS t = { tp, &prm };
			CC_pars.push_back(t);
		}
		break;

		case CPT_u64rvL:
		case CPT_u32rv:
			host_reg_to_shil_param(prm, w0);
			break;

		case CPT_u64rvH:
			Lsr(x10, x0, 32);
			host_reg_to_shil_param(prm, w10);
			break;

		case CPT_f32rv:
			host_reg_to_shil_param(prm, s0);
			break;
		}
	}

	void ngen_CC_Call(shil_opcode*op, void* function)
	{
		int regused = 0;
		int fregused = 0;

		// Args are pushed in reverse order by shil_canonical
		for (int i = CC_pars.size(); i-- > 0;)
		{
			verify(fregused < call_fregs.size() && regused < call_regs.size());
			shil_param& prm = *CC_pars[i].prm;
			switch (CC_pars[i].type)
			{
			// push the params

			case CPT_u32:
				shil_param_to_host_reg(prm, *call_regs[regused++]);

				break;

			case CPT_f32:
				if (prm.is_reg()) {
					Fmov(*call_fregs[fregused], regalloc.MapVRegister(prm));
				}
				else {
					verify(prm.is_null());
				}
				fregused++;
				break;

			case CPT_ptr:
				verify(prm.is_reg());
				// push the ptr itself
				Mov(*call_regs64[regused++], reinterpret_cast<uintptr_t>(prm.reg_ptr()));

				break;
			case CPT_u32rv:
			case CPT_u64rvL:
			case CPT_u64rvH:
			case CPT_f32rv:
				// return values are handled in ngen_CC_param()
				break;
			}
		}
		GenCallRuntime((void (*)())function);
	}

	MemOperand sh4_context_mem_operand(void *p)
	{
		u32 offset = (u8*)p - (u8*)&p_sh4rcb->cntx;
		verify((offset & 3) == 0 && offset <= 16380);	// FIXME 64-bit regs need multiple of 8 up to 32760
		return MemOperand(x28, offset);
	}

	void GenReadMemorySlow(const shil_opcode& op)
	{
		Instruction *start_instruction = GetCursorAddress<Instruction *>();
		u32 size = op.flags & 0x7f;

		if (mmu_enabled())
			Mov(*call_regs[1], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

		switch (size)
		{
		case 1:
			if (!mmu_enabled())
				GenCallRuntime(ReadMem8);
			else
				GenCallRuntime(ReadMemNoEx<u8>);
			Sxtb(w0, w0);
			break;

		case 2:
			if (!mmu_enabled())
				GenCallRuntime(ReadMem16);
			else
				GenCallRuntime(ReadMemNoEx<u16>);
			Sxth(w0, w0);
			break;

		case 4:
			if (!mmu_enabled())
				GenCallRuntime(ReadMem32);
			else
				GenCallRuntime(ReadMemNoEx<u32>);
			break;

		case 8:
			if (!mmu_enabled())
				GenCallRuntime(ReadMem64);
			else
				GenCallRuntime(ReadMemNoEx<u64>);
			break;

		default:
			die("1..8 bytes");
			break;
		}

		if (size != 8)
			host_reg_to_shil_param(op.rd, w0);
		else
		{
#ifdef EXPLODE_SPANS
			verify(op.rd.count() == 2 && regalloc.IsAllocf(op.rd, 0) && regalloc.IsAllocf(op.rd, 1));
			Fmov(regalloc.MapVRegister(op.rd, 0), w0);
			Lsr(x0, x0, 32);
			Fmov(regalloc.MapVRegister(op.rd, 1), w0);
#else
			host_reg_to_shil_param(op.rd, x0);
#endif
		}
		EnsureCodeSize(start_instruction, read_memory_rewrite_size);
	}

	void GenWriteMemorySlow(const shil_opcode& op)
	{
		Instruction *start_instruction = GetCursorAddress<Instruction *>();
		if (mmu_enabled())
			Mov(*call_regs[2], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

		u32 size = op.flags & 0x7f;
		switch (size)
		{
		case 1:
			if (!mmu_enabled())
				GenCallRuntime(WriteMem8);
			else
				GenCallRuntime(WriteMemNoEx<u8>);
			break;

		case 2:
			if (!mmu_enabled())
				GenCallRuntime(WriteMem16);
			else
				GenCallRuntime(WriteMemNoEx<u16>);
			break;

		case 4:
			if (!mmu_enabled())
				GenCallRuntime(WriteMem32);
			else
				GenCallRuntime(WriteMemNoEx<u32>);
			break;

		case 8:
			if (!mmu_enabled())
				GenCallRuntime(WriteMem64);
			else
				GenCallRuntime(WriteMemNoEx<u64>);
			break;

		default:
			die("1..8 bytes");
			break;
		}
		EnsureCodeSize(start_instruction, write_memory_rewrite_size);
	}

	void InitializeRewrite(RuntimeBlockInfo *block, size_t opid)
	{
		this->block = block;
		// writem rewrite doesn't use regalloc
		if (block->oplist[opid].op == shop_readm)
		{
			regalloc.DoAlloc(block);
			regalloc.current_opid = opid;
		}
	}

	u32 RelinkBlock(RuntimeBlockInfo *block)
	{
		ptrdiff_t start_offset = GetBuffer()->GetCursorOffset();

		switch (block->BlockType)
		{

		case BET_StaticJump:
		case BET_StaticCall:
			// next_pc = block->BranchBlock;
			if (block->pBranchBlock == NULL)
			{
				if (!mmu_enabled())
					GenCallRuntime(ngen_LinkBlock_Generic_stub);
				else
				{
					Mov(w29, block->BranchBlock);
					Str(w29, sh4_context_mem_operand(&next_pc));
					GenBranch(*arm64_no_update);
				}
			}
			else
				GenBranch(block->pBranchBlock->code);
			break;

		case BET_Cond_0:
		case BET_Cond_1:
			{
				// next_pc = next_pc_value;
				// if (*jdyn == 0)
				//   next_pc = branch_pc_value;

				if (block->has_jcond)
					Ldr(w11, sh4_context_mem_operand(&Sh4cntx.jdyn));
				else
					Ldr(w11, sh4_context_mem_operand(&sr.T));

				Cmp(w11, block->BlockType & 1);

				Label branch_not_taken;

				B(ne, &branch_not_taken);
				if (block->pBranchBlock != NULL)
					GenBranch(block->pBranchBlock->code);
				else
				{
					if (!mmu_enabled())
						GenCallRuntime(ngen_LinkBlock_cond_Branch_stub);
					else
					{
						Mov(w29, block->BranchBlock);
						Str(w29, sh4_context_mem_operand(&next_pc));
						GenBranch(*arm64_no_update);
					}
				}

				Bind(&branch_not_taken);

				if (block->pNextBlock != NULL)
					GenBranch(block->pNextBlock->code);
				else
				{
					if (!mmu_enabled())
						GenCallRuntime(ngen_LinkBlock_cond_Next_stub);
					else
					{
						Mov(w29, block->NextBlock);
						Str(w29, sh4_context_mem_operand(&next_pc));
						GenBranch(*arm64_no_update);
					}
				}
			}
			break;

		case BET_DynamicJump:
		case BET_DynamicCall:
		case BET_DynamicRet:
			// next_pc = *jdyn;

			Str(w29, sh4_context_mem_operand(&next_pc));
			if (!mmu_enabled())
			{
				// TODO Call no_update instead (and check CpuRunning less frequently?)
				Mov(x2, sizeof(Sh4RCB));
				Sub(x2, x28, x2);
				Add(x2, x2, sizeof(Sh4Context));		// x2 now points to FPCB
#if RAM_SIZE_MAX == 33554432
				Ubfx(w1, w29, 1, 24);
#else
				Ubfx(w1, w29, 1, 23);
#endif
				Ldr(x15, MemOperand(x2, x1, LSL, 3));	// Get block entry point
				Br(x15);
			}
			else
			{
				GenBranch(*arm64_no_update);
			}

			break;

		case BET_DynamicIntr:
		case BET_StaticIntr:
			if (block->BlockType == BET_StaticIntr)
				// next_pc = next_pc_value;
				Mov(w29, block->NextBlock);
			// else next_pc = *jdyn (already in w29)

			Str(w29, sh4_context_mem_operand(&next_pc));

			GenCallRuntime(UpdateINTC);

			Ldr(w29, sh4_context_mem_operand(&next_pc));
			GenBranch(*arm64_no_update);

			break;

		default:
			die("Invalid block end type");
		}

		return GetBuffer()->GetCursorOffset() - start_offset;
	}

	void Finalize(bool rewrite = false)
	{
		Label code_end;
		Bind(&code_end);

		FinalizeCode();

		if (!rewrite)
		{
			block->code = GetBuffer()->GetStartAddress<DynarecCodeEntryPtr>();
			block->host_code_size = GetBuffer()->GetSizeInBytes();
			block->host_opcodes = GetLabelAddress<u32*>(&code_end) - GetBuffer()->GetStartAddress<u32*>();

			emit_Skip(block->host_code_size);
		}

		// Flush and invalidate caches
		vmem_platform_flush_cache(
			CC_RW2RX(GetBuffer()->GetStartAddress<void*>()), CC_RW2RX(GetBuffer()->GetEndAddress<void*>()),
			GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>());

#if 0
//		if (rewrite)
		{
			Instruction* instr_start = (Instruction*)block->code;
//			Instruction* instr_end = GetLabelAddress<Instruction*>(&code_end);
			Instruction* instr_end = (Instruction*)((u8 *)block->code + block->host_code_size);
			Decoder decoder;
			Disassembler disasm;
			decoder.AppendVisitor(&disasm);
			Instruction* instr;
			for (instr = instr_start; instr < instr_end; instr += kInstructionSize) {
				decoder.Decode(instr);
				printf("VIXL\t %p:\t%s\n",
						   reinterpret_cast<void*>(instr),
						   disasm.GetOutput());
			}
		}
#endif
	}

	void GenMainloop()
	{
		Label no_update;
		Label intc_sched;
		Label end_mainloop;

		// void intc_sched()
		arm64_intc_sched = GetCursorAddress<void (*)()>();
		B(&intc_sched);

		// void no_update()
		Bind(&no_update);				// next_pc _MUST_ be on w29

		Ldr(w0, MemOperand(x28, offsetof(Sh4Context, CpuRunning)));
		Cbz(w0, &end_mainloop);
		if (!mmu_enabled())
		{
			Sub(x2, x28, offsetof(Sh4RCB, cntx));
			if (RAM_SIZE == 32 * 1024 * 1024)
				Ubfx(w1, w29, 1, 24);	// 24+1 bits: 32 MB
			else if (RAM_SIZE == 16 * 1024 * 1024)
				Ubfx(w1, w29, 1, 23);	// 23+1 bits: 16 MB
			else
				die("Unsupported RAM_SIZE");
			Ldr(x0, MemOperand(x2, x1, LSL, 3));
		}
		else
		{
			Mov(w0, w29);
			GenCallRuntime(bm_GetCodeByVAddr);
		}
		Br(x0);

		// void mainloop(void *context)
		mainloop = GetCursorAddress<void (*)(void *)>();

		// Save registers
		Stp(x19, x20, MemOperand(sp, -160, PreIndex));
		Stp(x21, x22, MemOperand(sp, 16));
		Stp(x23, x24, MemOperand(sp, 32));
		Stp(x25, x26, MemOperand(sp, 48));
		Stp(x27, x28, MemOperand(sp, 64));
		Stp(s14, s15, MemOperand(sp, 80));
		Stp(vixl::aarch64::s8, s9, MemOperand(sp, 96));
		Stp(s10, s11, MemOperand(sp, 112));
		Stp(s12, s13, MemOperand(sp, 128));
		Stp(x29, x30, MemOperand(sp, 144));

		Sub(x0, x0, sizeof(Sh4Context));
		if (mmu_enabled())
		{
			Ldr(x1, reinterpret_cast<uintptr_t>(&cycle_counter));
			// Push context, cycle_counter address
			Stp(x0, x1, MemOperand(sp, -16, PreIndex));
			Mov(w0, SH4_TIMESLICE);
			Str(w0, MemOperand(x1));

			Ldr(x0, reinterpret_cast<uintptr_t>(jmp_env));
			Ldr(x1, reinterpret_cast<uintptr_t>(&setjmp));
			Blr(x1);

			Ldr(x28, MemOperand(sp));	// Set context
		}
		else
		{
			// Use x28 as sh4 context pointer
			Mov(x28, x0);
			// Use x27 as cycle_counter
			Mov(w27, SH4_TIMESLICE);
		}
		Label do_interrupts;

		// w29 is next_pc
		Ldr(w29, MemOperand(x28, offsetof(Sh4Context, pc)));
		B(&no_update);

		Bind(&intc_sched);

		// Add timeslice to cycle counter
		if (!mmu_enabled())
		{
			Add(w27, w27, SH4_TIMESLICE);
		}
		else
		{
			Ldr(x1, MemOperand(sp, 8));	// &cycle_counter
			Ldr(w0, MemOperand(x1));	// cycle_counter
			Add(w0, w0, SH4_TIMESLICE);
			Str(w0, MemOperand(x1));
		}
		Mov(x29, lr);				// Trashing pc here but it will be reset at the end of the block or in DoInterrupts
		GenCallRuntime(UpdateSystem);
		Mov(lr, x29);
		Cbnz(w0, &do_interrupts);
		Ret();

		Bind(&do_interrupts);
		Mov(x0, x29);
		GenCallRuntime(rdv_DoInterrupts);	// Updates next_pc based on host pc
		Mov(w29, w0);

		B(&no_update);

		Bind(&end_mainloop);
		if (mmu_enabled())
			// Pop context
			Add(sp, sp, 16);
		// Restore registers
		Ldp(x29, x30, MemOperand(sp, 144));
		Ldp(s12, s13, MemOperand(sp, 128));
		Ldp(s10, s11, MemOperand(sp, 112));
		Ldp(vixl::aarch64::s8, s9, MemOperand(sp, 96));
		Ldp(s14, s15, MemOperand(sp, 80));
		Ldp(x27, x28, MemOperand(sp, 64));
		Ldp(x25, x26, MemOperand(sp, 48));
		Ldp(x23, x24, MemOperand(sp, 32));
		Ldp(x21, x22, MemOperand(sp, 16));
		Ldp(x19, x20, MemOperand(sp, 160, PostIndex));
		Ret();

		FinalizeCode();
		emit_Skip(GetBuffer()->GetSizeInBytes());

		arm64_no_update = GetLabelAddress<void (*)()>(&no_update);

		// Flush and invalidate caches
		vmem_platform_flush_cache(
			CC_RW2RX(GetBuffer()->GetStartAddress<void*>()), CC_RW2RX(GetBuffer()->GetEndAddress<void*>()),
			GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>());
	}


private:
	// Runtime branches/calls need to be adjusted if rx space is different to rw space.
	// Therefore can't mix GenBranch with GenBranchRuntime!

	template <typename R, typename... P>
	void GenCallRuntime(R (*function)(P...))
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(function) - reinterpret_cast<uintptr_t>(CC_RW2RX(GetBuffer()->GetStartAddress<void*>()));
		verify(offset >= -128 * 1024 * 1024 && offset <= 128 * 1024 * 1024);
		verify((offset & 3) == 0);
		Label function_label;
		BindToOffset(&function_label, offset);
		Bl(&function_label);
	}

	template <typename R, typename... P>
	void GenCall(R (*function)(P...))
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(function) - GetBuffer()->GetStartAddress<uintptr_t>();
		verify(offset >= -128 * 1024 * 1024 && offset <= 128 * 1024 * 1024);
		verify((offset & 3) == 0);
		Label function_label;
		BindToOffset(&function_label, offset);
		Bl(&function_label);
	}

   template <typename R, typename... P>
	void GenBranchRuntime(R (*target)(P...))
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(target) - reinterpret_cast<uintptr_t>(CC_RW2RX(GetBuffer()->GetStartAddress<void*>()));
		verify(offset >= -128 * 1024 * 1024 && offset <= 128 * 1024 * 1024);
		verify((offset & 3) == 0);
		Label target_label;
		BindToOffset(&target_label, offset);
		B(&target_label);
	}

	template <typename R, typename... P>
	void GenBranch(R (*code)(P...), Condition cond = al)
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(code) - GetBuffer()->GetStartAddress<uintptr_t>();
		verify(offset >= -128 * 1024 * 1024 && offset < 128 * 1024 * 1024);
		verify((offset & 3) == 0);
		Label code_label;
		BindToOffset(&code_label, offset);
		if (cond == al)
			B(&code_label);
		else
			B(&code_label, cond);
	}

	void GenReadMemory(const shil_opcode& op, size_t opid, bool optimise)
	{
		if (GenReadMemoryImmediate(op))
			return;

		GenMemAddr(op, call_regs[0]);

		if (optimise && GenReadMemoryFast(op, opid))
			return;

		GenReadMemorySlow(op);
	}

	bool GenReadMemoryImmediate(const shil_opcode& op)
	{
		if (!op.rs1.is_imm())
			return false;

		u32 size = op.flags & 0x7f;
		u32 addr = op.rs1._imm;
		if (mmu_enabled())
		{
			if ((addr >> 12) != (block->vaddr >> 12))
				// When full mmu is on, only consider addresses in the same 4k page
				return false;
			u32 paddr;
			u32 rv;
			if (size == 2)
				rv = mmu_data_translation<MMU_TT_DREAD, u16>(addr, paddr);
			else if (size == 4)
				rv = mmu_data_translation<MMU_TT_DREAD, u32>(addr, paddr);
			else
				die("Invalid immediate size");
			if (rv != MMU_ERROR_NONE)
				return false;
			addr = paddr;
		}
		bool isram = false;
		void* ptr = _vmem_read_const(addr, isram, size);

		if (isram)
		{
			Ldr(x1, reinterpret_cast<uintptr_t>(ptr));
			if (regalloc.IsAllocAny(op.rd))
			{
				switch (size)
				{
				case 2:
					Ldrsh(regalloc.MapRegister(op.rd), MemOperand(x1));
					break;

				case 4:
					if (op.rd.is_r32f())
						Ldr(regalloc.MapVRegister(op.rd), MemOperand(x1));
					else
						Ldr(regalloc.MapRegister(op.rd), MemOperand(x1));
					break;

				default:
					die("Invalid size");
					break;
				}
			}
			else
			{
				switch (size)
				{
				case 2:
					Ldrsh(w1, MemOperand(x1));
					break;

				case 4:
					Ldr(w1, MemOperand(x1));
					break;

				default:
					die("Invalid size");
					break;
				}
				Str(w1, sh4_context_mem_operand(op.rd.reg_ptr()));
			}
		}
		else
		{
			// Not RAM
			Mov(w0, addr);

			switch(size)
			{
			case 1:
				GenCallRuntime((void (*)())ptr);
				Sxtb(w0, w0);
				break;

			case 2:
				GenCallRuntime((void (*)())ptr);
				Sxth(w0, w0);
				break;

			case 4:
				GenCallRuntime((void (*)())ptr);
				break;

			case 8:
				die("SZ_64F not supported");
				break;
			}

			if (regalloc.IsAllocg(op.rd))
				Mov(regalloc.MapRegister(op.rd), w0);
			else
			{
				verify(regalloc.IsAllocf(op.rd));
				Fmov(regalloc.MapVRegister(op.rd), w0);
			}
		}

		return true;
	}

	bool GenReadMemoryFast(const shil_opcode& op, size_t opid)
	{
		// Direct memory access. Need to handle SIGSEGV and rewrite block as needed. See ngen_Rewrite()
		if (!_nvmem_enabled() || (mmu_enabled() && !vmem32_enabled()))
			return false;
		mem_reads++;

		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		// WARNING: the rewrite code relies on having 1-2 ops before the memory access (3 when mmu is enabled)
		// Update ngen_Rewrite (and perhaps read_memory_rewrite_size) if adding or removing code
		if (!_nvmem_4gb_space())
		{
			Ubfx(x1, *call_regs64[0], 0, 29);
			Add(x1, x1, sizeof(Sh4Context), LeaveFlags);
		}
		else
		{
			Add(x1, *call_regs64[0], sizeof(Sh4Context), LeaveFlags);
			if (mmu_enabled())
			{
				u32 exception_pc = block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0);
				Mov(w27, exception_pc & 0xFFFF);
				Movk(w27, exception_pc >> 16, 16);
			}
		}

		//printf("direct read memory access opid %d pc %p code addr %08x\n", opid, GetCursorAddress<void *>(), this->block->addr);
		this->block->memory_accesses[GetCursorAddress<void *>()] = (u32)opid;

		u32 size = op.flags & 0x7f;
		if (regalloc.IsAllocAny(op.rd))
		{
			switch(size)
			{
			case 1:
				Ldrsb(regalloc.MapRegister(op.rd), MemOperand(x28, x1));
				break;

			case 2:
				Ldrsh(regalloc.MapRegister(op.rd), MemOperand(x28, x1));
				break;

			case 4:
				if (!op.rd.is_r32f())
					Ldr(regalloc.MapRegister(op.rd), MemOperand(x28, x1));
				else
					Ldr(regalloc.MapVRegister(op.rd), MemOperand(x28, x1));
				break;

			case 8:
				Ldr(x1, MemOperand(x28, x1));
				break;
			}

			if (size == 8)
			{
#ifdef EXPLODE_SPANS
				verify(op.rd.count() == 2 && regalloc.IsAllocf(op.rd, 0) && regalloc.IsAllocf(op.rd, 1));
				Fmov(regalloc.MapVRegister(op.rd, 0), w1);
				Lsr(x1, x1, 32);
				Fmov(regalloc.MapVRegister(op.rd, 1), w1);
#else
				Str(x1, sh4_context_mem_operand(op.rd.reg_ptr()));
#endif
			}
		}
		else
		{
			switch(size)
			{
			case 1:
				Ldrsb(w1, MemOperand(x28, x1));
				break;

			case 2:
				Ldrsh(w1, MemOperand(x28, x1));
				break;

			case 4:
				Ldr(w1, MemOperand(x28, x1));
				break;

			case 8:
				Ldr(x1, MemOperand(x28, x1));
				break;
			}
			if (size == 8)
				Str(x1, sh4_context_mem_operand(op.rd.reg_ptr()));
			else
				Str(w1, sh4_context_mem_operand(op.rd.reg_ptr()));
		}
		EnsureCodeSize(start_instruction, read_memory_rewrite_size);

		return true;
	}

	void GenWriteMemory(const shil_opcode& op, size_t opid, bool optimise)
	{
		GenMemAddr(op, call_regs[0]);

		u32 size = op.flags & 0x7f;
		if (size != 8)
			shil_param_to_host_reg(op.rs2, *call_regs[1]);
		else
		{
#ifdef EXPLODE_SPANS
			verify(op.rs2.count() == 2 && regalloc.IsAllocf(op.rs2, 0) && regalloc.IsAllocf(op.rs2, 1));
			Fmov(*call_regs[1], regalloc.MapVRegister(op.rs2, 1));
			Lsl(*call_regs64[1], *call_regs64[1], 32);
			Fmov(w2, regalloc.MapVRegister(op.rs2, 0));
			Orr(*call_regs64[1], *call_regs64[1], x2);
#else
			shil_param_to_host_reg(op.rs2, *call_regs64[1]);
#endif
		}
		if (optimise && GenWriteMemoryFast(op, opid))
			return;

		GenWriteMemorySlow(op);
	}

	bool GenWriteMemoryFast(const shil_opcode& op, size_t opid)
	{
		// Direct memory access. Need to handle SIGSEGV and rewrite block as needed. See ngen_Rewrite()
		if (!_nvmem_enabled() || (mmu_enabled() && !vmem32_enabled()))
			return false;
		mem_writes++;

		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		// WARNING: the rewrite code relies on having 1-2 ops before the memory access (3 when mmu is enabled)
		// Update ngen_Rewrite (and perhaps write_memory_rewrite_size) if adding or removing code
		if (!_nvmem_4gb_space())
		{
			Ubfx(x7, *call_regs64[0], 0, 29);
			Add(x7, x7, sizeof(Sh4Context), LeaveFlags);
		}
		else
		{
			Add(x7, *call_regs64[0], sizeof(Sh4Context), LeaveFlags);
			if (mmu_enabled())
			{
				u32 exception_pc = block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0);
				Mov(w27, exception_pc & 0xFFFF);
				Movk(w27, exception_pc >> 16, 16);
			}
		}

		//printf("direct write memory access opid %d pc %p code addr %08x\n", opid, GetCursorAddress<void *>(), this->block->addr);
		this->block->memory_accesses[GetCursorAddress<void *>()] = (u32)opid;

		u32 size = op.flags & 0x7f;
		switch(size)
		{
		case 1:
			Strb(w1, MemOperand(x28, x7));
			break;

		case 2:
			Strh(w1, MemOperand(x28, x7));
			break;

		case 4:
			Str(w1, MemOperand(x28, x7));
			break;

		case 8:
			Str(x1, MemOperand(x28, x7));
			break;
		}
		EnsureCodeSize(start_instruction, write_memory_rewrite_size);

		return true;
	}

	void EnsureCodeSize(Instruction *start_instruction, int code_size)
	{
		while (GetCursorAddress<Instruction *>() - start_instruction < code_size * kInstructionSize)
			Nop();
		verify (GetCursorAddress<Instruction *>() - start_instruction == code_size * kInstructionSize);
	}

	void CheckBlock(SmcCheckEnum smc_checks, RuntimeBlockInfo* block)
	{

		Label blockcheck_fail;
		Label blockcheck_success;

		if (mmu_enabled())
		{
			Ldr(w10, sh4_context_mem_operand(&next_pc));
			Ldr(w11, block->vaddr);
			Cmp(w10, w11);
			B(ne, &blockcheck_fail);
		}

		switch (smc_checks) {
			case NoCheck:
				return;

			case FastCheck: {
				u8* ptr = GetMemPtr(block->addr, 4);
				if (ptr == NULL)
					return;
				Ldr(x9, reinterpret_cast<uintptr_t>(ptr));
				Ldr(w10, MemOperand(x9));
				Ldr(w11, *(u32*)ptr);
				Cmp(w10, w11);
				B(eq, &blockcheck_success);
			}
			break;

			case FullCheck: {
				s32 sz = block->sh4_code_size;

				u8* ptr = GetMemPtr(block->addr, sz);
				if (ptr == NULL)
					return;

				Ldr(x9, reinterpret_cast<uintptr_t>(ptr));

				while (sz > 0)
				{
					if (sz >= 8)
					{
						Ldr(x10, MemOperand(x9, 8, PostIndex));
						Ldr(x11, *(u64*)ptr);
						Cmp(x10, x11);
						sz -= 8;
						ptr += 8;
					}
					else if (sz >= 4)
					{
						Ldr(w10, MemOperand(x9, 4, PostIndex));
						Ldr(w11, *(u32*)ptr);
						Cmp(w10, w11);
						sz -= 4;
						ptr += 4;
					}
					else
					{
						Ldrh(w10, MemOperand(x9, 2, PostIndex));
						Mov(w11, *(u16*)ptr);
						Cmp(w10, w11);
						sz -= 2;
						ptr += 2;
					}
					B(ne, &blockcheck_fail);
				}
				B(&blockcheck_success);
			}
			break;

			default:
				die("unhandled smc_checks");
		}

		Bind(&blockcheck_fail);
		Ldr(w0, block->addr);
		TailCallRuntime(ngen_blockcheckfail);

		Bind(&blockcheck_success);

		if (mmu_enabled() && block->has_fpu_op)
		{
			Label fpu_enabled;
			Ldr(w10, sh4_context_mem_operand(&sr));
			Tbz(w10, 15, &fpu_enabled);			// test SR.FD bit

			Mov(*call_regs[0], block->vaddr);	// pc
			Mov(*call_regs[1], 0x800);			// event
			Mov(*call_regs[2], 0x100);			// vector
			CallRuntime(Do_Exception);
			Ldr(w29, sh4_context_mem_operand(&next_pc));
			GenBranch(*arm64_no_update);

			Bind(&fpu_enabled);
		}
	}

	void shil_param_to_host_reg(const shil_param& param, const Register& reg)
	{
		if (param.is_imm())
		{
			Mov(reg, param._imm);
		}
		else if (param.is_reg())
		{
			if (param.is_r64f())
				Ldr(reg, sh4_context_mem_operand(param.reg_ptr()));
			else if (param.is_r32f())
			{
				if (regalloc.IsAllocf(param))
					Fmov(reg, regalloc.MapVRegister(param));
				else
					Ldr(reg, sh4_context_mem_operand(param.reg_ptr()));
			}
			else
			{
				if (regalloc.IsAllocg(param))
					Mov(reg, regalloc.MapRegister(param));
				else
					Ldr(reg, sh4_context_mem_operand(param.reg_ptr()));
			}
		}
		else
		{
			verify(param.is_null());
		}
	}

	void host_reg_to_shil_param(const shil_param& param, const CPURegister& reg)
	{
		if (reg.Is64Bits())
		{
			Str((const Register&)reg, sh4_context_mem_operand(param.reg_ptr()));
		}
		else if (regalloc.IsAllocg(param))
		{
			if (reg.IsRegister())
				Mov(regalloc.MapRegister(param), (const Register&)reg);
			else
				Fmov(regalloc.MapRegister(param), (const VRegister&)reg);
		}
		else if (regalloc.IsAllocf(param))
		{
			if (reg.IsVRegister())
				Fmov(regalloc.MapVRegister(param), (const VRegister&)reg);
			else
				Fmov(regalloc.MapVRegister(param), (const Register&)reg);
		}
		else
		{
			Str(reg, sh4_context_mem_operand(param.reg_ptr()));
		}
	}

	struct CC_PS
	{
		CanonicalParamType type;
		shil_param* prm;
	};
	vector<CC_PS> CC_pars;
	std::vector<const WRegister*> call_regs;
	std::vector<const XRegister*> call_regs64;
	std::vector<const VRegister*> call_fregs;
	Arm64RegAlloc regalloc;
	RuntimeBlockInfo* block = NULL;
	const int read_memory_rewrite_size = 5;	// worst case for u64/mmu: add, mov, movk, ldr, str
											// FIXME rewrite size per read/write size?
	const int write_memory_rewrite_size = 4; // TODO only 2 if !mmu & 4gb
};

static Arm64Assembler* compiler;

void ngen_Compile(RuntimeBlockInfo* block, SmcCheckEnum smc_checks, bool reset, bool staging, bool optimise)
{
	verify(emit_FreeSpace() >= 16 * 1024);

	compiler = new Arm64Assembler();

	compiler->ngen_Compile(block, smc_checks, reset, staging, optimise);

	delete compiler;
	compiler = NULL;
}

void ngen_CC_Start(shil_opcode* op)
{
	compiler->ngen_CC_Start(op);
}

void ngen_CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
{
	compiler->ngen_CC_Param(*op, *par, tp);
}

void ngen_CC_Call(shil_opcode*op, void* function)
{
	compiler->ngen_CC_Call(op, function);
}

void ngen_CC_Finish(shil_opcode* op)
{

}

bool ngen_Rewrite(unat& host_pc, unat, unat)
{
	//printf("ngen_Rewrite pc %p\n", host_pc);
	void *host_pc_rw = (void*)CC_RX2RW(host_pc);
	RuntimeBlockInfo *block = bm_GetBlock((void*)host_pc);
	if (block == NULL)
	{
		printf("ngen_Rewrite: Block at %p not found\n", (void *)host_pc);
		return false;
	}
	u32 *code_ptr = (u32*)host_pc_rw;
	auto it = block->memory_accesses.find(code_ptr);
	if (it == block->memory_accesses.end())
	{
		printf("ngen_Rewrite: memory access at %p not found (%lu entries)\n", code_ptr, block->memory_accesses.size());
		return false;
	}
	u32 opid = it->second;
	verify(opid < block->oplist.size());
	const shil_opcode& op = block->oplist[opid];
	// Skip the preceding ops (add, bic, ...)
	u32 *code_rewrite = code_ptr - 1 - (!_nvmem_4gb_space() ? 1 : 0) - (mmu_enabled() ? 2 : 0);
	Arm64Assembler *assembler = new Arm64Assembler(code_rewrite);
	assembler->InitializeRewrite(block, opid);
	if (op.op == shop_readm)
	{
		mem_rewrites_r++;
		assembler->GenReadMemorySlow(op);
	}
	else
	{
		mem_rewrites_w++;
		assembler->GenWriteMemorySlow(op);
	}
	assembler->Finalize(true);
	delete assembler;
	host_pc = (unat)CC_RW2RX(code_rewrite);

	return true;
}

static void generate_mainloop()
{
	if (mainloop != NULL)
		return;
	compiler = new Arm64Assembler();

	compiler->GenMainloop();

	delete compiler;
	compiler = NULL;
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	generate_mainloop();
	return new DynaRBI();
}

void ngen_HandleException()
{
	longjmp(jmp_env, 1);
}

u32 DynaRBI::Relink()
{
	//printf("DynaRBI::Relink %08x\n", this->addr);
	Arm64Assembler *compiler = new Arm64Assembler((u8 *)this->code + this->relink_offset);

	u32 code_size = compiler->RelinkBlock(this);
	compiler->Finalize(true);
	delete compiler;

	return code_size;
}

void Arm64RegAlloc::Preload(u32 reg, eReg nreg)
{
	assembler->Ldr(Register(nreg, 32), assembler->sh4_context_mem_operand(GetRegPtr(reg)));
}
void Arm64RegAlloc::Writeback(u32 reg, eReg nreg)
{
	assembler->Str(Register(nreg, 32), assembler->sh4_context_mem_operand(GetRegPtr(reg)));
}
void Arm64RegAlloc::Preload_FPU(u32 reg, eFReg nreg)
{
	assembler->Ldr(VRegister(nreg, 32), assembler->sh4_context_mem_operand(GetRegPtr(reg)));
}
void Arm64RegAlloc::Writeback_FPU(u32 reg, eFReg nreg)
{
	assembler->Str(VRegister(nreg, 32), assembler->sh4_context_mem_operand(GetRegPtr(reg)));
}


extern "C" void do_sqw_nommu_area_3(u32 dst, u8* sqb)
{
	__asm__
	(
		"and x12, x0, #0x20			\n\t"	// SQ# selection, isolate
		"add x12, x12, x1			\n\t"	// SQ# selection, add to SQ ptr
		"ld2 { v0.2D, v1.2D }, [x12]\n\t"
		"movz x11, #0x0C00, lsl #16 \n\t"
		"add x11, x1, x11			\n\t"	// get ram ptr from x1, part 1
		"ubfx x0, x0, #5, #20		\n\t"	// get ram offset
		"add x11, x11, #512			\n\t"	// get ram ptr from x1, part 2
		"add x11, x11, x0, lsl #5	\n\t"	// ram + offset
		"st2 { v0.2D, v1.2D }, [x11] \n\t"
		"ret						\n"

		: : : "memory"
	);
}
#endif	// FEAT_SHREC == DYNAREC_JIT
