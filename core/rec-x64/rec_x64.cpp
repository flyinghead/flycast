#include "deps/xbyak/xbyak.h"

#include "types.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_mem.h"
#include "emitter/x86_emitter.h"
#include "profiler/profiler.h"
#include "oslib/oslib.h"
#include "x64_regalloc.h"

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

void ngen_mainloop(void* v_cntx)
{
	__asm__ volatile (
			"pushq %%rbx					\n\t"
			"pushq %%rbp					\n\t"
			"pushq %%r12					\n\t"
			"pushq %%r13					\n\t"
			"pushq %%r14					\n\t"
			"pushq %%r15					\n\t"
			"subq $136, %%rsp				\n\t"	// 128 for xmm regs plus 8 for stack 16-byte alignment
			"vmovdqu %%xmm8, 0(%%rsp)		\n\t"
			"vmovdqu %%xmm9, 16(%%rsp)		\n\t"
			"vmovdqu %%xmm10, 32(%%rsp)		\n\t"
			"vmovdqu %%xmm11, 48(%%rsp)		\n\t"
			"vmovdqu %%xmm12, 64(%%rsp) 	\n\t"
			"vmovdqu %%xmm13, 80(%%rsp)		\n\t"
			"vmovdqu %%xmm14, 96(%%rsp)		\n\t"
			"vmovdqu %%xmm15, 112(%%rsp)	\n\t"
			"movl %2, cycle_counter(%%rip)	\n"		// SH4_TIMESLICE

		"run_loop:							\n\t"
			"movq p_sh4rcb(%%rip), %%rax	\n\t"
			"movl %p0(%%rax), %%edx			\n\t"	// CpuRunning
			"testl %%edx, %%edx				\n\t"
			"je end_run_loop				\n"

		"slice_loop:						\n\t"
			"movq p_sh4rcb(%%rip), %%rax	\n\t"
#ifdef _WIN32
			"movl %p1(%%rax), %%ecx			\n\t"	// pc
#else
			"movl %p1(%%rax), %%edi			\n\t"	// pc
#endif
			"call _Z10bm_GetCodej			\n\t"	// was bm_GetCode2
			"call *%%rax					\n\t"
			"movl cycle_counter(%%rip), %%ecx \n\t"
			"testl %%ecx, %%ecx				\n\t"
			"jg slice_loop					\n\t"

			"addl %2, %%ecx					\n\t"	// SH4_TIMESLICE
			"movl %%ecx, cycle_counter(%%rip)	\n\t"
			"call UpdateSystem_INTC			\n\t"
			"jmp run_loop					\n"

		"end_run_loop:						\n\t"
			"vmovdqu 0(%%rsp), %%xmm8		\n\t"
			"vmovdqu 16(%%rsp), %%xmm9		\n\t"
			"vmovdqu 32(%%rsp), %%xmm10		\n\t"
			"vmovdqu 48(%%rsp), %%xmm11		\n\t"
			"vmovdqu 64(%%rsp), %%xmm12 	\n\t"
			"vmovdqu 80(%%rsp), %%xmm13		\n\t"
			"vmovdqu 96(%%rsp), %%xmm14		\n\t"
			"vmovdqu 112(%%rsp), %%xmm15	\n\t"
			"addq $136, %%rsp				\n\t"
			"popq %%r15						\n\t"
			"popq %%r14						\n\t"
			"popq %%r13						\n\t"
			"popq %%r12						\n\t"
			"popq %%rbp						\n\t"
			"popq %%rbx						\n\t"
			:
			: "i"(offsetof(Sh4RCB, cntx.CpuRunning)),
			  "i"(offsetof(Sh4RCB, cntx.pc)),
			  "i"(SH4_TIMESLICE)
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
	printf("X64 JIT: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
}

class BlockCompiler : public Xbyak::CodeGenerator
{
public:
	BlockCompiler() : Xbyak::CodeGenerator(64 * 1024, emit_GetCCPtr()), regalloc(this)
	{
		#if HOST_OS == OS_WINDOWS
			call_regs.push_back(ecx);
			call_regs.push_back(edx);
			call_regs.push_back(r8d);
			call_regs.push_back(r9d);

			call_regs64.push_back(rcx);
			call_regs64.push_back(rdx);
			call_regs64.push_back(r8);
			call_regs64.push_back(r9);
		#else
			call_regs.push_back(edi);
			call_regs.push_back(esi);
			call_regs.push_back(edx);
			call_regs.push_back(ecx);

			call_regs64.push_back(rdi);
			call_regs64.push_back(rsi);
			call_regs64.push_back(rdx);
			call_regs64.push_back(rcx);
		#endif

		call_regsxmm.push_back(xmm0);
		call_regsxmm.push_back(xmm1);
		call_regsxmm.push_back(xmm2);
		call_regsxmm.push_back(xmm3);
	}

	void compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
	{
		//printf("X86_64 compiling %08x to %p\n", block->addr, emit_GetCCPtr());
		if (force_checks) {
			CheckBlock(block);
		}
		regalloc.DoAlloc(block);

		mov(rax, (size_t)&cycle_counter);
		sub(dword[rax], block->guest_cycles);
#ifdef _WIN32
		sub(rsp, 0x28);		// 32-byte shadow space + 8 byte alignment
#else
		sub(rsp, 0x8);		// align stack
#endif

		for (size_t i = 0; i < block->oplist.size(); i++)
		{
			shil_opcode& op  = block->oplist[i];

			regalloc.OpBegin(&op, i);

			switch (op.op) {

			case shop_ifb:
				if (op.rs1._imm)
				{
					mov(rax, (size_t)&next_pc);
					mov(dword[rax], op.rs2._imm);
				}

				mov(call_regs[0], op.rs3._imm);

				call((void*)OpDesc[op.rs3._imm]->oph);
				break;

			case shop_jcond:
			case shop_jdyn:
				{
					if (op.rs2.is_imm())
					{
						mov(ecx, regalloc.MapRegister(op.rs1));
						add(ecx, op.rs2._imm);
						mov(regalloc.MapRegister(op.rd), ecx);
					}
					else
						mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				}
				break;

			case shop_mov32:
			{
				verify(op.rd.is_reg());
				verify(op.rs1.is_reg() || op.rs1.is_imm());

				if (regalloc.IsAllocf(op.rd))
					shil_param_to_host_reg(op.rs1, regalloc.MapXRegister(op.rd));
				else
					shil_param_to_host_reg(op.rs1, regalloc.MapRegister(op.rd));
			}
			break;

			case shop_mov64:
			{
				verify(op.rd.is_reg());
				verify(op.rs1.is_reg() || op.rs1.is_imm());

				if (op.rs1.is_imm())
				{
					mov(rax, op.rs1._imm);
				}
				else
				{
					mov(rax, (uintptr_t)op.rs1.reg_ptr());
					mov(rax, qword[rax]);
				}
				mov(rcx, (uintptr_t)op.rd.reg_ptr());
				mov(qword[rcx], rax);
			}
			break;

			case shop_readm:
			{
				shil_param_to_host_reg(op.rs1, call_regs[0]);
				if (!op.rs3.is_null())
				{
					if (op.rs3.is_imm())
						add(call_regs[0], op.rs3._imm);
					else
					{
						shil_param_to_host_reg(op.rs3, eax);
						add(call_regs[0], eax);
					}
				}

				u32 size = op.flags & 0x7f;

				if (size == 1) {
					call((void*)ReadMem8);
					movsx(rcx, al);
				}
				else if (size == 2) {
					call((void*)ReadMem16);
					movsx(rcx, ax);
				}
				else if (size == 4) {
					call((void*)ReadMem32);
					mov(rcx, rax);
				}
				else if (size == 8) {
					call((void*)ReadMem64);
					mov(rcx, rax);
				}
				else {
					die("1..8 bytes");
				}

				if (size != 8)
					host_reg_to_shil_param(op.rd, ecx);
				else {
					mov(rax, (uintptr_t)GetRegPtr(op.rd._reg));
					mov(qword[rax], rcx);
				}
			}
			break;

			case shop_writem:
			{
				u32 size = op.flags & 0x7f;
				shil_param_to_host_reg(op.rs1, call_regs[0]);
				if (!op.rs3.is_null())
				{
					if (op.rs3.is_imm())
						add(call_regs[0], op.rs3._imm);
					else
					{
						shil_param_to_host_reg(op.rs3, eax);
						add(call_regs[0], eax);
					}
				}

				if (size != 8)
					shil_param_to_host_reg(op.rs2, call_regs[1]);
				else {
					mov(rax, (uintptr_t)GetRegPtr(op.rs2._reg));
					mov(call_regs64[1], qword[rax]);
				}

				if (size == 1)
					call((void*)WriteMem8);
				else if (size == 2)
					call((void*)WriteMem16);
				else if (size == 4)
					call((void*)WriteMem32);
				else if (size == 8)
					call((void*)WriteMem64);
				else {
					die("1..8 bytes");
				}
			}
			break;

			case shop_sync_sr:
				call(UpdateSR);
				break;
			case shop_sync_fpscr:
				call(UpdateFPSCR);
				break;
/*
			case shop_swaplb:
				Mov(w9, Operand(regalloc.MapRegister(op.rs1), LSR, 16));
				Rev16(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				Bfc(regalloc.MapRegister(op.rd), 16, 16);
				Orr(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd), Operand(w9, LSL, 16));
				break;
*/
			case shop_neg:
				if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
					mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				neg(regalloc.MapRegister(op.rd));
				break;
			case shop_not:
				if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
					mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				not(regalloc.MapRegister(op.rd));
				break;

			case shop_and:
				GenBinaryOp(op, &BlockCompiler::and);
				break;
			case shop_or:
				GenBinaryOp(op, &BlockCompiler::or);
				break;
			case shop_xor:
				GenBinaryOp(op, &BlockCompiler::xor);
				break;
			case shop_add:
				GenBinaryOp(op, &BlockCompiler::add);
				break;
			case shop_sub:
				GenBinaryOp(op, &BlockCompiler::sub);
				break;

#define SHIFT_OP(natop) \
				if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))	\
					mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));	\
				if (op.rs2.is_imm())	\
					natop(regalloc.MapRegister(op.rd), op.rs2._imm);	\
				else if (op.rs2.is_reg())	\
					natop(regalloc.MapRegister(op.rd), Xbyak::Reg8(regalloc.MapRegister(op.rs2).getIdx()));
			case shop_shl:
				SHIFT_OP(shl)
				break;
			case shop_shr:
				SHIFT_OP(shr)
				break;
			case shop_sar:
				SHIFT_OP(sar)
				break;
			case shop_ror:
				SHIFT_OP(ror)
				break;

			case shop_adc:
				if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
					mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				cmp(regalloc.MapRegister(op.rs3), 1);	// C = ~rs3
				cmc();		// C = rs3
				mov(ecx, 1);
				mov(regalloc.MapRegister(op.rd2), 0);
				adc(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs2)); // (C,rd)=rs1+rs2+rs3(C)
				cmovc(regalloc.MapRegister(op.rd2), ecx);	// rd2 = C
				break;
			/* FIXME buggy
			case shop_sbc:
				if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
					mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				cmp(regalloc.MapRegister(op.rs3), 1);	// C = ~rs3
				cmc();		// C = rs3
				mov(ecx, 1);
				mov(regalloc.MapRegister(op.rd2), 0);
				mov(eax, regalloc.MapRegister(op.rs2));
				neg(eax);
				adc(regalloc.MapRegister(op.rd), eax); // (C,rd)=rs1-rs2+rs3(C)
				cmovc(regalloc.MapRegister(op.rd2), ecx);	// rd2 = C
				break;
			*/
			case shop_rocr:
			case shop_rocl:
				if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
					mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				cmp(regalloc.MapRegister(op.rs2), 1);	// C = ~rs2
				cmc();		// C = rs2
				mov(eax, 1);
				mov(regalloc.MapRegister(op.rd2), 0);
				if (op.op == shop_rocr)
					rcr(regalloc.MapRegister(op.rd), 1);
				else
					rcl(regalloc.MapRegister(op.rd), 1);
				cmovc(regalloc.MapRegister(op.rd2), eax);	// rd2 = C
				break;

			case shop_shld:
			case shop_shad:
				{
					if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
						mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
					Xbyak::Label negative_shift;
					Xbyak::Label non_zero;
					Xbyak::Label exit;

					mov(ecx, regalloc.MapRegister(op.rs2));
					cmp(ecx, 0);
					js(negative_shift);
					shl(regalloc.MapRegister(op.rd), cl);
					jmp(exit);

					L(negative_shift);
					test(ecx, 0x1f);
					jnz(non_zero);
					if (op.op == shop_shld)
						mov(regalloc.MapRegister(op.rd), 0);
					else
						sar(regalloc.MapRegister(op.rd), 31);
					jmp(exit);

					L(non_zero);
					neg(ecx);
					if (op.op == shop_shld)
						shr(regalloc.MapRegister(op.rd), cl);
					else
						sar(regalloc.MapRegister(op.rd), cl);
					L(exit);
				}
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
							test(regalloc.MapRegister(op.rs1), op.rs2._imm);
						else
							test(regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
					}
					else
					{
						if (op.rs2.is_imm())
							cmp(regalloc.MapRegister(op.rs1), op.rs2._imm);
						else
							cmp(regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
					}
					mov(regalloc.MapRegister(op.rd), 0);
					mov(ecx, 1);
					switch (op.op)
					{
					case shop_test:
					case shop_seteq:
						cmove(regalloc.MapRegister(op.rd), ecx);
						break;
					case shop_setge:
						cmovge(regalloc.MapRegister(op.rd), ecx);
						break;
					case shop_setgt:
						cmovg(regalloc.MapRegister(op.rd), ecx);
						break;
					case shop_setae:
						cmovnc(regalloc.MapRegister(op.rd), ecx);
						break;
					case shop_setab:
						cmova(regalloc.MapRegister(op.rd), ecx);
						break;
					default:
						die("invalid case");
						break;
					}
				}
				break;
/*
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
*/
			case shop_mul_u16:
				movzx(eax, Xbyak::Reg16(regalloc.MapRegister(op.rs1).getIdx()));
				movzx(ecx, Xbyak::Reg16(regalloc.MapRegister(op.rs2).getIdx()));
				mul(ecx);
				mov(regalloc.MapRegister(op.rd), eax);
				break;
			case shop_mul_s16:
				movsx(eax, Xbyak::Reg16(regalloc.MapRegister(op.rs1).getIdx()));
				movsx(ecx, Xbyak::Reg16(regalloc.MapRegister(op.rs2).getIdx()));
				mul(ecx);
				mov(regalloc.MapRegister(op.rd), eax);
				break;
			case shop_mul_i32:
				mov(eax, regalloc.MapRegister(op.rs1));
				mul(regalloc.MapRegister(op.rs2));
				mov(regalloc.MapRegister(op.rd), eax);
				break;
			case shop_mul_u64:
				mov(eax, regalloc.MapRegister(op.rs1));
				mul(regalloc.MapRegister(op.rs2));
				mov(regalloc.MapRegister(op.rd), eax);
				mov(regalloc.MapRegister(op.rd2), edx);
				break;
			case shop_mul_s64:
				mov(eax, regalloc.MapRegister(op.rs1));
				imul(regalloc.MapRegister(op.rs2));
				mov(regalloc.MapRegister(op.rd), eax);
				mov(regalloc.MapRegister(op.rd2), edx);
				break;
/*
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
				if (!frame_reg_saved)
				{
					Str(x30, MemOperand(sp, -16, PreIndex));
					frame_reg_saved = true;
				}
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
*/
			case shop_ext_s8:
				mov(eax, regalloc.MapRegister(op.rs1));
				movsx(regalloc.MapRegister(op.rd), al);
				break;
			case shop_ext_s16:
				movsx(regalloc.MapRegister(op.rd), Xbyak::Reg16(regalloc.MapRegister(op.rs1).getIdx()));
				break;

			//
			// FPU
			//

			case shop_fadd:
				GenBinaryFOp(op, &BlockCompiler::addss);
				break;
			case shop_fsub:
				GenBinaryFOp(op, &BlockCompiler::subss);
				break;
			case shop_fmul:
				GenBinaryFOp(op, &BlockCompiler::mulss);
				break;
			case shop_fdiv:
				GenBinaryFOp(op, &BlockCompiler::divss);
				break;

			case shop_fabs:
				mov(rcx, (size_t)&float_sign_mask);
				if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
				{
					movss(regalloc.MapXRegister(op.rd), dword[rcx]);
					pandn(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
				}
				else
				{
					movss(xmm0, regalloc.MapXRegister(op.rd));
					movss(regalloc.MapXRegister(op.rd), dword[rcx]);
					pandn(regalloc.MapXRegister(op.rd), xmm0);
				}
				break;
			case shop_fneg:
				mov(rcx, (size_t)&float_sign_mask);
				if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
					movss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
				movss(xmm0, dword[rcx]);
				pxor(regalloc.MapXRegister(op.rd), xmm0);
				break;

			case shop_fsqrt:
				sqrtss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
				break;

			case shop_fmac:
				if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
					movss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
				vfmadd231ss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs2), regalloc.MapXRegister(op.rs3));
				break;

			case shop_fsrra:
				rsqrtss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
				break;

			case shop_fsetgt:
			case shop_fseteq:
				movss(xmm0, regalloc.MapXRegister(op.rs1));
				if (op.op == shop_fsetgt)
					cmpnless(xmm0, regalloc.MapXRegister(op.rs2));
				else
					cmpeqss(xmm0, regalloc.MapXRegister(op.rs2));
				movd(regalloc.MapRegister(op.rd), xmm0);
				and(regalloc.MapRegister(op.rd), 1);
				break;

