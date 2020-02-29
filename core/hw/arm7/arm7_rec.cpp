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

#if	FEAT_AREC != DYNAREC_NONE

#include "arm7_rec.h"
#include "arm7.h"
#include "hw/aica/aica_if.h"
#include "hw/mem/_vmem.h"
#include "arm_mem.h"

#if 0
// for debug
#include "vixl/aarch32/disasm-aarch32.h"
#include <iostream>
#endif
#define arm_printf(...) DEBUG_LOG(AICA_ARM, __VA_ARGS__)

extern "C" u32 DYNACALL arm_compilecode();

extern reg_pair arm_Reg[RN_ARM_REG_COUNT];

u8* icPtr;
u8* ICache;
extern const u32 ICacheSize = 1024 * 1024 * 4;
void* EntryPoints[ARAM_SIZE_MAX / 4];

#ifdef _WIN32
alignas(4096) static u8 ARM7_TCB[ICacheSize];
#elif HOST_OS == OS_LINUX

alignas(4096) static u8 ARM7_TCB[ICacheSize] __attribute__((section(".text")));

#elif HOST_OS==OS_DARWIN
alignas(4096) static u8 ARM7_TCB[ICacheSize] __attribute__((section("__TEXT, .text")));
#else
#error ARM7_TCB ALLOC
#endif

#pragma pack(push,1)
union ArmOpBits
{
	ArmOpBits(u32 opcode) : full(opcode) {}

    struct {
    	// immediate (str/ldr)
    	u32 imm12:12;
        u32 rd:4;
		// common
		u32 rn : 4;
		// data processing
		u32 set_flags : 1;
		u32 op_type : 4;
		//
		u32 imm_op : 1;
		u32 op_group : 2;
		u32 condition : 4;
	};
    // Register
    struct
    {
        u32 rm:4;
        u32 shift_by_reg:1;
        u32 shift_type:2;
        // Shift by immediate
        u32 shift_imm:5;
        u32 :4;
    };
    // Immediate value
    struct
    {
        u32 imm8:8;
        u32 rotate:4;
        u32 :4;
    };
	struct {
		u32 :7;
		// op2 Shift by reg
		u32 _zero:1;
		u32 shift_reg:4;
		u32 :8;

		// LDR/STR
		u32 load:1;
		u32 write_back:1;
		u32 byte:1;
		u32 up:1;
		u32 pre_index:1;
		u32 :7;
	};
	u32 full;
};
#pragma pack(pop)
static_assert(sizeof(ArmOpBits) == sizeof(u32), "sizeof(ArmOpBits) == sizeof(u32)");

static std::vector<ArmOp> block_ops;
static u8 cpuBitsSet[256];

//findfirstset -- used in LDM/STM handling
#if HOST_CPU==CPU_X86 && !defined(__GNUC__)
#include <intrin.h>

u32 findfirstset(u32 v)
{
	unsigned long rv;
	_BitScanForward(&rv,v);
	return rv+1;
}
#else
#define findfirstset __builtin_ffs
#endif

