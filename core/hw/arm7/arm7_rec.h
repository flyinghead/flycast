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
#pragma once
#include <array>
#include "types.h"
#include "arm7.h"

struct ArmOp
{
	enum OpType {
		AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC,
		TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN,
		LDR, STR, B, BL, MSR, MRS, FALLBACK
	};
	enum Condition {
		EQ,	NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, UC
	};
	enum ShiftOp {
		LSL, LSR, ASR, ROR, RRX = ROR,
	};
	static_assert(UC == 15, "UC == 15");
	static const u32 OP_READS_FLAGS = 1;
	static const u32 OP_SETS_FLAGS = 4;
	static const u32 OP_SETS_PC = 8;

	ArmOp() : ArmOp(FALLBACK, AL) {}
	ArmOp(OpType type, Condition condition) : op_type(type), condition(condition) {	}

	struct Register {
		Register() : armreg((Arm7Reg)0) {}
		Register(Arm7Reg armreg) : armreg(armreg) {}
		Register(Arm7Reg armreg, int version) : armreg(armreg), version(version) {}

		Arm7Reg armreg;
		int version = 0;
	};
	struct Operand
	{
		Operand() : type(none) {}
		Operand(u32 v) : type(imm), imm_value(v) {}
		Operand(Arm7Reg r) : type(reg_), reg_value(r, 0) {}
		bool isImmediate() const { return type == imm; }
		bool isReg() const { return type == reg_; }
		bool isNone() const { return type == none; }

		u32 getImmediate() const { verify(isImmediate()); return imm_value; }
		void setImmediate(u32 v) { type = imm; imm_value = v; }
		Register& getReg() { verify(isReg()); return reg_value; }
		const Register& getReg() const { verify(isReg()); return reg_value; }
		void setReg(Arm7Reg armreg, int version = 0) { type = reg_; reg_value = Register(armreg, version); }
		bool isShifted() const { return !shift_imm || shift_value != 0 || shift_type != ArmOp::LSL; }

		enum { none, reg_, imm } type;
		union {
			u32 imm_value;
			Register reg_value;
		};

		// For flexible operand
		ShiftOp shift_type = LSL;
		bool shift_imm = true;
		union {
			u32 shift_value = 0;
			Register shift_reg;
		};
	};

	OpType op_type;
	Operand rd;
	std::array<Operand, 3> arg;
	// For LDR/STR
	bool pre_index = false;
	bool add_offset = false;
	bool byte_xfer = false;
	bool write_back = false;
	Condition condition;
	u8 flags = 0;
	u8 cycles = 6;
	bool spsr = false;

	bool isLogicalOp() const
	{
		return op_type == AND || op_type == EOR || op_type == TST || op_type == TEQ
				|| (op_type >= ORR && op_type <= MVN);
	}

	bool isCompOp() const
	{
		return op_type >= TST &&  op_type <= CMN;
	}

