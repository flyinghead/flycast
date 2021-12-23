#include "build.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64

//#define CANONICAL_TEST

#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>
using namespace Xbyak::util;

#include "types.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_mem.h"
#include "x64_regalloc.h"
#include "xbyak_base.h"
#include "oslib/oslib.h"

struct DynaRBI : RuntimeBlockInfo
{
	u32 Relink() override {
		return 0;
	}

	void Relocate(void* dst) override {
		verify(false);
	}
};

static void (*mainloop)();
static void (*handleException)();

u32 mem_writes, mem_reads;

static u64 jmp_rsp;

namespace MemSize {
	enum {
		S8,
		S16,
		S32,
		S64,
		Count
	};
}
namespace MemOp {
	enum {
		R,
		W,
		Count
	};
}
namespace MemType {
	enum {
		Fast,
		StoreQueue,
		Slow,
		Count
	};
}

static const void *MemHandlers[MemType::Count][MemSize::Count][MemOp::Count];
static const u8 *MemHandlerStart, *MemHandlerEnd;
static UnwindInfo unwinder;
#ifndef _WIN32
static float xmmSave[4];
#endif

void ngen_mainloop(void *)
{
	verify(mainloop != nullptr);
	try {
		mainloop();
	} catch (const SH4ThrownException&) {
		ERROR_LOG(DYNAREC, "SH4ThrownException in mainloop");
		throw FlycastException("Fatal: Unhandled SH4 exception");
	}
}

void ngen_init()
{
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	return new DynaRBI();
}

static void ngen_blockcheckfail(u32 pc) {
	//printf("X64 JIT: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
}

static void handle_sh4_exception(SH4ThrownException& ex, u32 pc)
{
	if (pc & 1)
	{
		// Delay slot
		AdjustDelaySlotException(ex);
		pc--;
	}
	Do_Exception(pc, ex.expEvn, ex.callVect);
	p_sh4rcb->cntx.cycle_counter += 4;	// probably more is needed
	handleException();
}

static void interpreter_fallback(u16 op, OpCallFP *oph, u32 pc)
{
	try {
		oph(op);
	} catch (SH4ThrownException& ex) {
		handle_sh4_exception(ex, pc);
	}
}

static void do_sqw_mmu_no_ex(u32 addr, u32 pc)
{
	try {
		do_sqw_mmu(addr);
	} catch (SH4ThrownException& ex) {
		handle_sh4_exception(ex, pc);
	}
}

const std::array<Xbyak::Reg32, 4> call_regs
#ifdef _WIN32
	{ ecx, edx, r8d, r9d };
#else
	{ edi, esi, edx, ecx };
#endif
const std::array<Xbyak::Reg64, 4> call_regs64
#ifdef _WIN32
	{ rcx, rdx, r8, r9 };
#else
	{ rdi, rsi, rdx, rcx };
#endif
const std::array<Xbyak::Xmm, 4> call_regsxmm { xmm0, xmm1, xmm2, xmm3 };

#ifdef _WIN32
constexpr u32 STACK_ALIGN = 0x28;	// 32-byte shadow space + 8 byte alignment
#else
constexpr u32 STACK_ALIGN = 8;
#endif

class BlockCompiler : public BaseXbyakRec<BlockCompiler, true>
{
public:
	using BaseCompiler = BaseXbyakRec<BlockCompiler, true>;
	friend class BaseXbyakRec<BlockCompiler, true>;

	BlockCompiler() : BaseCompiler(), regalloc(this) { }
	BlockCompiler(u8 *code_ptr) : BaseCompiler(code_ptr), regalloc(this) { }

