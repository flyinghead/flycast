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

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM64

#include <unistd.h>
#include <map>

#include <aarch64/macro-assembler-aarch64.h>
using namespace vixl::aarch64;

//#define NO_BLOCK_LINKING

#include "hw/sh4/sh4_opcode_list.h"

#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_rom.h"
#include "arm64_regalloc.h"
#include "hw/mem/_vmem.h"
#include "arm64_unwind.h"

#undef do_sqw_nommu

static void generate_mainloop();

struct DynaRBI : RuntimeBlockInfo
{
	u32 Relink() override;

	void Relocate(void* dst) override {
		verify(false);
	}
};

static u32 cycle_counter;
static u64 jmp_stack;
static Arm64UnwindInfo unwinder;

static void (*mainloop)(void *context);
static void (*handleException)();

struct DynaCode;

static DynaCode *arm64_intc_sched;
static DynaCode *arm64_no_update;
static DynaCode *blockCheckFail;
static DynaCode *linkBlockGenericStub;
static DynaCode *linkBlockBranchStub;
static DynaCode *linkBlockNextStub;
static DynaCode *writeStoreQueue32;
static DynaCode *writeStoreQueue64;

static bool restarting;

void ngen_mainloop(void* v_cntx)
{
	try {
		do {
			restarting = false;
			generate_mainloop();

			mainloop(v_cntx);
			if (restarting)
				p_sh4rcb->cntx.CpuRunning = 1;
		} while (restarting);
	} catch (const SH4ThrownException&) {
		ERROR_LOG(DYNAREC, "SH4ThrownException in mainloop");
		throw FlycastException("Fatal: Unhandled SH4 exception");
	}
}

#ifdef TARGET_IPHONE
static void JITWriteProtect(bool enable)
{
    if (enable)
        mem_region_set_exec(CodeCache, CODE_SIZE + TEMP_CODE_SIZE);
    else
        mem_region_unlock(CodeCache, CODE_SIZE + TEMP_CODE_SIZE);
}
#endif

void ngen_init()
{
	INFO_LOG(DYNAREC, "Initializing the ARM64 dynarec");
}

