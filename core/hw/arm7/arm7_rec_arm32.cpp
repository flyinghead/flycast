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
#include "rec-ARM/arm_unwind.h"
#include "oslib/virtmem.h"

#include <aarch32/macro-assembler-aarch32.h>
using namespace vixl::aarch32;

static ArmUnwindInfo unwinder;

namespace aica
{

namespace arm
{

class Arm32Assembler : public MacroAssembler
{
public:
	Arm32Assembler() = default;
	Arm32Assembler(u8 *buffer, size_t size) : MacroAssembler(buffer, size, A32) {}

	void Finalize() {
		FinalizeCode();
		virtmem::flush_cache(GetBuffer()->GetStartAddress<void *>(), GetCursorAddress<u8 *>() - 1,
				GetBuffer()->GetStartAddress<void *>(), GetCursorAddress<u8 *>() - 1);
	}
};

static Arm32Assembler ass;

static void (*arm_dispatch)();

static void loadReg(Register host_reg, Arm7Reg guest_reg, ConditionType cc = al)
{
	ass.Ldr(cc, host_reg, MemOperand(r8, (u8*)&arm_Reg[guest_reg].I - (u8*)&arm_Reg[0].I));
}

static void storeReg(Register host_reg, Arm7Reg guest_reg, ConditionType cc = al)
{
	ass.Str(cc, host_reg, MemOperand(r8, (u8*)&arm_Reg[guest_reg].I - (u8*)&arm_Reg[0].I));
}

const std::array<Register, 6> alloc_regs{
	r5, r6, r7, r9, r10, r11
};

class Arm32ArmRegAlloc : public ArmRegAlloc<std::size(alloc_regs), Arm32ArmRegAlloc>
{
	using super = ArmRegAlloc<std::size(alloc_regs), Arm32ArmRegAlloc>;

	void LoadReg(int host_reg, Arm7Reg armreg, ArmOp::Condition cc = ArmOp::AL)
	{
		loadReg(getReg(host_reg), armreg, (ConditionType)cc);
	}

	void StoreReg(int host_reg, Arm7Reg armreg, ArmOp::Condition cc = ArmOp::AL)
	{
		storeReg(getReg(host_reg), armreg, (ConditionType)cc);
	}

	static Register getReg(int i)
	{
		return alloc_regs[i];
	}

public:
	Arm32ArmRegAlloc(const std::vector<ArmOp>& block_ops)
		: super(block_ops) {}

	Register map(Arm7Reg r)
	{
		int i = super::map(r);
		return getReg(i);
	}