	void compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
	{
		//printf("X86_64 compiling %08x to %p\n", block->addr, emit_GetCCPtr());
		current_opid = -1;

		CheckBlock(force_checks, block);

		sub(rsp, STACK_ALIGN);

		if (mmu_enabled() && block->has_fpu_op)
		{
			Xbyak::Label fpu_enabled;
			mov(rax, (uintptr_t)&sr);
			test(dword[rax], 0x8000);			// test SR.FD bit
			jz(fpu_enabled);
			mov(call_regs[0], block->vaddr);	// pc
			mov(call_regs[1], 0x800);			// event
			mov(call_regs[2], 0x100);			// vector
			GenCall(Do_Exception);
			jmp(exit_block, T_NEAR);
			L(fpu_enabled);
		}
		mov(rax, (uintptr_t)&p_sh4rcb->cntx.cycle_counter);
		sub(dword[rax], block->guest_cycles);

		regalloc.DoAlloc(block);

		for (current_opid = 0; current_opid < block->oplist.size(); current_opid++)
		{
			shil_opcode& op  = block->oplist[current_opid];

			regalloc.OpBegin(&op, current_opid);

			switch (op.op)
			{
			case shop_ifb:
				if (mmu_enabled())
				{
					mov(call_regs64[1], reinterpret_cast<uintptr_t>(*OpDesc[op.rs3._imm]->oph));	// op handler
					mov(call_regs[2], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc
				}

				if (op.rs1._imm)
				{
					mov(rax, (size_t)&next_pc);
					mov(dword[rax], op.rs2._imm);
				}

				mov(call_regs[0], op.rs3._imm);
					
				if (!mmu_enabled())
					GenCall(OpDesc[op.rs3._imm]->oph);
				else
					GenCall(interpreter_fallback);

				break;

			case shop_mov64:
			{
				verify(op.rd.is_r64());
				verify(op.rs1.is_r64());

				mov(rax, (uintptr_t)op.rs1.reg_ptr());
				mov(rax, qword[rax]);
				mov(rcx, (uintptr_t)op.rd.reg_ptr());
				mov(qword[rcx], rax);
			}
			break;

			case shop_readm:
				if (!GenReadMemImmediate(op, block))
				{
					// Not an immediate address
					shil_param_to_host_reg(op.rs1, call_regs[0]);
					if (!op.rs3.is_null())
					{
						if (op.rs3.is_imm())
							add(call_regs[0], op.rs3._imm);
						else if (regalloc.IsAllocg(op.rs3))
							add(call_regs[0], regalloc.MapRegister(op.rs3));
						else
						{
							mov(rax, (uintptr_t)op.rs3.reg_ptr());
							add(call_regs[0], dword[rax]);
						}
					}
					genMmuLookup(block, op, 0);

					int size = op.flags & 0x7f;
					size = size == 1 ? MemSize::S8 : size == 2 ? MemSize::S16 : size == 4 ? MemSize::S32 : MemSize::S64;
					GenCall((void (*)())MemHandlers[optimise ? MemType::Fast : MemType::Slow][size][MemOp::R], mmu_enabled());

					if (size != MemSize::S64)
						host_reg_to_shil_param(op.rd, eax);
					else {
						mov(rcx, (uintptr_t)op.rd.reg_ptr());
						mov(qword[rcx], rax);
					}
				}
				break;

			case shop_writem:
			{
				if (!GenWriteMemImmediate(op, block))
				{
					shil_param_to_host_reg(op.rs1, call_regs[0]);
					if (!op.rs3.is_null())
					{
						if (op.rs3.is_imm())
							add(call_regs[0], op.rs3._imm);
						else if (regalloc.IsAllocg(op.rs3))
							add(call_regs[0], regalloc.MapRegister(op.rs3));
						else
						{
							mov(rax, (uintptr_t)op.rs3.reg_ptr());
							add(call_regs[0], dword[rax]);
						}
					}
					genMmuLookup(block, op, 1);

					u32 size = op.flags & 0x7f;
					if (size != 8)
						shil_param_to_host_reg(op.rs2, call_regs[1]);
					else {
						mov(rax, (uintptr_t)op.rs2.reg_ptr());
						mov(call_regs64[1], qword[rax]);
					}

					size = size == 1 ? MemSize::S8 : size == 2 ? MemSize::S16 : size == 4 ? MemSize::S32 : MemSize::S64;
					GenCall((void (*)())MemHandlers[optimise ? MemType::Fast : MemType::Slow][size][MemOp::W], mmu_enabled());
				}
			}
			break;

			case shop_jcond:
			case shop_jdyn:
			case shop_mov32:
				genBaseOpcode(op);
				break;

#ifndef CANONICAL_TEST
			case shop_sync_sr:
				GenCall(UpdateSR);
				break;
			case shop_sync_fpscr:
				GenCall(UpdateFPSCR);
				break;

			case shop_negc:
				{
					Xbyak::Reg32 rs2;
					if (op.rs2.is_reg())
					{
						rs2 = regalloc.MapRegister(op.rs2);
						if (regalloc.mapg(op.rd) == regalloc.mapg(op.rs2))
						{
							mov(ecx, rs2);
							rs2 = ecx;
						}
					}
					Xbyak::Reg32 rd = regalloc.MapRegister(op.rd);
					if (op.rs1.is_imm())
						mov(rd, op.rs1.imm_value());
					else if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
						mov(rd, regalloc.MapRegister(op.rs1));
					Xbyak::Reg64 rd64 = rd.cvt64();
					neg(rd64);
					if (op.rs2.is_imm())
						sub(rd64, op.rs2.imm_value());
					else
						sub(rd64, rs2.cvt64());
					Xbyak::Reg64 rd2_64 = regalloc.MapRegister(op.rd2).cvt64();
					mov(rd2_64, rd64);
					shr(rd2_64, 63);
				}
				break;

			case shop_mul_s64:
				movsxd(rax, regalloc.MapRegister(op.rs1));
				if (op.rs2.is_reg())
					movsxd(rcx, regalloc.MapRegister(op.rs2));
				else
					mov(rcx, (s64)(s32)op.rs2._imm);
				mul(rcx);
				mov(regalloc.MapRegister(op.rd), eax);
				shr(rax, 32);
				mov(regalloc.MapRegister(op.rd2), eax);
				break;

			case shop_pref:
				{
					Xbyak::Label no_sqw;
					if (op.rs1.is_imm())
					{
						// this test shouldn't be necessary
						if ((op.rs1._imm & 0xFC000000) != 0xE0000000)
							break;

						mov(call_regs[0], op.rs1._imm);
					}
					else
					{
						Xbyak::Reg32 rn;
						if (regalloc.IsAllocg(op.rs1))
						{
							rn = regalloc.MapRegister(op.rs1);
						}
						else
						{
							mov(rax, (uintptr_t)op.rs1.reg_ptr());
							mov(eax, dword[rax]);
							rn = eax;
						}
						mov(ecx, rn);
						shr(ecx, 26);
						cmp(ecx, 0x38);
						jne(no_sqw);

						mov(call_regs[0], rn);
					}
					if (mmu_enabled())
					{
						mov(call_regs[1], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

						GenCall(do_sqw_mmu_no_ex);
					}
					else
					{
						if (CCN_MMUCR.AT == 1)
						{
							GenCall(do_sqw_mmu);
						}
						else
						{
							mov(call_regs64[1], (uintptr_t)sq_both);
							mov(rax, (size_t)&do_sqw_nommu);
							saveXmmRegisters();
							call(qword[rax]);
							restoreXmmRegisters();
						}
					}
					L(no_sqw);
				}
				break;

			case shop_frswap:
				mov(rax, (uintptr_t)op.rs1.reg_ptr());
				mov(rcx, (uintptr_t)op.rd.reg_ptr());
				if (cpu.has(Cpu::tAVX512F))
				{
					vmovaps(zmm0, zword[rax]);
					vmovaps(zmm1, zword[rcx]);
					vmovaps(zword[rax], zmm1);
					vmovaps(zword[rcx], zmm0);
				}
				else if (cpu.has(Cpu::tAVX))
				{
					vmovaps(ymm0, yword[rax]);
					vmovaps(ymm1, yword[rcx]);
					vmovaps(yword[rax], ymm1);
					vmovaps(yword[rcx], ymm0);

					vmovaps(ymm0, yword[rax + 32]);
					vmovaps(ymm1, yword[rcx + 32]);
					vmovaps(yword[rax + 32], ymm1);
					vmovaps(yword[rcx + 32], ymm0);
				}
				else
				{
					for (int i = 0; i < 4; i++)
					{
						movaps(xmm0, xword[rax + (i * 16)]);
						movaps(xmm1, xword[rcx + (i * 16)]);
						movaps(xword[rax + (i * 16)], xmm1);
						movaps(xword[rcx + (i * 16)], xmm0);
					}
				}
				break;
#endif

			default:
#ifndef CANONICAL_TEST
				if (!genBaseOpcode(op))
#endif
					shil_chf[op.op](&op);
				break;
			}
			regalloc.OpEnd(&op);
		}
		regalloc.Cleanup();
		current_opid = -1;

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

			GenCall(UpdateINTC);
			break;

		default:
			die("Invalid block end type");
		}

		L(exit_block);
		add(rsp, STACK_ALIGN);
		ret();

		ready();

		block->code = (DynarecCodeEntryPtr)getCode();
		block->host_code_size = getSize();

		emit_Skip(getSize());
	}

	void ngen_CC_Start(const shil_opcode& op)
	{
		CC_pars.clear();
	}

	void ngen_CC_param(const shil_opcode& op, const shil_param& prm, CanonicalParamType tp) {
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


		// store from EAX
		case CPT_u64rvL:
		case CPT_u32rv:
			mov(rcx, rax);
			host_reg_to_shil_param(prm, ecx);
			break;

		case CPT_u64rvH:
			// assuming CPT_u64rvL has just been called
			shr(rcx, 32);
			host_reg_to_shil_param(prm, ecx);
			break;

		// store from xmm0
		case CPT_f32rv:
			host_reg_to_shil_param(prm, xmm0);
			break;
		}
	}