/*
			case shop_fsca:
				Mov(x1, reinterpret_cast<uintptr_t>(&sin_table));
				Add(x1, x1, Operand(regalloc.MapRegister(op.rs1), UXTH, 3));
				// TODO use regalloc
				//Ldr(regalloc.MapVRegister(op.rd, 0), MemOperand(x1, 4, PostIndex));
				//Ldr(regalloc.MapVRegister(op.rd, 1), MemOperand(x1));
				regalloc.writeback_fpu += 2;
				Ldr(w2, MemOperand(x1, 4, PostIndex));
				Str(w2, sh4_context_mem_operand(op.rd.reg_ptr()));
				Ldr(w2, MemOperand(x1));
				Str(w2, sh4_context_mem_operand(GetRegPtr(op.rd._reg + 1)));
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
*/
			case shop_cvt_f2i_t:
				cvtss2si(regalloc.MapRegister(op.rd), regalloc.MapXRegister(op.rs1));
				break;
			case shop_cvt_i2f_n:
			case shop_cvt_i2f_z:
				cvtsi2ss(regalloc.MapXRegister(op.rd), regalloc.MapRegister(op.rs1));
				break;

			default:
				shil_chf[op.op](&op);
				break;
			}
			regalloc.OpEnd(&op);
		}

		mov(rax, (size_t)&next_pc);

		switch (block->BlockType) {

		case BET_StaticJump:
		case BET_StaticCall:
			//next_pc = block->BranchBlock;
			mov(dword[rax], block->BranchBlock);
			break;

		case BET_Cond_0:
		case BET_Cond_1:
			{
				//next_pc = next_pc_value;
				//if (*jdyn == 0)
				//next_pc = branch_pc_value;

				mov(dword[rax], block->NextBlock);

				if (block->has_jcond)
					mov(rdx, (size_t)&Sh4cntx.jdyn);
				else
					mov(rdx, (size_t)&sr.T);

				cmp(dword[rdx], block->BlockType & 1);
				Xbyak::Label branch_not_taken;

				jne(branch_not_taken, T_SHORT);
				mov(dword[rax], block->BranchBlock);
				L(branch_not_taken);
			}
			break;

		case BET_DynamicJump:
		case BET_DynamicCall:
		case BET_DynamicRet:
			//next_pc = *jdyn;
			mov(rdx, (size_t)&Sh4cntx.jdyn);
			mov(edx, dword[rdx]);
			mov(dword[rax], edx);
			break;

		case BET_DynamicIntr:
		case BET_StaticIntr:
			if (block->BlockType == BET_DynamicIntr) {
				//next_pc = *jdyn;
				mov(rdx, (size_t)&Sh4cntx.jdyn);
				mov(edx, dword[rdx]);
				mov(dword[rax], edx);
			}
			else {
				//next_pc = next_pc_value;
				mov(dword[rax], block->NextBlock);
			}

			call((void*)UpdateINTC);
			break;

		default:
			die("Invalid block end type");
		}

