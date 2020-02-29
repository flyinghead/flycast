/*
	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "build.h"

#if	HOST_CPU == CPU_ARM64 && FEAT_AREC != DYNAREC_NONE

#include <sstream>
#include "arm7_rec.h"
#include "hw/mem/_vmem.h"
#include "deps/vixl/aarch64/macro-assembler-aarch64.h"
using namespace vixl::aarch64;
//#include "deps/vixl/aarch32/disasm-aarch32.h"

extern "C" void arm_dispatch();
extern "C" void arm_exit();

extern u8* icPtr;
extern u8* ICache;
extern const u32 ICacheSize;
extern reg_pair arm_Reg[RN_ARM_REG_COUNT];

class Arm7Compiler;

#define MAX_REGS 7

class AArch64ArmRegAlloc : public ArmRegAlloc<MAX_REGS, AArch64ArmRegAlloc>
{
	Arm7Compiler& assembler;

	void LoadReg(int host_reg, Arm7Reg armreg);
	void StoreReg(int host_reg, Arm7Reg armreg);

	static const WRegister& getReg(int i)
	{
		static const WRegister regs[] = {
				w19, w20, w21, w22, w23, w24, w25
		};
		static_assert(MAX_REGS == ARRAY_SIZE(regs), "MAX_REGS == ARRAY_SIZE(regs)");
		verify(i >= 0 && (u32)i < ARRAY_SIZE(regs));
		return regs[i];
	}

public:
	AArch64ArmRegAlloc(Arm7Compiler& assembler, const std::vector<ArmOp>& block_ops)
		: ArmRegAlloc<MAX_REGS, AArch64ArmRegAlloc>(block_ops), assembler(assembler) {}

	const WRegister& map(Arm7Reg r)
	{
		int i = ArmRegAlloc<MAX_REGS, AArch64ArmRegAlloc>::map(r);
		return getReg(i);
	}

	friend class ArmRegAlloc<MAX_REGS, AArch64ArmRegAlloc>;
};

static MemOperand arm_reg_operand(Arm7Reg reg)
{
	return MemOperand(x28, (u8*)&arm_Reg[reg].I - (u8*)&arm_Reg[0].I);
}

class Arm7Compiler : public MacroAssembler
{
	bool logical_op_set_flags = false;
	bool set_carry_bit = false;
	bool set_flags = false;
	AArch64ArmRegAlloc *regalloc = nullptr;

	static const u32 N_FLAG = 1 << 31;
	static const u32 Z_FLAG = 1 << 30;
	static const u32 C_FLAG = 1 << 29;
	static const u32 V_FLAG = 1 << 28;

	Label *startConditional(ArmOp::Condition cc)
	{
		if (cc == ArmOp::AL)
			return nullptr;
		Label *label = new Label();
		verify(cc <= ArmOp::LE);
		Condition condition = (Condition)((u32)cc ^ 1);
		B(label, condition);

		return label;
	}

	void endConditional(Label *label)
	{
		if (label != nullptr)
		{
			Bind(label);
			delete label;
		}
	}

	void call(void *loc)
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(loc) - GetBuffer()->GetStartAddress<uintptr_t>();
		Label function_label;
		BindToOffset(&function_label, offset);
		Bl(&function_label);
	}

	Operand getOperand(ArmOp::Operand arg, const Register& scratch_reg)
	{
		Register rm;
		if (arg.isNone())
			return Operand();
		else if (arg.isImmediate())
		{
			if (!arg.isShifted())
				return Operand(arg.getImmediate());

			Mov(scratch_reg, arg.getImmediate());
			rm = scratch_reg;
		}
		else if (arg.isReg())
		{
			rm = regalloc->map(arg.getReg().armreg);
		}
		Operand rv;
		if (!arg.shift_imm)
		{
			// Shift by register
			const Register& shift_reg = regalloc->map(arg.shift_reg.armreg);

			switch (arg.shift_type)
			{
			case ArmOp::LSL:
			case ArmOp::LSR:
				verify(!scratch_reg.Is(w0));
				Mrs(x0, NZCV);
				Cmp(shift_reg, 32);
				if (arg.shift_type == ArmOp::LSL)
					Lsl(scratch_reg, rm, shift_reg);
				else
					Lsr(scratch_reg, rm, shift_reg);
				Csel(scratch_reg, 0, scratch_reg, ge);			// LSL and LSR by 32 or more gives 0
				Msr(NZCV, x0);
				break;
			case ArmOp::ASR:
				verify(!scratch_reg.Is(w0));
				Mrs(x0, NZCV);
				Cmp(shift_reg, 32);
				Sbfx(w13, rm, 31, 1);
				Asr(scratch_reg, rm, shift_reg);
				Csel(scratch_reg, w13, scratch_reg, ge);		// ASR by 32 or more gives 0 or -1 depending on operand sign
				Msr(NZCV, x0);
				break;
			case ArmOp::ROR:
				Ror(scratch_reg, rm, shift_reg);
				break;
			default:
				die("Invalid shift");
				break;
			}
			rv = Operand(scratch_reg);
		}
		else
		{
			// Shift by immediate
			if (arg.shift_type != ArmOp::ROR && arg.shift_value != 0 && !logical_op_set_flags)
			{
				rv = Operand(rm, (Shift)arg.shift_type, arg.shift_value);
			}
			else if (arg.shift_value == 0)
			{
				if (arg.shift_type == ArmOp::LSL)
				{
					rv = Operand(rm);		// LSL 0 is a no-op
				}
				else
				{
					// Shift by 32
					if (logical_op_set_flags)
						set_carry_bit = true;
					if (arg.shift_type == ArmOp::LSR)
					{
						if (logical_op_set_flags)
							Ubfx(w14, rm, 31, 1);				// w14 = rm[31]
						Mov(scratch_reg, 0);					// scratch = 0
					}
					else if (arg.shift_type == ArmOp::ASR)
					{
						if (logical_op_set_flags)
							Ubfx(w14, rm, 31, 1);				// w14 = rm[31]
						Sbfx(scratch_reg, rm, 31, 1);			// scratch = rm < 0 ? -1 : 0
					}
					else if (arg.shift_type == ArmOp::ROR)
					{
						// RRX
						Cset(w14, cs);							// w14 = C
						if (logical_op_set_flags)
							Mov(w13, rm);						// save rm in case rm = scratch_reg
						Mov(scratch_reg, Operand(rm, LSR, 1));	// scratch = rm >> 1
						Bfi(scratch_reg, w14, 31, 1);			// scratch[31] = C
						if (logical_op_set_flags)
							Ubfx(w14, w13, 0, 1);				// w14 = rm[0] (new C)
					}
					else
						die("Invalid shift");
					rv = Operand(scratch_reg);
				}
			}
			else
			{
				// Carry must be preserved or Ror shift
				if (logical_op_set_flags)
					set_carry_bit = true;
				if (arg.shift_type == ArmOp::LSL)
				{
					Ubfx(w14, rm, 32 - arg.shift_value, 1);		// w14 = rm[lsb]
					Lsl(scratch_reg, rm, arg.shift_value);		// scratch <<= shift
				}
				else
				{
					if (logical_op_set_flags)
						Ubfx(w14, rm, arg.shift_value - 1, 1);	// w14 = rm[msb]

					if (arg.shift_type == ArmOp::LSR)
						Lsr(scratch_reg, rm, arg.shift_value);	// scratch >>= shift
					else if (arg.shift_type == ArmOp::ASR)
						Asr(scratch_reg, rm, arg.shift_value);
					else if (arg.shift_type == ArmOp::ROR)
						Ror(scratch_reg, rm, arg.shift_value);
					else
						die("Invalid shift");
				}
				rv = Operand(scratch_reg);
			}
		}
		return rv;
	}

	const Register getRegister(const Register& scratch_reg, const Operand& op)
	{
		if (op.IsImmediate())
		{
			Mov(scratch_reg, op.GetImmediate());
			return scratch_reg;
		}
		else if (op.IsPlainRegister())
			return op.GetRegister();

		switch (op.GetShift())
		{
		case LSL:
			Lsl(scratch_reg, op.GetRegister(), op.GetShiftAmount());
			break;
		case LSR:
			Lsr(scratch_reg, op.GetRegister(), op.GetShiftAmount());
			break;
		case ASR:
			Asr(scratch_reg, op.GetRegister(), op.GetShiftAmount());
			break;
		case ROR:
			Ror(scratch_reg, op.GetRegister(), op.GetShiftAmount());
			break;
		default:
			die("Invalid shift");
			break;
		}
		return scratch_reg;
	}

	void loadFlags()
	{
		//Load flags
		Ldr(w0, arm_reg_operand(RN_PSR_FLAGS));
		//move them to flags register
		Msr(NZCV, x0);
	}

	void storeFlags()
	{
		if (!set_flags)
			return;

		//get results from flags register
		Mrs(x1, NZCV);
		//Store flags
		Str(w1, arm_reg_operand(RN_PSR_FLAGS));
	}

	void emitDataProcOp(const ArmOp& op)
	{
		Operand arg0 = getOperand(op.arg[0], w1);
		Register rn;
		Operand op2;
		if (op.op_type != ArmOp::MOV && op.op_type != ArmOp::MVN)
		{
			rn = getRegister(w1, arg0);
			if (!rn.Is(w1))
			{
				Mov(w1, rn);
				rn = w1;
			}
			op2 = getOperand(op.arg[1], w2);
		}
		else
			op2 = arg0;
		WRegister rd;
		if (op.rd.isReg())
			rd = regalloc->map(op.rd.getReg().armreg);

		if (logical_op_set_flags)
		{
			// When an Operand2 constant is used with the instructions MOVS, MVNS, ANDS, ORRS, ORNS, EORS, BICS, TEQ or TST,
			// the carry flag is updated to bit[31] of the constant,
			// if the constant is greater than 255 and can be produced by shifting an 8-bit value.
			if (op.arg[0].isImmediate() && op.arg[0].getImmediate() > 255)
			{
				set_carry_bit = true;
				Mov(w14, (op.arg[0].getImmediate() & 0x80000000) >> 31);
			}
			else if (op.arg[1].isImmediate() && op.arg[1].getImmediate() > 255)
			{
				set_carry_bit = true;
				Mov(w14, (op.arg[1].getImmediate() & 0x80000000) >> 31);
			}
			else if (!set_carry_bit)
			{
				// Logical ops should only affect the carry bit based on the op2 shift
				// Here we're not shifting so the carry bit should be preserved
				set_carry_bit = true;
				Cset(w14, cs);
			}
		}

		switch (op.op_type)
		{
		case ArmOp::AND:		// AND
			if (set_flags)
				Ands(rd, rn, op2);
			else
				And(rd, rn, op2);
			break;
		case 1:		// EOR
			Eor(rd, rn, op2);
			if (set_flags)
				Tst(rd, rd);
			break;
		case 2:		// SUB
			if (set_flags)
				Subs(rd, rn, op2);
			else
				Sub(rd, rn, op2);
			break;
		case 3:		// RSB
			Neg(w0, rn);
			if (set_flags)
				Adds(rd, w0, op2);
			else
				Add(rd, w0, op2);
			break;
		case 4:		// ADD
			if (set_flags)
				Adds(rd, rn, op2);
			else
				Add(rd, rn, op2);
			break;
		case 12:	// ORR
			Orr(rd, rn, op2);
			if (set_flags)
				Tst(rd, rd);
			break;
		case 14:	// BIC
			if (set_flags)
				Bics(rd, rn, op2);
			else
				Bic(rd, rn, op2);
			break;
		case 5:		// ADC
			if (set_flags)
				Adcs(rd, rn, op2);
			else
				Adc(rd, rn, op2);
			break;
		case 6:		// SBC
			if (set_flags)
				Sbcs(rd, rn, op2);
			else
				Sbc(rd, rn, op2);
			break;
		case 7:		// RSC
			Ngc(w0, rn);
			if (set_flags)
				Adds(rd, w0, op2);
			else
				Add(rd, w0, op2);
			break;
		case 8:		// TST
			Tst(rn, op2);
			break;
		case 9:		// TEQ
			Eor(w0, rn, op2);
			Tst(w0, w0);
			break;
		case 10:	// CMP
			Cmp(rn, op2);
			break;
		case 11:	// CMN
			Cmn(rn, op2);
			break;
		case 13:	// MOV
			Mov(rd, op2);
			if (set_flags)
				Tst(rd, rd);
			break;
		case 15:	// MVN
			Mvn(rd, op2);
			if (set_flags)
				Tst(rd, rd);
			break;
		default:
			die("invalid op");
			break;
		}
		if (set_carry_bit)
		{
			Mrs(x0, NZCV);
			Bfi(x0, x14, 29, 1);		// C is bit 29 in NZCV
			Msr(NZCV, x0);
		}
	}

	void emitMemOp(const ArmOp& op)
	{
		Operand arg0 = getOperand(op.arg[0], w2);
		Register addr_reg = getRegister(w2, arg0);
		if (!w2.Is(addr_reg))
			Mov(w2, addr_reg);
		if (op.pre_index)
		{
			const ArmOp::Operand& offset = op.arg[1];
			Operand arg1 = getOperand(offset, w1);
			if (!arg1.IsImmediate())
			{
				Register offset_reg = getRegister(w1, arg1);
				if (op.add_offset)
					Add(w2, w2, offset_reg);
				else
					Sub(w2, w2, offset_reg);
			}
			else if (offset.isImmediate() && offset.getImmediate() != 0)
			{
				if (op.add_offset)
					Add(w2, w2, offset.getImmediate());
				else
					Sub(w2, w2, offset.getImmediate());
			}
		}
		Mov(w0, w2);
		if (op.op_type == ArmOp::STR)
		{
			if (op.arg[2].isImmediate())
				Mov(w1, op.arg[2].getImmediate());
			else
				Mov(w1, regalloc->map(op.arg[2].getReg().armreg));
		}

		call(arm7rec_getMemOp(op.op_type == ArmOp::LDR, op.byte_xfer));

		if (op.op_type == ArmOp::LDR)
			Mov(regalloc->map(op.rd.getReg().armreg), w0);
	}

	void emitBranch(const ArmOp& op)
	{
		if (op.arg[0].isImmediate())
			Mov(w0, op.arg[0].getImmediate());
		else
		{
			Mov(w0, regalloc->map(op.arg[0].getReg().armreg));
			And(w0, w0, 0xfffffffc);
		}
		Str(w0, arm_reg_operand(R15_ARM_NEXT));
	}

	void emitMRS(const ArmOp& op)
	{
		call((void*)CPUUpdateCPSR);

		if (op.spsr)
			Ldr(regalloc->map(op.rd.getReg().armreg), arm_reg_operand(RN_SPSR));
		else
			Ldr(regalloc->map(op.rd.getReg().armreg), arm_reg_operand(RN_CPSR));
	}

	void emitMSR(const ArmOp& op)
	{
		if (op.arg[0].isImmediate())
			Mov(w0, op.arg[0].getImmediate());
		else
			Mov(w0, regalloc->map(op.arg[0].getReg().armreg));
		if (op.spsr)
			call((void*)MSR_do<1>);
		else
			call((void*)MSR_do<0>);
	}

	void emitFallback(const ArmOp& op)
	{
		set_flags = false;
		Mov(w0, op.arg[0].getImmediate());
		call((void*)arm_single_op);
		Subs(w27, w27, w0);
	}

public:
	Arm7Compiler() : MacroAssembler(icPtr, ICache + ICacheSize - icPtr) {}

	void compile(const std::vector<ArmOp> block_ops, u32 cycles)
	{
		regalloc = new AArch64ArmRegAlloc(*this, block_ops);

		for (u32 i = 0; i < block_ops.size(); i++)
		{
			const ArmOp& op = block_ops[i];
			DEBUG_LOG(AICA_ARM, "-> %s\n", op.toString().c_str());

			set_flags = op.flags & ArmOp::OP_SETS_FLAGS;
			logical_op_set_flags = op.isLogicalOp() && set_flags;
			set_carry_bit = false;
			bool save_v_flag = true;	// FIXME is this needed?

			Label *condLabel = nullptr;

			if (op.flags & (ArmOp::OP_READS_FLAGS|ArmOp::OP_SETS_FLAGS))	// TODO OP_READS_FLAGS needed?
				loadFlags();

			if (op.op_type != ArmOp::FALLBACK)
				condLabel = startConditional(op.condition);

			regalloc->load(i);

			if (op.op_type <= ArmOp::MVN)
				// data processing op
				emitDataProcOp(op);
			else if (op.op_type <= ArmOp::STR)
				// memory load/store
				emitMemOp(op);
			else if (op.op_type <= ArmOp::BL)
				// branch
				emitBranch(op);
			else if (op.op_type == ArmOp::MRS)
				emitMRS(op);
			else if (op.op_type == ArmOp::MSR)
				emitMSR(op);
			else if (op.op_type == ArmOp::FALLBACK)
				emitFallback(op);
			else
				die("invalid");

			storeFlags();

			regalloc->store(i);

			endConditional(condLabel);
		}

		//pop registers & return
		Subs(w27, w27, cycles);
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(arm_exit) - GetBuffer()->GetStartAddress<uintptr_t>();
		Label arm_exit_label;
		BindToOffset(&arm_exit_label, offset);
		B(&arm_exit_label, mi);

		offset = reinterpret_cast<uintptr_t>(arm_dispatch) - GetBuffer()->GetStartAddress<uintptr_t>();
		Label arm_dispatch_label;
		BindToOffset(&arm_dispatch_label, offset);
		B(&arm_dispatch_label);

		FinalizeCode();
		verify(GetBuffer()->GetCursorOffset() <= GetBuffer()->GetCapacity());
		vmem_platform_flush_cache(
				GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>(),
				GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>());
		icPtr += GetBuffer()->GetSizeInBytes();

#if 0
		Instruction* instr_start = (Instruction *)codestart;
		Instruction* instr_end = GetBuffer()->GetEndAddress<Instruction*>();
		Decoder decoder;
		Disassembler disasm;
		decoder.AppendVisitor(&disasm);
		Instruction* instr;
		for (instr = instr_start; instr < instr_end; instr += kInstructionSize) {
			decoder.Decode(instr);
			DEBUG_LOG(AICA_ARM, "arm64 arec\t %p:\t%s",
					   reinterpret_cast<void*>(instr),
					   disasm.GetOutput());
		}
#endif
		delete regalloc;
		regalloc = nullptr;
	};
};

void AArch64ArmRegAlloc::LoadReg(int host_reg, Arm7Reg armreg)
{
	// printf("LoadReg W%d <- r%d\n", host_reg, armreg);
	assembler.Ldr(getReg(host_reg), arm_reg_operand(armreg));
}

void AArch64ArmRegAlloc::StoreReg(int host_reg, Arm7Reg armreg)
{
	// printf("StoreReg W%d -> r%d\n", host_reg, armreg);
	assembler.Str(getReg(host_reg), arm_reg_operand(armreg));
}

void arm7backend_compile(const std::vector<ArmOp> block_ops, u32 cycles)
{
	Arm7Compiler assembler;
	assembler.compile(block_ops, cycles);
}

//
// Dynarec main loop
//
// w25 is used for temp mem save (post increment op2)
// x26 is the entry points table
// w27 is the cycle counter
// x28 points to the arm7 registers base
__asm__ (
		".globl arm_compilecode				\n\t"
		".hidden arm_compilecode			\n"
	"arm_compilecode:						\n\t"
		"bl CompileCode						\n\t"
		"b arm_dispatch						\n\t"

		".globl arm_mainloop				\n\t"
		".hidden arm_mainloop				\n"
	"arm_mainloop:							\n\t"	//  arm_mainloop(cycles, regs, entry points)
		"stp x25, x26, [sp, #-96]!			\n\t"
		"stp x27, x28, [sp, #16]			\n\t"
		"stp x29, x30, [sp, #32]			\n\t"
		"stp x19, x20, [sp, #48]			\n\t"
		"stp x21, x22, [sp, #64]			\n\t"
		"stp x23, x24, [sp, #80]			\n\t"

		"mov x28, x1						\n\t"	// arm7 registers
		"mov x26, x2						\n\t"	// lookup base

		"ldr w27, [x28, #192]				\n\t"	// cycle count
		"add w27, w27, w0					\n\t"	// add cycles for this timeslice

		".globl arm_dispatch				\n\t"
		".hidden arm_dispatch				\n"
	"arm_dispatch:							\n\t"
		"ldp w0, w1, [x28, #184]			\n\t"	// load Next PC, interrupt
		"ubfx w2, w0, #2, #21				\n\t"	// w2 = pc >> 2. Note: assuming address space == 8 MB (23 bits)
		"cbnz w1, arm_dofiq					\n\t"	// if interrupt pending, handle it

		"add x2, x26, x2, lsl #3			\n\t"	// x2 = EntryPoints + pc << 1
		"ldr x3, [x2]						\n\t"
		"br x3								\n"

	"arm_dofiq:								\n\t"
		"bl CPUFiq							\n\t"
		"b arm_dispatch						\n\t"

		".globl arm_exit					\n\t"
		".hidden arm_exit					\n"
	"arm_exit:								\n\t"
		"str w27, [x28, #192]				\n\t"	// if timeslice is over, save remaining cycles
		"ldp x23, x24, [sp, #80]			\n\t"
		"ldp x21, x22, [sp, #64]			\n\t"
		"ldp x19, x20, [sp, #48]			\n\t"
		"ldp x29, x30, [sp, #32]			\n\t"
		"ldp x27, x28, [sp, #16]			\n\t"
		"ldp x25, x26, [sp], #96			\n\t"
		"ret								\n"
);

#endif // ARM64
