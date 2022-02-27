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

#if	HOST_CPU == CPU_X64 && FEAT_AREC != DYNAREC_NONE

#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>
using namespace Xbyak::util;

#include "arm7_rec.h"
#include "oslib/oslib.h"
#include "hw/mem/_vmem.h"

namespace aicaarm {

static void (*arm_dispatch)();

#ifdef _WIN32
static const Xbyak::Reg32 call_regs[] = { ecx, edx, r8d, r9d };
#else
static const Xbyak::Reg32 call_regs[] = { edi, esi, edx, ecx  };
#endif
static void (**entry_points)();
static UnwindInfo unwinder;

class Arm7Compiler;

#ifdef _WIN32
static const std::array<Xbyak::Reg32, 8> alloc_regs {
		ebx, ebp, edi, esi, r12d, r13d, r14d, r15d
};
#else
static const std::array<Xbyak::Reg32, 6> alloc_regs {
		ebx, ebp, r12d, r13d, r14d, r15d
};
#endif

class X64ArmRegAlloc : public ArmRegAlloc<sizeof(alloc_regs) / sizeof(alloc_regs[0]), X64ArmRegAlloc>
{
	using super = ArmRegAlloc<sizeof(alloc_regs) / sizeof(alloc_regs[0]), X64ArmRegAlloc>;
	Arm7Compiler& assembler;

	void LoadReg(int host_reg, Arm7Reg armreg);
	void StoreReg(int host_reg, Arm7Reg armreg);

	static const Xbyak::Reg32& getReg32(int i)
	{
		verify(i >= 0 && (u32)i < alloc_regs.size());
		return alloc_regs[i];
	}

public:
	X64ArmRegAlloc(Arm7Compiler& assembler, const std::vector<ArmOp>& block_ops)
		: super(block_ops), assembler(assembler) {}

	const Xbyak::Reg32& map(Arm7Reg r)
	{
		int i = super::map(r);
		return getReg32(i);
	}

	friend super;
};

class Arm7Compiler : public Xbyak::CodeGenerator
{
	bool logical_op_set_flags = false;
	bool set_carry_bit = false;
	bool set_flags = false;
	X64ArmRegAlloc *regalloc = nullptr;

	static const u32 N_FLAG = 1 << 31;
	static const u32 Z_FLAG = 1 << 30;
	static const u32 C_FLAG = 1 << 29;
	static const u32 V_FLAG = 1 << 28;

	Xbyak::util::Cpu cpu;