	const std::string& conditionToString() const {
		static const std::string labels[] = { "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "", "uc" };
		return labels[(int)condition];
	}
	std::string regToString(const Register &r) const {
		return "r" + std::to_string((int)r.armreg) + ":" + std::to_string(r.version);
	}
	std::string shiftToString(ShiftOp type, u32 value) const {
		switch (type) {
		case LSL:
			return "LSL";
		case LSR:
			return "LSR";
		case ASR:
			return "ASR";
		case ROR:
			return value == 0 ? "RRX" : "ROR";
		default:
			return "";
		}
	}

	std::string operandToString(const Operand& op) const {
		std::string s;
		if (op.isImmediate())
			s = "#" + std::to_string(op.getImmediate());
		else if (op.isReg())
			s = regToString(op.getReg());

		if (op.shift_type != LSL || op.shift_value != 0)
		{
			s += ", " + shiftToString(op.shift_type, op.shift_value);
			if (op.shift_imm)
				s += " #" + std::to_string(op.shift_value);
			else
				s += " " + regToString(op.shift_reg);
		}

		return s;
	}
	std::string toString() const {
		static const std::string labels[] = {
			"and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
			"tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn",
			"ldr", "str", "b", "bl", "msr", "mrs", "(fallback)",
		};
		std::string s = labels[(int)op_type];
		if (op_type <= MVN)
		{
			if (!isCompOp() && (flags & OP_SETS_FLAGS))
				s += "s";
			s += conditionToString();
			if (rd.isReg())
				s += " " + regToString(rd.getReg()) + ", ";
			else
				s += " ";
			if (!arg[0].isNone())
			{
				s += operandToString(arg[0]);
				if (!arg[1].isNone())
				{
					s += ", " + operandToString(arg[1]);
					if (!arg[2].isNone())
						s += ", " + operandToString(arg[2]);
				}
			}
		}
		else if (op_type <= STR)
		{
			if (byte_xfer)
				s += "b";
			s += conditionToString() + " ";
			if (rd.isReg())
				s += regToString(rd.getReg()) + ", ";
			if (arg[2].isReg())
				s += regToString(arg[2].getReg()) + ", ";
			s += "[" + operandToString(arg[0]);
			if (!pre_index)
				s += "]";
			if (!arg[1].isNone())
				s += ", " + operandToString(arg[1]);
			if (pre_index)
				s += "]";
			if (write_back)
				s += "!";
		}
		else if (op_type <= BL)
		{
			s += conditionToString() + " " + operandToString(arg[0]);
		}
		else if (op_type == MSR)
		{
			s += conditionToString() + " CPSR, " + operandToString(arg[0]);
		}
		else if (op_type == MRS)
			s += conditionToString() + " " + operandToString(rd) + ", CPSR";

		return s;
	}
};

template <int max_regs, typename T>
class ArmRegAlloc
{
	struct RegAlloc {
		int host_reg = -1;
		u16 version = 0;
		bool dirty = false;
		bool temporary = false;
	};
	std::array<RegAlloc, RN_ARM_REG_COUNT> allocs;
	std::vector<int> host_regs;
	const std::vector<ArmOp>& block_ops;

	void allocReg(const ArmOp::Register& reg, bool write, bool temporary, u32 opidx)
	{
		RegAlloc& alloc = allocs[(int)reg.armreg];
		if (alloc.host_reg == -1 || (alloc.version != reg.version && !write))
		{
			if (host_regs.empty())
			{
				// Need to flush a reg
				Arm7Reg bestReg = (Arm7Reg)-1;
				int bestRegUse = -1;
				for (u32 i = 0; i < allocs.size(); i++)
				{
					auto& alloc = allocs[i];
					if (alloc.host_reg == -1)
						continue;
					if (!alloc.dirty)
					{
						int nextUse_ = nextUse((Arm7Reg)i, alloc.version, opidx);
						if (nextUse_ == -1)
						{
							host_regs.push_back(alloc.host_reg);
							alloc.host_reg = -1;
							break;
						}
						if (nextUse_ != opidx && nextUse_ > bestRegUse)
						{
							bestRegUse = nextUse_;
							bestReg = (Arm7Reg)i;
						}
					}
				}
				if (host_regs.empty())
				{
					if (bestReg == (Arm7Reg)-1)
					{
						for (u32 i = 0; i < allocs.size(); i++)
						{
							auto& alloc = allocs[i];
							if (alloc.host_reg == -1)
								continue;
							int nextUse_ = nextUse((Arm7Reg)i, alloc.version, opidx);
							if (nextUse_ == -1)
							{
								bestReg = (Arm7Reg)i;
								break;
							}
							if (nextUse_ != opidx && nextUse_ > bestRegUse)
							{
								bestRegUse = nextUse_;
								bestReg = (Arm7Reg)i;
							}
						}
						verify(bestReg != (Arm7Reg)-1);
						DEBUG_LOG(AICA_ARM, "Flushing dirty register r%d", bestReg);
					}
					flushReg(allocs[bestReg]);
				}
				verify(!host_regs.empty());
			}
			alloc.host_reg = host_regs.back();
			host_regs.pop_back();
			alloc.version = reg.version;
			alloc.dirty = write;
			alloc.temporary = temporary;
			if (!write)
				static_cast<T*>(this)->LoadReg(alloc.host_reg, reg.armreg);
		}
		if (write)
		{
			alloc.dirty = true;
			alloc.version = reg.version;
			alloc.temporary = temporary;
		}
	}

