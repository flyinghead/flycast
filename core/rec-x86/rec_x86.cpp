/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "build.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86

#include "rec_x86.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/mem/_vmem.h"

static int cycle_counter;
static void (*mainloop)();
static void (*ngen_FailedToFindBlock_)();

static void (*intc_sched)();
static void (*no_update)();
static void (*ngen_LinkBlock_cond_Next_stub)();
static void (*ngen_LinkBlock_cond_Branch_stub)();
static void (*ngen_LinkBlock_Generic_stub)();
static void (*ngen_blockcheckfail)();

static X86Compiler* compiler;

static Xbyak::Operand::Code alloc_regs[] {  Xbyak::Operand::EBX,  Xbyak::Operand::EBP,  Xbyak::Operand::ESI,  Xbyak::Operand::EDI, (Xbyak::Operand::Code)-1 };
static s8 alloc_fregs[] = { 7, 6, 5, 4, -1 };
alignas(16) static f32 thaw_regs[4];

void X86RegAlloc::doAlloc(RuntimeBlockInfo* block)
{
	RegAlloc::DoAlloc(block, alloc_regs, alloc_fregs);
}
void X86RegAlloc::Preload(u32 reg, Xbyak::Operand::Code nreg)
{
	compiler->regPreload(reg, nreg);
}
void X86RegAlloc::Writeback(u32 reg, Xbyak::Operand::Code nreg)
{
	compiler->regWriteback(reg, nreg);
}
void X86RegAlloc::Preload_FPU(u32 reg, s8 nreg)
{
	compiler->regPreload_FPU(reg, nreg);
}
void X86RegAlloc::Writeback_FPU(u32 reg, s8 nreg)
{
	compiler->regWriteback_FPU(reg, nreg);
}

struct DynaRBI : RuntimeBlockInfo
{
	u32 Relink() override;

	void Relocate(void* dst) override {
		verify(false);
	}
};

void ngen_GetFeatures(ngen_features* dst)
{
	dst->InterpreterFallback = false;
	dst->OnlyDynamicEnds = false;
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	return new DynaRBI();
}

void X86Compiler::compile(RuntimeBlockInfo* block, bool force_checks, bool optimise)
{
	DEBUG_LOG(DYNAREC, "X86 compiling %08x to %p", block->addr, emit_GetCCPtr());
	current_opid = -1;

	checkBlock(force_checks, block);

	sub(dword[&cycle_counter], block->guest_cycles);
	Xbyak::Label no_up;
	jns(no_up);
	call((const void *)intc_sched);
	L(no_up);

	regalloc.doAlloc(block);

	for (current_opid = 0; current_opid < block->oplist.size(); current_opid++)
	{
		shil_opcode& op  = block->oplist[current_opid];

		regalloc.OpBegin(&op, current_opid);

		genOpcode(block, optimise, op);

		regalloc.OpEnd(&op);
	}
	regalloc.Cleanup();
	current_opid = -1;

	block->relink_offset = getCurr() - getCode();
	block->relink_data = 0;
	relinkBlock(block);

	block->code = (DynarecCodeEntryPtr)getCode();
	block->host_code_size = getSize();

	emit_Skip(getSize());
}

