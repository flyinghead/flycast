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

void X86Compiler::genOpcode(RuntimeBlockInfo* block, bool optimise, shil_opcode& op)
{
	switch (op.op)
	{
	case shop_ifb:
		if (op.rs1._imm)
			mov(dword[&next_pc], op.rs2._imm);
		mov(ecx, op.rs3._imm);
		genCall(OpDesc[op.rs3._imm]->oph);

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
			if (!optimise || !genReadMemoryFast(op, block))
				genReadMemorySlow(op, block);

			u32 size = op.flags & 0x7f;
			if (size != 8)
				host_reg_to_shil_param(op.rd, eax);
			else {
#ifdef EXPLODE_SPANS
				if (op.rd.count() == 2 && regalloc.IsAllocf(op.rd, 0) && regalloc.IsAllocf(op.rd, 1))
				{
					movd(regalloc.MapXRegister(op.rd, 0), eax);
					movd(regalloc.MapXRegister(op.rd, 1), edx);
				}
				else
#endif
				{
					verify(!regalloc.IsAllocAny(op.rd));
					mov(dword[op.rd.reg_ptr()], eax);
					mov(dword[op.rd.reg_ptr() + 1], edx);
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

			u32 size = op.flags & 0x7f;
			if (size != 8)
				shil_param_to_host_reg(op.rs2, edx);
			else {
#ifdef EXPLODE_SPANS
				if (op.rs2.count() == 2 && regalloc.IsAllocf(op.rs2, 0) && regalloc.IsAllocf(op.rs2, 1))
				{
					sub(esp, 8);
					movsd(dword[esp + 4], regalloc.MapXRegister(op.rs2, 1));
					movsd(dword[esp], regalloc.MapXRegister(op.rs2, 0));
				}
				else
#endif
				{
					sub(esp, 8);
					mov(eax, dword[op.rs2.reg_ptr() + 1]);
					mov(dword[esp + 4], eax);
					mov(eax, dword[op.rs2.reg_ptr()]);
					mov(dword[esp], eax);
				}
			}
			if (!optimise || !genWriteMemoryFast(op, block))
				genWriteMemorySlow(op, block);
		}
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

#endif