void ngen_ResetBlocks()
{
	unwinder.clear();
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

class Arm64Assembler : public MacroAssembler
{
	typedef void (MacroAssembler::*Arm64Op_RRO)(const Register&, const Register&, const Operand&);
	typedef void (MacroAssembler::*Arm64Op_RROF)(const Register&, const Register&, const Operand&, enum FlagsUpdate);
	typedef void (MacroAssembler::*Arm64Fop_RRR)(const VRegister&, const VRegister&, const VRegister&);

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

	void ngen_BinaryFop(shil_opcode* op, Arm64Fop_RRR arm_op)
	{
		VRegister reg1;
		VRegister reg2;
		if (op->rs1.is_imm())
		{
			Fmov(s0, reinterpret_cast<f32&>(op->rs1._imm));
			reg1 = s0;
		}
		else
		{
			reg1 = regalloc.MapVRegister(op->rs1);
		}
		if (op->rs2.is_imm())
		{
			Fmov(s1, reinterpret_cast<f32&>(op->rs2._imm));
			reg2 = s1;
		}
		else
		{
			reg2 = regalloc.MapVRegister(op->rs2);
		}
		((*this).*arm_op)(regalloc.MapVRegister(op->rd), reg1, reg2);
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

	void ngen_Compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
	{
		//printf("REC-ARM64 compiling %08x\n", block->addr);
		JITWriteProtect(false);
		this->block = block;
		CheckBlock(force_checks, block);
		
		// run register allocator
		regalloc.DoAlloc(block);

		// scheduler
		if (mmu_enabled())
		{
			Ldr(w0, sh4_context_mem_operand(&Sh4cntx.cycle_counter));
			Subs(w0, w0, block->guest_cycles);
			Str(w0, sh4_context_mem_operand(&Sh4cntx.cycle_counter));
		}
		else
		{
			Subs(w27, w27, block->guest_cycles);
		}
		Label cycles_remaining;
		B(&cycles_remaining, pl);
		GenCall(arm64_intc_sched);
		Label cpu_running;
		Cbnz(w0, &cpu_running);
		Mov(w29, block->vaddr);
		Str(w29, sh4_context_mem_operand(&next_pc));
		GenBranch(arm64_no_update);
		Bind(&cpu_running);
		Bind(&cycles_remaining);

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
				Mov(w0, op.rs3._imm);

				if (!mmu_enabled())
				{
					GenCallRuntime(OpDesc[op.rs3._imm]->oph);
				}
				else
				{
					Mov(x1, reinterpret_cast<uintptr_t>(*OpDesc[op.rs3._imm]->oph));	// op handler
					Mov(w2, block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

					GenCallRuntime(interpreter_fallback);
				}

				break;

			case shop_jcond:
			case shop_jdyn:
				{
					const Register rd = regalloc.MapRegister(op.rd);
					if (op.rs2.is_imm())
						Add(rd, regalloc.MapRegister(op.rs1), op.rs2._imm);
					else
						Mov(rd, regalloc.MapRegister(op.rs1));
					// Save it for the branching at the end of the block
					Mov(w29, rd);
				}
				break;

			case shop_mov32:
				verify(op.rd.is_reg());
				verify(op.rs1.is_reg() || op.rs1.is_imm());

				if (regalloc.IsAllocf(op.rd))
				{
					const VRegister rd = regalloc.MapVRegister(op.rd);
					if (op.rs1.is_imm())
						Fmov(rd, reinterpret_cast<f32&>(op.rs1._imm));
					else if (regalloc.IsAllocf(op.rs1))
						Fmov(rd, regalloc.MapVRegister(op.rs1));
					else
						Fmov(rd, regalloc.MapRegister(op.rs1));
				}
				else
				{
					const Register rd = regalloc.MapRegister(op.rd);
					if (op.rs1.is_imm())
						Mov(rd, op.rs1._imm);
					else if (regalloc.IsAllocg(op.rs1))
						Mov(rd, regalloc.MapRegister(op.rs1));
					else
						Fmov(rd, regalloc.MapVRegister(op.rs1));
				}
				break;

			case shop_mov64:
				verify(op.rd.is_reg());
				verify(op.rs1.is_reg() || op.rs1.is_imm());

				shil_param_to_host_reg(op.rs1, x15);
				host_reg_to_shil_param(op.rd, x15);
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
				{
					const Register rs1 = regalloc.MapRegister(op.rs1);
					const Register rd = regalloc.MapRegister(op.rd);
					Mov(w9, Operand(rs1, LSR, 16));
					Rev16(rd, rs1);
					Bfi(rd, w9, 16, 16);
				}
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
				{
					Register reg1;
					Operand op2;
					Register reg3;
					if (op.rs1.is_imm())
					{
						Mov(w0, op.rs1.imm_value());
						reg1 = w0;
					}
					else
					{
						reg1 = regalloc.MapRegister(op.rs1);
					}
					if (op.rs2.is_imm())
						op2 = Operand(op.rs2.imm_value());
					else
						op2 = regalloc.MapRegister(op.rs2);
					if (op.rs3.is_imm())
					{
						Mov(w1, op.rs3.imm_value());
						reg3 = w1;
					}
					else
					{
						reg3 = regalloc.MapRegister(op.rs3);
					}
					Cmp(reg3, 1);	// C = rs3
					Adcs(regalloc.MapRegister(op.rd), reg1, op2); // (C,rd)=rs1+rs2+rs3(C)
					Cset(regalloc.MapRegister(op.rd2), cs);	// rd2 = C
				}
				break;
			case shop_sbc:
				{
					Register reg1;
					Operand op2;
					Operand op3;
					if (op.rs1.is_imm())
					{
						Mov(w0, op.rs1.imm_value());
						reg1 = w0;
					}
					else
					{
						reg1 = regalloc.MapRegister(op.rs1);
					}
					if (op.rs2.is_imm())
						op2 = Operand(op.rs2.imm_value());
					else
						op2 = regalloc.MapRegister(op.rs2);
					if (op.rs3.is_imm())
						op3 = Operand(op.rs3.imm_value());
					else
						op3 = regalloc.MapRegister(op.rs3);
					Cmp(wzr, op3);	// C = ~rs3
					Sbcs(regalloc.MapRegister(op.rd), reg1, op2); // (C,rd) = rs1 - rs2 - ~rs3(C)
					Cset(regalloc.MapRegister(op.rd2), cc);	// rd2 = ~C
				}
				break;
			case shop_negc:
				{
					Operand op1;
					Operand op2;
					if (op.rs1.is_imm())
						op1 = Operand(op.rs1.imm_value());
					else
						op1 = regalloc.MapRegister(op.rs1);
					if (op.rs2.is_imm())
						op2 = Operand(op.rs2.imm_value());
					else
						op2 = regalloc.MapRegister(op.rs2);
					Cmp(wzr, op2);	// C = ~rs2
					Sbcs(regalloc.MapRegister(op.rd), wzr, op1);	// (C,rd) = 0 - rs1 - ~rs2(C)
					Cset(regalloc.MapRegister(op.rd2), cc);			// rd2 = ~C
				}
				break;

			case shop_rocr:
				{
					Register reg1;
					Register reg2;
					if (op.rs1.is_imm())
					{
						Mov(w1, op.rs1.imm_value());
						reg1 = w1;
					}
					else
					{
						reg1 = regalloc.MapRegister(op.rs1);
					}
					if (op.rs2.is_imm())
					{
						Mov(w2, op.rs2.imm_value());
						reg2 = w2;
					}
					else
					{
						reg2 = regalloc.MapRegister(op.rs2);
					}
					Ubfx(w0, reg1, 0, 1);										// w0 = rs1[0] (new C)
					const Register rd = regalloc.MapRegister(op.rd);
					Mov(rd, Operand(reg1, LSR, 1));	// rd = rs1 >> 1
					Bfi(rd, reg2, 31, 1);				// rd |= C << 31
					Mov(regalloc.MapRegister(op.rd2), w0);						// rd2 = w0 (new C)
				}
				break;
			case shop_rocl:
				{
					Register reg1;
					Register reg2;
					if (op.rs1.is_imm())
					{
						Mov(w0, op.rs1.imm_value());
						reg1 = w0;
					}
					else
					{
						reg1 = regalloc.MapRegister(op.rs1);
					}
					if (op.rs2.is_imm())
					{
						Mov(w1, op.rs2.imm_value());
						reg2 = w1;
					}
					else
					{
						reg2 = regalloc.MapRegister(op.rs2);
					}
					Tst(reg1, 0x80000000);						// Z = ~rs1[31]
					Orr(regalloc.MapRegister(op.rd), reg2, Operand(reg1, LSL, 1)); // rd = rs1 << 1 | rs2(C)
					Cset(regalloc.MapRegister(op.rd2), ne);		// rd2 = ~Z(C)
				}
				break;

			case shop_shld:
			case shop_shad:
				{
					Register reg1;
					if (op.rs1.is_imm())
					{
						Mov(w0, op.rs1.imm_value());
						reg1 = w0;
					}
					else
					{
						reg1 = regalloc.MapRegister(op.rs1);
					}
					Label positive_shift, negative_shift, end;
					const Register rs2 = regalloc.MapRegister(op.rs2);
					Tbz(rs2, 31, &positive_shift);
					Cmn(rs2, 32);
					B(&negative_shift, ne);
					const Register rd = regalloc.MapRegister(op.rd);
					// rs2 == -32 => rd = 0 (logical) or 0/-1 (arith)
					if (op.op == shop_shld)
						// Logical shift
						//Lsr(rd, reg1, 31);
						Mov(rd, wzr);
					else
						// Arithmetic shift
						Asr(rd, reg1, 31);
					B(&end);

					Bind(&positive_shift);
					// rs2 >= 0 => left shift
					Lsl(rd, reg1, rs2);
					B(&end);

					Bind(&negative_shift);
					// rs2 < 0 => right shift
					Neg(w1, rs2);
					if (op.op == shop_shld)
						// Logical shift
						Lsr(rd, reg1, w1);
					else
						// Arithmetic shift
						Asr(rd, reg1, w1);
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
					const Register rs1 = regalloc.MapRegister(op.rs1);
					if (op.op == shop_test)
					{
						if (op.rs2.is_imm())
							Tst(rs1, op.rs2._imm);
						else
							Tst(rs1, regalloc.MapRegister(op.rs2));
					}
					else
					{
						if (op.rs2.is_imm())
							Cmp(rs1, op.rs2._imm);
						else
							Cmp(rs1, regalloc.MapRegister(op.rs2));
					}

					static const Condition shop_conditions[] = { eq, eq, ge, gt, hs, hi };

					Cset(regalloc.MapRegister(op.rd), shop_conditions[op.op - shop_test]);
				}
				break;
			case shop_setpeq:
				{
					Register reg1;
					Register reg2;
					if (op.rs1.is_imm())
					{
						Mov(w0, op.rs1.imm_value());
						reg1 = w0;
					}
					else
					{
						reg1 = regalloc.MapRegister(op.rs1);
					}
					if (op.rs2.is_imm())
					{
						Mov(w1, op.rs2.imm_value());
						reg2 = w1;
					}
					else
					{
						reg2 = regalloc.MapRegister(op.rs2);
					}
					Eor(w1, reg1, reg2);
					const Register rd = regalloc.MapRegister(op.rd);
					Mov(rd, wzr);
					Mov(w2, wzr);	// wzr not supported by csinc (?!)
					Tst(w1, 0xFF000000);
					Csinc(rd, rd, w2, ne);
					Tst(w1, 0x00FF0000);
					Csinc(rd, rd, w2, ne);
					Tst(w1, 0x0000FF00);
					Csinc(rd, rd, w2, ne);
					Tst(w1, 0x000000FF);
					Csinc(rd, rd, w2, ne);
				}
				break;

			case shop_mul_u16:
				{
					Register reg2;
					if (op.rs2.is_imm())
					{
						Mov(w0, op.rs2.imm_value());
						reg2 = w0;
					}
					else
					{
						reg2 = regalloc.MapRegister(op.rs2);
					}
					Uxth(w10, regalloc.MapRegister(op.rs1));
					Uxth(w11, reg2);
					Mul(regalloc.MapRegister(op.rd), w10, w11);
				}
				break;
			case shop_mul_s16:
				{
					Register reg2;
					if (op.rs2.is_imm())
					{
						Mov(w0, op.rs2.imm_value());
						reg2 = w0;
					}
					else
					{
						reg2 = regalloc.MapRegister(op.rs2);
					}
					Sxth(w10, regalloc.MapRegister(op.rs1));
					Sxth(w11, reg2);
					Mul(regalloc.MapRegister(op.rd), w10, w11);
				}
				break;
			case shop_mul_i32:
				{
					Register reg2;
					if (op.rs2.is_imm())
					{
						Mov(w0, op.rs2.imm_value());
						reg2 = w0;
					}
					else
					{
						reg2 = regalloc.MapRegister(op.rs2);
					}
					Mul(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), reg2);
				}
				break;
			case shop_mul_u64:
			case shop_mul_s64:
				{
					Register reg2;
					if (op.rs2.is_imm())
					{
						Mov(w0, op.rs2.imm_value());
						reg2 = w0;
					}
					else
					{
						reg2 = regalloc.MapRegister(op.rs2);
					}
					const Register& rd_xreg = Register::GetXRegFromCode(regalloc.MapRegister(op.rd).GetCode());
					if (op.op == shop_mul_u64)
						Umull(rd_xreg, regalloc.MapRegister(op.rs1), reg2);
					else
						Smull(rd_xreg, regalloc.MapRegister(op.rs1), reg2);
					const Register& rd2_xreg = Register::GetXRegFromCode(regalloc.MapRegister(op.rd2).GetCode());
					Lsr(rd2_xreg, rd_xreg, 32);
				}
				break;

			case shop_pref:
				{
					Label not_sqw;
					if (op.rs1.is_imm())
						Mov(w0, op.rs1._imm);
					else
					{
						if (regalloc.IsAllocg(op.rs1))
							Lsr(w1, regalloc.MapRegister(op.rs1), 26);
						else
						{
							Ldr(w0, sh4_context_mem_operand(op.rs1.reg_ptr()));
							Lsr(w1, w0, 26);
						}
						Cmp(w1, 0x38);
						B(&not_sqw, ne);
						if (regalloc.IsAllocg(op.rs1))
							Mov(w0, regalloc.MapRegister(op.rs1));
					}

					if (mmu_enabled())
					{
						Mov(w1, block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

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
				{
					const Register rd = regalloc.MapRegister(op.rd);
					const Register rs1 = regalloc.MapRegister(op.rs1);
					const Register rs2 = regalloc.MapRegister(op.rs2);
					if (op.rs1._reg == op.rd._reg)
					{
						verify(op.rs2._reg != op.rd._reg);
						Lsr(rd, rs1, 16);
						Lsl(w0, rs2, 16);
					}
					else
					{
						Lsl(rd, rs2, 16);
						Lsr(w0, rs1, 16);
					}
					Orr(rd, rd, w0);
				}
				break;

			//
			// FPU
			//

			case shop_fadd:
				ngen_BinaryFop(&op, &MacroAssembler::Fadd);
				break;
			case shop_fsub:
				ngen_BinaryFop(&op, &MacroAssembler::Fsub);
				break;
			case shop_fmul:
				ngen_BinaryFop(&op, &MacroAssembler::Fmul);
				break;
			case shop_fdiv:
				ngen_BinaryFop(&op, &MacroAssembler::Fdiv);
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
				if (op.rs1.is_reg())
					Add(x1, x1, Operand(regalloc.MapRegister(op.rs1), UXTH, 3));
				else
					Add(x1, x1, Operand(op.rs1.imm_value() << 3));
				Ldr(x2, MemOperand(x1));
				Str(x2, sh4_context_mem_operand(op.rd.reg_ptr()));
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
		JITWriteProtect(true);
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
			verify(fregused < (int)call_fregs.size() && regused < (int)call_regs.size());
			shil_param& prm = *CC_pars[i].prm;
			switch (CC_pars[i].type)
			{
			// push the params

			case CPT_u32:
				shil_param_to_host_reg(prm, *call_regs[regused++]);

				break;

			case CPT_f32:
				if (prm.is_reg())
					Fmov(*call_fregs[fregused], regalloc.MapVRegister(prm));
				else if (prm.is_imm())
					Fmov(*call_fregs[fregused], reinterpret_cast<f32&>(prm._imm));
				else
					verify(prm.is_null());
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

	void GenReadMemorySlow(u32 size)
	{
		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		switch (size)
		{
		case 1:
			GenCallRuntime(_vmem_ReadMem8);
			Sxtb(w0, w0);
			break;

		case 2:
			GenCallRuntime(_vmem_ReadMem16);
			Sxth(w0, w0);
			break;

		case 4:
			GenCallRuntime(_vmem_ReadMem32);
			break;

		case 8:
			GenCallRuntime(_vmem_ReadMem64);
			break;

		default:
			die("1..8 bytes");
			break;
		}
		EnsureCodeSize(start_instruction, read_memory_rewrite_size);
	}

	void GenWriteMemorySlow(u32 size)
	{
		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		switch (size)
		{
		case 1:
			GenCallRuntime(_vmem_WriteMem8);
			break;

		case 2:
			GenCallRuntime(_vmem_WriteMem16);
			break;

		case 4:
			GenCallRuntime(_vmem_WriteMem32);
			break;

		case 8:
			GenCallRuntime(_vmem_WriteMem64);
			break;

		default:
			die("1..8 bytes");
			break;
		}
		EnsureCodeSize(start_instruction, write_memory_rewrite_size);
	}

	u32 RelinkBlock(RuntimeBlockInfo *block)
	{
		ptrdiff_t start_offset = GetBuffer()->GetCursorOffset();

		switch (block->BlockType)
		{

		case BET_StaticJump:
		case BET_StaticCall:
			// next_pc = block->BranchBlock;
#ifndef NO_BLOCK_LINKING
			if (block->pBranchBlock != NULL)
				GenBranch((DynaCode *)block->pBranchBlock->code);
			else
			{
				if (!mmu_enabled())
					GenCall(linkBlockGenericStub);
				else
#else
			{
#endif
				{
					Mov(w29, block->BranchBlock);
					Str(w29, sh4_context_mem_operand(&next_pc));
					GenBranch(arm64_no_update);
				}
			}
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
#ifndef NO_BLOCK_LINKING
				if (block->pBranchBlock != NULL)
					GenBranch((DynaCode *)block->pBranchBlock->code);
				else
				{
					if (!mmu_enabled())
						GenCall(linkBlockBranchStub);
					else
#else
				{
#endif
					{
						Mov(w29, block->BranchBlock);
						Str(w29, sh4_context_mem_operand(&next_pc));
						GenBranch(arm64_no_update);
					}
				}

				Bind(&branch_not_taken);

#ifndef NO_BLOCK_LINKING
				if (block->pNextBlock != NULL)
					GenBranch((DynaCode *)block->pNextBlock->code);
				else
				{
					if (!mmu_enabled())
						GenCall(linkBlockNextStub);
					else
#else
				{
#endif
					{
						Mov(w29, block->NextBlock);
						Str(w29, sh4_context_mem_operand(&next_pc));
						GenBranch(arm64_no_update);
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
				Sub(x2, x28, offsetof(Sh4RCB, cntx));
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
				GenBranch(arm64_no_update);
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
			GenBranch(arm64_no_update);

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
		if (rewrite && block != NULL)
		{
			INFO_LOG(DYNAREC, "BLOCK %08x", block->vaddr);
			Instruction* instr_start = (Instruction*)block->code;
//			Instruction* instr_end = GetLabelAddress<Instruction*>(&code_end);
			Instruction* instr_end = (Instruction*)((u8 *)block->code + block->host_code_size);
			Decoder decoder;
			Disassembler disasm;
			decoder.AppendVisitor(&disasm);
			Instruction* instr;
			for (instr = instr_start; instr < instr_end; instr += kInstructionSize) {
				decoder.Decode(instr);
				INFO_LOG(DYNAREC, "VIXL  %p:  %s",
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

		// int intc_sched()
		arm64_intc_sched = GetCursorAddress<DynaCode *>();
		verify((void *)arm64_intc_sched == (void *)CodeCache);
		B(&intc_sched);

		// Not yet compiled block stub
		// WARNING: this function must be at a fixed address, or transitioning to mmu will fail (switch)
		ngen_FailedToFindBlock = (void (*)())CC_RW2RX(GetCursorAddress<uintptr_t>());
		if (mmu_enabled())
		{
			GenCallRuntime(rdv_FailedToFindBlock_pc);
		}
		else
		{
			Mov(w0, w29);
			GenCallRuntime(rdv_FailedToFindBlock);
		}
		Br(x0);

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
		mainloop = (void (*)(void *))CC_RW2RX(GetCursorAddress<uintptr_t>());
		// For stack unwinding purposes, we pretend that the entire code block is just one function, with the same
		// unwinding instructions everywhere. This isn't true until the end of the following prolog, but exceptions
		// can only be thrown by called functions so this is good enough.
		unwinder.start(CodeCache);

		// Save registers
		Stp(x19, x20, MemOperand(sp, -160, PreIndex));
		unwinder.allocStack(0, 160);
		unwinder.saveReg(0, x19, 160);
		unwinder.saveReg(0, x20, 152);
		Stp(x21, x22, MemOperand(sp, 16));
		unwinder.saveReg(0, x21, 144);
		unwinder.saveReg(0, x22, 136);
		Stp(x23, x24, MemOperand(sp, 32));
		unwinder.saveReg(0, x23, 128);
		unwinder.saveReg(0, x24, 120);
		Stp(x25, x26, MemOperand(sp, 48));
		unwinder.saveReg(0, x25, 112);
		unwinder.saveReg(0, x26, 104);
		Stp(x27, x28, MemOperand(sp, 64));
		unwinder.saveReg(0, x27, 96);
		unwinder.saveReg(0, x28, 88);
		Stp(d14, d15, MemOperand(sp, 80));
		unwinder.saveReg(0, d14, 80);
		unwinder.saveReg(0, d15, 72);
		Stp(d8, d9, MemOperand(sp, 96));
		unwinder.saveReg(0, d8, 64);
		unwinder.saveReg(0, d9, 56);
		Stp(d10, d11, MemOperand(sp, 112));
		unwinder.saveReg(0, d10, 48);
		unwinder.saveReg(0, d11, 40);
		Stp(d12, d13, MemOperand(sp, 128));
		unwinder.saveReg(0, d12, 32);
		unwinder.saveReg(0, d13, 24);
		Stp(x29, x30, MemOperand(sp, 144));
		unwinder.saveReg(0, x29, 16);
		unwinder.saveReg(0, x30, 8);

		Sub(x0, x0, sizeof(Sh4Context));
		Label reenterLabel;
		if (mmu_enabled())
		{
			// Push context
			Stp(x0, x1, MemOperand(sp, -16, PreIndex));
			unwinder.allocStack(0, 16);

			Ldr(x0, reinterpret_cast<uintptr_t>(&jmp_stack));
			Mov(x1, sp);
			Str(x1, MemOperand(x0));

			Bind(&reenterLabel);
			Ldr(x28, MemOperand(sp));	// Set context
			Mov(x27, reinterpret_cast<uintptr_t>(mmuAddressLUT));
		}
		else
		{
			// Use x28 as sh4 context pointer
			Mov(x28, x0);
			// Use x27 as cycle_counter
			Ldr(w27, sh4_context_mem_operand(&Sh4cntx.cycle_counter));
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
			Ldr(w0, sh4_context_mem_operand(&Sh4cntx.cycle_counter));
			Add(w0, w0, SH4_TIMESLICE);
			Str(w0, sh4_context_mem_operand(&Sh4cntx.cycle_counter));
		}
		Mov(x29, lr);				// Trashing pc here but it will be reset at the end of the block or in DoInterrupts
		GenCallRuntime(UpdateSystem);
		Mov(lr, x29);
		Cbnz(w0, &do_interrupts);
		Ldr(w0, MemOperand(x28, offsetof(Sh4Context, CpuRunning)));
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
		else
			// save cycle counter
			Str(w27, sh4_context_mem_operand(&Sh4cntx.cycle_counter));
		// Restore registers
		Ldp(x29, x30, MemOperand(sp, 144));
		Ldp(d12, d13, MemOperand(sp, 128));
		Ldp(d10, d11, MemOperand(sp, 112));
		Ldp(d8, d9, MemOperand(sp, 96));
		Ldp(d14, d15, MemOperand(sp, 80));
		Ldp(x27, x28, MemOperand(sp, 64));
		Ldp(x25, x26, MemOperand(sp, 48));
		Ldp(x23, x24, MemOperand(sp, 32));
		Ldp(x21, x22, MemOperand(sp, 16));
		Ldp(x19, x20, MemOperand(sp, 160, PostIndex));
		Ret();

		// Exception handler
		Label handleExceptionLabel;
		Bind(&handleExceptionLabel);
		if (mmu_enabled())
		{
			Ldr(x0, reinterpret_cast<uintptr_t>(&jmp_stack));
			Ldr(x1, MemOperand(x0));
			Mov(sp, x1);
			B(&reenterLabel);
		}

		// Block check fail
		blockCheckFail = GetCursorAddress<DynaCode *>();
		GenCallRuntime(rdv_BlockCheckFail);
		if (mmu_enabled())
		{
			Label jumpblockLabel;
			Cbnz(x0, &jumpblockLabel);
			Ldr(w0, MemOperand(x28, offsetof(Sh4Context, pc)));
			GenCallRuntime(bm_GetCodeByVAddr);
			Bind(&jumpblockLabel);
		}
		Br(x0);

		// Block linking stubs
		linkBlockBranchStub = GetCursorAddress<DynaCode *>();
		Label linkBlockShared;
		Mov(w1, 1);
		B(&linkBlockShared);

		linkBlockNextStub = GetCursorAddress<DynaCode *>();
		Mov(w1, 0);
		B(&linkBlockShared);

		linkBlockGenericStub = GetCursorAddress<DynaCode *>();
		Mov(w1, w29);	// djump/pc -> in case we need it ..

		Bind(&linkBlockShared);
		Sub(x0, lr, 4);	// go before the call
		GenCallRuntime(rdv_LinkBlock);	// returns an RX addr
		Br(x0);

		// Store Queue write handlers
		Label writeStoreQueue32Label;
		Bind(&writeStoreQueue32Label);
		Lsr(x7, x0, 26);
		Cmp(x7, 0x38);
		GenBranchRuntime(_vmem_WriteMem32, Condition::ne);
		And(x0, x0, 0x3f);
		Sub(x7, x0, sizeof(Sh4RCB::sq_buffer), LeaveFlags);
		Str(w1, MemOperand(x28, x7));
		Ret();

		Label writeStoreQueue64Label;
		Bind(&writeStoreQueue64Label);
		Lsr(x7, x0, 26);
		Cmp(x7, 0x38);
		GenBranchRuntime(_vmem_WriteMem64, Condition::ne);
		And(x0, x0, 0x3f);
		Sub(x7, x0, sizeof(Sh4RCB::sq_buffer), LeaveFlags);
		Str(x1, MemOperand(x28, x7));
		Ret();

		FinalizeCode();
		emit_Skip(GetBuffer()->GetSizeInBytes());

		size_t unwindSize = unwinder.end(CODE_SIZE - 128, (ptrdiff_t)CC_RW2RX(0));
		verify(unwindSize <= 128);

		arm64_no_update = GetLabelAddress<DynaCode *>(&no_update);
		handleException = (void (*)())CC_RW2RX(GetLabelAddress<uintptr_t>(&handleExceptionLabel));
		writeStoreQueue32 = GetLabelAddress<DynaCode *>(&writeStoreQueue32Label);
		writeStoreQueue64 = GetLabelAddress<DynaCode *>(&writeStoreQueue64Label);

		// Flush and invalidate caches
		vmem_platform_flush_cache(
			CC_RW2RX(GetBuffer()->GetStartAddress<void*>()), CC_RW2RX(GetBuffer()->GetEndAddress<void*>()),
			GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>());
	}

	void GenWriteStoreQueue(u32 size)
	{
		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		if (size == 4)
			GenCall(writeStoreQueue32);
		else
			GenCall(writeStoreQueue64);
		EnsureCodeSize(start_instruction, write_memory_rewrite_size);
	}

private:
	// Runtime branches/calls need to be adjusted if rx space is different to rw space.
	// Therefore can't mix GenBranch with GenBranchRuntime!

	template <typename R, typename... P>
	void GenCallRuntime(R (*function)(P...))
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(function) - reinterpret_cast<uintptr_t>(CC_RW2RX(GetBuffer()->GetStartAddress<void*>()));
		verify((offset & 3) == 0);
		if (offset < -128 * 1024 * 1024 || offset > 128 * 1024 * 1024)
		{
			Mov(x4, reinterpret_cast<uintptr_t>(function));
			Blr(x4);
		}
		else
		{
			Label function_label;
			BindToOffset(&function_label, offset);
			Bl(&function_label);
		}
	}

	void GenCall(DynaCode *function)
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(function) - GetBuffer()->GetStartAddress<uintptr_t>();
		verify(offset >= -128 * 1024 * 1024 && offset <= 128 * 1024 * 1024);
		verify((offset & 3) == 0);
		Label function_label;
		BindToOffset(&function_label, offset);
		Bl(&function_label);
	}

   template <typename R, typename... P>
	void GenBranchRuntime(R (*target)(P...), Condition cond = al)
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(target) - reinterpret_cast<uintptr_t>(CC_RW2RX(GetBuffer()->GetStartAddress<void*>()));
		verify((offset & 3) == 0);
		if (offset < -128 * 1024 * 1024 || offset > 128 * 1024 * 1024)
		{
			if (cond == al)
			{
				Mov(x4, reinterpret_cast<uintptr_t>(target));
				Br(x4);
			}
			else
			{
				Label skip_target;
				Condition inverse_cond = (Condition)((u32)cond ^ 1);
				
				B(&skip_target, inverse_cond);
				Mov(x4, reinterpret_cast<uintptr_t>(target));
				Br(x4);
				Bind(&skip_target);
			}
		}
		else
		{
			Label target_label;
			BindToOffset(&target_label, offset);
			if (cond == al)
				B(&target_label);
			else
				B(&target_label, cond);
		}
	}

	void GenBranch(DynaCode *code, Condition cond = al)
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

	void genMmuLookup(const shil_opcode& op, u32 write)
	{
		if (mmu_enabled())
		{
			Label inCache;
			Label done;

			Lsr(w1, w0, 12);
			Ldr(w1, MemOperand(x27, x1, LSL, 2));
			Cbnz(w1, &inCache);
			Mov(w1, write);
			Mov(w2, block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0));	// pc
			GenCallRuntime(mmuDynarecLookup);
			B(&done);
			Bind(&inCache);
			And(w0, w0, 0xFFF);
			Orr(w0, w0, w1);
			Bind(&done);
		}
	}

	void GenReadMemory(const shil_opcode& op, size_t opid, bool optimise)
	{
		if (GenReadMemoryImmediate(op))
			return;

		GenMemAddr(op, &w0);
		genMmuLookup(op, 0);

		u32 size = op.flags & 0x7f;
		if (!optimise || !GenReadMemoryFast(op, opid))
			GenReadMemorySlow(size);

		if (size < 8)
			host_reg_to_shil_param(op.rd, w0);
		else
			Str(x0, sh4_context_mem_operand(op.rd.reg_ptr()));
	}

	bool GenReadMemoryImmediate(const shil_opcode& op)
	{
		if (!op.rs1.is_imm())
			return false;

		u32 size = op.flags & 0x7f;
		u32 addr = op.rs1._imm;
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
		bool isram = false;
		void* ptr = _vmem_read_const(addr, isram, size > 4 ? 4 : size);

		if (isram)
		{
			Ldr(x1, reinterpret_cast<uintptr_t>(ptr));	// faster than Mov
			if (regalloc.IsAllocAny(op.rd))
			{
				switch (size)
				{
				case 1:
					Ldrsb(regalloc.MapRegister(op.rd), MemOperand(x1));
					break;

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
				case 1:
					Ldrsb(w1, MemOperand(x1));
					break;

				case 2:
					Ldrsh(w1, MemOperand(x1));
					break;

				case 4:
					Ldr(w1, MemOperand(x1));
					break;

				case 8:
					Ldr(x1, MemOperand(x1));
					break;

				default:
					die("Invalid size");
					break;
				}
				if (size == 8)
					Str(x1, sh4_context_mem_operand(op.rd.reg_ptr()));
				else
					Str(w1, sh4_context_mem_operand(op.rd.reg_ptr()));
			}
		}
		else
		{
			// Not RAM
			if (size == 8)
			{
				verify(!regalloc.IsAllocAny(op.rd));
				// Need to call the handler twice
				Mov(w0, addr);
				GenCallRuntime((void (*)())ptr);
				Str(w0, sh4_context_mem_operand(op.rd.reg_ptr()));

				Mov(w0, addr + 4);
				GenCallRuntime((void (*)())ptr);
				Str(w0, sh4_context_mem_operand((u8*)op.rd.reg_ptr() + 4));
			}
			else
			{
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

				default:
					die("Invalid size");
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
		}

		return true;
	}

	bool GenReadMemoryFast(const shil_opcode& op, size_t opid)
	{
		// Direct memory access. Need to handle SIGSEGV and rewrite block as needed. See ngen_Rewrite()
		if (!_nvmem_enabled())
			return false;

		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		// WARNING: the rewrite code relies on having 1 or 2 ops before the memory access
		// Update ngen_Rewrite (and perhaps read_memory_rewrite_size) if adding or removing code
		if (!_nvmem_4gb_space())
		{
			Ubfx(x1, x0, 0, 29);
			Add(x1, x1, sizeof(Sh4Context), LeaveFlags);
		}
		else
		{
			Add(x1, x0, sizeof(Sh4Context), LeaveFlags);
		}

		u32 size = op.flags & 0x7f;
		switch(size)
		{
		case 1:
			Ldrsb(w0, MemOperand(x28, x1));
			break;

		case 2:
			Ldrsh(w0, MemOperand(x28, x1));
			break;

		case 4:
			Ldr(w0, MemOperand(x28, x1));
			break;

		case 8:
			Ldr(x0, MemOperand(x28, x1));
			break;
		}
		EnsureCodeSize(start_instruction, read_memory_rewrite_size);

		return true;
	}

	void GenWriteMemory(const shil_opcode& op, size_t opid, bool optimise)
	{
		if (GenWriteMemoryImmediate(op))
			return;

		GenMemAddr(op, &w0);
		genMmuLookup(op, 1);

		u32 size = op.flags & 0x7f;
		if (size != 8)
			shil_param_to_host_reg(op.rs2, w1);
		else
			shil_param_to_host_reg(op.rs2, x1);
		if (optimise && GenWriteMemoryFast(op, opid))
			return;

		GenWriteMemorySlow(size);
	}

	bool GenWriteMemoryImmediate(const shil_opcode& op)
	{
		if (!op.rs1.is_imm())
			return false;

		u32 size = op.flags & 0x7f;
		u32 addr = op.rs1._imm;
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
		bool isram = false;
		void* ptr = _vmem_write_const(addr, isram, size > 4 ? 4 : size);

		Register reg2;
		if (size != 8)
		{
			if (op.rs2.is_imm())
			{
				Mov(w1, op.rs2._imm);
				reg2 = w1;
			}
			else if (regalloc.IsAllocg(op.rs2))
			{
				reg2 = regalloc.MapRegister(op.rs2);
			}
			else if (regalloc.IsAllocf(op.rs2))
			{
				Fmov(w1, regalloc.MapVRegister(op.rs2));
				reg2 = w1;
			}
			else
				die("Invalid rs2 param");
		}
		if (isram)
		{
			Ldr(x0, reinterpret_cast<uintptr_t>(ptr));
			switch (size)
			{
			case 1:
				Strb(reg2, MemOperand(x0));
				break;

			case 2:
				Strh(reg2, MemOperand(x0));
				break;

			case 4:
				Str(reg2, MemOperand(x0));
				break;

			case 8:
				shil_param_to_host_reg(op.rs2, x1);
				Str(x1, MemOperand(x0));
				break;

			default:
				die("Invalid size");
				break;
			}
		}
		else
		{
			// Not RAM
			Mov(w0, addr);
			if (size == 8)
			{
				// Need to call the handler twice
				shil_param_to_host_reg(op.rs2, x1);
				GenCallRuntime((void (*)())ptr);

				Mov(w0, addr + 4);
				shil_param_to_host_reg(op.rs2, x1);
				Lsr(x1, x1, 32);
				GenCallRuntime((void (*)())ptr);
			}
			else
			{
				Mov(w1, reg2);
				GenCallRuntime((void (*)())ptr);
			}
		}

		return true;
	}

	bool GenWriteMemoryFast(const shil_opcode& op, size_t opid)
	{
		// Direct memory access. Need to handle SIGSEGV and rewrite block as needed. See ngen_Rewrite()
		if (!_nvmem_enabled())
			return false;

		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		// WARNING: the rewrite code relies on having 1 or 2 ops before the memory access
		// Update ngen_Rewrite (and perhaps write_memory_rewrite_size) if adding or removing code
		if (!_nvmem_4gb_space())
		{
			Ubfx(x7, x0, 0, 29);
			Add(x7, x7, sizeof(Sh4Context), LeaveFlags);
		}
		else
		{
			Add(x7, x0, sizeof(Sh4Context), LeaveFlags);
		}

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

	void CheckBlock(bool force_checks, RuntimeBlockInfo* block)
	{
		if (!mmu_enabled() && !force_checks)
			return;

		Label blockcheck_fail;

		if (mmu_enabled())
		{
			Ldr(w10, sh4_context_mem_operand(&next_pc));
			Ldr(w11, block->vaddr);
			Cmp(w10, w11);
			B(ne, &blockcheck_fail);
		}
		if (force_checks)
		{
			s32 sz = block->sh4_code_size;
			u8* ptr = GetMemPtr(block->addr, sz);
			if (ptr != NULL)
			{
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
			}
		}
		Label blockcheck_success;
		B(&blockcheck_success);
		Bind(&blockcheck_fail);
		Mov(w0, block->addr);
		GenBranch(blockCheckFail);

		Bind(&blockcheck_success);

		if (mmu_enabled() && block->has_fpu_op)
		{
			Label fpu_enabled;
			Ldr(w10, sh4_context_mem_operand(&sr));
			Tbz(w10, 15, &fpu_enabled);			// test SR.FD bit

			Mov(w0, block->vaddr);	// pc
			Mov(w1, 0x800);			// event
			Mov(w2, 0x100);			// vector
			CallRuntime(Do_Exception);
			Ldr(w29, sh4_context_mem_operand(&next_pc));
			GenBranch(arm64_no_update);

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
	std::vector<CC_PS> CC_pars;
	std::vector<const WRegister*> call_regs;
	std::vector<const XRegister*> call_regs64;
	std::vector<const VRegister*> call_fregs;
	Arm64RegAlloc regalloc;
	RuntimeBlockInfo* block = NULL;
	const int read_memory_rewrite_size = 5;	// ubfx, add, ldr
	const int write_memory_rewrite_size = 5; // ubfx, add, str
};

static Arm64Assembler* compiler;

void ngen_Compile(RuntimeBlockInfo* block, bool smc_checks, bool reset, bool staging, bool optimise)
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

#define STR_LDR_MASK   0xFFE0EC00

static const u32 armv8_mem_ops[] = {
		0x38E06800,		// Ldrsb
		0x78E06800,		// Ldrsh
		0xB8606800,		// Ldr w
		0xF8606800,		// Ldr x
		0x38206800,		// Strb
		0x78206800,		// Strh
		0xB8206800,		// Str w
		0xF8206800,		// Str x
};
static const bool read_ops[] = {
		true,
		true,
		true,
		true,
		false,
		false,
		false,
		false,
};
static const u32 op_sizes[] = {
		1,
		2,
		4,
		8,
		1,
		2,
		4,
		8,
};
bool ngen_Rewrite(host_context_t &context, void *faultAddress)
{
	JITWriteProtect(false);
	//LOGI("ngen_Rewrite pc %zx\n", context.pc);
	u32 *code_ptr = (u32 *)CC_RX2RW(context.pc);
	u32 armv8_op = *code_ptr;
	bool is_read = false;
	u32 size = 0;
	bool found = false;
	u32 masked = armv8_op & STR_LDR_MASK;
	for (u32 i = 0; i < ARRAY_SIZE(armv8_mem_ops); i++)
	{
		if (masked == armv8_mem_ops[i])
		{
			size = op_sizes[i];
			is_read = read_ops[i];
			found = true;
			break;
		}
	}
	verify(found);

	// Skip the preceding ops (add, ubfx)
	u32 *code_rewrite = code_ptr - 1 - (!_nvmem_4gb_space() ? 1 : 0);
	Arm64Assembler *assembler = new Arm64Assembler(code_rewrite);
	if (is_read)
		assembler->GenReadMemorySlow(size);
	else if (!is_read && size >= 4 && (((u8 *)faultAddress - virt_ram_base) >> 26) == 0x38)
		assembler->GenWriteStoreQueue(size);
	else
		assembler->GenWriteMemorySlow(size);
	assembler->Finalize(true);
	delete assembler;
	context.pc = (unat)CC_RW2RX(code_rewrite);
	JITWriteProtect(true);

	return true;
}

static void generate_mainloop()
{
	if (mainloop != nullptr)
		return;
	JITWriteProtect(false);
	compiler = new Arm64Assembler();

	compiler->GenMainloop();

	delete compiler;
	compiler = nullptr;
	JITWriteProtect(true);
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	generate_mainloop();
	return new DynaRBI();
}

void ngen_HandleException(host_context_t &context)
{
	context.pc = (uintptr_t)handleException;
}

u32 DynaRBI::Relink()
{
#ifndef NO_BLOCK_LINKING
	//printf("DynaRBI::Relink %08x\n", this->addr);
	JITWriteProtect(false);
	Arm64Assembler *compiler = new Arm64Assembler((u8 *)this->code + this->relink_offset);

	u32 code_size = compiler->RelinkBlock(this);
	compiler->Finalize(true);
	delete compiler;
	JITWriteProtect(true);

	return code_size;
#else
	return 0;
#endif
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
#endif	// FEAT_SHREC == DYNAREC_JIT
