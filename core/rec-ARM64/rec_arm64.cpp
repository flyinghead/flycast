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
#include <sys/mman.h>
#include <map>

#include "deps/vixl/aarch64/macro-assembler-aarch64.h"
using namespace vixl::aarch64;

#define EXPLODE_SPANS

#include "hw/sh4/sh4_opcode_list.h"

#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_rom.h"
#include "arm64_regalloc.h"

#undef do_sqw_nommu

struct DynaRBI : RuntimeBlockInfo
{
	virtual u32 Relink() {
		return 0;
	}

	virtual void Relocate(void* dst) {
		verify(false);
	}
};

// Code borrowed from Dolphin https://github.com/dolphin-emu/dolphin
static void CacheFlush(void* start, void* end)
{
	if (start == end)
		return;

#if HOST_OS == OS_DARWIN
	// Header file says this is equivalent to: sys_icache_invalidate(start, end - start);
	sys_cache_control(kCacheFunctionPrepareForExecution, start, end - start);
#else
	// Don't rely on GCC's __clear_cache implementation, as it caches
	// icache/dcache cache line sizes, that can vary between cores on
	// big.LITTLE architectures.
	u64 addr, ctr_el0;
	static size_t icache_line_size = 0xffff, dcache_line_size = 0xffff;
	size_t isize, dsize;

	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr_el0));
	isize = 4 << ((ctr_el0 >> 0) & 0xf);
	dsize = 4 << ((ctr_el0 >> 16) & 0xf);

	// use the global minimum cache line size
	icache_line_size = isize = icache_line_size < isize ? icache_line_size : isize;
	dcache_line_size = dsize = dcache_line_size < dsize ? dcache_line_size : dsize;

	addr = (u64)start & ~(u64)(dsize - 1);
	for (; addr < (u64)end; addr += dsize)
		// use "civac" instead of "cvau", as this is the suggested workaround for
		// Cortex-A53 errata 819472, 826319, 827319 and 824069.
		__asm__ volatile("dc civac, %0" : : "r"(addr) : "memory");
	__asm__ volatile("dsb ish" : : : "memory");

	addr = (u64)start & ~(u64)(isize - 1);
	for (; addr < (u64)end; addr += isize)
		__asm__ volatile("ic ivau, %0" : : "r"(addr) : "memory");

	__asm__ volatile("dsb ish" : : : "memory");
	__asm__ volatile("isb" : : : "memory");
#endif
}

void ngen_mainloop(void* v_cntx)
{
	Sh4RCB* ctx = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4RCB));

	__asm__ volatile
	(
		"stp x19, x20, [sp, #-144]!	\n\t"
		"stp x21, x22, [sp, #16]	\n\t"
		"stp x23, x24, [sp, #32]	\n\t"
		"stp x25, x26, [sp, #48]	\n\t"
		"stp x27, x28, [sp, #64]	\n\t"
		"stp s8, s9, [sp, #80]		\n\t"
		"stp s10, s11, [sp, #96]	\n\t"
		"stp s12, s13, [sp, #112]	\n\t"
		"stp s14, s15, [sp, #128]	\n\t"
		// Use x28 as sh4 context pointer
		"mov x28, %0				\n\t"
		// Use x27 as cycle_counter
		"mov w27, %[_SH4_TIMESLICE]	\n\t"

	"run_loop:						\n\t"
		"ldr w0, [x28, %[CpuRunning]]	\n\t"
		"cbz w0, end_run_loop		\n\t"

	"slice_loop:					\n\t"
		"ldr w0, [x28, %[pc]]		\n\t"
		"bl bm_GetCode2				\n\t"
		"blr x0						\n\t"
		"cmp w27, #0				\n\t"
		"b.gt slice_loop			\n\t"

		"add w27, w27, %[_SH4_TIMESLICE]	\n\t"
		"bl UpdateSystem_INTC		\n\t"
		"b run_loop					\n\t"

	"end_run_loop:					\n\t"
		"ldp s14, s15, [sp, #128]	\n\t"
		"ldp s12, s13, [sp, #112]	\n\t"
		"ldp s10, s11, [sp, #96]	\n\t"
		"ldp s8, s9, [sp, #80]		\n\t"
		"ldp x27, x28, [sp, #64]	\n\t"
		"ldp x25, x26, [sp, #48]	\n\t"
		"ldp x23, x24, [sp, #32]	\n\t"
		"ldp x21, x22, [sp, #16]	\n\t"
		"ldp x19, x20, [sp], #144	\n\t"
		:
		: [cntx] "r"(reinterpret_cast<uintptr_t>(&ctx->cntx)),
		  [pc] "i"(offsetof(Sh4Context, pc)),
		  [_SH4_TIMESLICE] "i"(SH4_TIMESLICE),
		  [CpuRunning] "i"(offsetof(Sh4Context, CpuRunning))
		: "memory"
	);
}

