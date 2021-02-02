#include "build.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
#include <setjmp.h>

//#define PROFILING
//#define CANONICAL_TEST

#define XBYAK_NO_OP_NAMES
#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>

#include "types.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/mem/vmem32.h"
#include "profiler/profiler.h"
#include "oslib/oslib.h"
#include "x64_regalloc.h"
#include "xbyak_base.h"

struct DynaRBI : RuntimeBlockInfo
{
	virtual u32 Relink() {
		return 0;
	}

	virtual void Relocate(void* dst) {
		verify(false);
	}
};

extern "C" {
	int cycle_counter;
}

u64 host_cpu_time;

u32 mem_writes, mem_reads;
u32 mem_rewrites_w, mem_rewrites_r;

#ifdef PROFILING
static clock_t slice_start;
int start_cycle;
extern "C"
{
static __attribute((used)) void* start_slice(void *p)
{
	slice_start = clock();
	start_cycle = cycle_counter;
	return p;
}
static __attribute((used)) void end_slice()
{
	clock_t now = clock();
	if (slice_start != 0)
	{
		host_cpu_time += now - slice_start;
		guest_cpu_cycles += start_cycle - cycle_counter;
	}
	slice_start = now;
	start_cycle = cycle_counter;
}
}
#endif

#if defined(__APPLE__)
#define _U "_"
#else
#define _U
#endif

#ifdef _WIN32
#define WIN32_ONLY(x) x
#else
#define WIN32_ONLY(x)
#endif

#define STRINGIFY(x) #x
#define _S(x) STRINGIFY(x)
#if RAM_SIZE_MAX == 16*1024*1024
#define CPU_RUNNING 68157284
#define PC 68157256
#elif RAM_SIZE_MAX == 32*1024*1024
#define CPU_RUNNING 135266148
#define PC 135266120
#else
#error RAM_SIZE_MAX unknown
#endif

extern "C" {
	jmp_buf jmp_env;
}

#ifndef _MSC_VER

#ifdef _WIN32
        // Fully naked function in win32 for proper SEH prologue
	__asm__ (
			".text                          \n\t"
			".p2align 4,,15                 \n\t"
			".globl ngen_mainloop           \n\t"
			".def   ngen_mainloop;  .scl    2;      .type   32;     .endef  \n\t"
			".seh_proc      ngen_mainloop   \n\t"
		"ngen_mainloop:                     \n\t"
#else
void ngen_mainloop(void* v_cntx)
{
	__asm__ (
#endif
			"pushq %rbx						\n\t"
WIN32_ONLY( ".seh_pushreg %rbx				\n\t")
#if !defined(__APPLE__)	// rbp is pushed in the standard function prologue
			"pushq %rbp                     \n\t"
#endif
#ifdef _WIN32
			".seh_pushreg %rbp              \n\t"
			"pushq %rdi                     \n\t"
			".seh_pushreg %rdi              \n\t"
			"pushq %rsi                     \n\t"
			".seh_pushreg %rsi              \n\t"
#endif
			"pushq %r12                     \n\t"
WIN32_ONLY( ".seh_pushreg %r12              \n\t")
			"pushq %r13                     \n\t"
WIN32_ONLY( ".seh_pushreg %r13              \n\t")
			"pushq %r14                     \n\t"
WIN32_ONLY( ".seh_pushreg %r14              \n\t")
			"pushq %r15                     \n\t"
#ifdef _WIN32
			".seh_pushreg %r15              \n\t"
			"subq $40, %rsp                 \n\t"   // 32-byte shadow space + 8 for stack 16-byte alignment
			".seh_stackalloc 40             \n\t"
			".seh_endprologue               \n\t"
#else
			"subq $8, %rsp                  \n\t"   // 8 for stack 16-byte alignment
#endif
			"movl $" _S(SH4_TIMESLICE) "," _U "cycle_counter(%rip)  \n\t"

#ifdef _WIN32
			"leaq " _U "jmp_env(%rip), %rcx	\n\t"	// SETJMP
			"xor %rdx, %rdx					\n\t"	// no frame pointer
#else
			"leaq " _U "jmp_env(%rip), %rdi	\n\t"
#endif
			"call " _U "setjmp				\n"

		"1:                                 \n\t"   // run_loop
			"movq " _U "p_sh4rcb(%rip), %rax		\n\t"
			"movl " _S(CPU_RUNNING) "(%rax), %edx	\n\t"
			"testl %edx, %edx               \n\t"
			"je 3f                          \n"     // end_run_loop
#ifdef PROFILING
			"call start_slice				\n\t"
#endif

		"2:                                 \n\t"   // slice_loop
			"movq " _U "p_sh4rcb(%rip), %rax	\n\t"
#ifdef _WIN32
			"movl " _S(PC)"(%rax), %ecx     \n\t"
#else
			"movl " _S(PC)"(%rax), %edi     \n\t"
#endif
			"call " _U "bm_GetCodeByVAddr	\n\t"
			"call *%rax                     \n\t"
#ifdef PROFILING
			"call end_slice					\n\t"
#endif
			"movl " _U "cycle_counter(%rip), %ecx \n\t"
			"testl %ecx, %ecx               \n\t"
			"jg 2b                          \n\t"   // slice_loop

			"addl $" _S(SH4_TIMESLICE) ", %ecx		\n\t"
			"movl %ecx, " _U "cycle_counter(%rip)	\n\t"
			"call " _U "UpdateSystem_INTC   \n\t"
			"jmp 1b                         \n"     // run_loop

		"3:                                 \n\t"   // end_run_loop

#ifdef _WIN32
			"addq $40, %rsp                 \n\t"
#else
			"addq $8, %rsp                  \n\t"
#endif
			"popq %r15                      \n\t"
			"popq %r14                      \n\t"
			"popq %r13                      \n\t"
			"popq %r12                      \n\t"
#ifdef _WIN32
			"popq %rsi                      \n\t"
			"popq %rdi                      \n\t"
#endif
#if !defined(__APPLE__)
			"popq %rbp                      \n\t"
#endif
			"popq %rbx                      \n\t"
#ifdef _WIN32
			"ret                            \n\t"
			".seh_endproc                   \n"
	);
#else
	);
}
#endif

