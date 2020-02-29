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

#if HOST_CPU == CPU_ARM && FEAT_AREC != DYNAREC_NONE
#include "arm7_rec.h"
#include "hw/mem/_vmem.h"

#define _DEVEL          (1)
#define EMIT_I          armEmit32((I))
#define EMIT_GET_PTR()  armGetEmitPtr()
static void armEmit32(u32 emit32);
static void *armGetEmitPtr();
#include "arm_emitter/arm_emitter.h"
#undef I
using namespace ARM;

extern "C" void arm_dispatch();
extern "C" void arm_exit();

extern u8* icPtr;
extern u8* ICache;
extern const u32 ICacheSize;
extern reg_pair arm_Reg[RN_ARM_REG_COUNT];

static void loadReg(eReg host_reg, Arm7Reg guest_reg, ArmOp::Condition cc = ArmOp::AL)
{
	LDR(host_reg, r8, (u8*)&arm_Reg[guest_reg].I - (u8*)&arm_Reg[0].I, ARM::Offset, (ARM::ConditionCode)cc);
}

static void storeReg(eReg host_reg, Arm7Reg guest_reg, ArmOp::Condition cc = ArmOp::AL)
{
	STR(host_reg, r8, (u8*)&arm_Reg[guest_reg].I - (u8*)&arm_Reg[0].I, ARM::Offset, (ARM::ConditionCode)cc);
}

static const std::array<eReg, 5> alloc_regs{
	r6, r7, r9, r10, r11
};

class Arm32ArmRegAlloc : public ArmRegAlloc<alloc_regs.size(), Arm32ArmRegAlloc>
{
	using super = ArmRegAlloc<alloc_regs.size(), Arm32ArmRegAlloc>;

	void LoadReg(int host_reg, Arm7Reg armreg, ArmOp::Condition cc = ArmOp::AL)
	{
		// printf("LoadReg R%d <- r%d\n", host_reg, armreg);
		loadReg(getReg(host_reg), armreg, cc);
	}

	void StoreReg(int host_reg, Arm7Reg armreg, ArmOp::Condition cc = ArmOp::AL)
	{
		// printf("StoreReg R%d -> r%d\n", host_reg, armreg);
		storeReg(getReg(host_reg), armreg, cc);
	}

	static eReg getReg(int i)
	{
		verify(i >= 0 && (u32)i < alloc_regs.size());
		return alloc_regs[i];
	}

public:
	Arm32ArmRegAlloc(const std::vector<ArmOp>& block_ops)
		: super(block_ops) {}

	eReg map(Arm7Reg r)
	{
		int i = super::map(r);
		return getReg(i);
	}

	friend super;
};

static void armEmit32(u32 emit32)
{
	if (icPtr >= ICache + ICacheSize - 1024)
	{
		ERROR_LOG(AICA_ARM, "JIT buffer full: %zd bytes free", ICacheSize - (icPtr - ICache));
		die("AICA ARM code buffer full");
	}

	*(u32*)icPtr = emit32;
	icPtr += 4;
}

static void *armGetEmitPtr()
{
	return icPtr;
}

static Arm32ArmRegAlloc *regalloc;

static void loadFlags()
{
	//Load flags
	loadReg(r3, RN_PSR_FLAGS);
	//move them to flags register
	MSR(0, 8, r3);
}

static void storeFlags()
{
	//get results from flags register
	MRS(r3, 0);
	//Store flags
	storeReg(r3, RN_PSR_FLAGS);
}

static u32 *startConditional(ArmOp::Condition cc)
{
	if (cc == ArmOp::AL)
		return nullptr;
	verify(cc <= ArmOp::LE);
	ARM::ConditionCode condition = (ARM::ConditionCode)((u32)cc ^ 1);
	u32 *code = (u32 *)icPtr;
	JUMP((u32)code, condition);

	return code;
}

static void endConditional(u32 *pos)
{
	if (pos != nullptr)
	{
		u32 *curpos = (u32 *)icPtr;
		ARM::ConditionCode condition = (ARM::ConditionCode)(*pos >> 28);
		icPtr = (u8 *)pos;
		JUMP((u32)curpos, condition);
		icPtr = (u8 *)curpos;
	}
}

static eReg getOperand(ArmOp::Operand arg, eReg scratch_reg)
{
	if (arg.isNone())
		return (eReg)-1;
	else if (arg.isImmediate())
	{
		if (is_i8r4(arg.getImmediate()))
			MOV(scratch_reg, arg.getImmediate());
		else
			MOV32(scratch_reg, arg.getImmediate());
	}
	else if (arg.isReg())
	{
		if (!arg.isShifted())
			return regalloc->map(arg.getReg().armreg);
		MOV(scratch_reg, regalloc->map(arg.getReg().armreg));
	}

	if (!arg.shift_imm)
	{
		// Shift by register
		eReg shift_reg = regalloc->map(arg.shift_reg.armreg);
		MOV(scratch_reg, scratch_reg, (ARM::ShiftOp)arg.shift_type, shift_reg);
	}
	else
	{
		// Shift by immediate
		if (arg.shift_value != 0 || arg.shift_type != ArmOp::LSL)	// LSL 0 is a no-op
			MOV(scratch_reg, scratch_reg, (ARM::ShiftOp)arg.shift_type, arg.shift_value);
	}

	return scratch_reg;
}