static ArmOp decodeArmOp(u32 opcode, u32 arm_pc)
{
	ArmOpBits bits(opcode);

	ArmOp op;
	op.condition = (ArmOp::Condition)bits.condition;
	if (op.condition == ArmOp::UC)
	{
		//NV condition means VFP on newer cores, let interpreter handle it...
		op.op_type = ArmOp::FALLBACK;
		op.arg[0] = ArmOp::Operand(opcode);
		return op;
	}
	if (op.condition != ArmOp::AL)
		op.flags |= ArmOp::OP_READS_FLAGS;

	switch (bits.op_group)
	{
	case 0:	// data processing, Multiply, Swap
		// disambiguate
		if ((bits.op_type & 4) == 0 && bits.imm_op == 0 && bits.shift_by_reg && bits._zero != 0)
		{
			// MUL, MLA or SWP
			op.op_type = ArmOp::FALLBACK;
			op.arg[0] = ArmOp::Operand(opcode);
			op.cycles = 0;
			return op;
		}
		if (bits.op_type >= (u32)ArmOp::TST && bits.op_type <= ArmOp::CMN && bits.set_flags == 0)
		{
			// MSR, MRS
			op.spsr = bits.op_type & 2;
			if ((bits.full & 0x0FBF0FFF) == 0x010F0000)
			{
				op.op_type = ArmOp::MRS;
				op.rd = ArmOp::Operand((Arm7Reg)bits.rd);
				verify(bits.rd != 15);
			}
			else if ((bits.full & 0x0FBFFFF0) == 0x0129F000)
			{
				op.op_type = ArmOp::MSR;
				op.arg[0] = ArmOp::Operand((Arm7Reg)bits.rm);
				op.cycles++;
			}
			else if ((bits.full & 0x0DBFF000) == 0x0128F000)
			{
				op.op_type = ArmOp::MSR;
				if (bits.imm_op == 0)
				{
					// source is reg
					op.arg[0] = ArmOp::Operand((Arm7Reg)bits.rm);
					verify(bits.rm != 15);
				}
				else
				{
					u32 rotate = bits.rotate * 2;
					op.arg[0] = ArmOp::Operand((bits.imm8 >> rotate) | (bits.imm8 << (32 - rotate)));
				}
			}
			else
			{
				// Unsupported op
				op.op_type = ArmOp::MOV;
				op.condition = ArmOp::AL;
				op.flags = 0;
				op.rd = ArmOp::Operand((Arm7Reg)0);
				op.arg[0] = op.rd;
			}
			return op;
		}
		{
			op.op_type = (ArmOp::OpType)bits.op_type;
			if (!op.isCompOp())
				op.rd = ArmOp::Operand((Arm7Reg)bits.rd);
			int argidx = 0;
			if (op.op_type != ArmOp::MOV && op.op_type != ArmOp::MVN)
			{
				op.arg[0] = ArmOp::Operand((Arm7Reg)bits.rn);
				if (op.arg[0].getReg().armreg == RN_PC)
					op.arg[0] = ArmOp::Operand(arm_pc + (!bits.imm_op && bits.shift_by_reg ? 12 : 8));
				argidx++;
			}
			if (bits.set_flags)
				op.flags |= ArmOp::OP_SETS_FLAGS;
			if (bits.imm_op)
			{
				u32 rotate = bits.rotate * 2;
				op.arg[argidx] = ArmOp::Operand((bits.imm8 >> rotate) | (bits.imm8 << (32 - rotate)));
			}
			else
			{
				op.arg[argidx] = ArmOp::Operand((Arm7Reg)bits.rm);
				op.arg[argidx].shift_type = (ArmOp::ShiftOp)bits.shift_type;
				op.arg[argidx].shift_imm = bits.shift_by_reg == 0;
				if (op.arg[argidx].shift_imm)
				{
					op.arg[argidx].shift_value = bits.shift_imm;
					if (op.arg[argidx].shift_type == ArmOp::RRX && op.arg[argidx].shift_value == 0)
						op.flags |= ArmOp::OP_READS_FLAGS;
				}
				else
				{
					op.arg[argidx].shift_reg = ArmOp::Register((Arm7Reg)bits.shift_reg);
				}
				// Compute pc-relative addresses
				if (op.arg[argidx].getReg().armreg == RN_PC)
				{
					if (op.arg[argidx].shift_imm && !((op.flags & ArmOp::OP_SETS_FLAGS) && op.isLogicalOp()) && op.arg[argidx].shift_type != ArmOp::ROR)
					{

						const u32 next_pc = arm_pc + 8;
						switch (op.arg[argidx].shift_type)
						{
						case ArmOp::LSL:
							op.arg[argidx] = ArmOp::Operand(next_pc << op.arg[argidx].shift_value);
							break;
						case ArmOp::LSR:
							op.arg[argidx] = ArmOp::Operand(next_pc >> op.arg[argidx].shift_value);
							break;
						case ArmOp::ASR:
							op.arg[argidx] = ArmOp::Operand((int)next_pc >> op.arg[argidx].shift_value);
							break;
						default:
							break;
						}
					}
					else
					{
						op.arg[argidx].setImmediate(arm_pc + (op.arg[argidx].shift_imm ? 8 : 12));
					}
				}
			}
			if (op.rd.isReg() && op.rd.getReg().armreg == RN_PC)
			{
				if (op.op_type == ArmOp::MOV && op.arg[0].isReg() && !bits.set_flags && !op.arg[0].isShifted())
				{
					// MOVcc pc, rn -> B
					op.op_type = ArmOp::B;
					op.flags |= ArmOp::OP_SETS_PC;
					op.rd = ArmOp::Operand();
					return op;
				}
				if (op.condition != ArmOp::AL || (op.flags & ArmOp::OP_SETS_FLAGS))
				{
					// TODO no support for conditional/setflags ops that set pc except the case above
					op.op_type = ArmOp::FALLBACK;
					op.arg[0] = ArmOp::Operand(opcode);
					op.arg[1] = ArmOp::Operand();
					op.arg[2] = ArmOp::Operand();
					op.rd = ArmOp::Operand();
					op.cycles = 0;
				}
				else
					op.rd.getReg().armreg = R15_ARM_NEXT;
				op.flags |= ArmOp::OP_SETS_PC;
			}
			if (op.op_type == ArmOp::ADC || op.op_type == ArmOp::SBC || op.op_type == ArmOp::RSC)
				op.flags |= ArmOp::OP_READS_FLAGS;
		}
		break;

	case 1: // LDR/STR
		{
			op.add_offset = bits.up;
			op.byte_xfer = bits.byte;
			op.pre_index = bits.pre_index;
			op.write_back = (bits.write_back || !bits.pre_index) && bits.rd != bits.rn && (bits.imm_op != 0 || bits.imm12 != 0);
			if (bits.load)
			{
				op.op_type = ArmOp::LDR;
				op.rd = ArmOp::Operand((Arm7Reg)bits.rd);
				if (op.rd.getReg().armreg == RN_PC)			// LDR w/ rd=pc
				{
					op.flags |= ArmOp::OP_SETS_PC;
					if (op.condition != ArmOp::AL)
					{
						// TODO no support for conditional ops
						op.op_type = ArmOp::FALLBACK;
						op.arg[0] = ArmOp::Operand(opcode);
						op.cycles = 0;
						return op;
					}
					op.rd.setReg(R15_ARM_NEXT);
				}
				op.cycles += 4;
			}
			else
			{
				op.op_type = ArmOp::STR;
				op.arg[2] = ArmOp::Operand((Arm7Reg)bits.rd);
				if (op.arg[2].getReg().armreg == RN_PC)
					op.arg[2] = ArmOp::Operand(arm_pc + 12);
				op.cycles += 3;
			}
			op.arg[0] = ArmOp::Operand((Arm7Reg)bits.rn);
			if (op.arg[0].getReg().armreg == RN_PC && op.write_back)
			{
				// LDR/STR w pc-based offset and write-back
				op.flags |= ArmOp::OP_SETS_PC;
				// TODO not supported
				op.op_type = ArmOp::FALLBACK;
				op.arg[0] = ArmOp::Operand(opcode);
				op.arg[1] = ArmOp::Operand();
				op.arg[2] = ArmOp::Operand();
				op.cycles = 0;
				return op;
			}
			if (bits.imm_op == 0)
			{
				// Immediate offset
				if (op.arg[0].getReg().armreg == RN_PC)
					// Compute pc-relative address
					op.arg[0] = ArmOp::Operand(arm_pc + 8 + (op.add_offset ? bits.imm12 : -bits.imm12));
				else
					op.arg[1] = ArmOp::Operand(bits.imm12);
			}
			else
			{
				// Offset by register, optionally shifted
				if (op.arg[0].getReg().armreg == RN_PC)
					op.arg[0] = ArmOp::Operand(arm_pc + 8);
				op.arg[1] = ArmOp::Operand((Arm7Reg)bits.rm);
				op.arg[1].shift_type = (ArmOp::ShiftOp)bits.shift_type;
				op.arg[1].shift_imm = true;
				op.arg[1].shift_value = bits.shift_imm;
				if (op.arg[1].getReg().armreg == RN_PC)
				{
					verify(op.arg[1].shift_value == 0 && op.arg[1].shift_type == ArmOp::LSL);
					op.arg[1] = ArmOp::Operand(arm_pc + 8);
				}
				if (op.arg[1].shift_type == ArmOp::RRX && op.arg[1].shift_value == 0)
					op.flags |= ArmOp::OP_READS_FLAGS;
			}
		}
		break;

	case 2:	// LDM/STM and B,BL
		if (bits.imm_op)
		{
			// B, BL
			op.op_type = bits.op_type & 8 ? ArmOp::BL : ArmOp::B;	// L bit
			op.arg[0] = ArmOp::Operand(arm_pc + 8 + (((int)(opcode & 0xffffff) << 8) >> 6));
			op.flags |= ArmOp::OP_SETS_PC;
			op.cycles += 3;
		}
		else
		{
			// LDM/STM
			u32 reg_list = opcode & 0xffff;
			// one register selected and no PSR
			if (!(opcode & (1 << 22)) && cpuBitsSet[reg_list & 255] + cpuBitsSet[(reg_list >> 8) & 255] == 1)
			{
				if (opcode & (1 << 20))
				{
					// LDM
					//One register xfered
					//Can be rewriten as normal mem opcode ..
					ArmOpBits newbits(0x04000000 | (opcode & 0xf0000000));

					//Imm offset
					//opcd |= 0<<25;
					//Post incr
					newbits.pre_index = bits.pre_index;
					//Up/Dn
					newbits.up = bits.up;
					//Word/Byte
					//newbits.byte = 0;
					//Write back (must be 0 for post-incr)
					newbits.write_back = bits.write_back & bits.pre_index;
					//Load
					newbits.load = 1;

					//Rn
					newbits.rn = bits.rn;

					//Rd
					newbits.rd = findfirstset(reg_list) - 1;

					//Offset
					newbits.full |= 4;

					arm_printf("ARM: MEM TFX R %08X -> %08X\n", opcode, newbits.full);

					return decodeArmOp(newbits.full, arm_pc);
				}
				//STM common case
				else
				{
					ArmOpBits newbits(0x04000000 | (opcode & 0xf0000000));

					//Imm offset
					//opcd |= 0<<25;
					//Post incr
					newbits.pre_index = bits.pre_index;
					//Up/Dn
					newbits.up = bits.up;
					//Word/Byte
					//newbits.byte = 0;
					//Write back (must be 0 for PI)
					newbits.write_back = bits.pre_index;
					//Load
					newbits.load = 0;

					//Rn
					newbits.rn = bits.rn;

					//Rd
					newbits.rd = findfirstset(reg_list) - 1;

					//Offset
					newbits.full |= 4;

					arm_printf("ARM: MEM TFX W %08X -> %08X\n", opcode, newbits.full);

					return decodeArmOp(newbits.full, arm_pc);
				}
			}
			op.op_type = ArmOp::FALLBACK;
			op.arg[0] = ArmOp::Operand(opcode);
			op.cycles = 0;

			if ((opcode & 0x8000) && bits.load)	// LDM w/ pc
				op.flags |= ArmOp::OP_SETS_PC;
		}
		break;

	case 3: // coproc, SWI
		op.op_type = ArmOp::FALLBACK;
		op.arg[0] = ArmOp::Operand(opcode);
		op.cycles = 0;
		if (bits.imm_op == 1 && (bits.op_type & 8)) // SWI
			op.flags |= ArmOp::OP_SETS_PC;
		break;
	}

	return op;
}

