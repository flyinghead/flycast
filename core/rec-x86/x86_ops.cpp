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

//#define CANONICAL_TEST

#include "rec_x86.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/mem/addrspace.h"
#include "oslib/unwind_info.h"

extern UnwindInfo unwinder;

namespace MemSize {
enum {
	S8,
	S16,
	S32,
	F32,
	F64,
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

void X86Compiler::genMemHandlers()
{
	MemHandlerStart = getCurr();
	for (int type = 0; type < MemType::Count; type++)
	{
		for (int size = 0; size < MemSize::Count; size++)
		{
			for (int op = 0; op < MemOp::Count; op++)
			{
				MemHandlers[type][size][op] = getCurr();

				if (type == MemType::Fast && addrspace::virtmemEnabled())
				{
					// save the original address in eax so it can be restored during rewriting
					mov(eax, ecx);
					and_(ecx, 0x1FFFFFFF);
					Xbyak::Address address = dword[ecx];
					Xbyak::Reg reg;
					switch (size)
					{
					case MemSize::S8:
						address = byte[ecx + (size_t)addrspace::ram_base];
						reg = op == MemOp::R ? (Xbyak::Reg)eax : (Xbyak::Reg)dl;
						break;
					case MemSize::S16:
						address = word[ecx + (size_t)addrspace::ram_base];
						reg = op == MemOp::R ? (Xbyak::Reg)eax : (Xbyak::Reg)dx;
						break;
					case MemSize::S32:
						address = dword[ecx + (size_t)addrspace::ram_base];
						reg = op == MemOp::R ? eax : edx;
						break;
					default:
						address = dword[ecx + (size_t)addrspace::ram_base];
						break;
					}
					if (size >= MemSize::F32)
					{
						if (op == MemOp::R)
							movss(xmm0, address);
						else
							movss(address, xmm0);
						if (size == MemSize::F64)
						{
							address = dword[ecx + (size_t)addrspace::ram_base + 4];
							if (op == MemOp::R)
								movss(xmm1, address);
							else
								movss(address, xmm1);
						}
					}
					else
					{
						if (op == MemOp::R)
						{
							if (size <= MemSize::S16)
								movsx(reg, address);
							else
								mov(reg, address);
						}
						else
							mov(address, reg);
					}
				}
				else if (type == MemType::StoreQueue)
				{
					if (op != MemOp::W || size < MemSize::S32)
						continue;
					Xbyak::Label no_sqw;

					mov(eax, ecx);
					shr(eax, 28);
					cmp(eax, 0xE);
					jne(no_sqw);
					and_(ecx, 0x3F);

					if (size == MemSize::S32)
						mov(dword[(size_t)p_sh4rcb->sq_buffer + ecx], edx);
					else if (size >= MemSize::F32)
					{
						movss(dword[(size_t)p_sh4rcb->sq_buffer + ecx], xmm0);
						if (size == MemSize::F64)
							movss(dword[((size_t)p_sh4rcb->sq_buffer + 4) + ecx], xmm1);
					}
					ret();
					L(no_sqw);
					// TODO Fall through SQ -> slow path to avoid code dup
					if (size == MemSize::F64)
					{
#ifndef _WIN32
						// 16-byte alignment
						alignStack(-12);
#else
						sub(esp, 8);
						unwinder.allocStackPtr(getCurr(), 8);
#endif
						movss(dword[esp], xmm0);
						movss(dword[esp + 4], xmm1);
						call((const void *)addrspace::write64);	// dynacall adds 8 to esp
						alignStack(4);
					}
					else
					{
						if (size == MemSize::F32)
							movd(edx, xmm0);
						jmp((const void *)addrspace::write32);	// tail call
						continue;
					}
				}
				else
				{
					// Slow path
					if (op == MemOp::R)
					{
						switch (size) {
						case MemSize::S8:
							// 16-byte alignment
							alignStack(-12);
							call((const void *)addrspace::read8);
							movsx(eax, al);
							alignStack(12);
							break;
						case MemSize::S16:
							// 16-byte alignment
							alignStack(-12);
							call((const void *)addrspace::read16);
							movsx(eax, ax);
							alignStack(12);
							break;
						case MemSize::S32:
							jmp((const void *)addrspace::read32);	// tail call
							continue;
						case MemSize::F32:
							// 16-byte alignment
							alignStack(-12);
							call((const void *)addrspace::read32);
							movd(xmm0, eax);
							alignStack(12);
							break;
						case MemSize::F64:
							// 16-byte alignment
							alignStack(-12);
							call((const void *)addrspace::read64);
							movd(xmm0, eax);
							movd(xmm1, edx);
							alignStack(12);
							break;
						default:
							die("1..8 bytes");
						}
					}
					else
					{
						switch (size) {
						case MemSize::S8:
							jmp((const void *)addrspace::write8);	// tail call
							continue;
						case MemSize::S16:
							jmp((const void *)addrspace::write16);	// tail call
							continue;
						case MemSize::S32:
							jmp((const void *)addrspace::write32);	// tail call
							continue;
						case MemSize::F32:
							movd(edx, xmm0);
							jmp((const void *)addrspace::write32);	// tail call
							continue;
						case MemSize::F64:
#ifndef _WIN32
							// 16-byte alignment
							alignStack(-12);
#else
							sub(esp, 8);
							unwinder.allocStackPtr(getCurr(), 8);
#endif
							movss(dword[esp], xmm0);
							movss(dword[esp + 4], xmm1);
							call((const void *)addrspace::write64);	// dynacall adds 8 to esp
							alignStack(4);
							break;
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

void X86Compiler::genMmuLookup(RuntimeBlockInfo* block, const shil_opcode& op, u32 write)
{
	if (mmu_enabled())
	{
#ifdef FAST_MMU
		Xbyak::Label inCache;
		Xbyak::Label done;

		mov(eax, ecx);
		shr(eax, 12);
		mov(eax, dword[(uintptr_t)mmuAddressLUT + eax * 4]);
		test(eax, eax);
		jne(inCache);
#endif
		mov(edx, write);
		push(block->vaddr + op.guest_offs - (op.delay_slot ? 2 : 0));	// pc
		call((void*)mmuDynarecLookup);
		mov(ecx, eax);
#ifdef FAST_MMU
		jmp(done);
		L(inCache);
		and_(ecx, 0xFFF);
		or_(ecx, eax);
		L(done);
#endif
	}
}

[[noreturn]]
static void DYNACALL handle_sh4_exception(SH4ThrownException& ex, u32 pc)
{
	if (pc & 1)
	{
		// Delay slot
		AdjustDelaySlotException(ex);
		pc--;
	}
	Do_Exception(pc, ex.expEvn);
	p_sh4rcb->cntx.cycle_counter += 4;	// probably more is needed
	X86Compiler::handleException();
	// not reached
	std::abort();
}

static void DYNACALL interpreter_fallback(u16 op, OpCallFP *oph, u32 pc)
{
	try {
		oph(op);
	} catch (SH4ThrownException& ex) {
		handle_sh4_exception(ex, pc);
	}
}

static void DYNACALL do_sqw_mmu_no_ex(u32 addr, u32 pc)
{
	try {
		do_sqw_mmu(addr);
	} catch (SH4ThrownException& ex) {
		handle_sh4_exception(ex, pc);
	}
}

void X86Compiler::genOpcode(RuntimeBlockInfo* block, bool optimise, shil_opcode& op)
{
	switch (op.op)
	{
	case shop_ifb:
		if (mmu_enabled())
		{
			mov(edx, reinterpret_cast<uintptr_t>(*OpDesc[op.rs3._imm]->oph));	// op handler
			push(block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc
		}
		if (op.rs1.is_imm() && op.rs1.imm_value())
			mov(dword[&next_pc], op.rs2.imm_value());
		mov(ecx, op.rs3.imm_value());
		if (!mmu_enabled())
			genCall(OpDesc[op.rs3.imm_value()]->oph);
		else
			genCall(interpreter_fallback);

		break;

	case shop_mov64:
		verify(op.rd.is_r64f());
		verify(op.rs1.is_r64f());

#if ALLOC_F64 == true
		movss(regalloc.MapXRegister(op.rd, 0), regalloc.MapXRegister(op.rs1, 0));
		movss(regalloc.MapXRegister(op.rd, 1), regalloc.MapXRegister(op.rs1, 1));
#else
		verify(!regalloc.IsAllocAny(op.rd));
		movq(xmm0, qword[op.rs1.reg_ptr()]);
		movq(qword[op.rd.reg_ptr()], xmm0);
#endif
		break;

	case shop_readm:
		if (!genReadMemImmediate(op, block))
		{
			// Not an immediate address
			shil_param_to_host_reg(op.rs1, ecx);
			if (!op.rs3.is_null())
			{
				if (op.rs3.is_imm())
					add(ecx, op.rs3._imm);
				else if (regalloc.IsAllocg(op.rs3))
					add(ecx, regalloc.MapRegister(op.rs3));
				else
					add(ecx, dword[op.rs3.reg_ptr()]);
			}

			int memOpSize;
			switch (op.size)
			{
			case 1:
				memOpSize = MemSize::S8;
				break;
			case 2:
				memOpSize = MemSize::S16;
				break;
			case 4:
				memOpSize = regalloc.IsAllocf(op.rd) ? MemSize::F32 : MemSize::S32;
				break;
			case 8:
				memOpSize = MemSize::F64;
				break;
			}

			if (mmu_enabled())
				freezeXMM();
			genMmuLookup(block, op, 0);
			const u8 *start = getCurr();
			call(MemHandlers[optimise ? MemType::Fast : MemType::Slow][memOpSize][MemOp::R]);
			verify(getCurr() - start == 5);
			if (mmu_enabled())
				thawXMM();

			if (memOpSize <= MemSize::S32)
			{
				host_reg_to_shil_param(op.rd, eax);
			}
			else if (memOpSize == MemSize::F32)
			{
				host_reg_to_shil_param(op.rd, xmm0);
			}
			else
			{
				if (op.rd.count() == 2 && regalloc.IsAllocf(op.rd))
				{
					mov(regalloc.MapXRegister(op.rd, 0), xmm0);
					mov(regalloc.MapXRegister(op.rd, 1), xmm1);
				}
				else
				{
					verify(!regalloc.IsAllocAny(op.rd));
					movss(dword[op.rd.reg_ptr()], xmm0);
					movss(dword[op.rd.reg_ptr() + 1], xmm1);
				}
			}
		}
		break;

	case shop_writem:
		if (!genWriteMemImmediate(op, block))
		{
			shil_param_to_host_reg(op.rs1, ecx);
			if (!op.rs3.is_null())
			{
				if (op.rs3.is_imm())
					add(ecx, op.rs3._imm);
				else if (regalloc.IsAllocg(op.rs3))
					add(ecx, regalloc.MapRegister(op.rs3));
				else
					add(ecx, dword[op.rs3.reg_ptr()]);
			}

			int memOpSize;
			switch (op.size)
			{
			case 1:
				memOpSize = MemSize::S8;
				break;
			case 2:
				memOpSize = MemSize::S16;
				break;
			case 4:
				memOpSize = regalloc.IsAllocf(op.rs2) ? MemSize::F32 : MemSize::S32;
				break;
			case 8:
				memOpSize = MemSize::F64;
				break;
			}

			if (mmu_enabled())
				freezeXMM();
			genMmuLookup(block, op, 1);

			if (memOpSize <= MemSize::S32)
				shil_param_to_host_reg(op.rs2, edx);
			else if (memOpSize == MemSize::F32)
				shil_param_to_host_reg(op.rs2, xmm0);
			else {
				if (op.rs2.count() == 2 && regalloc.IsAllocf(op.rs2))
				{
					mov(xmm0, regalloc.MapXRegister(op.rs2, 0));
					mov(xmm1, regalloc.MapXRegister(op.rs2, 1));
				}
				else
				{
					movd(xmm0, dword[op.rs2.reg_ptr()]);
					movd(xmm1, dword[op.rs2.reg_ptr() + 1]);
				}
			}
			const u8 *start = getCurr();
			call(MemHandlers[optimise ? MemType::Fast : MemType::Slow][memOpSize][MemOp::W]);
			verify(getCurr() - start == 5);
			if (mmu_enabled())
				thawXMM();
		}
		break;

	case shop_jcond:
	case shop_jdyn:
	case shop_mov32:
		genBaseOpcode(op);
		break;

#ifndef CANONICAL_TEST
	case shop_sync_sr:
		genCallCdecl(UpdateSR);
		break;
	case shop_sync_fpscr:
		genCallCdecl(UpdateFPSCR);
		break;

	case shop_pref:
		{
			Xbyak::Label no_sqw;

			if (op.rs1.is_imm())
			{
				// this test shouldn't be necessary
				if ((op.rs1.imm_value() & 0xF0000000) != 0xE0000000)
					break;
				mov(ecx, op.rs1.imm_value());
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
					mov(eax, dword[op.rs1.reg_ptr()]);
					rn = eax;
				}
				mov(ecx, rn);
				shr(ecx, 28);
				cmp(ecx, 0xE);
				jne(no_sqw);

				mov(ecx, rn);
			}
			if (mmu_enabled())
			{
				mov(edx, block->vaddr + op.guest_offs - (op.delay_slot ? 1 : 0));	// pc
				genCall(do_sqw_mmu_no_ex);
			}
			else
			{
				mov(edx, (size_t)sh4rcb.sq_buffer);
				freezeXMM();
				call(dword[&do_sqw_nommu]);
				thawXMM();
			}
			L(no_sqw);
		}
		break;

	case shop_mul_s64:
		mov(eax, regalloc.MapRegister(op.rs1));
		if (op.rs2.is_reg())
			mov(edx, regalloc.MapRegister(op.rs2));
		else
			mov(edx, op.rs2._imm);
		imul(edx);
		mov(regalloc.MapRegister(op.rd), eax);
		mov(regalloc.MapRegister(op.rd2), edx);
		break;

	case shop_frswap:
		mov(eax, (uintptr_t)op.rs1.reg_ptr());
		mov(ecx, (uintptr_t)op.rd.reg_ptr());
		for (int i = 0; i < 4; i++)
		{
			movaps(xmm0, xword[eax + (i * 16)]);
			movaps(xmm1, xword[ecx + (i * 16)]);
			movaps(xword[eax + (i * 16)], xmm1);
			movaps(xword[ecx + (i * 16)], xmm0);
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
}

bool X86Compiler::rewriteMemAccess(host_context_t &context)
{
	u8 *retAddr = *(u8 **)context.esp;
	//DEBUG_LOG(DYNAREC, "rewriteMemAccess hpc %08x retadr %08x", context.pc, (size_t)retAddr);
	if (context.pc < (size_t)MemHandlerStart || context.pc >= (size_t)MemHandlerEnd)
		return false;

	void *ca = *(u32 *)(retAddr - 4) + retAddr;

	for (int size = 0; size < MemSize::Count; size++)
	{
		for (int op = 0; op < MemOp::Count; op++)
		{
			if ((void *)MemHandlers[MemType::Fast][size][op] != ca)
				continue;

			//found !
			const u8 *start = getCurr();
			if (op == MemOp::W && size >= MemSize::S32 && (context.eax >> 28) == 0xE)
				call(MemHandlers[MemType::StoreQueue][size][MemOp::W]);
			else
				call(MemHandlers[MemType::Slow][size][op]);
			verify(getCurr() - start == 5);

			ready();

			context.pc = (size_t)(retAddr - 5);
			//remove the call from call stack
			context.esp += 4;
			//restore the addr from eax to ecx so it's valid again
			context.ecx = context.eax;

			return true;
		}
	}
	ERROR_LOG(DYNAREC, "rewriteMemAccess code not found: hpc %08x retadr %p acc %08x", context.pc, retAddr, context.eax);
	die("Failed to match the code");

	return false;
}
#endif