#endif	// !_MSC_VER
#undef _U
#undef _S

void ngen_init()
{
	verify(CPU_RUNNING == offsetof(Sh4RCB, cntx.CpuRunning));
	verify(PC == offsetof(Sh4RCB, cntx.pc));
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
	//printf("X64 JIT: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
}

static void handle_mem_exception(u32 exception_raised, u32 pc)
{
	if (exception_raised)
	{
		if (pc & 1)
			// Delay slot
			spc = pc - 1;
		else
			spc = pc;
		cycle_counter += 2;	// probably more is needed but no easy way to find out
		longjmp(jmp_env, 1);
	}
}

static u32 exception_raised;

template<typename T>
static T ReadMemNoEx(u32 addr, u32 pc)
{
#ifndef NO_MMU
	T rv = mmu_ReadMemNoEx<T>(addr, &exception_raised);
	handle_mem_exception(exception_raised, pc);

	return rv;
#else
	// not used
	return (T)0;
#endif
}

template<typename T>
static u32 WriteMemNoEx(u32 addr, T data, u32 pc)
{
#ifndef NO_MMU
	u32 exception_raised = mmu_WriteMemNoEx<T>(addr, data);
	handle_mem_exception(exception_raised, pc);
	return exception_raised;
#endif
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
	cycle_counter += 4;	// probably more is needed
	longjmp(jmp_env, 1);
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

static void do_sqw_nommu_local(u32 addr, u8* sqb)
{
	do_sqw_nommu(addr, sqb);
}

const std::array<Xbyak::Reg32, 4> call_regs
#ifdef _WIN32
	{ Xbyak::util::ecx, Xbyak::util::edx, Xbyak::util::r8d, Xbyak::util::r9d };
#else
	{ Xbyak::util::edi, Xbyak::util::esi, Xbyak::util::edx, Xbyak::util::ecx };
#endif
const std::array<Xbyak::Reg64, 4> call_regs64
#ifdef _WIN32
	{ Xbyak::util::rcx, Xbyak::util::rdx, Xbyak::util::r8, Xbyak::util::r9 };
#else
	{ Xbyak::util::rdi, Xbyak::util::rsi, Xbyak::util::rdx, Xbyak::util::rcx };
#endif
const std::array<Xbyak::Xmm, 4> call_regsxmm { Xbyak::util::xmm0, Xbyak::util::xmm1, Xbyak::util::xmm2, Xbyak::util::xmm3 };

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

#ifdef _WIN32
		sub(rsp, 0x28);		// 32-byte shadow space + 8 byte alignment
#else
		sub(rsp, 0x8);		// align stack
#endif
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
#ifdef FEAT_NO_RWX_PAGES
		// Use absolute addressing for this one
		// TODO(davidgfnet) remove the ifsef using CC_RX2RW/CC_RW2RX
		mov(rax, (uintptr_t)&cycle_counter);
		sub(dword[rax], block->guest_cycles);
#else
		sub(dword[rip + &cycle_counter], block->guest_cycles);
#endif
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
					if (!optimise || !GenReadMemoryFast(op, block))
						GenReadMemorySlow(op, block);

					u32 size = op.flags & 0x7f;
					if (size != 8)
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

					u32 size = op.flags & 0x7f;
					if (size != 8)
						shil_param_to_host_reg(op.rs2, call_regs[1]);
					else {
						mov(rax, (uintptr_t)op.rs2.reg_ptr());
						mov(call_regs64[1], qword[rax]);
					}
					if (!optimise || !GenWriteMemoryFast(op, block))
						GenWriteMemorySlow(op, block);
				}
			}
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
				if (op.rs1.is_imm())
				{
					// this test shouldn't be necessary
					if ((op.rs1._imm & 0xFC000000) == 0xE0000000)
					{
						mov(call_regs[0], op.rs1._imm);
						if (mmu_enabled())
						{
							mov(call_regs[1], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));      // pc

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
								GenCall(&do_sqw_nommu_local);
							}
						}
					}
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
					Xbyak::Label no_sqw;
					jne(no_sqw);

					mov(call_regs[0], rn);
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
							GenCall(&do_sqw_nommu_local);
						}
					}
					L(no_sqw);
				}
				break;

			case shop_frswap:
				mov(rax, (uintptr_t)op.rs1.reg_ptr());
				mov(rcx, (uintptr_t)op.rd.reg_ptr());
				if (cpu.has(Xbyak::util::Cpu::tAVX512F))
				{
					vmovaps(zmm0, zword[rax]);
					vmovaps(zmm1, zword[rcx]);
					vmovaps(zword[rax], zmm1);
					vmovaps(zword[rcx], zmm0);
				}
				else if (cpu.has(Xbyak::util::Cpu::tAVX))
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
#ifdef _WIN32
		add(rsp, 0x28);