u32 X86Compiler::relinkBlock(RuntimeBlockInfo* block)
{
	const u8 *startPosition = getCurr();

#define BLOCK_LINKING
#ifndef BLOCK_LINKING
	switch (block->BlockType) {

	case BET_StaticJump:
	case BET_StaticCall:
		//next_pc = block->BranchBlock;
		mov(ecx, block->BranchBlock);
		break;

	case BET_Cond_0:
	case BET_Cond_1:
		{
			//next_pc = next_pc_value;
			//if (*jdyn == 0)
			//next_pc = branch_pc_value;

			mov(ecx, block->NextBlock);

			cmp(dword[GetRegPtr(block->has_jcond ? reg_pc_dyn : reg_sr_T)], (u32)block->BlockType & 1);
			Xbyak::Label branch_not_taken;

			jne(branch_not_taken, T_SHORT);
			mov(ecx, block->BranchBlock);
			L(branch_not_taken);
		}
		break;

	case BET_DynamicJump:
	case BET_DynamicCall:
	case BET_DynamicRet:
		//next_pc = *jdyn;
		mov(ecx, dword[GetRegPtr(reg_pc_dyn)]);
		break;

	case BET_DynamicIntr:
	case BET_StaticIntr:
		if (block->BlockType == BET_DynamicIntr)
		{
			//next_pc = *jdyn;
			mov(ecx, dword[GetRegPtr(reg_pc_dyn)]);
			mov(dword[&next_pc], ecx);
		}
		else
		{
			//next_pc = next_pc_value;
			mov(dword[&next_pc], block->NextBlock);
		}
		call(UpdateINTC);
		mov(ecx, dword[&next_pc]);
		break;

	default:
		die("Invalid block end type");
	}

	jmp((const void *)no_update);

#else

	switch(block->BlockType)
	{
	case BET_Cond_0:
	case BET_Cond_1:
		{
			cmp(dword[GetRegPtr(block->has_jcond ? reg_pc_dyn : reg_sr_T)], (u32)block->BlockType & 1);

			Xbyak::Label noBranch;

			jne(noBranch);
			{
				//branch block
				if (block->pBranchBlock)
					jmp((const void *)block->pBranchBlock->code, T_NEAR);
				else
					call(ngen_LinkBlock_cond_Branch_stub);
			}
			L(noBranch);
			{
				//no branch block
				if (block->pNextBlock)
					jmp((const void *)block->pNextBlock->code, T_NEAR);
				else
					call(ngen_LinkBlock_cond_Next_stub);
			}
		}
		break;


	case BET_DynamicRet:
	case BET_DynamicCall:
	case BET_DynamicJump:
		mov(ecx, dword[GetRegPtr(reg_pc_dyn)]);
		jmp((const void *)no_update);

		break;

	case BET_StaticCall:
	case BET_StaticJump:
		if (block->pBranchBlock)
			jmp((const void *)block->pBranchBlock->code, T_NEAR);
		else
			call(ngen_LinkBlock_Generic_stub);
		break;

	case BET_StaticIntr:
	case BET_DynamicIntr:
		if (block->BlockType == BET_StaticIntr)
		{
			mov(dword[&next_pc], block->NextBlock);
		}
		else
		{
			mov(eax, dword[GetRegPtr(reg_pc_dyn)]);
			mov(dword[&next_pc], eax);
		}
		call(UpdateINTC);

		mov(ecx, dword[&next_pc]);
		jmp((const void *)no_update);

		break;

	default:
		die("Invalid block end type");
	}
#endif

	ready();

	return getCurr() - startPosition;
}

u32 DynaRBI::Relink()
{
	X86Compiler *compiler = new X86Compiler((u8*)code + relink_offset);
	u32 codeSize = compiler->relinkBlock(this);
	delete compiler;

	return codeSize;
}

void X86Compiler::ngen_CC_param(const shil_opcode& op, const shil_param& param, CanonicalParamType tp)
{
	switch (tp)
	{
		//push the contents
		case CPT_u32:
		case CPT_f32:
			if (param.is_reg())
			{
				if (regalloc.IsAllocg(param))
					push(regalloc.MapRegister(param));
				else if (regalloc.IsAllocf(param))
				{
					sub(esp, 4);
					movss(dword[esp], regalloc.MapXRegister(param));
				}
				else
					die("Must not happen!");
			}
			else if (param.is_imm())
				push(param.imm_value());
			else
				die("invalid combination");
			CC_stackSize += 4;
			break;

		//push the ptr itself
		case CPT_ptr:
			verify(param.is_reg());

			push((unat)param.reg_ptr());
			CC_stackSize += 4;
			break;

		// store from EAX
		case CPT_u64rvL:
		case CPT_u32rv:
			if (regalloc.IsAllocg(param))
				mov(regalloc.MapRegister(param), eax);
			/*else if (regalloc.IsAllocf(param))
				mov(regalloc.MapXRegister(param), eax); */
			else
				die("Must not happen!");
			break;

		// store from EDX
		case CPT_u64rvH:
			if (regalloc.IsAllocg(param))
				mov(regalloc.MapRegister(param), edx);
			else
				die("Must not happen!");
			break;

		// store from ST(0)
		case CPT_f32rv:
			verify(regalloc.IsAllocf(param));
			fstp(dword[param.reg_ptr()]);
			movss(regalloc.MapXRegister(param), dword[param.reg_ptr()]);
			break;
	}
}

void X86Compiler::freezeXMM()
{
	s8 *fpreg = alloc_fregs;
	f32 *slpc = thaw_regs;
	while (*fpreg != -1)
	{
		//if (regalloc.SpanNRegfIntr(current_opid, *fpreg))
		if (current_opid != (size_t)-1 && regalloc.IsMapped(Xbyak::Xmm(*fpreg), current_opid))
			movss(dword[slpc++], Xbyak::Xmm(*fpreg));
		fpreg++;
	}
}