void ngen_init()
{
}

void ngen_ResetBlocks()
{
}

void ngen_GetFeatures(ngen_features* dst)
{
	dst->InterpreterFallback = false;
	dst->OnlyDynamicEnds = false;
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	return new DynaRBI();
}

static void ngen_blockcheckfail(u32 pc) {
	printf("arm64 JIT: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
}

class Arm64Assembler : public MacroAssembler
{
	typedef void (MacroAssembler::*Arm64Op_RRO)(const Register&, const Register&, const Operand&);
	typedef void (MacroAssembler::*Arm64Op_RROF)(const Register&, const Register&, const Operand&, enum FlagsUpdate);

public:
	Arm64Assembler() : Arm64Assembler(emit_GetCCPtr())
	{
	}
	Arm64Assembler(void *buffer) : MacroAssembler((u8 *)buffer, 64 * 1024), regalloc(this)
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
			Add(*ret_reg, regalloc.MapRegister(op.rs1), op.rs3._imm);
		}
		else if (op.rs3.is_r32i())
		{
			Add(*ret_reg, regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs3));
		}
		else if (!op.rs3.is_null())
		{
			die("invalid rs3");
		}
		else
		{
			if (raddr == NULL)
				ret_reg = &regalloc.MapRegister(op.rs1);
			else
				Mov(*ret_reg, regalloc.MapRegister(op.rs1));
		}

		return *ret_reg;
	}

	void ngen_Compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
	{
		//printf("REC-ARM64 compiling %08x\n", block->addr);
		this->block = block;
		if (force_checks)
			CheckBlock(block);

		// run register allocator
		regalloc.DoAlloc(block);

		// scheduler
		Sub(w27, w27, block->guest_cycles);

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

				GenCallRuntime(OpDesc[op.rs3._imm]->oph);
				break;

			case shop_jcond:
			case shop_jdyn:
				if (op.rs2.is_imm())
				{
					Add(w10, regalloc.MapRegister(op.rs1), op.rs2._imm);
					Mov(regalloc.MapRegister(op.rd), w10);
				}
				else
					Mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
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
				GenReadMemory(op, i);
				break;

			case shop_writem:
				GenWriteMemory(op, i);
				break;

			case shop_sync_sr:
				GenCallRuntime(UpdateSR);
				break;
			case shop_sync_fpscr:
				GenCallRuntime(UpdateFPSCR);
				break;

			case shop_swaplb:
				// TODO Optimize
				Mov(w9, Operand(regalloc.MapRegister(op.rs1), LSR, 16));
				Rev16(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				Bfc(regalloc.MapRegister(op.rd), 16, 16);
				Orr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), Operand(w9, LSL, 16));
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

			case shop_rocr:
				Ubfm(w0, regalloc.MapRegister(op.rs1), 0, 0);										// w0 = rs1[0]
				Mov(regalloc.MapRegister(op.rd), Operand(regalloc.MapRegister(op.rs1), LSR, 1));	// rd = rs1 >> 1
				Orr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), Operand(regalloc.MapRegister(op.rs2), LSL, 31));	// rd |= C << 31
				Mov(regalloc.MapRegister(op.rd2), w0);												// rd2 = w0 (new C)
				break;
			case shop_rocl:
				Tst(regalloc.MapRegister(op.rs1), 0x80000000);	// Z = ~rs1[31]
				Orr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs2), Operand(regalloc.MapRegister(op.rs1), LSL, 1)); // rd = rs1 << 1 | rs2(C)
				Cset(regalloc.MapRegister(op.rd2), ne);			// rd2 = ~Z(C)
				break;

			case shop_shld:
			case shop_shad:
				// TODO optimize
				Cmp(regalloc.MapRegister(op.rs2), 0);
				Csel(w1, regalloc.MapRegister(op.rs2), wzr, ge);	// if shift >= 0 then w1 = shift else w1 = 0
				Mov(w0, wzr);	// wzr not supported by csneg
				Csneg(w2, w0, regalloc.MapRegister(op.rs2), ge);	// if shift < 0 then w2 = -shift else w2 = 0
				Cmp(w2, 32);
				Csel(w2, 31, w2, eq);								// if shift == -32 then w2 = 31
				Lsl(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1), w1);		// Left shift by w1
				if (op.op == shop_shld)													// Right shift by w2
					// Logical shift
					Lsr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), w2);
				else
					// Arithmetic shift
					Asr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), w2);
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
				Mov(w0, regalloc.MapRegister(op.rs1));
				if (op.flags != 0x1337)
				{
					Lsr(w1, regalloc.MapRegister(op.rs1), 26);
					Cmp(w1, 0x38);
				}

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
				SaveFramePointer();
				if (op.flags == 0x1337)
					Blr(x9);
				else
				{
					Label no_branch;
					B(&no_branch, ne);
					Blr(x9);
					Bind(&no_branch);
				}
				break;

			case shop_ext_s8:
				Sxtb(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				break;
			case shop_ext_s16:
				Sxth(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
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

		switch (block->BlockType)
		{

		case BET_StaticJump:
		case BET_StaticCall:
			// next_pc = block->BranchBlock;
			Ldr(w10, block->BranchBlock);
			Str(w10, sh4_context_mem_operand(&next_pc));
			break;

		case BET_Cond_0:
		case BET_Cond_1:
			{
				// next_pc = next_pc_value;
				// if (*jdyn == 0)
				//   next_pc = branch_pc_value;

				Mov(w10, block->NextBlock);

				if (block->has_jcond)
					Ldr(w11, sh4_context_mem_operand(&Sh4cntx.jdyn));
				else
					Ldr(w11, sh4_context_mem_operand(&sr.T));

				Cmp(w11, block->BlockType & 1);
				Label branch_not_taken;

				B(ne, &branch_not_taken);
				Mov(w10, block->BranchBlock);
				Bind(&branch_not_taken);

				Str(w10, sh4_context_mem_operand(&next_pc));
			}
			break;

		case BET_DynamicJump:
		case BET_DynamicCall:
		case BET_DynamicRet:
			// next_pc = *jdyn;
			Ldr(w10, sh4_context_mem_operand(&Sh4cntx.jdyn));
			Str(w10, sh4_context_mem_operand(&next_pc));
			break;

		case BET_DynamicIntr:
		case BET_StaticIntr:
			if (block->BlockType == BET_DynamicIntr)
			{
				// next_pc = *jdyn;
				Ldr(w10, sh4_context_mem_operand(&Sh4cntx.jdyn));
			}
			else
			{
				// next_pc = next_pc_value;
				Mov(w10, block->NextBlock);
			}
			Str(w10, sh4_context_mem_operand(&next_pc));

			GenCallRuntime(UpdateINTC);
			break;

		default:
			die("Invalid block end type");
		}

		if (frame_reg_saved)
			Ldr(x30, MemOperand(sp, 16, PostIndex));
		Ret();

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

		switch (size)
		{
		case 1:
			GenCallRuntime(ReadMem8);
			Sxtb(w0, w0);
			break;

		case 2:
			GenCallRuntime(ReadMem16);
			Sxth(w0, w0);
			break;

		case 4:
			GenCallRuntime(ReadMem32);
			break;

		case 8:
			GenCallRuntime(ReadMem64);
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
		u32 size = op.flags & 0x7f;
		switch (size)
		{
		case 1:
			GenCallRuntime(WriteMem8);
			break;

		case 2:
			GenCallRuntime(WriteMem16);
			break;

		case 4:
			GenCallRuntime(WriteMem32);
			break;

		case 8:
			GenCallRuntime(WriteMem64);
			break;

		default:
			die("1..8 bytes");
			break;
		}
		EnsureCodeSize(start_instruction, write_memory_rewrite_size);
	}

	void InitializeRewrite(RuntimeBlockInfo *block, size_t opid)
	{
		regalloc.DoAlloc(block);
		regalloc.current_opid = opid;
		frame_reg_saved = true;
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
		CacheFlush(GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>());
#if 0
		if (rewrite)
		{
			Instruction* instr_start = GetBuffer()->GetStartAddress<Instruction*>();
			Instruction* instr_end = GetLabelAddress<Instruction*>(&code_end);
//			Instruction* instr_end = (Instruction*)((u8 *)block->code + block->host_code_size);
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

private:
	template <typename R, typename... P>
	void GenCallRuntime(R (*function)(P...))
	{
		SaveFramePointer();
		uintptr_t offset = reinterpret_cast<uintptr_t>(function) - GetBuffer()->GetStartAddress<uintptr_t>();
		Label function_label;
		BindToOffset(&function_label, offset);
		Bl(&function_label);
	}

	void GenReadMemory(const shil_opcode& op, size_t opid)
	{
		u32 size = op.flags & 0x7f;

		if (GenReadMemoryImmediate(op))
			return;

		SaveFramePointer();	// needed if rewritten

		GenMemAddr(op, call_regs[0]);

		if (GenReadMemoryFast(op, opid))
			return;

		GenReadMemorySlow(op);
	}

	bool GenReadMemoryImmediate(const shil_opcode& op)
	{
		if (!op.rs1.is_imm())
			return false;

		u32 size = op.flags & 0x7f;
		bool isram = false;
		void* ptr = _vmem_read_const(op.rs1._imm, isram, size);

		if (isram)
		{
			Ldr(x1, reinterpret_cast<uintptr_t>(ptr));
			switch (size)
			{
			case 2:
				Ldrsh(regalloc.MapRegister(op.rd), MemOperand(x1, xzr, SXTW));
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
			// Not RAM
			Mov(w0, op.rs1._imm);

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
				Fmov(regalloc.MapVRegister(op.rd), w0);
		}

		return true;
	}

	bool GenReadMemoryFast(const shil_opcode& op, size_t opid)
	{
		// Direct memory access. Need to handle SIGSEGV and rewrite block as needed. See ngen_Rewrite()
		if (!_nvmem_enabled())
			return false;

		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		// WARNING: the rewrite code relies on having two ops before the memory access
		// Update ngen_Rewrite (and perhaps read_memory_rewrite_size) if adding or removing code
		Add(w1, *call_regs[0], sizeof(Sh4Context), LeaveFlags);
		Bfc(w1, 29, 3);		// addr &= ~0xE0000000

		//printf("direct read memory access opid %d pc %p code addr %08x\n", opid, GetCursorAddress<void *>(), this->block->addr);
		this->block->memory_accesses[GetCursorAddress<void *>()] = (u32)opid;

		u32 size = op.flags & 0x7f;
		switch(size)
		{
		case 1:
			Ldrsb(regalloc.MapRegister(op.rd), MemOperand(x28, x1, SXTW));
			break;

		case 2:
			Ldrsh(regalloc.MapRegister(op.rd), MemOperand(x28, x1, SXTW));
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
		EnsureCodeSize(start_instruction, read_memory_rewrite_size);

		return true;
	}

	void GenWriteMemory(const shil_opcode& op, size_t opid)
	{
		SaveFramePointer();	// needed if rewritten

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
		if (GenWriteMemoryFast(op, opid))
			return;

		GenWriteMemorySlow(op);
	}

	bool GenWriteMemoryFast(const shil_opcode& op, size_t opid)
	{
		// Direct memory access. Need to handle SIGSEGV and rewrite block as needed. See ngen_Rewrite()
		if (!_nvmem_enabled())
			return false;

		Instruction *start_instruction = GetCursorAddress<Instruction *>();

		// WARNING: the rewrite code relies on having two ops before the memory access
		// Update ngen_Rewrite (and perhaps write_memory_rewrite_size) if adding or removing code
		Add(w7, *call_regs[0], sizeof(Sh4Context), LeaveFlags);
		Bfc(w7, 29, 3);		// addr &= ~0xE0000000

		//printf("direct write memory access opid %d pc %p code addr %08x\n", opid, GetCursorAddress<void *>(), this->block->addr);
		this->block->memory_accesses[GetCursorAddress<void *>()] = (u32)opid;

		u32 size = op.flags & 0x7f;
		switch(size)
		{
		case 1:
			Strb(w1, MemOperand(x28, x7, SXTW));
			break;

		case 2:
			Strh(w1, MemOperand(x28, x7, SXTW));
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

	void CheckBlock(RuntimeBlockInfo* block)
	{
		s32 sz = block->sh4_code_size;

		Label blockcheck_fail;
		Label blockcheck_success;

		u8* ptr = GetMemPtr(block->addr, sz);
		if (ptr == NULL)
			// FIXME Can a block cross a RAM / non-RAM boundary??
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

		Bind(&blockcheck_fail);
		Ldr(w0, block->addr);
		TailCallRuntime(ngen_blockcheckfail);

		Bind(&blockcheck_success);
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
				Fmov(reg, regalloc.MapVRegister(param));
			else
				Mov(reg, regalloc.MapRegister(param));
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
		else
		{
			if (reg.IsVRegister())
				Fmov(regalloc.MapVRegister(param), (const VRegister&)reg);
			else
				Fmov(regalloc.MapVRegister(param), (const Register&)reg);
		}
	}

	void SaveFramePointer()
	{
		if (!frame_reg_saved)
		{
			Str(x30, MemOperand(sp, -16, PreIndex));
			frame_reg_saved = true;
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
	bool frame_reg_saved = false;
	RuntimeBlockInfo* block;
	const int read_memory_rewrite_size = 6;	// worst case for u64: add, bfc, ldr, fmov, lsr, fmov
											// FIXME rewrite size per read/write size?
	const int write_memory_rewrite_size = 3;
};

static Arm64Assembler* compiler;

void ngen_Compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
{
	verify(emit_FreeSpace() >= 16 * 1024);

	compiler = new Arm64Assembler();

	compiler->ngen_Compile(block, force_checks, reset, staging, optimise);

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
	u32 guest_pc = p_sh4rcb->cntx.pc;
	//printf("ngen_Rewrite pc %p code addr %08x\n", host_pc, guest_pc);
	RuntimeBlockInfo *block = bm_GetBlock(guest_pc);
	if (block == NULL)
	{
		printf("ngen_Rewrite: Block at %08x not found\n", guest_pc);
		return false;
	}
	u32 *code_ptr = (u32*)host_pc;
	auto it = block->memory_accesses.find(code_ptr);
	if (it == block->memory_accesses.end())
	{
		printf("ngen_Rewrite: memory access at %p not found (%lu entries)\n", code_ptr, block->memory_accesses.size());
		return false;
	}
	u32 opid = it->second;
	verify(opid < block->oplist.size());
	const shil_opcode& op = block->oplist[opid];
	Arm64Assembler *assembler = new Arm64Assembler(code_ptr - 2);	// Skip the 2 preceding ops (bic, add)
	assembler->InitializeRewrite(block, opid);
	if (op.op == shop_readm)
		assembler->GenReadMemorySlow(op);
	else
		assembler->GenWriteMemorySlow(op);
	assembler->Finalize(true);
	delete assembler;
	host_pc = (unat)(code_ptr - 2);

	return true;
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
	__asm__ volatile
	(
		"movz x11, #0x0C00, lsl #16 \n\t"
		"add x11, x1, x11			\n\t"	// get ram ptr from x1, part 1
		"and x12, x0, #0x20			\n\t"	// SQ# selection, isolate
		"ubfx x0, x0, #5, #20		\n\t"	// get ram offset
		"add x1, x12, x1			\n\t"	// SQ# selection, add to SQ ptr
		"add x11, x11, #512			\n\t"	// get ram ptr from x1, part 2
		"add x11, x11, x0, lsl #5	\n\t"	// ram + offset
		"ld2 { v0.2D, v1.2D }, [x1]	\n\t"
		"st2 { v0.2D, v1.2D }, [x11] \n\t"
		"ret						\n"

		: : : "memory"
	);
}
#endif	// FEAT_SHREC == DYNAREC_JIT