static void block_ssa_pass()
{
	std::array<u32, RN_ARM_REG_COUNT> versions{};
	for (auto it = block_ops.begin(); it != block_ops.end(); it++)
	{
		if (it->op_type == ArmOp::FALLBACK)
			for (auto& v : versions)
				v++;
		else
		{
			if ((it->op_type == ArmOp::STR || it->op_type == ArmOp::LDR) && it->write_back)
			{
				// Extract add/sub operation from STR/LDR
				if (it->op_type == ArmOp::LDR && !it->pre_index && it->arg[1].isReg()
						&& it->arg[1].getReg().armreg == it->rd.getReg().armreg)
				{
					// Edge case where the offset reg is the target register but its value before the op
					// must be used in post-increment/decrement
					// Thus we save its value in a scratch register.
					ArmOp newop(ArmOp::MOV, it->condition);
					newop.rd = ArmOp::Operand(RN_SCRATCH);
					newop.arg[0] = ArmOp::Operand(it->rd);
					// Insert before
					it = block_ops.insert(it, newop);
					it++;
					ArmOp newop2(it->add_offset ? ArmOp::ADD : ArmOp::SUB, it->condition);
					newop2.rd = ArmOp::Operand(it->arg[0]);
					newop2.arg[0] = newop2.rd;
					newop2.arg[1] = it->arg[1];
					newop2.arg[1].setReg(RN_SCRATCH);
					if (newop2.condition != ArmOp::AL || (it->arg[1].shift_type == ArmOp::RRX && it->arg[1].shift_value == 0))
						newop2.flags |= ArmOp::OP_READS_FLAGS;
					it->flags &= ~ArmOp::OP_READS_FLAGS;
					it->write_back = false;
					it->arg[1] = ArmOp::Operand();
					// Insert after
					it = block_ops.insert(it + 1, newop2);
					it--;
					it--;
				}
				else
				{
					ArmOp newop(it->add_offset ? ArmOp::ADD : ArmOp::SUB, it->condition);
					newop.rd = ArmOp::Operand(it->arg[0]);
					newop.arg[0] = newop.rd;
					newop.arg[1] = it->arg[1];
					if (newop.condition != ArmOp::AL || (it->arg[1].shift_type == ArmOp::RRX && it->arg[1].shift_value == 0))
						newop.flags |= ArmOp::OP_READS_FLAGS;
					it->flags &= ~ArmOp::OP_READS_FLAGS;
					it->write_back = false;
					it->arg[1] = ArmOp::Operand();
					if (it->pre_index)
						// Insert before
						it = block_ops.insert(it, newop);
					else
					{
						// Insert after
						it = block_ops.insert(it + 1, newop);
						it--;
					}
				}
			}
			// Set versions
			for (auto& arg : it->arg)
			{
				if (arg.isReg())
					arg.getReg().version = versions[(size_t)arg.getReg().armreg];
				if (!arg.shift_imm)
					arg.shift_reg.version = versions[(size_t)arg.shift_reg.armreg];
			}
			if (it->rd.isReg())
				it->rd.getReg().version = ++versions[(size_t)it->rd.getReg().armreg];
		}
	}
}