	bool needsWriteback(const ArmOp::Register& reg, u32 opidx)
	{
		for (auto it = block_ops.begin() + opidx; it != block_ops.end(); it++)
		{
			if (it->op_type == ArmOp::FALLBACK)
				// assume the value is being used
				return true;
			if (it->rd.isReg() && it->rd.getReg().armreg == reg.armreg)
			{
				if (it->condition == ArmOp::AL)
					// register is overwritten so it's not used
					return false;
				else
					// might be overwritten but we don't know so write it back
					return true;
			}
		}
		// assume the value will be used
		return true;
	}

	// Returns the index of the next op that uses the given reg version.
	// returns -1 if not used in the remainder of the block or if a fallback is found
	int nextUse(Arm7Reg reg, int version, u32 opidx)
	{
		auto get_index_or_max = [version](const ArmOp::Register& opreg, int idx) {
			if (opreg.version == version)
				return idx;
			else
				return -1;
		};
		for (auto it = block_ops.begin() + opidx; it != block_ops.end(); it++, opidx++)
		{
			if (it->op_type == ArmOp::FALLBACK)
				return -1;
			for (const auto& arg : it->arg)
			{
				if (arg.isReg() && arg.getReg().armreg == reg)
					return get_index_or_max(arg.getReg(), opidx);
				if (!arg.shift_imm && arg.shift_reg.armreg == reg)
					return get_index_or_max(arg.shift_reg, opidx);
			}
			if (it->rd.isReg() && it->rd.getReg().armreg == reg)
				return -1;
		}
		return -1;
	}

	void flushReg(RegAlloc& alloc)
	{
		if (alloc.dirty)
		{
			static_cast<T*>(this)->StoreReg(alloc.host_reg, (Arm7Reg)(&alloc - allocs.begin()));
			alloc.dirty = false;
		}
		host_regs.push_back(alloc.host_reg);
		alloc.host_reg = -1;
	}

	void flushAllRegs()
	{
		for (auto& alloc : allocs)
			if (alloc.host_reg != -1)
				flushReg(alloc);
	}

public:
	ArmRegAlloc(const std::vector<ArmOp>& block_ops) : block_ops(block_ops) {
		host_regs.clear();
		for (int i = 0; i < max_regs; i++)
			host_regs.push_back(i);
	}

	void load(u32 opidx)
	{
		const ArmOp& op = block_ops[opidx];
		if (op.op_type == ArmOp::FALLBACK)
			flushAllRegs();
		else
		{
			bool conditional = op.condition != ArmOp::AL;
			for (const auto& arg : op.arg)
			{
				if (arg.isReg())
					allocReg(arg.getReg(), false, conditional, opidx);
				if (!arg.shift_imm)
					allocReg(arg.shift_reg, false, conditional, opidx);
			}
			if (op.rd.isReg())
				allocReg(op.rd.getReg(), true, conditional, opidx);
		}
	}

	void store(u32 opidx)
	{
		const ArmOp& op = block_ops[opidx];
		if (op.op_type == ArmOp::FALLBACK)
			return;
		if (op.condition != ArmOp::AL)
		{
			for (auto& alloc : allocs)
				if (alloc.host_reg != -1 && alloc.temporary)
					flushReg(alloc);
		}
		else if (op.rd.isReg() && needsWriteback(op.rd.getReg(), opidx + 1))
		{
			RegAlloc& alloc = allocs[(int)op.rd.getReg().armreg];
			static_cast<T*>(this)->StoreReg(alloc.host_reg, op.rd.getReg().armreg);
			alloc.dirty = false;
		}
	}

protected:
	int map(Arm7Reg r)
	{
		return allocs[(int)r].host_reg;
	}
};

void CPUUpdateCPSR();

void arm7rec_init();
void arm7rec_flush();
extern "C" void arm7rec_compile();
void *arm7rec_getMemOp(bool load, bool byte);
template<u32 Pd> void DYNACALL MSR_do(u32 v);
u32 DYNACALL arm_single_op(u32 opcode);

void arm7backend_compile(const std::vector<ArmOp> block_ops, u32 cycles);