	void ngen_CC_Call(const shil_opcode& op, void* function)
	{
		int regused = 0;
		int xmmused = 0;

		for (int i = CC_pars.size(); i-- > 0;)
		{
			verify(xmmused < 4 && regused < 4);
			const shil_param& prm = *CC_pars[i].prm;
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
            default:
               // Other cases handled in ngen_CC_param
               break;
			}
		}
		GenCall((void (*)())function);
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

	void genMainloop()
	{
		unwinder.start((void *)getCurr());

		push(rbx);
		unwinder.pushReg(getSize(), Xbyak::Operand::RBX);
		push(rbp);
		unwinder.pushReg(getSize(), Xbyak::Operand::RBP);
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
		sub(rsp, STACK_ALIGN);
		unwinder.allocStack(getSize(), STACK_ALIGN);
		unwinder.endProlog(getSize());

		mov(qword[rip + &jmp_rsp], rsp);

	//run_loop:
		Xbyak::Label run_loop;
		L(run_loop);
		Xbyak::Label end_run_loop;
		mov(rax, (size_t)&p_sh4rcb->cntx.CpuRunning);
		mov(edx, dword[rax]);

		test(edx, edx);
		je(end_run_loop);

	//slice_loop:
		Xbyak::Label slice_loop;
		L(slice_loop);
		mov(rax, (size_t)&p_sh4rcb->cntx.pc);
		mov(call_regs[0], dword[rax]);
		call(bm_GetCodeByVAddr);
		call(rax);
		mov(rax, (uintptr_t)&p_sh4rcb->cntx.cycle_counter);
		mov(ecx, dword[rax]);
		test(ecx, ecx);
		jg(slice_loop);

		add(ecx, SH4_TIMESLICE);
		mov(dword[rax], ecx);
		call(UpdateSystem_INTC);
		jmp(run_loop);

	//end_run_loop:
		L(end_run_loop);
		add(rsp, STACK_ALIGN);
		pop(r15);
		pop(r14);
		pop(r13);
		pop(r12);
#ifdef _WIN32
		pop(rsi);
		pop(rdi);
#endif
		pop(rbp);
		pop(rbx);
		ret();
		size_t unwindSize = unwinder.end(getSize());
		setSize(getSize() + unwindSize);

		unwinder.start((void *)getCurr());
		size_t startOffset = getSize();
#ifdef _WIN32
		// 32-byte shadow space + 8 for stack 16-byte alignment
		unwinder.allocStack(0, 40);
#else
		// stack 16-byte alignment
		unwinder.allocStack(0, 8);
#endif
		unwinder.endProlog(0);

	//handleException:
		Xbyak::Label handleExceptionLabel;
		L(handleExceptionLabel);
		mov(rsp, qword[rip + &jmp_rsp]);
		jmp(run_loop);

		genMemHandlers();

		size_t savedSize = getSize();
		setSize(CODE_SIZE - 128 - startOffset);
		unwindSize = unwinder.end(getSize());
		verify(unwindSize <= 128);
		setSize(savedSize);

		ready();
		mainloop = (void (*)())getCode();
		handleException = (void(*)())handleExceptionLabel.getAddress();

		emit_Skip(getSize());
	}