#ifdef _WIN32
		add(rsp, 0x28);
#else
		add(rsp, 0x8);
#endif
		ret();

		ready();

		block->code = (DynarecCodeEntryPtr)getCode();

		emit_Skip(getSize());
	}

	void ngen_CC_Start(shil_opcode* op)
	{
		CC_pars.clear();
	}

	void ngen_CC_param(shil_opcode& op, shil_param& prm, CanonicalParamType tp) {
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


		//store from EAX
		case CPT_u64rvL:
		case CPT_u32rv:
			mov(rcx, rax);
			host_reg_to_shil_param(prm, ecx);
			break;

		case CPT_u64rvH:
			shr(rcx, 32);
			host_reg_to_shil_param(prm, ecx);
			break;

			//Store from ST(0)
		case CPT_f32rv:
			host_reg_to_shil_param(prm, xmm0);
			break;
		}
	}

	void ngen_CC_Call(shil_opcode*op, void* function)
	{
		int regused = 0;
		int xmmused = 0;

		for (int i = CC_pars.size(); i-- > 0;)
		{
			verify(xmmused < 4 && regused < 4);
			shil_param& prm = *CC_pars[i].prm;
			switch (CC_pars[i].type) {
				//push the contents

			case CPT_u32:
				shil_param_to_host_reg(prm, call_regs[regused++]);
				break;

			case CPT_f32:
				shil_param_to_host_reg(prm, call_regsxmm[xmmused++]);
				break;

				//push the ptr itself
			case CPT_ptr:
				verify(prm.is_reg());

				mov(call_regs64[regused++], (size_t)prm.reg_ptr());

				break;
			}
		}
		call(function);
	}

	void RegPreload(u32 reg, Xbyak::Operand::Code nreg)
	{
		mov(rax, (size_t)GetRegPtr(reg));
		mov(Xbyak::Reg32(nreg), dword[rax]);
	}

	void RegWriteback(u32 reg, Xbyak::Operand::Code nreg)
	{
		mov(rax, (size_t)GetRegPtr(reg));
		mov(dword[rax], Xbyak::Reg32(nreg));
	}
	void RegPreload_FPU(u32 reg, s8 nreg)
	{
		mov(rax, (size_t)GetRegPtr(reg));
		movss(Xbyak::Xmm(nreg), dword[rax]);
	}
	void RegWriteback_FPU(u32 reg, s8 nreg)
	{
		mov(rax, (size_t)GetRegPtr(reg));
		movss(dword[rax], Xbyak::Xmm(nreg));
	}

