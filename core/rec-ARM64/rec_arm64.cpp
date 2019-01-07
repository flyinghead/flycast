#include <unistd.h>
#include <sys/mman.h>
#include "types.h"

#if FEAT_SHREC == DYNAREC_JIT

#include "deps/vixl/aarch64/macro-assembler-aarch64.h"
using namespace vixl::aarch64;

#include "hw/sh4/sh4_opcode_list.h"

#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"

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

static void ngen_FailedToFindBlock_internal() {
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

void(*ngen_FailedToFindBlock)() = &ngen_FailedToFindBlock_internal;

static int cycle_counter;

void ngen_mainloop(void* v_cntx)
{
	Sh4RCB* ctx = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4RCB));

	cycle_counter = SH4_TIMESLICE;

	while (sh4_int_bCpuRun) {
		do {
			DynarecCodeEntryPtr rcb = bm_GetCode(ctx->cntx.pc);
			rcb();
		} while (cycle_counter > 0);

		cycle_counter += SH4_TIMESLICE;

		if (UpdateSystem()) {
			rdv_DoInterrupts_pc(ctx->cntx.pc);
		}
	}
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

u32* GetRegPtr(u32 reg)
{
	return Sh4_int_GetRegisterPtr((Sh4RegType)reg);
}

void ngen_blockcheckfail(u32 pc) {
	printf("arm64 JIT: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
}

class Arm64Assembler : public MacroAssembler
{
public:
	Arm64Assembler() : MacroAssembler((u8 *)emit_GetCCPtr(), 64 * 1024)
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

	void ngen_Compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
	{
		//printf("REC-ARM64 compiling %08x\n", block->addr);
		if (force_checks)
			CheckBlock(block);

		Stp(x29, x30, MemOperand(sp, -16, PreIndex));
		Mov(x29, sp);

		Mov(x9, (size_t)&cycle_counter);
		Ldr(w10, MemOperand(x9));
		Sub(w10, w10, block->guest_cycles);
		Str(w10, MemOperand(x9));

		for (size_t i = 0; i < block->oplist.size(); i++)
		{
			shil_opcode& op  = block->oplist[i];
			switch (op.op)
			{
			case shop_ifb:	// Interpreter fallback
				if (op.rs1._imm)	// if NeedPC()
				{
					Mov(x9, (size_t)&next_pc);
					Mov(w10, op.rs2._imm);
					Str(w10, MemOperand(x9));
				}
				Mov(*call_regs[0], op.rs3._imm);

				CallRuntime(OpDesc[op.rs3._imm]->oph);
				break;

			case shop_jcond:
			case shop_jdyn:
				Mov(x9, (size_t)op.rs1.reg_ptr());

				Ldr(w10, MemOperand(x9));

				if (op.rs2.is_imm()) {
					Mov(w9, op.rs2._imm);
					Add(w10, w10, w9);
				}

				Mov(x9, (size_t)op.rd.reg_ptr());
				Str(w10, MemOperand(x9));
				break;

			case shop_mov32:
				verify(op.rd.is_reg());
				verify(op.rs1.is_reg() || op.rs1.is_imm());

				shil_param_to_host_reg(op.rs1, w10);
				host_reg_to_shil_param(op.rd, w10);
				break;

			case shop_mov64:
				verify(op.rd.is_reg());
				verify(op.rs1.is_reg() || op.rs1.is_imm());

				shil_param_to_host_reg(op.rs1, x10);
				host_reg_to_shil_param(op.rd, x10);
				break;

			case shop_readm:
			{
				shil_param_to_host_reg(op.rs1, *call_regs[0]);
				if (!op.rs3.is_null())
				{
					shil_param_to_host_reg(op.rs3, w10);
					Add(*call_regs[0], *call_regs[0], w10);
				}

				u32 size = op.flags & 0x7f;

				switch (size)
				{
				case 1:
					CallRuntime(ReadMem8);
					Sxtb(w0, w0);
					break;

				case 2:
					CallRuntime(ReadMem16);
					Sxth(w0, w0);
					break;

				case 4:
					CallRuntime(ReadMem32);
					break;

				case 8:
					CallRuntime(ReadMem64);
					break;

				default:
					die("1..8 bytes");
					break;
				}

				if (size != 8)
					host_reg_to_shil_param(op.rd, w0);
				else
					host_reg_to_shil_param(op.rd, x0);
			}
			break;

			case shop_writem:
			{
				shil_param_to_host_reg(op.rs1, *call_regs[0]);
				if (!op.rs3.is_null())
				{
					shil_param_to_host_reg(op.rs3, w10);
					Add(*call_regs[0], *call_regs[0], w10);
				}

				u32 size = op.flags & 0x7f;
				if (size != 8)
					shil_param_to_host_reg(op.rs2, *call_regs[1]);
				else
					shil_param_to_host_reg(op.rs2, *call_regs64[1]);

				switch (size)
				{
				case 1:
					CallRuntime(WriteMem8);
					break;

				case 2:
					CallRuntime(WriteMem16);
					break;

				case 4:
					CallRuntime(WriteMem32);
					break;

				case 8:
					CallRuntime(WriteMem64);
					break;

				default:
					die("1..8 bytes");
					break;
				}
			}
			break;

			default:
				shil_chf[op.op](&op);
				break;
			}
		}

		Mov(x9, (size_t)&next_pc);

		switch (block->BlockType)
		{

		case BET_StaticJump:
		case BET_StaticCall:
			// next_pc = block->BranchBlock;
			Ldr(w10, block->BranchBlock);
			Str(w10, MemOperand(x9));
			break;

		case BET_Cond_0:
		case BET_Cond_1:
			{
				// next_pc = next_pc_value;
				// if (*jdyn == 0)
				//   next_pc = branch_pc_value;

				Mov(w10, block->NextBlock);

				if (block->has_jcond)
					Mov(x11, (size_t)&Sh4cntx.jdyn);
				else
					Mov(x11, (size_t)&sr.T);
				Ldr(w11, MemOperand(x11));

				Cmp(w11, block->BlockType & 1);
				Label branch_not_taken;

				B(ne, &branch_not_taken);
				Mov(w10, block->BranchBlock);
				Bind(&branch_not_taken);

				Str(w10, MemOperand(x9));
			}
			break;

		case BET_DynamicJump:
		case BET_DynamicCall:
		case BET_DynamicRet:
			// next_pc = *jdyn;
			Mov(x10, (size_t)&Sh4cntx.jdyn);
			Ldr(w10, MemOperand(x10));
			Str(w10, MemOperand(x9));
			break;

		case BET_DynamicIntr:
		case BET_StaticIntr:
			if (block->BlockType == BET_DynamicIntr)
			{
				// next_pc = *jdyn;
				Mov(x10, (size_t)&Sh4cntx.jdyn);
				Ldr(w10, MemOperand(x10));
			}
			else
			{
				// next_pc = next_pc_value;
				Mov(w10, block->NextBlock);
			}
			Str(w10, MemOperand(x9));

			CallRuntime(UpdateINTC);
			break;

		default:
			die("Invalid block end type");
		}


		Ldp(x29, x30, MemOperand(sp, 16, PostIndex));
		Ret();

		Label code_end;
		Bind(&code_end);

		FinalizeCode();

		block->code = GetBuffer()->GetStartAddress<DynarecCodeEntryPtr>();
		block->host_code_size = GetBuffer()->GetSizeInBytes();
		block->host_opcodes = GetLabelAddress<u32*>(&code_end) - GetBuffer()->GetStartAddress<u32*>();

		emit_Skip(block->host_code_size);
		CacheFlush((void*)block->code, GetBuffer()->GetEndAddress<void*>());
#if 0
		Instruction* instr_start = GetBuffer()->GetStartAddress<Instruction*>();
		Instruction* instr_end = GetLabelAddress<Instruction*>(&code_end);
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
#endif
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
					Mov(x9, (size_t)prm.reg_ptr());
					Ldr(*call_fregs[fregused], MemOperand(x9));
				}
				else {
					verify(prm.is_null());
				}
				fregused++;
				break;

			case CPT_ptr:
				verify(prm.is_reg());
				// push the ptr itself
				Mov(*call_regs64[regused++], (size_t)prm.reg_ptr());

				break;
			case CPT_u32rv:
			case CPT_u64rvL:
			case CPT_u64rvH:
			case CPT_f32rv:
				// return values are handled in ngen_CC_param()
				break;
			}
		}
		CallRuntime((void (*)())function);
	}