void X86Compiler::thawXMM()
{
	s8* fpreg = alloc_fregs;
	f32* slpc = thaw_regs;
	while (*fpreg != -1)
	{
		//if (regalloc.SpanNRegfIntr(current_opid, *fpreg))
		if (current_opid != (size_t)-1 && regalloc.IsMapped(Xbyak::Xmm(*fpreg), current_opid))
			movss(Xbyak::Xmm(*fpreg), dword[slpc++]);
		fpreg++;
	}
}

void X86Compiler::genMainloop()
{
	push(esi);
	push(edi);
	push(ebp);
	push(ebx);
#ifndef _WIN32
	// 16-byte alignment
	sub(esp, 12);
#endif

	mov(ecx, dword[&Sh4cntx.pc]);

	mov(dword[&cycle_counter], SH4_TIMESLICE);

	mov(eax, 0);
	//next_pc _MUST_ be on ecx
	Xbyak::Label do_iter;
	Xbyak::Label cleanup;
//no_update:
	Xbyak::Label no_updateLabel;
	L(no_updateLabel);
	mov(esi, ecx);	// save sh4 pc in ESI, used below if FPCB is still empty for this address
	mov(eax, (size_t)&p_sh4rcb->fpcb[0]);
	and_(ecx, RAM_SIZE_MAX - 2);
	jmp(dword[eax + ecx * 2]);

//intc_sched:
	Xbyak::Label intc_schedLabel;
	L(intc_schedLabel);
	add(dword[&cycle_counter], SH4_TIMESLICE);
	call((void *)UpdateSystem);
	cmp(eax, 0);
	jnz(do_iter);
	ret();

//do_iter:
	L(do_iter);
	pop(ecx);
	call((void *)rdv_DoInterrupts);
	mov(ecx, eax);
	mov(edx, dword[&Sh4cntx.CpuRunning]);
	cmp(edx, 0);
	jz(cleanup);
	jmp(no_updateLabel);

//cleanup:
	L(cleanup);
#ifndef _WIN32
	// 16-byte alignment
	add(esp, 12);
#endif
	pop(ebx);
	pop(ebp);
	pop(edi);
	pop(esi);

	ret();

//ngen_LinkBlock_Shared_stub:
	Xbyak::Label ngen_LinkBlock_Shared_stub;
	L(ngen_LinkBlock_Shared_stub);
	pop(ecx);
	sub(ecx, 5);
	call((void *)rdv_LinkBlock);
	jmp(eax);

//ngen_LinkBlock_cond_Next_stub:
	Xbyak::Label ngen_LinkBlock_cond_Next_label;
	L(ngen_LinkBlock_cond_Next_label);
	mov(edx, 0);
	jmp(ngen_LinkBlock_Shared_stub);

//ngen_LinkBlock_cond_Branch_stub:
	Xbyak::Label ngen_LinkBlock_cond_Branch_label;
	L(ngen_LinkBlock_cond_Branch_label);
	mov(edx, 1);
	jmp(ngen_LinkBlock_Shared_stub);

//ngen_LinkBlock_Generic_stub:
	Xbyak::Label ngen_LinkBlock_Generic_label;
	L(ngen_LinkBlock_Generic_label);
	mov(edx, dword[&Sh4cntx.jdyn]);
	jmp(ngen_LinkBlock_Shared_stub);

//ngen_FailedToFindBlock_:
	Xbyak::Label failedToFindBlock;
	L(failedToFindBlock);
	mov(ecx, esi);	// get back the saved sh4 PC saved above
	call((void *)rdv_FailedToFindBlock);
	jmp(eax);

//ngen_blockcheckfail:
	Xbyak::Label ngen_blockcheckfailLabel;
	L(ngen_blockcheckfailLabel);
	call((void *)rdv_BlockCheckFail);
	jmp(eax);

	genMemHandlers();

	ready();

	mainloop = (void (*)())getCode();
	ngen_FailedToFindBlock_ = (void (*)())failedToFindBlock.getAddress();
	intc_sched = (void (*)())intc_schedLabel.getAddress();
	no_update = (void (*)())no_updateLabel.getAddress();
	ngen_LinkBlock_cond_Next_stub = (void (*)())ngen_LinkBlock_cond_Next_label.getAddress();
	ngen_LinkBlock_cond_Branch_stub = (void (*)())ngen_LinkBlock_cond_Branch_label.getAddress();
	ngen_LinkBlock_Generic_stub = (void (*)())ngen_LinkBlock_Generic_label.getAddress();
	ngen_blockcheckfail = (void (*)())ngen_blockcheckfailLabel.getAddress();

	emit_Skip(getSize());
}