#else
		add(rsp, 0x8);
#endif
		ret();

		ready();

		block->code = (DynarecCodeEntryPtr)getCode();
		block->host_code_size = getSize();

		emit_Skip(getSize());
	}

	void GenReadMemorySlow(const shil_opcode& op, RuntimeBlockInfo* block)
	{
		const u8 *start_addr = getCurr();
		if (mmu_enabled())
			mov(call_regs[1], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

		u32 size = op.flags & 0x7f;
		switch (size) {
		case 1:
			if (!mmu_enabled())
				GenCall(ReadMem8);
			else
				GenCall(ReadMemNoEx<u8>, true);
			movsx(eax, al);
			break;
		case 2:
			if (!mmu_enabled())
				GenCall(ReadMem16);
			else
				GenCall(ReadMemNoEx<u16>, true);
			movsx(eax, ax);
			break;

		case 4:
			if (!mmu_enabled())
				GenCall(ReadMem32);
			else
				GenCall(ReadMemNoEx<u32>, true);
			break;
		case 8:
			if (!mmu_enabled())
				GenCall(ReadMem64);
			else
				GenCall(ReadMemNoEx<u64>, true);
			break;
		default:
			die("1..8 bytes");
		}

		if (mmu_enabled() && vmem32_enabled())
		{
			Xbyak::Label quick_exit;
			if (getCurr() - start_addr <= read_mem_op_size - 6)
				jmp(quick_exit, T_NEAR);
			while (getCurr() - start_addr < read_mem_op_size)
				nop();
			L(quick_exit);
			verify(getCurr() - start_addr == read_mem_op_size);
		}
	}

	void GenWriteMemorySlow(const shil_opcode& op, RuntimeBlockInfo* block)
	{
		const u8 *start_addr = getCurr();
		if (mmu_enabled())
			mov(call_regs[2], block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc

		u32 size = op.flags & 0x7f;
		switch (size) {
		case 1:
			if (!mmu_enabled())
				GenCall(WriteMem8);
			else
				GenCall(WriteMemNoEx<u8>, true);
			break;
		case 2:
			if (!mmu_enabled())
				GenCall(WriteMem16);
			else
				GenCall(WriteMemNoEx<u16>, true);
			break;
		case 4:
			if (!mmu_enabled())
				GenCall(WriteMem32);
			else
				GenCall(WriteMemNoEx<u32>, true);
			break;
		case 8:
			if (!mmu_enabled())
				GenCall(WriteMem64);
			else
				GenCall(WriteMemNoEx<u64>, true);
			break;
		default:
			die("1..8 bytes");
		}
		if (mmu_enabled() && vmem32_enabled())
		{
			Xbyak::Label quick_exit;
			if (getCurr() - start_addr <= write_mem_op_size - 6)
				jmp(quick_exit, T_NEAR);
			while (getCurr() - start_addr < write_mem_op_size)
				nop();
			L(quick_exit);
			verify(getCurr() - start_addr == write_mem_op_size);
		}
	}

	void InitializeRewrite(RuntimeBlockInfo *block, size_t opid)
	{
	}

	void FinalizeRewrite()
	{
		ready();
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

private:
	bool GenReadMemImmediate(const shil_opcode& op, RuntimeBlockInfo* block)
	{
		if (!op.rs1.is_imm())
			return false;
		u32 size = op.flags & 0x7f;
		u32 addr = op.rs1._imm;
		if (mmu_enabled() && mmu_is_translated<MMU_TT_DREAD>(addr, size))
		{
			if ((addr >> 12) != (block->vaddr >> 12))
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
		if (mmu_enabled() && mmu_is_translated<MMU_TT_DWRITE>(addr, size))
		{
			if ((addr >> 12) != (block->vaddr >> 12))
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

	bool GenReadMemoryFast(const shil_opcode& op, RuntimeBlockInfo* block)
	{
		if (!mmu_enabled() || !vmem32_enabled())
			return false;
		mem_reads++;
		const u8 *start_addr = getCurr();

		mov(rax, (uintptr_t)&p_sh4rcb->cntx.exception_pc);
		mov(dword[rax], block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0));

		mov(rax, (uintptr_t)virt_ram_base);

		u32 size = op.flags & 0x7f;
		//verify(getCurr() - start_addr == 26);
		if (mem_access_offset == 0)
			mem_access_offset = getCurr() - start_addr;
		else
			verify(getCurr() - start_addr == mem_access_offset);

		block->memory_accesses[(void*)getCurr()] = (u32)current_opid;
		switch (size)
		{
		case 1:
			movsx(eax, byte[rax + call_regs64[0]]);
			break;

		case 2:
			movsx(eax, word[rax + call_regs64[0]]);
			break;

		case 4:
			mov(eax, dword[rax + call_regs64[0]]);
			break;

		case 8:
			mov(rax, qword[rax + call_regs64[0]]);
			break;

		default:
			die("1..8 bytes");
		}

		while (getCurr() - start_addr < read_mem_op_size)
			nop();
		verify(getCurr() - start_addr == read_mem_op_size);

		return true;
	}

	bool GenWriteMemoryFast(const shil_opcode& op, RuntimeBlockInfo* block)
	{
		if (!mmu_enabled() || !vmem32_enabled())
			return false;
		mem_writes++;
		const u8 *start_addr = getCurr();

		mov(rax, (uintptr_t)&p_sh4rcb->cntx.exception_pc);
		mov(dword[rax], block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0));

		mov(rax, (uintptr_t)virt_ram_base);

		u32 size = op.flags & 0x7f;
		//verify(getCurr() - start_addr == 26);
		if (mem_access_offset == 0)
			mem_access_offset = getCurr() - start_addr;
		else
			verify(getCurr() - start_addr == mem_access_offset);

		block->memory_accesses[(void*)getCurr()] = (u32)current_opid;
		switch (size)
		{
		case 1:
			mov(byte[rax + call_regs64[0] + 0], call_regs[1].cvt8());
			break;

		case 2:
			mov(word[rax + call_regs64[0]], call_regs[1].cvt16());
			break;

		case 4:
			mov(dword[rax + call_regs64[0]], call_regs[1]);
			break;

		case 8:
			mov(qword[rax + call_regs64[0]], call_regs64[1]);
			break;

		default:
			die("1..8 bytes");
		}

		while (getCurr() - start_addr < write_mem_op_size)
			nop();
		verify(getCurr() - start_addr == write_mem_op_size);

		return true;
	}

	void CheckBlock(bool force_checks, RuntimeBlockInfo* block) {
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

	template<class Ret, class... Params>
	void GenCall(Ret(*function)(Params...), bool skip_floats = false)
	{
#ifndef _WIN32
		bool xmm8_mapped = !skip_floats && current_opid != (size_t)-1 && regalloc.IsMapped(xmm8, current_opid);
		bool xmm9_mapped = !skip_floats && current_opid != (size_t)-1 && regalloc.IsMapped(xmm9, current_opid);
		bool xmm10_mapped = !skip_floats && current_opid != (size_t)-1 && regalloc.IsMapped(xmm10, current_opid);
		bool xmm11_mapped = !skip_floats && current_opid != (size_t)-1 && regalloc.IsMapped(xmm11, current_opid);

		// Need to save xmm registers as they are not preserved in linux/mach
		int offset = 0;
		u32 stack_size = 0;
		if (xmm8_mapped || xmm9_mapped || xmm10_mapped || xmm11_mapped)
		{
			stack_size = 4 * (xmm8_mapped + xmm9_mapped + xmm10_mapped + xmm11_mapped);
			stack_size = (((stack_size + 15) >> 4) << 4); // Stack needs to be 16-byte aligned before the call
			sub(rsp, stack_size);
			if (xmm8_mapped)
			{
				movd(ptr[rsp + offset], xmm8);
				offset += 4;
			}
			if (xmm9_mapped)
			{
				movd(ptr[rsp + offset], xmm9);
				offset += 4;
			}
			if (xmm10_mapped)
			{
				movd(ptr[rsp + offset], xmm10);
				offset += 4;
			}
			if (xmm11_mapped)
			{
				movd(ptr[rsp + offset], xmm11);
				offset += 4;
			}
		}
#endif

		call(CC_RX2RW(function));

#ifndef _WIN32
		if (xmm8_mapped || xmm9_mapped || xmm10_mapped || xmm11_mapped)
		{
			if (xmm11_mapped)
			{
				offset -= 4;
				movd(xmm11, ptr[rsp + offset]);
			}
			if (xmm10_mapped)
			{
				offset -= 4;
				movd(xmm10, ptr[rsp + offset]);
			}
			if (xmm9_mapped)
			{
				offset -= 4;
				movd(xmm9, ptr[rsp + offset]);
			}
			if (xmm8_mapped)
			{
				offset -= 4;
				movd(xmm8, ptr[rsp + offset]);
			}
			add(rsp, stack_size);
		}
#endif
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
	static const u32 read_mem_op_size;
	static const u32 write_mem_op_size;
public:
	static u32 mem_access_offset;
};

const u32 BlockCompiler::read_mem_op_size = 30;
const u32 BlockCompiler::write_mem_op_size = 30;
u32 BlockCompiler::mem_access_offset = 0;

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

void ngen_Compile(RuntimeBlockInfo* block, bool smc_checks, bool reset, bool staging, bool optimise)
{
	verify(emit_FreeSpace() >= 16 * 1024);

	compiler = new BlockCompiler();
	
	compiler->compile(block, smc_checks, reset, staging, optimise);

	delete compiler;
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
}

bool ngen_Rewrite(unat& host_pc, unat, unat)
{
	if (!mmu_enabled() || !vmem32_enabled())
		return false;

	//printf("ngen_Rewrite pc %p\n", host_pc);
	RuntimeBlockInfoPtr block = bm_GetBlock((void *)host_pc);
	if (block == NULL)
	{
		WARN_LOG(DYNAREC, "ngen_Rewrite: Block at %p not found", (void *)host_pc);
		return false;
	}
	u8 *code_ptr = (u8*)host_pc;
	auto it = block->memory_accesses.find(code_ptr);
	if (it == block->memory_accesses.end())
	{
		WARN_LOG(DYNAREC, "ngen_Rewrite: memory access at %p not found (%lu entries)", code_ptr, block->memory_accesses.size());
		return false;
	}
	u32 opid = it->second;
	verify(opid < block->oplist.size());
	const shil_opcode& op = block->oplist[opid];

	BlockCompiler *assembler = new BlockCompiler(code_ptr - BlockCompiler::mem_access_offset);
	assembler->InitializeRewrite(block.get(), opid);
	if (op.op == shop_readm)
	{
		mem_rewrites_r++;
		assembler->GenReadMemorySlow(op, block.get());
	}
	else
	{
		mem_rewrites_w++;
		assembler->GenWriteMemorySlow(op, block.get());
	}
	assembler->FinalizeRewrite();
	verify(block->host_code_size >= assembler->getSize());
	delete assembler;
	block->memory_accesses.erase(it);
	host_pc = (unat)(code_ptr - BlockCompiler::mem_access_offset);

	return true;
}

void ngen_HandleException()
{
	longjmp(jmp_env, 1);
}
#endif