private:
	void CheckBlock(RuntimeBlockInfo* block)
	{
		s32 sz = block->sh4_code_size;
		u32 sa = block->addr;

		Label blockcheck_fail;
		Label blockcheck_success;

		while (sz > 0)
		{
			void* ptr = GetMemPtr(sa, sz > 8 ? 8 : sz);
			if (ptr != NULL)
			{
				Mov(x9, (size_t)reinterpret_cast<uintptr_t>(ptr));

				if (sz >= 8)
				{
					Ldr(x10, MemOperand(x9));
					Mov(x11, *(u64*)ptr);
					Cmp(x10, x11);
					sz -= 8;
					sa += 8;
				}
				else if (sz >= 4)
				{
					Ldr(w10, MemOperand(x9));
					Mov(w11, *(u32*)ptr);
					Cmp(w10, w11);
					sz -= 4;
					sa += 4;
				}
				else
				{
					Ldrh(w10, MemOperand(x9));
					Mov(w11, *(u16*)ptr);
					Cmp(w10, w11);
					sz -= 2;
					sa += 2;
				}
				B(ne, &blockcheck_fail);
			}
			else
			{
				sz -= 4;
				sa += 4;
			}
		}
		B(&blockcheck_success);

		Bind(&blockcheck_fail);
		Ldr(w0, block->addr);
		TailCallRuntime(ngen_blockcheckfail);

		Bind(&blockcheck_success);
	}

	void shil_param_to_host_reg(const shil_param& param, const Register& reg)
	{
		if (param.is_imm()) {
			Mov(reg, param._imm);
		}
		else if (param.is_reg()) {
			Mov(x9, (size_t)param.reg_ptr());
			Ldr(reg, MemOperand(x9));
		}
		else {
			verify(param.is_null());
		}
	}

	void host_reg_to_shil_param(const shil_param& param, const CPURegister& reg)
	{
		Mov(x9, (size_t)param.reg_ptr());
		Str(reg, MemOperand(x9));
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

#endif	// FEAT_SHREC == DYNAREC_JIT