private:
	typedef void (BlockCompiler::*X64BinaryOp)(const Xbyak::Operand&, const Xbyak::Operand&);
	typedef void (BlockCompiler::*X64BinaryFOp)(const Xbyak::Xmm&, const Xbyak::Operand&);

	void CheckBlock(RuntimeBlockInfo* block) {
		mov(call_regs[0], block->addr);

		s32 sz=block->sh4_code_size;
		u32 sa=block->addr;

		while (sz > 0)
		{
			void* ptr = (void*)GetMemPtr(sa, sz > 8 ? 8 : sz);
			if (ptr)
			{
				mov(rax, reinterpret_cast<uintptr_t>(ptr));

				if (sz >= 8) {
					mov(rdx, *(u64*)ptr);
					cmp(qword[rax], rdx);
					sz -= 8;
					sa += 8;
				}
				else if (sz >= 4) {
					mov(edx, *(u32*)ptr);
					cmp(dword[rax], edx);
					sz -= 4;
					sa += 4;
				}
				else {
					mov(edx, *(u16*)ptr);
					cmp(word[rax],dx);
					sz -= 2;
					sa += 2;
				}
				jne(reinterpret_cast<const void*>(&ngen_blockcheckfail));
			}
		}

	}

	void GenBinaryOp(const shil_opcode &op, X64BinaryOp natop)
	{
		if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
			mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
		if (op.rs2.is_imm())
		{
			mov(ecx, op.rs2._imm);
			(this->*natop)(regalloc.MapRegister(op.rd), ecx);
		}
		else
			(this->*natop)(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs2));
	}

	void GenBinaryFOp(const shil_opcode &op, X64BinaryFOp natop)
	{
		if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
			movss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
		(this->*natop)(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs2));
	}

	void shil_param_to_host_reg(const shil_param& param, const Xbyak::Reg& reg)
	{
		if (param.is_imm())
		{
			if (!reg.isXMM())
				mov(reg, param._imm);
			else
			{
				mov(eax, param._imm);
				movd((const Xbyak::Xmm &)reg, eax);
			}
		}
		else if (param.is_reg())
		{
			if (param.is_r64f())
			{
				// TODO use regalloc
				mov(rax, (uintptr_t)param.reg_ptr());
				mov(reg, qword[rax]);
			}
			else if (param.is_r32f())
			{
				if (!reg.isXMM())
					movd((const Xbyak::Reg32 &)reg, regalloc.MapXRegister(param));
				else
					movss((const Xbyak::Xmm &)reg, regalloc.MapXRegister(param));
			}
			else
			{
				if (!reg.isXMM())
					mov((const Xbyak::Reg32 &)reg, regalloc.MapRegister(param));
				else
					movd((const Xbyak::Xmm &)reg, regalloc.MapRegister(param));
			}
		}
		else
		{
			verify(param.is_null());
		}
	}

	void host_reg_to_shil_param(const shil_param& param, const Xbyak::Reg& reg)
	{
		if (reg.isREG(64))
		{
			// TODO use regalloc
			mov(rcx, (uintptr_t)param.reg_ptr());
			mov(qword[rcx], reg);
		}
		else if (regalloc.IsAllocg(param))
		{
			if (!reg.isXMM())
				mov(regalloc.MapRegister(param), (const Xbyak::Reg32 &)reg);
			else
				movd(regalloc.MapRegister(param), (const Xbyak::Xmm &)reg);
		}
		else
		{
			if (!reg.isXMM())
				movd(regalloc.MapXRegister(param), (const Xbyak::Reg32 &)reg);
			else
				movss(regalloc.MapXRegister(param), (const Xbyak::Xmm &)reg);
		}
	}

	vector<Xbyak::Reg32> call_regs;
	vector<Xbyak::Reg64> call_regs64;
	vector<Xbyak::Xmm> call_regsxmm;

	struct CC_PS
	{
		CanonicalParamType type;
		shil_param* prm;
	};
	vector<CC_PS> CC_pars;

	X64RegAlloc regalloc;
	static const u32 float_sign_mask;
};

