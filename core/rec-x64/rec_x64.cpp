#include "xbyak\xbyak.h"

#include "types.h"

#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/dyna/regalloc.h"
#include "emitter/x86_emitter.h"
#include "profiler/profiler.h"
#include "oslib/oslib.h"


struct DynaRBI : RuntimeBlockInfo
{
	virtual u32 Relink() {
		//verify(false);
		return 0;
	}

	virtual void Relocate(void* dst) {
		verify(false);
	}
};



int cycle_counter;

void ngen_FailedToFindBlock_internal() {
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

void(*ngen_FailedToFindBlock)() = &ngen_FailedToFindBlock_internal;

void ngen_mainloop(void* v_cntx)
{
	auto ctx = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4RCB));

	cycle_counter = SH4_TIMESLICE;

	for (;;) {
		do {
			auto rcb = bm_GetCode(ctx->cntx.pc);
			rcb();
		} while (cycle_counter > 0);

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
	dst->InterpreterFallback = true;
	dst->OnlyDynamicEnds = true;
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	return new DynaRBI();
}

u32* GetRegPtr(u32 reg)
{
	return Sh4_int_GetRegisterPtr((Sh4RegType)reg);
}

class BlockCompiler : Xbyak::CodeGenerator{
public:
	BlockCompiler() : Xbyak::CodeGenerator(64 * 1024, emit_GetCCPtr()) {

	}

	void compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise) {
		mov(rax, (size_t)&cycle_counter);

		sub(dword[rax], block->guest_cycles);

		sub(rsp, 8);

		for (auto op : block->oplist) {
			switch (op.op) {

			case shop_ifb:
				if (op.rs1._imm)
				{
					mov(rax, (size_t)&next_pc);
					mov(dword[rax], op.rs2._imm);
				}

				mov(rcx, op.rs3._imm);

				call(OpDesc[op.rs3._imm]->oph);
				break;

			case shop_jdyn:
				{
					mov(rax, (size_t)op.rs1.reg_ptr());

					mov(ecx, dword[rax]);

					if (op.rs2.is_imm()) {
						add(ecx, op.rs2._imm);
					}
					mov(dword[rax], ecx);
				}
				break;

			case shop_mov32:
			{
				verify(op.rd.is_reg());

				verify(op.rs1.is_reg() || op.rs1.is_imm());

				if (op.rs1.is_imm()) {
					mov(ecx, op.rs1._imm);
				}
				else {
					mov(rax, (size_t)op.rs1.reg_ptr());

					mov(ecx, dword[rax]);
				}

				mov(rax, (size_t)op.rd.reg_ptr());

				mov(dword[rax], ecx);
			}
			break;

			default:
				shil_chf[op.op](&op);
				break;
			}
		}

		verify(block->BlockType == BET_DynamicJump);

		add(rsp, 8);
		ret();

		ready();

		block->code = (DynarecCodeEntryPtr)getCode();

		emit_Skip(getSize());
	}
};

void ngen_Compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
{
	verify(emit_FreeSpace() >= 64 * 1024);

	auto compiler = new BlockCompiler();

	
	compiler->compile(block, force_checks, reset, staging, optimise);
}

u32 ngen_CC_BytesPushed;
void ngen_CC_Start(shil_opcode* op)
{
	ngen_CC_BytesPushed = 0;
}

void ngen_CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
{
#if 0
	switch (tp)
	{
		//push the contents
	case CPT_u32:
	case CPT_f32:
		if (par->is_reg())
		{
			if (reg.IsAllocg(*par))
				x86e->Emit(op_push32, reg.mapg(*par));
			else if (reg.IsAllocf(*par))
			{
				x86e->Emit(op_sub32, ESP, 4);
				x86e->Emit(op_movss, x86_mrm(ESP), reg.mapf(*par));
			}
			else
			{
				die("Must not happen !\n");
				x86e->Emit(op_push32, x86_ptr(par->reg_ptr()));
			}
		}
		else if (par->is_imm())
			x86e->Emit(op_push, par->_imm);
		else
			die("invalid combination");
		ngen_CC_BytesPushed += 4;
		break;
		//push the ptr itself
	case CPT_ptr:
		verify(par->is_reg());

		die("FAIL");
		x86e->Emit(op_push, (unat)par->reg_ptr());

		for (u32 ri = 0; ri<(*par).count(); ri++)
		{
			if (reg.IsAllocf(*par, ri))
			{
				x86e->Emit(op_sub32, ESP, 4);
				x86e->Emit(op_movss, x86_mrm(ESP), reg.mapfv(*par, ri));
			}
			else
			{
				verify(!reg.IsAllocAny((Sh4RegType)(par->_reg + ri)));
			}
		}


		ngen_CC_BytesPushed += 4;
		break;

		//store from EAX
	case CPT_u64rvL:
	case CPT_u32rv:
		if (reg.IsAllocg(*par))
			x86e->Emit(op_mov32, reg.mapg(*par), EAX);
		/*else if (reg.IsAllocf(*par))
		x86e->Emit(op_movd_xmm_from_r32,reg.mapf(*par),EAX);*/
		else
			die("Must not happen!\n");
		break;

	case CPT_u64rvH:
		if (reg.IsAllocg(*par))
			x86e->Emit(op_mov32, reg.mapg(*par), EDX);
		else
			die("Must not happen!\n");
		break;

		//Store from ST(0)
	case CPT_f32rv:
		verify(reg.IsAllocf(*par));
		x86e->Emit(op_fstp32f, x86_ptr(par->reg_ptr()));
		x86e->Emit(op_movss, reg.mapf(*par), x86_ptr(par->reg_ptr()));
		break;

	}

#endif
}

void ngen_CC_Call(shil_opcode*op, void* function)
{
	/*
		reg.FreezeXMM();
		x86e->Emit(op_call, x86_ptr_imm(function));
		reg.ThawXMM();
	*/
}
void ngen_CC_Finish(shil_opcode* op)
{
	/*
		x86e->Emit(op_add32, ESP, ngen_CC_BytesPushed);
	*/
}