bool X86Compiler::genReadMemImmediate(const shil_opcode& op, RuntimeBlockInfo* block)
{
	if (!op.rs1.is_imm())
		return false;
	u32 size = op.flags & 0x7f;
	u32 addr = op.rs1.imm_value();
	bool isram = false;
	void* ptr = _vmem_read_const(addr, isram, size > 4 ? 4 : size);

	if (isram)
	{
		// Immediate pointer to RAM: super-duper fast access
		switch (size)
		{
		case 1:
			if (regalloc.IsAllocg(op.rd))
				movsx(regalloc.MapRegister(op.rd), byte[ptr]);
			else
			{
				movsx(eax, byte[ptr]);
				mov(dword[op.rd.reg_ptr()], eax);
			}
			break;

		case 2:
			if (regalloc.IsAllocg(op.rd))
				movsx(regalloc.MapRegister(op.rd), word[ptr]);
			else
			{
				movsx(eax, word[ptr]);
				mov(dword[op.rd.reg_ptr()], eax);
			}
			break;

		case 4:
			if (regalloc.IsAllocg(op.rd))
				mov(regalloc.MapRegister(op.rd), dword[ptr]);
			else if (regalloc.IsAllocf(op.rd))
				movd(regalloc.MapXRegister(op.rd), dword[ptr]);
			else
			{
				mov(eax, dword[ptr]);
				mov(dword[op.rd.reg_ptr()], eax);
			}
			break;

		case 8:
#ifdef EXPLODE_SPANS
			if (op.rd.count() == 2 && regalloc.IsAllocf(op.rd, 0) && regalloc.IsAllocf(op.rd, 1))
			{
				movd(regalloc.MapXRegister(op.rd, 0), dword[ptr]);
				movd(regalloc.MapXRegister(op.rd, 1), dword[(u32 *)ptr + 1]);
			}
			else
#endif
			{
				movq(xmm0, qword[ptr]);
				movq(qword[op.rd.reg_ptr()], xmm0);
			}
			break;

		default:
			die("Invalid immediate size");
				break;
		}
	}
	else
	{
		// Not RAM: the returned pointer is a memory handler
		if (size == 8)
		{
			verify(!regalloc.IsAllocAny(op.rd));

			// Need to call the handler twice
			mov(ecx, addr);
			genCall((void (DYNACALL *)())ptr);
			mov(dword[op.rd.reg_ptr()], eax);

			mov(ecx, addr + 4);
			genCall((void (DYNACALL *)())ptr);
			mov(dword[op.rd.reg_ptr() + 1], eax);
		}
		else
		{
			mov(ecx, addr);

			switch(size)
			{
			case 1:
				genCall((void (DYNACALL *)())ptr);
				movsx(eax, al);
				break;

			case 2:
				genCall((void (DYNACALL *)())ptr);
				movsx(eax, ax);
				break;

			case 4:
				genCall((void (DYNACALL *)())ptr);
				break;

			default:
				die("Invalid immediate size");
					break;
			}
			host_reg_to_shil_param(op.rd, eax);
		}
	}

	return true;
}