template <void (*OpImmediate)(eReg rd, eReg rn, s32 imm8, bool S, ConditionCode cc),
		void (*OpShiftImm)(eReg rd, eReg rn, eReg rm, ShiftOp Shift, u32 ImmShift, bool S, ConditionCode cc),
		void (*OpShiftReg)(eReg rd, eReg rn, eReg rm, ShiftOp Shift, eReg shift_reg, bool S, ConditionCode cc)>
void emit3ArgOp(const ArmOp& op)
{
	eReg rn;
	const ArmOp::Operand *op2;
	if (op.op_type != ArmOp::MOV && op.op_type != ArmOp::MVN)
	{
		rn = getOperand(op.arg[0], r2);
		op2 = &op.arg[1];
	}
	else
		op2 = &op.arg[0];

	eReg rd = regalloc->map(op.rd.getReg().armreg);

	bool set_flags = op.flags & ArmOp::OP_SETS_FLAGS;
	eReg rm;
	if (op2->isImmediate())
	{
		if (is_i8r4(op2->getImmediate()) && op2->shift_imm)
		{
			OpImmediate(rd, rn, op2->getImmediate(), set_flags, CC_AL);
			return;
		}
		MOV32(r0, op2->getImmediate());
		rm = r0;
	}
	else if (op2->isReg())
		rm = regalloc->map(op2->getReg().armreg);

	if (op2->shift_imm)
		OpShiftImm(rd, rn, rm, (ShiftOp)op2->shift_type, op2->shift_value, set_flags, CC_AL);
	else
	{
		// Shift by reg
		eReg shift_reg = regalloc->map(op2->shift_reg.armreg);
		OpShiftReg(rd, rn, rm, (ShiftOp)op2->shift_type, shift_reg, set_flags, CC_AL);
	}
}

template <void (*OpImmediate)(eReg rd, s32 imm8, bool S, ConditionCode cc),
		void (*OpShiftImm)(eReg rd, eReg rm, ShiftOp Shift, u32 ImmShift, bool S, ConditionCode cc),
		void (*OpShiftReg)(eReg rd, eReg rm, ShiftOp Shift, eReg shift_reg, bool S, ConditionCode cc)>
void emit2ArgOp(const ArmOp& op)
{
	// Used for rd (MOV, MVN) and rn (CMP, TST, ...)
	eReg rd;
	const ArmOp::Operand *op2;
	if (op.op_type != ArmOp::MOV && op.op_type != ArmOp::MVN)
	{
		rd = getOperand(op.arg[0], r2);
		op2 = &op.arg[1];
	}
	else {
		op2 = &op.arg[0];
		rd = regalloc->map(op.rd.getReg().armreg);
	}

	bool set_flags = op.flags & ArmOp::OP_SETS_FLAGS;
	eReg rm;
	if (op2->isImmediate())
	{
		if (is_i8r4(op2->getImmediate()) && op2->shift_imm)
		{
			OpImmediate(rd, op2->getImmediate(), set_flags, CC_AL);
			return;
		}
		MOV32(r0, op2->getImmediate());
		rm = r0;
	}
	else if (op2->isReg())
		rm = regalloc->map(op2->getReg().armreg);

	if (op2->shift_imm)
		OpShiftImm(rd, rm, (ShiftOp)op2->shift_type, op2->shift_value, set_flags, CC_AL);
	else
	{
		// Shift by reg
		eReg shift_reg = regalloc->map(op2->shift_reg.armreg);
		OpShiftReg(rd, rm, (ShiftOp)op2->shift_type, shift_reg, set_flags, CC_AL);
	}
}

static void emitDataProcOp(const ArmOp& op)
{
	switch (op.op_type)
	{
	case ArmOp::AND:
		emit3ArgOp<&AND, &AND, &AND>(op);
		break;
	case ArmOp::EOR:
		emit3ArgOp<&EOR, &EOR, &EOR>(op);
		break;
	case ArmOp::SUB:
		emit3ArgOp<&SUB, &SUB, &SUB>(op);
		break;
	case ArmOp::RSB:
		emit3ArgOp<&RSB, &RSB, &RSB>(op);
		break;
	case ArmOp::ADD:
		emit3ArgOp<&ADD, &ADD, &ADD>(op);
		break;
	case ArmOp::ORR:
		emit3ArgOp<&ORR, &ORR, &ORR>(op);
		break;
	case ArmOp::BIC:
		emit3ArgOp<&BIC, &BIC, &BIC>(op);
		break;
	case ArmOp::ADC:
		emit3ArgOp<&ADC, &ADC, &ADC>(op);
		break;
	case ArmOp::SBC:
		emit3ArgOp<&SBC, &SBC, &SBC>(op);
		break;
	case ArmOp::RSC:
		emit3ArgOp<&RSC, &RSC, &RSC>(op);
		break;
	case ArmOp::TST:
		emit2ArgOp<&TST, &TST, &TST>(op);
		break;
	case ArmOp::TEQ:
		emit2ArgOp<&TEQ, &TEQ, &TEQ>(op);
		break;
	case ArmOp::CMP:
		emit2ArgOp<&CMP, &CMP, &CMP>(op);
		break;
	case ArmOp::CMN:
		emit2ArgOp<&CMN, &CMN, &CMN>(op);
		break;
	case ArmOp::MOV:
		emit2ArgOp<&MOV, &MOV, &MOV>(op);
		break;
	case ArmOp::MVN:
		emit2ArgOp<&MVN, &MVN, &MVN>(op);
		break;
	default:
		die("invalid op");
		break;
	}
}