const u32 BlockCompiler::float_sign_mask = 0x80000000;

void X64RegAlloc::Preload(u32 reg, Xbyak::Operand::Code nreg)
{
	compiler->RegPreload(reg, nreg);
}
void X64RegAlloc::Writeback(u32 reg, Xbyak::Operand::Code nreg)
{
	compiler->RegWriteback(reg, nreg);
}
void X64RegAlloc::Preload_FPU(u32 reg, s8 nreg)
{
	compiler->RegPreload_FPU(reg, nreg);
}
void X64RegAlloc::Writeback_FPU(u32 reg, s8 nreg)
{
	compiler->RegWriteback_FPU(reg, nreg);
}

static BlockCompiler* compiler;

void ngen_Compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
{
	verify(emit_FreeSpace() >= 16 * 1024);

	compiler = new BlockCompiler();
	
	compiler->compile(block, force_checks, reset, staging, optimise);

	delete compiler;
}

void ngen_CC_Start(shil_opcode* op)
{
	compiler->ngen_CC_Start(op);
}

void ngen_CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
{
	compiler->ngen_CC_param(*op, *par, tp);
}

void ngen_CC_Call(shil_opcode*op, void* function)
{
	compiler->ngen_CC_Call(op, function);
}

void ngen_CC_Finish(shil_opcode* op)
{
}
#endif