extern "C" void arm7rec_compile()
{
	//Get the code ptr
	void* rv = icPtr;

	//setup local pc counter
	u32 pc = arm_Reg[R15_ARM_NEXT].I;

	//update the block table
	// Note that we mask with the max aica size (8 MB), which is
	// also the size of the EntryPoints table. This way the dynarec
	// main loop doesn't have to worry about the actual aica
	// ram size. The aica ram always wraps to 8 MB anyway.
	EntryPoints[(pc & (ARAM_SIZE_MAX - 1)) / 4] = rv;

	block_ops.clear();

	u32 cycles = 0;

	arm_printf("ARM7 Block %x", pc);
	//the ops counter is used to terminate the block (max op count for a single block is 32 currently)
	//We don't want too long blocks for timing accuracy
	for (u32 ops = 0; ops < 32; ops++)
	{
		//Read opcode ...
		u32 opcd = *(u32*)&aica_ram[pc & ARAM_MASK];

#if 0
		vixl::aarch32::Disassembler disassembler(std::cout, pc);
		disassembler.DecodeA32(opcd);
		std::cout << std::endl;
#endif

		ArmOp last_op = decodeArmOp(opcd, pc);
		cycles += last_op.cycles;

		//Goto next opcode
		pc += 4;

		if (last_op.op_type == ArmOp::FALLBACK)
		{
			// Interpreter needs pc + 8 in r15
			ArmOp armop(ArmOp::MOV, ArmOp::AL);
			armop.rd = ArmOp::Operand(RN_PC);
			armop.arg[0] = ArmOp::Operand(pc + 4);
			block_ops.push_back(armop);
		}
		//Branch ?
		if (last_op.flags & ArmOp::OP_SETS_PC)
		{
			if (last_op.condition != ArmOp::AL)
			{
				// insert a "mov armNextPC, pc + 4" before the jump if not taken
				ArmOp armop(ArmOp::MOV, ArmOp::AL);
				armop.rd = ArmOp::Operand(R15_ARM_NEXT);
				armop.arg[0] = ArmOp::Operand(pc);
				block_ops.push_back(armop);
			}
			if (last_op.op_type == ArmOp::BL)
			{
				// Save pc+4 into r14
				ArmOp armop(ArmOp::MOV, last_op.condition);
				armop.rd = ArmOp::Operand(RN_LR);
				armop.arg[0] = ArmOp::Operand(pc);
				block_ops.push_back(armop);
			}
			block_ops.push_back(last_op);
			arm_printf("ARM: %06X: Block End %d", pc, ops);
			break;
		}
		block_ops.push_back(last_op);

		//block size limit ?
		if (ops == 31)
		{
			// Update armNextPC
			ArmOp armop(ArmOp::MOV, ArmOp::AL);
			armop.rd = ArmOp::Operand(R15_ARM_NEXT);
			armop.arg[0] = ArmOp::Operand(pc);
			block_ops.push_back(armop);
			arm_printf("ARM: %06X: Block split", pc);
		}
	}

	block_ssa_pass();

	arm7backend_compile(block_ops, cycles);

	arm_printf("arm7rec_compile done: %p,%p", rv, icPtr);
}