	friend super;
};

static Arm32ArmRegAlloc *regalloc;

static void loadFlags()
{
	//Load flags
	loadReg(r3, RN_PSR_FLAGS);
	//move them to flags register
	ass.Msr(APSR_nzcvq, r3);
}

static void storeFlags()
{
	//get results from flags register
	ass.Mrs(r3, APSR);
	//Store flags
	storeReg(r3, RN_PSR_FLAGS);
}

static Label *startConditional(ArmOp::Condition cc)
{
	if (cc == ArmOp::AL)
		return nullptr;
	ConditionType condition = (ConditionType)((u32)cc ^ 1);
	Label *label = new Label();
	ass.B(condition, label);

	return label;
}

static void endConditional(Label *label)
{
	if (label != nullptr)
	{
		ass.Bind(label);
		delete label;
	}
}

static Operand getOperand(const ArmOp::Operand& arg)
{
	Register reg;
	if (arg.isNone())
		return reg;
	if (arg.isImmediate())
	{
		if (!arg.isShifted())
			return Operand(arg.getImmediate());
		// Used by pc-rel ops: pc is immediate but can be shifted by reg (or even imm if op sets flags)
		ass.Mov(r1, arg.getImmediate());
		reg = r1;
	}
	else if (arg.isReg())
		reg = regalloc->map(arg.getReg().armreg);

	if (arg.isShifted())
	{
		if (!arg.shift_imm)
		{
			// Shift by register
			Register shift_reg = regalloc->map(arg.shift_reg.armreg);
			return Operand(reg, (ShiftType)arg.shift_type, shift_reg);
		}
		else
		{
			// Shift by immediate
			if (arg.shift_value != 0 || arg.shift_type != ArmOp::LSL)	// LSL 0 is a no-op
			{
				if (arg.shift_value == 0 && arg.shift_type == ArmOp::ROR)
					return Operand(reg, RRX);
				else
				{
					u32 shiftValue = arg.shift_value;
					if (shiftValue == 0 && (arg.shift_type == ArmOp::LSR || arg.shift_type == ArmOp::ASR))
						shiftValue = 32;
					return Operand(reg, (ShiftType)arg.shift_type, shiftValue);
				}
			}
		}
	}

	return reg;

}

static Register loadOperand(const ArmOp::Operand& arg, Register scratch_reg)
{
	Operand operand = getOperand(arg);
	if (operand.IsPlainRegister())
		return operand.GetBaseRegister();
	ass.Mov(scratch_reg, operand);
	return scratch_reg;
}

template <void (MacroAssembler::*Op)(FlagsUpdate flags, Condition cond, Register rd, Register rn, const Operand& operand)>
void emit3ArgOp(const ArmOp& op)
{
	bool set_flags = op.flags & ArmOp::OP_SETS_FLAGS;
	Register rd = regalloc->map(op.rd.getReg().armreg);
	Register rn = loadOperand(op.arg[0], r2);
	Operand operand = getOperand(op.arg[1]);
	(ass.*Op)((FlagsUpdate)set_flags, al, rd, rn, operand);
}

template <void (MacroAssembler::*Op)(FlagsUpdate flags, Condition cond, Register rd, const Operand& operand)>
void emit2ArgOp(const ArmOp& op)
{
	bool set_flags = op.flags & ArmOp::OP_SETS_FLAGS;
	Register rd = regalloc->map(op.rd.getReg().armreg);
	Operand operand = getOperand(op.arg[0]);
	(ass.*Op)((FlagsUpdate)set_flags, al, rd, operand);
}

template <void (MacroAssembler::*Op)(Condition cond, Register rn, const Operand& operand)>
void emitTestOp(const ArmOp& op)
{
	Register rn = loadOperand(op.arg[0], r2);
	Operand operand = getOperand(op.arg[1]);
	(ass.*Op)(al, rn, operand);
}

static void emitDataProcOp(const ArmOp& op)
{
	switch (op.op_type)
	{
	case ArmOp::AND:
		emit3ArgOp<&MacroAssembler::And>(op);
		break;
	case ArmOp::EOR:
		emit3ArgOp<&MacroAssembler::Eor>(op);
		break;
	case ArmOp::SUB:
		emit3ArgOp<&MacroAssembler::Sub>(op);
		break;
	case ArmOp::RSB:
		emit3ArgOp<&MacroAssembler::Rsb>(op);
		break;
	case ArmOp::ADD:
		emit3ArgOp<&MacroAssembler::Add>(op);
		break;
	case ArmOp::ORR:
		emit3ArgOp<&MacroAssembler::Orr>(op);
		break;
	case ArmOp::BIC:
		emit3ArgOp<&MacroAssembler::Bic>(op);
		break;
	case ArmOp::ADC:
		emit3ArgOp<&MacroAssembler::Adc>(op);
		break;
	case ArmOp::SBC:
		emit3ArgOp<&MacroAssembler::Sbc>(op);
		break;
	case ArmOp::RSC:
		emit3ArgOp<&MacroAssembler::Rsc>(op);
		break;
	case ArmOp::TST:
		emitTestOp<&MacroAssembler::Tst>(op);
		break;
	case ArmOp::TEQ:
		emitTestOp<&MacroAssembler::Teq>(op);
		break;
	case ArmOp::CMP:
		emitTestOp<&MacroAssembler::Cmp>(op);
		break;
	case ArmOp::CMN:
		emitTestOp<&MacroAssembler::Cmn>(op);
		break;
	case ArmOp::MOV:
		emit2ArgOp<&MacroAssembler::Mov>(op);
		break;
	case ArmOp::MVN:
		emit2ArgOp<&MacroAssembler::Mvn>(op);
		break;
	default:
		die("invalid op");
		break;
	}
}

static void jump(const void *code)
{
	ptrdiff_t offset = reinterpret_cast<uintptr_t>(code) - ass.GetBuffer()->GetStartAddress<uintptr_t>();
	if (offset < -32 * 1024 * 1024 || offset >= 32 * 1024 * 1024)
	{
		INFO_LOG(AICA_ARM, "jump offset too large: %d", offset);
		UseScratchRegisterScope scope(&ass);
		Register reg = scope.Acquire();
		ass.Mov(reg, (u32)code);
		ass.Bx(reg);
	}
	else
	{
		Label code_label(offset);
		ass.B(&code_label);
	}
}

static void call(const void *code, bool saveFlags = true)
{
	if (saveFlags)
		storeFlags();
	ptrdiff_t offset = reinterpret_cast<uintptr_t>(code) - ass.GetBuffer()->GetStartAddress<uintptr_t>();
	if (offset < -32 * 1024 * 1024 || offset >= 32 * 1024 * 1024)
	{
		INFO_LOG(AICA_ARM, "call offset too large: %d", offset);
		UseScratchRegisterScope scope(&ass);
		Register reg = scope.Acquire();
		ass.Mov(reg, (u32)code);
		ass.Blx(reg);
	}
	else
	{
		Label code_label(offset);
		ass.Bl(&code_label);
	}
	if (saveFlags)
		loadFlags();
}

static void emitMemOp(const ArmOp& op)
{
	Register addr_reg = loadOperand(op.arg[0], r2);
	if (op.pre_index)
	{
		const ArmOp::Operand& offset = op.arg[1];
		if (offset.isReg())
		{
			Register offset_reg = loadOperand(offset, r3);
			if (op.add_offset)
				ass.Add(r0, addr_reg, offset_reg);
			else
				ass.Sub(r0, addr_reg, offset_reg);
			addr_reg = r0;
		}
		else if (offset.isImmediate() && offset.getImmediate() != 0)
		{
			if (ImmediateA32::IsImmediateA32(offset.getImmediate()))
			{
				if (op.add_offset)
					ass.Add(r0, addr_reg, offset.getImmediate());
				else
					ass.Sub(r0, addr_reg, offset.getImmediate());
			}
			else
			{
				ass.Mov(r0, offset.getImmediate());
				if (op.add_offset)
					ass.Add(r0, addr_reg, r0);
				else
					ass.Sub(r0, addr_reg, r0);
			}
			addr_reg = r0;
		}
	}
	if (!addr_reg.Is(r0))
		ass.Mov(r0, addr_reg);
	if (op.op_type == ArmOp::STR)
	{
		if (op.arg[2].isImmediate())
			ass.Mov(r1, op.arg[2].getImmediate());
		else
			ass.Mov(r1, regalloc->map(op.arg[2].getReg().armreg));
	}

	call(recompiler::getMemOp(op.op_type == ArmOp::LDR, op.byte_xfer));

	if (op.op_type == ArmOp::LDR)
		ass.Mov(regalloc->map(op.rd.getReg().armreg), r0);

}

static void emitBranch(const ArmOp& op)
{
	if (op.arg[0].isImmediate())
		ass.Mov(r0, op.arg[0].getImmediate());
	else
	{
		ass.Mov(r0, regalloc->map(op.arg[0].getReg().armreg));
		ass.Bic(r0, r0, 3);
	}
	storeReg(r0, R15_ARM_NEXT);
}

static void emitMRS(const ArmOp& op)
{
	call((void *)CPUUpdateCPSR);

	if (op.spsr)
		loadReg(regalloc->map(op.rd.getReg().armreg), RN_SPSR);
	else
		loadReg(regalloc->map(op.rd.getReg().armreg), RN_CPSR);
}

static void emitMSR(const ArmOp& op)
{
	if (op.arg[0].isImmediate())
		ass.Mov(r0, op.arg[0].getImmediate());
	else
		ass.Mov(r0, regalloc->map(op.arg[0].getReg().armreg));

	if (op.spsr)
		call((void *)recompiler::MSR_do<1>);
	else
		call((void *)recompiler::MSR_do<0>);
}

static void emitFallback(const ArmOp& op)
{
	//Call interpreter
	ass.Mov(r0, op.arg[0].getImmediate());
	call((void *)recompiler::interpret);
}

void arm7backend_compile(const std::vector<ArmOp>& block_ops, u32 cycles)
{
	ass = Arm32Assembler((u8 *)recompiler::currentCode(), recompiler::spaceLeft());

	loadReg(r2, CYCL_CNT);
	while (!ImmediateA32::IsImmediateA32(cycles))
	{
		ass.Sub(r2, r2, 256);
		cycles -= 256;
	}
	ass.Sub(r2, r2, cycles);
	storeReg(r2, CYCL_CNT);

	regalloc = new Arm32ArmRegAlloc(block_ops);

	loadFlags();

	for (u32 i = 0; i < block_ops.size(); i++)
	{
		const ArmOp& op = block_ops[i];
		DEBUG_LOG(AICA_ARM, "-> %s", op.toString().c_str());

		Label *condLabel = nullptr;

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

		regalloc->store(i);

		endConditional(condLabel);
	}
	storeFlags();

	jump((void *)arm_dispatch);

	ass.Finalize();
	recompiler::advance(ass.GetBuffer()->GetSizeInBytes());

	delete regalloc;
	regalloc = nullptr;
}

void arm7backend_flush()
{
	if (!recompiler::empty())
	{
		verify(arm_mainloop != nullptr);
		verify(arm_compilecode != nullptr);
		return;
	}
	ass = Arm32Assembler((u8 *)recompiler::currentCode(), recompiler::spaceLeft());
	Label arm_exit;
	Label arm_dofiq;

	// For stack unwinding purposes, we pretend that the entire code block is a single function
	unwinder.clear();
	unwinder.start(ass.GetCursorAddress<void *>());

	// arm_mainloop:
	arm_mainloop = ass.GetCursorAddress<arm_mainloop_t>();
	RegisterList regList = RegisterList::Union(
			RegisterList(r4, r5, r6, r7),
			RegisterList(r8, r9, r10, r11),
			RegisterList(lr));
	ass.Push(regList);
	ass.Sub(sp, sp, 4);						// 8-byte stack alignment
	unwinder.allocStack(0, 40);
	unwinder.saveReg(0, r4, 36);
	unwinder.saveReg(0, r5, 32);
	unwinder.saveReg(0, r6, 28);
	unwinder.saveReg(0, r7, 24);
	unwinder.saveReg(0, r8, 20);
	unwinder.saveReg(0, r9, 16);
	unwinder.saveReg(0, r10, 12);
	unwinder.saveReg(0, r11, 8);
	unwinder.saveReg(0, lr, 4);

	ass.Mov(r8, r0);						// load regs
	ass.Mov(r4, r1);						// load entry points

	// arm_dispatch:
	arm_dispatch = ass.GetCursorAddress<void (*)()>();
	loadReg(r3, CYCL_CNT);					// load cycle counter
	loadReg(r0, R15_ARM_NEXT);				// load Next PC
	loadReg(r1, INTR_PEND);					// load Interrupt
	ass.Cmp(r3, 0);
	ass.B(le, &arm_exit);					// exit if counter <= 0
	ass.Ubfx(r2, r0, 2, 21);				// assuming 8 MB address space max (23 bits)
	ass.Cmp(r1, 0);
	ass.B(ne, &arm_dofiq);					// if interrupt pending, handle it

	ass.Ldr(pc, MemOperand(r4, r2, LSL, 2));

	// arm_dofiq:
	ass.Bind(&arm_dofiq);
	call((void *)CPUFiq, false);
	jump((void *)arm_dispatch);

	// arm_exit:
	ass.Bind(&arm_exit);
	ass.Add(sp, sp, 4);
	ass.Pop(regList);
	ass.Mov(pc, lr);

	// arm_compilecode:
	arm_compilecode = ass.GetCursorAddress<void (*)()>();
	call((void *)recompiler::compile, false);
	jump((void *)arm_dispatch);

	ass.Finalize();

	size_t unwindSize = unwinder.end(recompiler::spaceLeft() - 128);
	verify(unwindSize <= 128);

	recompiler::advance(ass.GetBuffer()->GetSizeInBytes());
}

} // namespace arm
} // namespace aica
#endif // HOST_CPU == CPU_ARM && FEAT_AREC != DYNAREC_NONE