bool X86Compiler::genWriteMemImmediate(const shil_opcode& op, RuntimeBlockInfo* block)
{
	if (!op.rs1.is_imm())
		return false;
	u32 size = op.flags & 0x7f;
	u32 addr = op.rs1.imm_value();
	bool isram = false;
	void* ptr = _vmem_write_const(addr, isram, size > 4 ? 4 : size);

	if (isram)
	{
		// Immediate pointer to RAM: super-duper fast access
		switch (size)
		{
		case 1:
			if (regalloc.IsAllocg(op.rs2))
			{
				Xbyak::Reg32 rs2 = regalloc.MapRegister(op.rs2);
				if (rs2.getIdx() >= 4)
				{
					mov(eax, rs2);
					mov(byte[ptr], al);
				}
				else
					mov(byte[ptr], rs2.cvt8());
			}
			else if (op.rs2.is_imm())
				mov(byte[ptr], (u8)op.rs2.imm_value());
			else
			{
				mov(al, byte[op.rs2.reg_ptr()]);
				mov(byte[ptr], al);
			}
			break;

		case 2:
			if (regalloc.IsAllocg(op.rs2))
				mov(word[ptr], regalloc.MapRegister(op.rs2).cvt16());
			else if (op.rs2.is_imm())
				mov(word[ptr], (u16)op.rs2.imm_value());
			else
			{
				mov(cx, word[op.rs2.reg_ptr()]);
				mov(word[ptr], cx);
			}
			break;

		case 4:
			if (regalloc.IsAllocg(op.rs2))
				mov(dword[ptr], regalloc.MapRegister(op.rs2));
			else if (regalloc.IsAllocf(op.rs2))
				movd(dword[ptr], regalloc.MapXRegister(op.rs2));
			else if (op.rs2.is_imm())
				mov(dword[ptr], op.rs2.imm_value());
			else
			{
				mov(ecx, dword[op.rs2.reg_ptr()]);
				mov(dword[ptr], ecx);
			}
			break;

		case 8:
#ifdef EXPLODE_SPANS
			if (op.rs2.count() == 2 && regalloc.IsAllocf(op.rs2, 0) && regalloc.IsAllocf(op.rs2, 1))
			{
				movd(dword[ptr], regalloc.MapXRegister(op.rs2, 0));
				movd(dword[(u32 *)ptr + 1], regalloc.MapXRegister(op.rs2, 1));
			}
			else
#endif
			{
				movq(xmm0, qword[op.rs2.reg_ptr()]);
				movq(qword[ptr], xmm0);
			}
			break;

		default:
			die("Invalid immediate size");
			break;
		}
	}
	else
	{
		// Not RAM: the returned pointer is a memory handler
		mov(ecx, addr);
		shil_param_to_host_reg(op.rs2, edx);

		genCall((void (DYNACALL *)())ptr);
	}

	return true;
}

void X86Compiler::checkBlock(bool smc_checks, RuntimeBlockInfo* block)
{
	if (!smc_checks)
		return;

	mov(ecx, block->addr);
	s32 sz = block->sh4_code_size;
	u32 sa = block->addr;
	while (sz > 0)
	{
		void* p = GetMemPtr(sa, 4);
		if (p)
		{
			if (sz == 2)
				cmp(word[p], (u32)*(s16*)p);
			else
				cmp(dword[p], *(u32*)p);
			jne((const void *)ngen_blockcheckfail);
		}
		sz -= 4;
		sa += 4;
	}
}

void ngen_init()
{
}

void ngen_ResetBlocks()
{
	// Avoid generating the main loop more than once
	if (mainloop != nullptr)
		return;

	compiler = new X86Compiler();

	try {
		compiler->genMainloop();
	} catch (const Xbyak::Error& e) {
		ERROR_LOG(DYNAREC, "Fatal xbyak error: %s", e.what());
	}

	delete compiler;
	compiler = nullptr;
	emit_SetBaseAddr();

	ngen_FailedToFindBlock = ngen_FailedToFindBlock_;
}

void ngen_mainloop(void* v_cntx)
{
	try {
		mainloop();
	} catch (const SH4ThrownException&) {
		ERROR_LOG(DYNAREC, "SH4ThrownException in mainloop");
	} catch (...) {
		ERROR_LOG(DYNAREC, "Uncaught unknown exception in mainloop");
	}
}

void ngen_Compile(RuntimeBlockInfo* block, bool smc_checks, bool, bool, bool optimise)
{
	verify(emit_FreeSpace() >= 16 * 1024);

	compiler = new X86Compiler();

	try {
		compiler->compile(block, smc_checks, optimise);
	} catch (const Xbyak::Error& e) {
		ERROR_LOG(DYNAREC, "Fatal xbyak error: %s", e.what());
	}

	delete compiler;
}

bool ngen_Rewrite(host_context_t &context, void *faultAddress)
{
	u8 *rewriteAddr = *(u8 **)context.esp - 5;
	X86Compiler *compiler = new X86Compiler(rewriteAddr);
	bool rv = compiler->rewriteMemAccess(context);
	delete compiler;

	return rv;
}

void ngen_CC_Start(shil_opcode* op)
{
	compiler->ngen_CC_Start(*op);
}

void ngen_CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
{
	compiler->ngen_CC_param(*op, *par, tp);
}

void ngen_CC_Call(shil_opcode* op, void* function)
{
	compiler->ngen_CC_Call(*op, function);
}

void ngen_CC_Finish(shil_opcode* op)
{
	compiler->ngen_CC_Finish(*op);
}
#endif