	bool rewriteMemAccess(host_context_t &context)
	{
		if (!_nvmem_enabled())
			return false;

		//printf("ngen_Rewrite pc %p\n", context.pc);
		if (context.pc < (size_t)MemHandlerStart || context.pc >= (size_t)MemHandlerEnd)
			return false;

		u8 *retAddr = *(u8 **)context.rsp;
		void *ca = *(s32 *)(retAddr - 4) + retAddr;
		for (int size = 0; size < MemSize::Count; size++)
		{
			for (int op = 0; op < MemOp::Count; op++)
			{
				if ((void *)MemHandlers[MemType::Fast][size][op] != ca)
					continue;

				//found !
				const u8 *start = getCurr();
				u32 memAddress = _nvmem_4gb_space() ?
#ifdef _WIN32
						context.rcx
#else
						context.rdi
#endif
						: context.r9;
				if (op == MemOp::W && size >= MemSize::S32 && (memAddress >> 26) == 0x38)
					call(MemHandlers[MemType::StoreQueue][size][MemOp::W]);
				else
					call(MemHandlers[MemType::Slow][size][op]);
				verify(getCurr() - start == 5);

				ready();

				context.pc = (uintptr_t)(retAddr - 5);
				// remove the call from the stack
				context.rsp += 8;
				if (!_nvmem_4gb_space())
					//restore the addr from r9 to arg0 (rcx or rdi) so it's valid again
#ifdef _WIN32
					context.rcx = memAddress;
#else
					context.rdi = memAddress;
#endif

				return true;
			}
		}
		ERROR_LOG(DYNAREC, "rewriteMemAccess code not found: host pc %p", (void *)context.pc);
		die("Failed to match the code");

		return false;
	}

private:
	void genMmuLookup(const RuntimeBlockInfo* block, const shil_opcode& op, u32 write)
	{
		if (mmu_enabled())
		{
			Xbyak::Label inCache;
			Xbyak::Label done;

			mov(eax, call_regs[0]);
			shr(eax, 12);
			if ((uintptr_t)mmuAddressLUT >> 32 != 0)
			{
				mov(r9, (uintptr_t)mmuAddressLUT);
				mov(eax, dword[r9 + rax * 4]);
			}
			else
			{
				mov(eax, dword[(uintptr_t)mmuAddressLUT + rax * 4]);
			}
			test(eax, eax);
			jne(inCache);
			mov(call_regs[1], write);
			mov(call_regs[2], block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0));	// pc
			GenCall(mmuDynarecLookup);
			mov(call_regs[0], eax);
			jmp(done);
			L(inCache);
			and_(call_regs[0], 0xFFF);
			or_(call_regs[0], eax);
			L(done);
		}
	}
	bool GenReadMemImmediate(const shil_opcode& op, RuntimeBlockInfo* block)
	{
		if (!op.rs1.is_imm())
			return false;
		u32 size = op.flags & 0x7f;
		u32 addr = op.rs1._imm;
		if (mmu_enabled() && mmu_is_translated(addr, size))
		{
			if ((addr >> 12) != (block->vaddr >> 12) && ((addr >> 12) != ((block->vaddr + block->guest_opcodes * 2 - 1) >> 12)))
				// When full mmu is on, only consider addresses in the same 4k page
				return false;

			u32 paddr;
			u32 rv;
			switch (size)
			{
			case 1:
				rv = mmu_data_translation<MMU_TT_DREAD, u8>(addr, paddr);
				break;
			case 2:
				rv = mmu_data_translation<MMU_TT_DREAD, u16>(addr, paddr);
				break;
			case 4:
			case 8:
				rv = mmu_data_translation<MMU_TT_DREAD, u32>(addr, paddr);
				break;
			default:
				die("Invalid immediate size");
				return false;
			}
			if (rv != MMU_ERROR_NONE)
				return false;

			addr = paddr;
		}
		bool isram = false;
		void* ptr = _vmem_read_const(addr, isram, size > 4 ? 4 : size);

		if (isram)
		{
			// Immediate pointer to RAM: super-duper fast access
			mov(rax, reinterpret_cast<uintptr_t>(ptr));
			switch (size)
			{
			case 1:
				if (regalloc.IsAllocg(op.rd))
					movsx(regalloc.MapRegister(op.rd), byte[rax]);
				else
				{
					movsx(eax, byte[rax]);
					mov(rcx, (uintptr_t)op.rd.reg_ptr());
					mov(dword[rcx], eax);
				}
				break;

			case 2:
				if (regalloc.IsAllocg(op.rd))
					movsx(regalloc.MapRegister(op.rd), word[rax]);
				else
				{
					movsx(eax, word[rax]);
					mov(rcx, (uintptr_t)op.rd.reg_ptr());
					mov(dword[rcx], eax);
				}
				break;

			case 4:
				if (regalloc.IsAllocg(op.rd))
					mov(regalloc.MapRegister(op.rd), dword[rax]);
				else if (regalloc.IsAllocf(op.rd))
					movd(regalloc.MapXRegister(op.rd), dword[rax]);
				else
				{
					mov(eax, dword[rax]);
					mov(rcx, (uintptr_t)op.rd.reg_ptr());
					mov(dword[rcx], eax);
				}
				break;

			case 8:
				mov(rcx, qword[rax]);
				mov(rax, (uintptr_t)op.rd.reg_ptr());
				mov(qword[rax], rcx);
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
				mov(call_regs[0], addr);
				GenCall((void (*)())ptr);
				mov(rcx, (size_t)op.rd.reg_ptr());
				mov(dword[rcx], eax);

				mov(call_regs[0], addr + 4);
				GenCall((void (*)())ptr);
				mov(rcx, (size_t)op.rd.reg_ptr() + 4);
				mov(dword[rcx], eax);
			}
			else
			{
				mov(call_regs[0], addr);

				switch(size)
				{
				case 1:
					GenCall((void (*)())ptr);
					movsx(eax, al);
					break;

				case 2:
					GenCall((void (*)())ptr);
					movsx(eax, ax);
					break;

				case 4:
					GenCall((void (*)())ptr);
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

	bool GenWriteMemImmediate(const shil_opcode& op, RuntimeBlockInfo* block)
	{
		if (!op.rs1.is_imm())
			return false;
		u32 size = op.flags & 0x7f;
		u32 addr = op.rs1._imm;
		if (mmu_enabled() && mmu_is_translated(addr, size))
		{
			if ((addr >> 12) != (block->vaddr >> 12) && ((addr >> 12) != ((block->vaddr + block->guest_opcodes * 2 - 1) >> 12)))
				// When full mmu is on, only consider addresses in the same 4k page
				return false;

			u32 paddr;
			u32 rv;
			switch (size)
			{
			case 1:
				rv = mmu_data_translation<MMU_TT_DWRITE, u8>(addr, paddr);
				break;
			case 2:
				rv = mmu_data_translation<MMU_TT_DWRITE, u16>(addr, paddr);
				break;
			case 4:
			case 8:
				rv = mmu_data_translation<MMU_TT_DWRITE, u32>(addr, paddr);
				break;
			default:
				die("Invalid immediate size");
				return false;
			}
			if (rv != MMU_ERROR_NONE)
				return false;

			addr = paddr;
		}
		bool isram = false;
		void* ptr = _vmem_write_const(addr, isram, size > 4 ? 4 : size);

		if (isram)
		{
			// Immediate pointer to RAM: super-duper fast access
			mov(rax, reinterpret_cast<uintptr_t>(ptr));
			switch (size)
			{
			case 1:
				if (regalloc.IsAllocg(op.rs2))
					mov(byte[rax], regalloc.MapRegister(op.rs2).cvt8());
				else if (op.rs2.is_imm())
					mov(byte[rax], (u8)op.rs2._imm);
				else
				{
					mov(rcx, (uintptr_t)op.rs2.reg_ptr());
					mov(cl, byte[rcx]);
					mov(byte[rax], cl);
				}
				break;

			case 2:
				if (regalloc.IsAllocg(op.rs2))
					mov(word[rax], regalloc.MapRegister(op.rs2).cvt16());
				else if (op.rs2.is_imm())
					mov(word[rax], (u16)op.rs2._imm);
				else
				{
					mov(rcx, (uintptr_t)op.rs2.reg_ptr());
					mov(cx, word[rcx]);
					mov(word[rax], cx);
				}
				break;

			case 4:
				if (regalloc.IsAllocg(op.rs2))
					mov(dword[rax], regalloc.MapRegister(op.rs2));
				else if (regalloc.IsAllocf(op.rs2))
					movd(dword[rax], regalloc.MapXRegister(op.rs2));
				else if (op.rs2.is_imm())
					mov(dword[rax], op.rs2._imm);
				else
				{
					mov(rcx, (uintptr_t)op.rs2.reg_ptr());
					mov(ecx, dword[rcx]);
					mov(dword[rax], ecx);
				}
				break;

			case 8:
				mov(rcx, (uintptr_t)op.rs2.reg_ptr());
				mov(rcx, qword[rcx]);
				mov(qword[rax], rcx);
				break;

			default:
				die("Invalid immediate size");
				break;
			}
		}
		else
		{
			// Not RAM: the returned pointer is a memory handler
			mov(call_regs[0], addr);
			shil_param_to_host_reg(op.rs2, call_regs[1]);

			GenCall((void (*)())ptr);
		}

		return true;
	}

	void CheckBlock(bool force_checks, RuntimeBlockInfo* block)
	{
		if (mmu_enabled() || force_checks)
			mov(call_regs[0], block->addr);

		// FIXME This test shouldn't be necessary
		// However the decoder makes various assumptions about the current PC value, which are simply not
		// true in a virtualized memory model. So this can only work if virtual and phy addresses are the
		// same at compile and run times.
		if (mmu_enabled())
		{
			mov(rax, (uintptr_t)&next_pc);
			cmp(dword[rax], block->vaddr);
			jne(reinterpret_cast<const void*>(&ngen_blockcheckfail));
		}

		if (!force_checks)
			return;

		s32 sz=block->sh4_code_size;
		u32 sa=block->addr;

		void* ptr = (void*)GetMemPtr(sa, sz > 8 ? 8 : sz);
		if (ptr)
		{
			while (sz > 0)
			{
				uintptr_t uintptr = reinterpret_cast<uintptr_t>(ptr);
				mov(rax, uintptr);

				if (sz >= 8 && !(uintptr & 7)) {
					mov(rdx, *(u64*)ptr);
					cmp(qword[rax], rdx);
					sz -= 8;
					sa += 8;
				}
				else if (sz >= 4 && !(uintptr & 3)) {
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
				jne(reinterpret_cast<const void*>(CC_RX2RW(&ngen_blockcheckfail)));
				ptr = (void*)GetMemPtr(sa, sz > 8 ? 8 : sz);
			}
		}
	}

	void genMemHandlers()
	{
		// make sure the memory handlers are set
		verify(ReadMem8 != nullptr);

		MemHandlerStart = getCurr();
		for (int type = 0; type < MemType::Count; type++)
		{
			for (int size = 0; size < MemSize::Count; size++)
			{
				for (int op = 0; op < MemOp::Count; op++)
				{
					MemHandlers[type][size][op] = getCurr();
					if (type == MemType::Fast && _nvmem_enabled())
					{
						mov(rax, (uintptr_t)virt_ram_base);
						if (!_nvmem_4gb_space())
						{
							mov(r9, call_regs64[0]);
							and_(call_regs[0], 0x1FFFFFFF);
						}
						switch (size)
						{
						case MemSize::S8:
							if (op == MemOp::R)
								movsx(eax, byte[rax + call_regs64[0]]);
							else
								mov(byte[rax + call_regs64[0]], call_regs[1].cvt8());
							break;

						case MemSize::S16:
							if (op == MemOp::R)
								movsx(eax, word[rax + call_regs64[0]]);
							else
								mov(word[rax + call_regs64[0]], call_regs[1].cvt16());
							break;

						case MemSize::S32:
							if (op == MemOp::R)
								mov(eax, dword[rax + call_regs64[0]]);
							else
								mov(dword[rax + call_regs64[0]], call_regs[1]);
							break;

						case MemSize::S64:
							if (op == MemOp::R)
								mov(rax, qword[rax + call_regs64[0]]);
							else
								mov(qword[rax + call_regs64[0]], call_regs64[1]);
							break;
						}
					}
					else if (type == MemType::StoreQueue)
					{
						if (op != MemOp::W || size < MemSize::S32)
							continue;
						Xbyak::Label no_sqw;

						mov(r9d, call_regs[0]);
						shr(r9d, 26);
						cmp(r9d, 0x38);
						jne(no_sqw);
						mov(rax, (uintptr_t)p_sh4rcb->sq_buffer);
						and_(call_regs[0], 0x3F);

						if (size == MemSize::S32)
							mov(dword[rax + call_regs64[0]], call_regs[1]);
						else
							mov(qword[rax + call_regs64[0]], call_regs64[1]);
						ret();
						L(no_sqw);
						if (size == MemSize::S32)
							jmp((const void *)_vmem_WriteMem32);	// tail call
						else
							jmp((const void *)_vmem_WriteMem64);	// tail call
						continue;
					}
					else
					{
						// Slow path
						if (op == MemOp::R)
						{
							switch (size) {
							case MemSize::S8:
								sub(rsp, STACK_ALIGN);
								call((const void *)_vmem_ReadMem8);
								movsx(eax, al);
								add(rsp, STACK_ALIGN);
								break;
							case MemSize::S16:
								sub(rsp, STACK_ALIGN);
								call((const void *)_vmem_ReadMem16);
								movsx(eax, ax);
								add(rsp, STACK_ALIGN);
								break;
							case MemSize::S32:
								jmp((const void *)_vmem_ReadMem32);	// tail call
								continue;
							case MemSize::S64:
								jmp((const void *)_vmem_ReadMem64);	// tail call
								continue;
							default:
								die("1..8 bytes");
							}
						}
						else
						{
							switch (size) {
							case MemSize::S8:
								jmp((const void *)_vmem_WriteMem8);		// tail call
								continue;
							case MemSize::S16:
								jmp((const void *)_vmem_WriteMem16);	// tail call
								continue;
							case MemSize::S32:
								jmp((const void *)_vmem_WriteMem32);	// tail call
								continue;
							case MemSize::S64:
								jmp((const void *)_vmem_WriteMem64);	// tail call
								continue;
							default:
								die("1..8 bytes");
							}
						}
					}
					ret();
				}
			}
		}
		MemHandlerEnd = getCurr();
	}

	void saveXmmRegisters()
	{
#ifndef _WIN32
		if (current_opid == (size_t)-1)
			return;

		if (regalloc.IsMapped(xmm8, current_opid))
			movd(ptr[rip + &xmmSave[0]], xmm8);
		if (regalloc.IsMapped(xmm9, current_opid))
			movd(ptr[rip + &xmmSave[1]], xmm9);
		if (regalloc.IsMapped(xmm10, current_opid))
			movd(ptr[rip + &xmmSave[2]], xmm10);
		if (regalloc.IsMapped(xmm11, current_opid))
			movd(ptr[rip + &xmmSave[3]], xmm11);
#endif
	}

	void restoreXmmRegisters()
	{
#ifndef _WIN32
		if (current_opid == (size_t)-1)
			return;

		if (regalloc.IsMapped(xmm8, current_opid))
			movd(xmm8, ptr[rip + &xmmSave[0]]);
		if (regalloc.IsMapped(xmm9, current_opid))
			movd(xmm9, ptr[rip + &xmmSave[1]]);
		if (regalloc.IsMapped(xmm10, current_opid))
			movd(xmm10, ptr[rip + &xmmSave[2]]);
		if (regalloc.IsMapped(xmm11, current_opid))
			movd(xmm11, ptr[rip + &xmmSave[3]]);
#endif
	}

	template<class Ret, class... Params>
	void GenCall(Ret(*function)(Params...), bool skip_floats = false)
	{
		if (!skip_floats)
			saveXmmRegisters();
		call(CC_RX2RW(function));
		if (!skip_floats)
			restoreXmmRegisters();
	}

	struct CC_PS
	{
		CanonicalParamType type;
		const shil_param* prm;
	};
	std::vector<CC_PS> CC_pars;

	X64RegAlloc regalloc;
	Xbyak::util::Cpu cpu;
	size_t current_opid;
	Xbyak::Label exit_block;
};

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

static BlockCompiler* ccCompiler;

void ngen_Compile(RuntimeBlockInfo* block, bool smc_checks, bool reset, bool staging, bool optimise)
{
	verify(emit_FreeSpace() >= 16 * 1024);
	void* protStart = emit_GetCCPtr();
	size_t protSize = emit_FreeSpace();
	vmem_platform_jit_set_exec(protStart, protSize, false);

	BlockCompiler compiler;
	::ccCompiler = &compiler;
	try {
		compiler.compile(block, smc_checks, reset, staging, optimise);
	} catch (const Xbyak::Error& e) {
		ERROR_LOG(DYNAREC, "Fatal xbyak error: %s", e.what());
	}
	::ccCompiler = nullptr;
	vmem_platform_jit_set_exec(protStart, protSize, true);
}

void ngen_CC_Start(shil_opcode* op)
{
	ccCompiler->ngen_CC_Start(*op);
}

void ngen_CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
{
	ccCompiler->ngen_CC_param(*op, *par, tp);
}

void ngen_CC_Call(shil_opcode* op, void* function)
{
	ccCompiler->ngen_CC_Call(*op, function);
}

void ngen_CC_Finish(shil_opcode* op)
{
}

bool ngen_Rewrite(host_context_t &context, void *faultAddress)
{
	void* protStart = emit_GetCCPtr();
	size_t protSize = emit_FreeSpace();
	vmem_platform_jit_set_exec(protStart, protSize, false);

	u8 *retAddr = *(u8 **)context.rsp - 5;
	BlockCompiler compiler(retAddr);
	bool rc = false;
	try {
		rc = compiler.rewriteMemAccess(context);
		vmem_platform_jit_set_exec(protStart, protSize, true);
	} catch (const Xbyak::Error& e) {
		ERROR_LOG(DYNAREC, "Fatal xbyak error: %s", e.what());
	}
	return rc;
}

void ngen_HandleException(host_context_t &context)
{
	context.pc = (uintptr_t)handleException;
}

void ngen_ResetBlocks()
{
	unwinder.clear();
	// Avoid generating the main loop more than once
	if (mainloop != nullptr && mainloop != emit_GetCCPtr())
		return;

	void* protStart = emit_GetCCPtr();
	size_t protSize = emit_FreeSpace();
	vmem_platform_jit_set_exec(protStart, protSize, false);

	BlockCompiler compiler;
	try {
		compiler.genMainloop();
	} catch (const Xbyak::Error& e) {
		ERROR_LOG(DYNAREC, "Fatal xbyak error: %s", e.what());
	}
	vmem_platform_jit_set_exec(protStart, protSize, true);
}

#endif
