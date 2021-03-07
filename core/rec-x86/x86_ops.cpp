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
#include "hw/mem/_vmem.h"

namespace MemOp {
enum Size {
	S8,
	S16,
	S32,
	F32,
	F64,
	SizeCount
};

enum Op {
	R,
	W,
	OpCount
};

enum Type {
	Fast,
	Slow,
	TypeCount
};
}

static const void *MemHandlers[MemOp::TypeCount][MemOp::SizeCount][MemOp::OpCount];
static const u8 *MemHandlerStart, *MemHandlerEnd;

void X86Compiler::genMemHandlers()
{
	// make sure the memory handlers are set
	verify(ReadMem8 != nullptr);

	MemHandlerStart = getCurr();
	for (int type = 0; type < MemOp::TypeCount; type++)
	{
		for (int size = 0; size < MemOp::SizeCount; size++)
		{
			for (int op = 0; op < MemOp::OpCount; op++)
			{
				MemHandlers[type][size][op] = getCurr();

				if (type == MemOp::Fast && _nvmem_enabled())
				{
					mov(eax, ecx);
					and_(ecx, 0x1FFFFFFF);
					Xbyak::Address address = dword[ecx];
					Xbyak::Reg reg;
					switch (size)
					{
					case MemOp::S8:
						address = byte[ecx + (size_t)virt_ram_base];
						reg = op == MemOp::R ? (Xbyak::Reg)eax : (Xbyak::Reg)dl;
						break;
					case MemOp::S16:
						address = word[ecx + (size_t)virt_ram_base];
						reg = op == MemOp::R ? (Xbyak::Reg)eax : (Xbyak::Reg)dx;
						break;
					case MemOp::S32:
						address = dword[ecx + (size_t)virt_ram_base];
						reg = op == MemOp::R ? eax : edx;
						break;
					default:
						address = dword[ecx + (size_t)virt_ram_base];
						break;
					}
					if (size >= MemOp::F32)
					{
						if (op == MemOp::R)
							movss(xmm0, address);
						else
							movss(address, xmm0);
						if (size == MemOp::F64)
						{
							address = dword[ecx + (size_t)virt_ram_base + 4];
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
							if (size <= MemOp::S16)
								movsx(reg, address);
							else
								mov(reg, address);
						}
						else
							mov(address, reg);
					}
				}
				else
				{
					// Slow path
					if (op == MemOp::R)
					{
						switch (size) {
						case MemOp::S8:
							// 16-byte alignment
							alignStack(-12);
							call((const void *)ReadMem8);
							movsx(eax, al);
							alignStack(12);
							break;
						case MemOp::S16:
							// 16-byte alignment
							alignStack(-12);
							call((const void *)ReadMem16);
							movsx(eax, ax);
							alignStack(12);
							break;
						case MemOp::S32:
							jmp((const void *)ReadMem32);	// tail call
							continue;
						case MemOp::F32:
							// 16-byte alignment
							alignStack(-12);
							call((const void *)ReadMem32);
							movd(xmm0, eax);
							alignStack(12);
							break;
						case MemOp::F64:
							// 16-byte alignment
							alignStack(-12);
							call((const void *)ReadMem64);
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
						case MemOp::S8:
							jmp((const void *)WriteMem8);	// tail call
							continue;
						case MemOp::S16:
							jmp((const void *)WriteMem16);	// tail call
							continue;
						case MemOp::S32:
							jmp((const void *)WriteMem32);	// tail call
							continue;
						case MemOp::F32:
							movd(edx, xmm0);
							jmp((const void *)WriteMem32);	// tail call
							continue;
						case MemOp::F64:
#ifndef _WIN32
							// 16-byte alignment
							alignStack(-12);
#else
							sub(esp, 8);
#endif
							movss(dword[esp], xmm0);
							movss(dword[esp + 4], xmm1);
							call((const void *)WriteMem64);	// dynacall adds 8 to esp
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

void X86Compiler::genOpcode(RuntimeBlockInfo* block, bool optimise, shil_opcode& op)
{
	switch (op.op)
	{
	case shop_ifb:
		if (op.rs1.is_imm() && op.rs1.imm_value())
			mov(dword[&next_pc], op.rs2.imm_value());
		mov(ecx, op.rs3.imm_value());
		genCall(OpDesc[op.rs3.imm_value()]->oph);

		break;

	case shop_mov64:
		verify(op.rd.is_r64());
		verify(op.rs1.is_r64());

#ifdef EXPLODE_SPANS
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
			switch (op.flags & 0x7f)
			{
			case 1:
				memOpSize = MemOp::S8;
				break;
			case 2:
				memOpSize = MemOp::S16;
				break;
			case 4:
				memOpSize = regalloc.IsAllocf(op.rd) ? MemOp::F32 : MemOp::S32;
				break;
			case 8:
				memOpSize = MemOp::F64;
				break;
			}

			freezeXMM();
			const u8 *start = getCurr();
			call(MemHandlers[optimise ? MemOp::Fast : MemOp::Slow][memOpSize][MemOp::R]);
			verify(getCurr() - start == 5);
			thawXMM();

			if (memOpSize <= MemOp::S32)
			{
				host_reg_to_shil_param(op.rd, eax);
			}
			else if (memOpSize == MemOp::F32)
			{
				host_reg_to_shil_param(op.rd, xmm0);
			}
			else
			{
#ifdef EXPLODE_SPANS
				if (op.rd.count() == 2 && regalloc.IsAllocf(op.rd, 0) && regalloc.IsAllocf(op.rd, 1))
				{
					mov(regalloc.MapXRegister(op.rd, 0), xmm0);
					mov(regalloc.MapXRegister(op.rd, 1), xmm1);
				}
				else
#endif
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
			switch (op.flags & 0x7f)
			{
			case 1:
				memOpSize = MemOp::S8;
				break;
			case 2:
				memOpSize = MemOp::S16;
				break;
			case 4:
				memOpSize = regalloc.IsAllocf(op.rs2) ? MemOp::F32 : MemOp::S32;
				break;
			case 8:
				memOpSize = MemOp::F64;
				break;
			}

			if (memOpSize <= MemOp::S32)
				shil_param_to_host_reg(op.rs2, edx);
			else if (memOpSize == MemOp::F32)
				shil_param_to_host_reg(op.rs2, xmm0);
			else {
#ifdef EXPLODE_SPANS
				if (op.rs2.count() == 2 && regalloc.IsAllocf(op.rs2, 0) && regalloc.IsAllocf(op.rs2, 1))
				{
					mov(xmm0, regalloc.MapXRegister(op.rs2, 0));
					mov(xmm1, regalloc.MapXRegister(op.rs2, 1));
				}
				else
#endif
				{
					movd(xmm0, dword[op.rs2.reg_ptr()]);
					movd(xmm1, dword[op.rs2.reg_ptr() + 1]);
				}
			}
			freezeXMM();
			const u8 *start = getCurr();
			call(MemHandlers[optimise ? MemOp::Fast : MemOp::Slow][memOpSize][MemOp::W]);
			verify(getCurr() - start == 5);
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
				if ((op.rs1.imm_value() & 0xFC000000) != 0xE0000000)
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
				shr(ecx, 26);
				cmp(ecx, 0x38);
				jne(no_sqw);

				mov(ecx, rn);
			}
			if (CCN_MMUCR.AT == 1)
				genCall(do_sqw_mmu);
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

	for (int size = 0; size < MemOp::SizeCount; size++)
	{
		for (int op = 0; op < MemOp::OpCount; op++)
		{
			if ((void *)MemHandlers[MemOp::Fast][size][op] != ca)
				continue;

			//found !
			const u8 *start = getCurr();
			call(MemHandlers[MemOp::Slow][size][op]);
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