static void call(u32 addr, ARM::ConditionCode cc = ARM::CC_AL)
{
	storeFlags();
	CALL(addr, cc);
	loadFlags();
}

static void emitMemOp(const ArmOp& op)
{
	eReg addr_reg = getOperand(op.arg[0], r2);
	if (op.pre_index)
	{
		const ArmOp::Operand& offset = op.arg[1];
		if (offset.isReg())
		{
			eReg offset_reg = getOperand(offset, r3);
			if (op.add_offset)
				ADD(r0, addr_reg, offset_reg);
			else
				SUB(r0, addr_reg, offset_reg);
			addr_reg = r0;
		}
		else if (offset.isImmediate() && offset.getImmediate() != 0)
		{
			if (is_i8r4(offset.getImmediate()))
			{
				if (op.add_offset)
					ADD(r0, addr_reg, offset.getImmediate());
				else
					SUB(r0, addr_reg, offset.getImmediate());
			}
			else
			{
				MOV32(r0, offset.getImmediate());
				if (op.add_offset)
					ADD(r0, addr_reg, r0);
				else
					SUB(r0, addr_reg, r0);
			}
			addr_reg = r0;
		}
	}
	if (addr_reg != r0)
		MOV(r0, addr_reg);
	if (op.op_type == ArmOp::STR)
	{
		if (op.arg[2].isImmediate())
		{
			if (is_i8r4(op.arg[2].getImmediate()))
				MOV(r1, op.arg[2].getImmediate());
			else
				MOV32(r1, op.arg[2].getImmediate());
		}
		else
			MOV(r1, regalloc->map(op.arg[2].getReg().armreg));
	}

	call((u32)arm7rec_getMemOp(op.op_type == ArmOp::LDR, op.byte_xfer));

	if (op.op_type == ArmOp::LDR)
		MOV(regalloc->map(op.rd.getReg().armreg), r0);

}

static void emitBranch(const ArmOp& op)
{
	if (op.arg[0].isImmediate())
		MOV32(r0, op.arg[0].getImmediate());
	else
	{
		MOV(r0, regalloc->map(op.arg[0].getReg().armreg));
		BIC(r0, r0, 3);
	}
	storeReg(r0, R15_ARM_NEXT);
}

static void emitMRS(const ArmOp& op)
{
	call((u32)CPUUpdateCPSR);

	if (op.spsr)
		loadReg(regalloc->map(op.rd.getReg().armreg), RN_SPSR);
	else
		loadReg(regalloc->map(op.rd.getReg().armreg), RN_CPSR);
}

static void emitMSR(const ArmOp& op)
{
	if (op.arg[0].isImmediate())
		MOV32(r0, op.arg[0].getImmediate());
	else
		MOV(r0, regalloc->map(op.arg[0].getReg().armreg));

	if (op.spsr)
		call((u32)MSR_do<1>);
	else
		call((u32)MSR_do<0>);
}

static void emitFallback(const ArmOp& op)
{
	//Call interpreter
	MOV32(r0, op.arg[0].getImmediate());
	call((u32)arm_single_op);
	SUB(r5, r5, r0, false);
}

void arm7backend_compile(const std::vector<ArmOp> block_ops, u32 cycles)
{
	regalloc = new Arm32ArmRegAlloc(block_ops);
	void *codestart = icPtr;

	loadFlags();

	for (u32 i = 0; i < block_ops.size(); i++)
	{
		const ArmOp& op = block_ops[i];
		DEBUG_LOG(AICA_ARM, "-> %s", op.toString().c_str());

		u32 *condPos = nullptr;

		if (op.op_type != ArmOp::FALLBACK)
			condPos = startConditional(op.condition);

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

		regalloc->store(i);

		endConditional(condPos);
	}
	storeFlags();

	if (is_i8r4(cycles))
		SUB(r5, r5, cycles, true);
	else
	{
		u32 togo = cycles;
		while(ARMImmid8r4_enc(togo) == -1)
		{
			SUB(r5, r5, 256);
			togo -= 256;
		}
		SUB(r5, r5, togo, true);
	}
	JUMP((u32)&arm_exit, CC_MI);	//statically predicted as not taken
	JUMP((u32)&arm_dispatch);

	vmem_platform_flush_cache(codestart, (u8*)icPtr - 1, codestart, (u8*)icPtr - 1);

	delete regalloc;
	regalloc = nullptr;
}

#endif // HOST_CPU == CPU_ARM && FEAT_AREC != DYNAREC_NONE