	Xbyak::Operand getOperand(const ArmOp::Operand& arg, Xbyak::Reg32 scratch_reg)
	{
		Xbyak::Reg32 r;
		if (!arg.isReg())
		{
			if (arg.isNone() || arg.shift_imm)
				return Xbyak::Operand();
			mov(scratch_reg, arg.getImmediate());
			r = scratch_reg;
		}
		else
			r = regalloc->map(arg.getReg().armreg);
		if (arg.isShifted())
		{
			if (r != scratch_reg)
			{
				mov(scratch_reg, r);
				r = scratch_reg;
			}
			if (arg.shift_imm)
			{
				// shift by immediate
				if (arg.shift_type != ArmOp::ROR && arg.shift_value != 0 && !logical_op_set_flags)
				{
					switch (arg.shift_type)
					{
					case ArmOp::LSL:
						shl(r, arg.shift_value);
						break;
					case ArmOp::LSR:
						shr(r, arg.shift_value);
						break;
					case ArmOp::ASR:
						sar(r, arg.shift_value);
						break;
					default:
						die("invalid");
						break;
					}
				}
				else if (arg.shift_value == 0)
				{
					// Shift by 32
					if (logical_op_set_flags)
						set_carry_bit = true;
					if (arg.shift_type == ArmOp::LSR)
					{
						if (set_carry_bit)
						{
							mov(r10d, r);			// r10d = rm[31]
							shr(r10d, 31);
						}
						mov(r, 0);					// r = 0
					}
					else if (arg.shift_type == ArmOp::ASR)
					{
						if (set_carry_bit)
						{
							mov(r10d, r);			// r10d = rm[31]
							shr(r10d, 31);
						}
						sar(r, 31);					// r = rm < 0 ? -1 : 0
					}
					else if (arg.shift_type == ArmOp::ROR)
					{
						// RRX
						mov(r10d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
						shl(r10d, 2);
						verify(r != eax);
						mov(eax, r);				// eax = rm
						and_(r10d, 0x80000000);		// r10[31] = C
						shr(eax, 1);				// eax = eax >> 1
						or_(eax, r10d);				// eax[31] = C
						if (set_carry_bit)
						{
							mov(r10d, r);
							and_(r10d, 1);			// r10 = rm[0] (new C)
						}
						mov(r, eax);				// r = eax
					}
					else
						die("Invalid shift");
				}
				else
				{
					// Carry must be preserved or Ror shift
					if (logical_op_set_flags)
						set_carry_bit = true;
					if (arg.shift_type == ArmOp::LSL)
					{
						if (set_carry_bit)
							mov(r10d, r);
						if (set_carry_bit)
							shr(r10d, 32 - arg.shift_value);
						shl(r, arg.shift_value);			// r <<= shift
						if (set_carry_bit)
							and_(r10d, 1);					// r10d = rm[lsb]
					}
					else
					{
						if (set_carry_bit)
						{
							mov(r10d, r);
							shr(r10d, arg.shift_value - 1);
							and_(r10d, 1);					// r10d = rm[msb]
						}

						if (arg.shift_type == ArmOp::LSR)
							shr(r, arg.shift_value);		// r >>= shift
						else if (arg.shift_type == ArmOp::ASR)
							sar(r, arg.shift_value);
						else if (arg.shift_type == ArmOp::ROR)
							ror(r, arg.shift_value);
						else
							die("Invalid shift");
					}
				}
			}
			else
			{
				// shift by register
				const Xbyak::Reg32 shift_reg = regalloc->map(arg.shift_reg.armreg);
				switch (arg.shift_type)
				{
				case ArmOp::LSL:
				case ArmOp::LSR:
					mov(ecx, shift_reg);
					mov(eax, 0);
					if (arg.shift_type == ArmOp::LSL)
						shl(r, cl);
					else
						shr(r, cl);
					cmp(shift_reg, 32);
					cmovnb(r, eax);		// LSL and LSR by 32 or more gives 0
					break;
				case ArmOp::ASR:
					mov(ecx, shift_reg);
					mov(eax, r);
					sar(eax, 31);
					sar(r, cl);
					cmp(shift_reg, 32);
					cmovnb(r, eax);		// ASR by 32 or more gives 0 or -1 depending on operand sign
					break;
				case ArmOp::ROR:
					mov(ecx, shift_reg);
					ror(r, cl);
					break;
				default:
					die("Invalid shift");
					break;
				}
			}
		}
		return r;
	}

	Xbyak::Label *startConditional(ArmOp::Condition cc)
	{
		if (cc == ArmOp::AL)
			return nullptr;
		Xbyak::Label *label = new Xbyak::Label();
		cc = (ArmOp::Condition)((u32)cc ^ 1);	// invert the condition
		mov(eax, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
		switch (cc)
		{
		case ArmOp::EQ:	// Z==1
			and_(eax, Z_FLAG);
			jnz(*label, T_NEAR);
			break;
		case ArmOp::NE:	// Z==0
			and_(eax, Z_FLAG);
			jz(*label, T_NEAR);
			break;
		case ArmOp::CS:	// C==1
			and_(eax, C_FLAG);
			jnz(*label, T_NEAR);
			break;
		case ArmOp::CC:	// C==0
			and_(eax, C_FLAG);
			jz(*label, T_NEAR);
			break;
		case ArmOp::MI:	// N==1
			and_(eax, N_FLAG);
			jnz(*label, T_NEAR);
			break;
		case ArmOp::PL:	// N==0
			and_(eax, N_FLAG);
			jz(*label, T_NEAR);
			break;
		case ArmOp::VS:	// V==1
			and_(eax, V_FLAG);
			jnz(*label, T_NEAR);
			break;
		case ArmOp::VC:	// V==0
			and_(eax, V_FLAG);
			jz(*label, T_NEAR);
			break;
		case ArmOp::HI:	// (C==1) && (Z==0)
			and_(eax, C_FLAG | Z_FLAG);
			cmp(eax, C_FLAG);
			jz(*label, T_NEAR);
			break;
		case ArmOp::LS:	// (C==0) || (Z==1)
			and_(eax, C_FLAG | Z_FLAG);
			cmp(eax, C_FLAG);
			jnz(*label, T_NEAR);
			break;
		case ArmOp::GE:	// N==V
			mov(ecx, eax);
			shl(ecx, 3);
			xor_(eax, ecx);
			and_(eax, N_FLAG);
			jz(*label, T_NEAR);
			break;
		case ArmOp::LT:	// N!=V
			mov(ecx, eax);
			shl(ecx, 3);
			xor_(eax, ecx);
			and_(eax, N_FLAG);
			jnz(*label, T_NEAR);
			break;
		case ArmOp::GT:	// (Z==0) && (N==V)
			mov(ecx, eax);
			mov(edx, eax);
			shl(ecx, 3);
			shl(edx, 1);
			xor_(eax, ecx);
			or_(eax, edx);
			and_(eax, N_FLAG);
			jz(*label, T_NEAR);
			break;
		case ArmOp::LE:	// (Z==1) || (N!=V)
			mov(ecx, eax);
			mov(edx, eax);
			shl(ecx, 3);
			shl(edx, 1);
			xor_(eax, ecx);
			or_(eax, edx);
			and_(eax, N_FLAG);
			jnz(*label, T_NEAR);
			break;
		default:
			die("Invalid condition code");
			break;
		}

		return label;
	}

	void endConditional(Xbyak::Label *label)
	{
		if (label != nullptr)
		{
			L(*label);
			delete label;
		}
	}

	bool emitDataProcOp(const ArmOp& op)
	{
		bool save_v_flag = true;

		Xbyak::Operand arg0 = getOperand(op.arg[0], r8d);
		Xbyak::Operand arg1 = getOperand(op.arg[1], r9d);
		Xbyak::Reg32 rd;
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
				mov(r10d, (op.arg[0].getImmediate() & 0x80000000) >> 31);
			}
			else if (op.arg[1].isImmediate() && op.arg[1].getImmediate() > 255)
			{
				set_carry_bit = true;
				mov(r10d, (op.arg[1].getImmediate() & 0x80000000) >> 31);
			}
		}

		switch (op.op_type)
		{
		case ArmOp::AND:
			if (arg1 == rd)
				and_(rd, arg0);
			else
			{
				if (rd != arg0)
				{
					mov(rd, arg0);
					verify(rd != arg1);
				}
				if (!arg1.isNone())
					and_(rd, arg1);
				else
					and_(rd, op.arg[1].getImmediate());
			}
			save_v_flag = false;
			break;
		case ArmOp::ORR:
			if (arg1 == rd)
				or_(rd, arg0);
			else
			{
				if (rd != arg0)
				{
					// FIXME need static evaluation or this must be duplicated
					if (arg0.isNone())
						mov(rd, op.arg[0].getImmediate());
					else
						mov(rd, arg0);
					verify(rd != arg1);
				}
				if (!arg1.isNone())
					or_(rd, arg1);
				else
					or_(rd, op.arg[1].getImmediate());
			}
			save_v_flag = false;
			break;
		case ArmOp::EOR:
			if (arg1 == rd)
				xor_(rd, arg0);
			else
			{
				if (rd != arg0)
				{
					verify(rd != arg1);
					mov(rd, arg0);
				}
				if (!arg1.isNone())
					xor_(rd, arg1);
				else
					xor_(rd, op.arg[1].getImmediate());
			}
			save_v_flag = false;
			break;
		case ArmOp::BIC:
			if (arg1.isNone())
			{
				mov(eax, op.arg[1].getImmediate());
				arg1 = eax;
			}
			if (cpu.has(Cpu::tBMI1))
				andn(rd, static_cast<Xbyak::Reg32&>(arg1), arg0);
			else
			{
				if (rd == arg0)
				{
					if (arg1 != r9d)
						mov(r9d, arg1);
					not_(r9d);
					and_(rd, r9d);
				}
				else
				{
					if (arg1 != rd)
						mov(rd, static_cast<Xbyak::Reg32&>(arg1));
					not_(rd);
					and_(rd, arg0);
				}
			}
			save_v_flag = false;
			break;

		case ArmOp::TST:
			if (!arg1.isNone())
				test(arg0, static_cast<Xbyak::Reg32&>(arg1));
			else
				test(arg0, op.arg[1].getImmediate());
			save_v_flag = false;
			break;
		case ArmOp::TEQ:
			if (arg0 != r8d)
				mov(r8d, arg0);
			if (!arg1.isNone())
				xor_(r8d, arg1);
			else
				xor_(r8d, op.arg[1].getImmediate());
			save_v_flag = false;
			break;
		case ArmOp::CMP:
			if (!arg1.isNone())
				cmp(arg0, arg1);
			else
				cmp(arg0, op.arg[1].getImmediate());
			if (set_flags)
			{
				setnb(r10b);
				set_carry_bit = true;
			}
			break;
		case ArmOp::CMN:
			if (arg0 != r8d)
				mov(r8d, arg0);
			if (!arg1.isNone())
				add(r8d, arg1);
			else
				add(r8d, op.arg[1].getImmediate());
			if (set_flags)
			{
				setb(r10b);
				set_carry_bit = true;
			}
			break;

		case ArmOp::MOV:
			if (arg0.isNone())
				mov(rd, op.arg[0].getImmediate());
			else if (arg0 != rd)
				mov(rd, arg0);
			if (set_flags)
			{
				test(rd, rd);
				save_v_flag = false;
			}
			break;
		case ArmOp::MVN:
			if (arg0.isNone())
				mov(rd, ~op.arg[0].getImmediate());
			else
			{
				if (arg0 != rd)
					mov(rd, arg0);
				not_(rd);
			}
			if (set_flags)
			{
				test(rd, rd);
				save_v_flag = false;
			}
			break;

		case ArmOp::SUB:
			if (arg1 == rd)
			{
				sub(arg0, arg1);
				if (rd != arg0)
					mov(rd, arg0);
			}
			else
			{
				if (rd != arg0)
					mov(rd, arg0);
				if (arg1.isNone())
					sub(rd, op.arg[1].getImmediate());
				else
					sub(rd, arg1);
			}
			if (set_flags)
			{
				setnb(r10b);
				set_carry_bit = true;
			}
			break;
		case ArmOp::RSB:
			if (arg1 == rd)
				sub(rd, arg0);
			else
			{
				if (rd != arg0)
					mov(rd, arg0);
				neg(rd);
				if (arg1.isNone())
					add(rd, op.arg[1].getImmediate());
				else
					add(rd, arg1);
			}
			if (set_flags)
			{
				setb(r10b);
				set_carry_bit = true;
			}
			break;
		case ArmOp::ADD:
			if (arg1 == rd)
				add(rd, arg0);
			else
			{
				if (rd != arg0)
				{
					// FIXME need static evaluation or this must be duplicated
					if (arg0.isNone())
						mov(rd, op.arg[0].getImmediate());
					else
						mov(rd, arg0);
				}
				if (arg1.isNone())
					add(rd, op.arg[1].getImmediate());
				else
					add(rd, arg1);
			}
			if (set_flags)
			{
				setb(r10b);
				set_carry_bit = true;
			}
			break;
		case ArmOp::ADC:
			mov(r11d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
			and_(r11d, C_FLAG);
			neg(r11d);
			if (arg1 == rd)
				adc(rd, arg0);
			else
			{
				if (rd != arg0)
					mov(rd, arg0);
				if (arg1.isNone())
					adc(rd, op.arg[1].getImmediate());
				else
					adc(rd, arg1);
			}
			if (set_flags)
			{
				setb(r10b);
				set_carry_bit = true;
			}
			break;
		case ArmOp::SBC:
			// rd = rn - op2 - !C
			mov(r11d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
			and_(r11d, C_FLAG);
			neg(r11d);
			cmc();		// on arm, -1 if carry is clear
			if (arg1 == rd)
			{
				sbb(arg0, arg1);
				if (rd != arg0)
					mov(rd, arg0);
			}
			else
			{
				if (rd != arg0)
					mov(rd, arg0);
				if (arg1.isNone())
					sbb(rd, op.arg[1].getImmediate());
				else
					sbb(rd, arg1);
			}
			if (set_flags)
			{
				setnb(r10b);
				set_carry_bit = true;
			}
			break;
		case ArmOp::RSC:
			// rd = op2 - rn - !C
			mov(r11d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
			and_(r11d, C_FLAG);
			neg(r11d);
			cmc();		// on arm, -1 if carry is clear
			if (arg1 == rd)
				sbb(rd, arg0);
			else
			{
				if (arg1.isNone())
					mov(rd, op.arg[1].getImmediate());
				else if (rd != arg1)
					mov(rd, arg1);
				sbb(rd, arg0);
			}
			if (set_flags)
			{
				setnb(r10b);
				set_carry_bit = true;
			}
			break;
		default:
			die("invalid");
			break;
		}

		return save_v_flag;
	}

	void emitMemOp(const ArmOp& op)
	{
		Xbyak::Operand addr_reg = getOperand(op.arg[0], call_regs[0]);
		if (addr_reg != call_regs[0])
		{
			if (addr_reg.isNone())
				mov(call_regs[0], op.arg[0].getImmediate());
			else
				mov(call_regs[0], addr_reg);
			addr_reg = call_regs[0];
		}
		if (op.pre_index)
		{
			const ArmOp::Operand& offset = op.arg[1];
			Xbyak::Operand offset_reg = getOperand(offset, r9d);
			if (!offset_reg.isNone())
			{
				if (op.add_offset)
					add(addr_reg, offset_reg);
				else
					sub(addr_reg, offset_reg);
			}
			else if (offset.isImmediate() && offset.getImmediate() != 0)
			{
				if (op.add_offset)
					add(addr_reg, offset.getImmediate());
				else
					sub(addr_reg, offset.getImmediate());
			}
		}
		if (op.op_type == ArmOp::STR)
		{
			if (op.arg[2].isImmediate())
				mov(call_regs[1], op.arg[2].getImmediate());
			else
				mov(call_regs[1], regalloc->map(op.arg[2].getReg().armreg));
		}

		call(recompiler::getMemOp(op.op_type == ArmOp::LDR, op.byte_xfer));

		if (op.op_type == ArmOp::LDR)
			mov(regalloc->map(op.rd.getReg().armreg), eax);
	}

	void saveFlags(bool save_v_flag)
	{
		if (!set_flags)
			return;

		pushf();
		pop(rax);

		if (save_v_flag)
		{
			mov(r11d, eax);
			shl(r11d, 28 - 11);		// V
		}
		shl(eax, 30 - 6);			// Z,N
		if (save_v_flag)
			and_(r11d, V_FLAG);		// V
		and_(eax, Z_FLAG | N_FLAG);	// Z,N
		if (save_v_flag)
			or_(eax, r11d);

		mov(r11d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
		if (set_carry_bit)
		{
			if (save_v_flag)
				and_(r11d, ~(Z_FLAG | N_FLAG | C_FLAG | V_FLAG));
			else
				and_(r11d, ~(Z_FLAG | N_FLAG | C_FLAG));
			shl(r10d, 29);
			or_(r11d, r10d);
		}
		else
		{
			if (save_v_flag)
				and_(r11d, ~(Z_FLAG | N_FLAG | V_FLAG));
			else
				and_(r11d, ~(Z_FLAG | N_FLAG));
		}
		or_(r11d, eax);
		mov(dword[rip + &arm_Reg[RN_PSR_FLAGS].I], r11d);
	}

	void emitBranch(const ArmOp& op)
	{
		Xbyak::Operand addr_reg = getOperand(op.arg[0], eax);
		if (addr_reg.isNone())
			mov(eax, op.arg[0].getImmediate());
		else
		{
			if (eax != addr_reg)
				mov(eax, addr_reg);
			and_(eax, 0xfffffffc);
		}
		mov(dword[rip + &arm_Reg[R15_ARM_NEXT].I], eax);
	}

	void emitMSR(const ArmOp& op)
	{
		if (op.arg[0].isImmediate())
			mov(call_regs[0], op.arg[0].getImmediate());
		else
			mov(call_regs[0], regalloc->map(op.arg[0].getReg().armreg));
		if (op.spsr)
			call(recompiler::MSR_do<1>);
		else
			call(recompiler::MSR_do<0>);
	}

	void emitMRS(const ArmOp& op)
	{
		call(CPUUpdateCPSR);

		if (op.spsr)
			mov(regalloc->map(op.rd.getReg().armreg), dword[rip + &arm_Reg[RN_SPSR]]);
		else
			mov(regalloc->map(op.rd.getReg().armreg), dword[rip + &arm_Reg[RN_CPSR]]);
	}

	void emitFallback(const ArmOp& op)
	{
		set_flags = false;
		mov(call_regs[0], op.arg[0].getImmediate());
		call(recompiler::interpret);
	}

public:
	Arm7Compiler() : Xbyak::CodeGenerator(recompiler::spaceLeft(), recompiler::currentCode()) { }

	void compile(const std::vector<ArmOp>& block_ops, u32 cycles)
	{
		regalloc = new X64ArmRegAlloc(*this, block_ops);

		sub(dword[rip + &arm_Reg[CYCL_CNT]], cycles);

		ArmOp::Condition currentCondition = ArmOp::AL;
		Xbyak::Label *condLabel = nullptr;

		for (u32 i = 0; i < block_ops.size(); i++)
		{
			const ArmOp& op = block_ops[i];
			DEBUG_LOG(AICA_ARM, "-> %s", op.toString().c_str());

			set_flags = op.flags & ArmOp::OP_SETS_FLAGS;
			logical_op_set_flags = op.isLogicalOp() && set_flags;
			set_carry_bit = false;
			bool save_v_flag = true;

			if (op.op_type == ArmOp::FALLBACK)
			{
				endConditional(condLabel);
				condLabel = nullptr;
				currentCondition = ArmOp::AL;
			}
			else if (op.condition != currentCondition)
			{
				endConditional(condLabel);
				currentCondition = op.condition;
				condLabel = startConditional(op.condition);
			}

			regalloc->load(i);

			if (op.op_type <= ArmOp::MVN)
				// data processing op
				save_v_flag = emitDataProcOp(op);
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

			saveFlags(save_v_flag);

			regalloc->store(i);

			if (set_flags)
			{
				currentCondition = ArmOp::AL;
				endConditional(condLabel);
				condLabel = nullptr;
			}
		}
		endConditional(condLabel);

		jmp((void*)arm_dispatch);

		ready();
		recompiler::advance(getSize());

		delete regalloc;
		regalloc = nullptr;
	}

	void generateMainLoop()
	{
		if (!recompiler::empty())
		{
			verify(arm_mainloop != nullptr);
			verify(arm_compilecode != nullptr);
			return;
		}
		Xbyak::Label arm_dispatch_label;
		Xbyak::Label arm_mainloop_label;

		//arm_compilecode:
		call(recompiler::compile);
		jmp(arm_dispatch_label);

		// arm_mainloop:
		L(arm_mainloop_label);
		unwinder.start((void *)getCurr());
		size_t startOffset = getSize();
#ifdef _WIN32
		push(rdi);
		unwinder.pushReg(getSize(), Xbyak::Operand::RDI);
		push(rsi);
		unwinder.pushReg(getSize(), Xbyak::Operand::RSI);
#endif
		push(r12);
		unwinder.pushReg(getSize(), Xbyak::Operand::R12);
		push(r13);
		unwinder.pushReg(getSize(), Xbyak::Operand::R13);
		push(r14);
		unwinder.pushReg(getSize(), Xbyak::Operand::R14);
		push(r15);
		unwinder.pushReg(getSize(), Xbyak::Operand::R15);
		push(rbx);
		unwinder.pushReg(getSize(), Xbyak::Operand::RBX);
		push(rbp);
		unwinder.pushReg(getSize(), Xbyak::Operand::RBP);
#ifdef _WIN32
		sub(rsp, 40);	// 32-byte shadow space + 16-byte stack alignment
		unwinder.allocStack(getSize(), 40);
#else
		sub(rsp, 8);		// 16-byte stack alignment
		unwinder.allocStack(getSize(), 8);
#endif
		unwinder.endProlog(getSize());

		mov(qword[rip + &entry_points], call_regs[1].cvt64());

		// arm_dispatch:
		L(arm_dispatch_label);
		mov(rdx, qword[rip + &entry_points]);
		mov(ecx, dword[rip + &arm_Reg[R15_ARM_NEXT]]);
		mov(eax, dword[rip + &arm_Reg[INTR_PEND]]);
		cmp(dword[rip + &arm_Reg[CYCL_CNT]], 0);
		Xbyak::Label arm_exit;
		jle(arm_exit);			// timeslice is over
		test(eax, eax);
		Xbyak::Label arm_dofiq;
		jne(arm_dofiq);			// if interrupt pending, handle it

		and_(ecx, 0x7ffffc);
		jmp(qword[rdx + rcx * 2]);

		// arm_dofiq:
		L(arm_dofiq);
		call(CPUFiq);
		jmp(arm_dispatch_label);

		// arm_exit:
		L(arm_exit);
#ifdef _WIN32
		add(rsp, 40);
#else
		add(rsp, 8);
#endif
		pop(rbp);
		pop(rbx);
		pop(r15);
		pop(r14);
		pop(r13);
		pop(r12);
#ifdef _WIN32
		pop(rsi);
		pop(rdi);
#endif
		ret();

		size_t savedSize = getSize();
		setSize(recompiler::spaceLeft() - 128 - startOffset); // FIXME size of unwind record unknown
		size_t unwindSize = unwinder.end(getSize());
		verify(unwindSize <= 128);
		setSize(savedSize);

		ready();
		arm_compilecode = (void (*)())getCode();
		arm_mainloop = (arm_mainloop_t)arm_mainloop_label.getAddress();
		arm_dispatch = (void (*)())arm_dispatch_label.getAddress();

		recompiler::advance(getSize());
	}

};

void X64ArmRegAlloc::LoadReg(int host_reg, Arm7Reg armreg)
{
	// printf("LoadReg X%d <- r%d\n", host_reg, armreg);
	assembler.mov(getReg32(host_reg), dword[rip + &arm_Reg[(u32)armreg].I]);
}

void X64ArmRegAlloc::StoreReg(int host_reg, Arm7Reg armreg)
{
	// printf("StoreReg X%d -> r%d\n", host_reg, armreg);
	assembler.mov(dword[rip + &arm_Reg[(u32)armreg].I], getReg32(host_reg));
}

void arm7backend_compile(const std::vector<ArmOp>& block_ops, u32 cycles)
{
	void* protStart = recompiler::currentCode();
	size_t protSize = recompiler::spaceLeft();
	vmem_platform_jit_set_exec(protStart, protSize, false);

	Arm7Compiler assembler;
	assembler.compile(block_ops, cycles);

	vmem_platform_jit_set_exec(protStart, protSize, true);
}

void arm7backend_flush()
{
	void* protStart = recompiler::currentCode();
	size_t protSize = recompiler::spaceLeft();
	vmem_platform_jit_set_exec(protStart, protSize, false);
	unwinder.clear();

	Arm7Compiler assembler;
	assembler.generateMainLoop();

	vmem_platform_jit_set_exec(protStart, protSize, true);
}

}
#endif // X64 && DYNAREC_JIT