void arm7rec_flush()
{
	icPtr = ICache;
	for (u32 i = 0; i < ARRAY_SIZE(EntryPoints); i++)
		EntryPoints[i] = (void*)&arm_compilecode;
}

extern "C" void CompileCode()
{
	arm7rec_compile();
}

void arm7rec_init()
{
	if (!vmem_platform_prepare_jit_block(ARM7_TCB, ICacheSize, (void**)&ICache))
		die("vmem_platform_prepare_jit_block failed");

	icPtr = ICache;

	for (int i = 0; i < 256; i++)
	{
		int count = 0;
		for (int j = 0; j < 8; j++)
			if (i & (1 << j))
				count++;

		cpuBitsSet[i] = count;
	}
}

template <bool Load, bool Byte>
u32 DYNACALL DoMemOp(u32 addr,u32 data)
{
	u32 rv=0;

	if (Load)
	{
		if (Byte)
			rv=arm_ReadMem8(addr);
		else
			rv=arm_ReadMem32(addr);
	}
	else
	{
		if (Byte)
			arm_WriteMem8(addr,data);
		else
			arm_WriteMem32(addr,data);
	}

	return rv;
}

void *arm7rec_getMemOp(bool Load, bool Byte)
{
	if (Load)
	{
		if (Byte)
			return (void*)(u32(DYNACALL*)(u32,u32))&DoMemOp<true,true>;
		else
			return (void*)(u32(DYNACALL*)(u32,u32))&DoMemOp<true,false>;
	}
	else
	{
		if (Byte)
			return (void*)(u32(DYNACALL*)(u32,u32))&DoMemOp<false,true>;
		else
			return (void*)(u32(DYNACALL*)(u32,u32))&DoMemOp<false,false>;
	}
}

extern bool Arm7Enabled;
extern "C" void DYNACALL arm_mainloop(u32 cycl, void* regs, void* entrypoints);

// Run a timeslice of arm7

void arm_Run(u32 CycleCount)
{
	if (Arm7Enabled)
		arm_mainloop(CycleCount, arm_Reg, EntryPoints);
}

#endif // FEAT_AREC != DYNAREC_NONE
