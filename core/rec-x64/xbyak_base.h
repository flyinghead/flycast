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
#pragma once
#include "types.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_rom.h"

#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>

template<typename T, bool ArchX64>
class BaseXbyakRec : public Xbyak::CodeGenerator
{
protected:
	BaseXbyakRec() : BaseXbyakRec((u8 *)emit_GetCCPtr()) { }
	BaseXbyakRec(u8 *code_ptr) : Xbyak::CodeGenerator(emit_FreeSpace(), code_ptr) { }

	using BinaryOp = void (BaseXbyakRec::*)(const Xbyak::Operand&, const Xbyak::Operand&);
	using BinaryFOp = void (BaseXbyakRec::*)(const Xbyak::Xmm&, const Xbyak::Operand&);

	void genBinaryOp(const shil_opcode &op, BinaryOp natop)
	{
		Xbyak::Reg32 rd = mapRegister(op.rd);
		const shil_param *rs2 = &op.rs2;
		if (mapg(op.rd) != mapg(op.rs1))
		{
			if (op.rs2.is_reg() && mapg(op.rd) == mapg(op.rs2))
			{
				if (op.op == shop_sub)
				{
					// This op isn't commutative
					neg(rd);
					add(rd, mapRegister(op.rs1));

					return;
				}
				// otherwise just swap the operands
				rs2 = &op.rs1;
			}
			else
				mov(rd, mapRegister(op.rs1));
		}
		if (op.rs2.is_imm())
		{
			mov(ecx, op.rs2._imm);
			(this->*natop)(rd, ecx);
		}
		else
			(this->*natop)(rd, mapRegister(*rs2));
	}

	void genBinaryFOp(const shil_opcode &op, BinaryFOp natop)
	{
		Xbyak::Xmm rd = mapXRegister(op.rd);
		const shil_param *rs2 = &op.rs2;
		if (mapf(op.rd) != mapf(op.rs1))
		{
			if (op.rs2.is_reg() && mapf(op.rd) == mapf(op.rs2))
			{
				if (op.op == shop_fsub || op.op == shop_fdiv)
				{
					// these ops aren't commutative so we need a scratch reg
					movss(xmm0, mapXRegister(op.rs2));
					movss(rd, mapXRegister(op.rs1));
					(this->*natop)(rd, xmm0);

					return;
				}
				// otherwise just swap the operands
				rs2 = &op.rs1;
			}
			else
				movss(rd, mapXRegister(op.rs1));
		}
		if (op.rs2.is_imm())
		{
			mov(eax, op.rs2._imm);
			movd(xmm0, eax);
			(this->*natop)(rd, xmm0);
		}
		else
			(this->*natop)(rd, mapXRegister(*rs2));
	}

	bool genBaseOpcode(shil_opcode& op)
	{
		switch (op.op)
		{
		case shop_jcond:
		case shop_jdyn:
			{
				Xbyak::Reg32 rd = mapRegister(op.rd);
				// This shouldn't happen since the block type would have been changed to static.
				// But it doesn't hurt and is handy when partially disabling ssa for testing
				if (op.rs1.is_imm())
				{
					if (op.rs2.is_imm())
						mov(rd, op.rs1._imm + op.rs2._imm);
					else
					{
						mov(rd, op.rs1._imm);
						verify(op.rs2.is_null());
					}
				}
				else
				{
					Xbyak::Reg32 rs1 = mapRegister(op.rs1);
					if (rd != rs1)
						mov(rd, rs1);
					if (op.rs2.is_imm())
						add(rd, op.rs2._imm);
				}
			}
			break;

		case shop_mov32:
			verify(op.rd.is_reg());
			verify(op.rs1.is_reg() || op.rs1.is_imm());

			if (isAllocf(op.rd))
				shil_param_to_host_reg(op.rs1, mapXRegister(op.rd));
			else
				shil_param_to_host_reg(op.rs1, mapRegister(op.rd));
			break;

		case shop_swaplb:
			if (mapg(op.rd) != mapg(op.rs1))
				mov(mapRegister(op.rd), mapRegister(op.rs1));
			ror(mapRegister(op.rd).cvt16(), 8);
			break;

		case shop_neg:
			if (mapg(op.rd) != mapg(op.rs1))
				mov(mapRegister(op.rd), mapRegister(op.rs1));
			neg(mapRegister(op.rd));
			break;
		case shop_not:
			if (mapg(op.rd) != mapg(op.rs1))
				mov(mapRegister(op.rd), mapRegister(op.rs1));
			not_(mapRegister(op.rd));
			break;

		case shop_and:
			genBinaryOp(op, &BaseXbyakRec::and_);
			break;
		case shop_or:
			genBinaryOp(op, &BaseXbyakRec::or_);
			break;
		case shop_xor:
			genBinaryOp(op, &BaseXbyakRec::xor_);
			break;
		case shop_add:
			genBinaryOp(op, &BaseXbyakRec::add);
			break;
		case shop_sub:
			genBinaryOp(op, &BaseXbyakRec::sub);
			break;

#define SHIFT_OP(natop) \
			if (mapg(op.rd) != mapg(op.rs1))	\
				mov(mapRegister(op.rd), mapRegister(op.rs1));	\
			if (op.rs2.is_imm())	\
				natop(mapRegister(op.rd), op.rs2._imm);	\
			else  \
				die("Unsupported operand");
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
#undef SHIFT_OP

		case shop_adc:
			{
				cmp(mapRegister(op.rs3), 1);		// C = ~rs3
				Xbyak::Reg32 rs2;
				Xbyak::Reg32 rd = mapRegister(op.rd);
				if (op.rs2.is_reg())
				{
					rs2 = mapRegister(op.rs2);
					if (mapg(op.rd) == mapg(op.rs2))
					{
						mov(ecx, rs2);
						rs2 = ecx;
					}
				}
				if (op.rs1.is_imm())
					mov(rd, op.rs1.imm_value());
				else if (mapg(op.rd) != mapg(op.rs1))
					mov(rd, mapRegister(op.rs1));
				cmc();										// C = rs3
				if (op.rs2.is_reg())
					adc(rd, rs2); 							// (C,rd)=rs1+rs2+rs3(C)
				else
					adc(rd, op.rs2.imm_value());
				Xbyak::Reg32 rd2 = mapRegister(op.rd2);
				if (rd2.getIdx() >= 4 && !ArchX64)
				{
					setc(al);
					movzx(rd2, al);
				}
				else
					setc(rd2.cvt8());	// rd2 = C
			}
			break;

		/* FIXME buggy
		case shop_sbc:
			if (mapg(op.rd) != mapg(op.rs1))
				mov(mapRegister(op.rd), mapRegister(op.rs1));
			cmp(mapRegister(op.rs3), 1);	// C = ~rs3
			cmc();		// C = rs3
			mov(ecx, 1);
			mov(mapRegister(op.rd2), 0);
			mov(eax, mapRegister(op.rs2));
			neg(eax);
			adc(mapRegister(op.rd), eax); // (C,rd)=rs1-rs2+rs3(C)
			cmovc(mapRegister(op.rd2), ecx);	// rd2 = C
			break;
		*/

		//TODO case shop_negc:

		case shop_rocr:
		case shop_rocl:
			{
				Xbyak::Reg32 rd = mapRegister(op.rd);
				cmp(mapRegister(op.rs2), 1);		// C = ~rs2
				if (op.rs1.is_imm())
					mov(rd, op.rs1.imm_value());
				else if (mapg(op.rd) != mapg(op.rs1))
					mov(rd, mapRegister(op.rs1));
				cmc();		// C = rs2
				if (op.op == shop_rocr)
					rcr(rd, 1);
				else
					rcl(rd, 1);
				setc(al);
				movzx(mapRegister(op.rd2), al);	// rd2 = C
			}
			break;

		case shop_shld:
		case shop_shad:
			{
				if (op.rs2.is_reg())
					mov(ecx, mapRegister(op.rs2));
				else
					// This shouldn't happen. If arg is imm -> shop_shl/shr/sar
					mov(ecx, op.rs2.imm_value());
				Xbyak::Reg32 rd = mapRegister(op.rd);
				if (op.rs1.is_imm())
					mov(rd, op.rs1.imm_value());
				else if (mapg(op.rd) != mapg(op.rs1))
					mov(rd, mapRegister(op.rs1));
				Xbyak::Label negative_shift;
				Xbyak::Label non_zero;
				Xbyak::Label exit;

				cmp(ecx, 0);
				js(negative_shift);
				shl(rd, cl);
				jmp(exit);

				L(negative_shift);
				test(ecx, 0x1f);
				jnz(non_zero);
				if (op.op == shop_shld)
					xor_(rd, rd);
				else
					sar(rd, 31);
				jmp(exit);

				L(non_zero);
				neg(ecx);
				if (op.op == shop_shld)
					shr(rd, cl);
				else
					sar(rd, cl);
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
						test(mapRegister(op.rs1), op.rs2._imm);
					else
						test(mapRegister(op.rs1), mapRegister(op.rs2));
				}
				else
				{
					if (op.rs2.is_imm())
						cmp(mapRegister(op.rs1), op.rs2._imm);
					else
						cmp(mapRegister(op.rs1), mapRegister(op.rs2));
				}
				switch (op.op)
				{
				case shop_test:
				case shop_seteq:
					sete(al);
					break;
				case shop_setge:
					setge(al);
					break;
				case shop_setgt:
					setg(al);
					break;
				case shop_setae:
					setae(al);
					break;
				case shop_setab:
					seta(al);
					break;
				default:
					die("invalid case");
					break;
				}
				movzx(mapRegister(op.rd), al);
			}
			break;

		case shop_setpeq:
			{
				Xbyak::Label end;
				mov(ecx, mapRegister(op.rs1));
				if (op.rs2.is_r32i())
					xor_(ecx, mapRegister(op.rs2));
				else
					xor_(ecx, op.rs2._imm);

				Xbyak::Reg32 rd = mapRegister(op.rd);
				mov(rd, 1);
				test(ecx, 0xFF000000);
				je(end);
				test(ecx, 0x00FF0000);
				je(end);
				test(ecx, 0x0000FF00);
				je(end);
				xor_(rd, rd);
				test(cl, cl);
				if (rd.getIdx() >= 4 && !ArchX64)
				{
					sete(al);
					movzx(rd, al);
				}
				else
					sete(rd.cvt8());
				L(end);
			}
			break;

		case shop_mul_u16:
			movzx(eax, mapRegister(op.rs1).cvt16());
			if (op.rs2.is_reg())
				movzx(ecx, mapRegister(op.rs2).cvt16());
			else
				mov(ecx, op.rs2._imm & 0xFFFF);
			mul(ecx);
			mov(mapRegister(op.rd), eax);
			break;
		case shop_mul_s16:
			movsx(eax, mapRegister(op.rs1).cvt16());
			if (op.rs2.is_reg())
				movsx(ecx, mapRegister(op.rs2).cvt16());
			else
				mov(ecx, (s32)(s16)op.rs2._imm);
			mul(ecx);
			mov(mapRegister(op.rd), eax);
			break;
		case shop_mul_i32:
			mov(eax, mapRegister(op.rs1));
			if (op.rs2.is_reg())
				mul(mapRegister(op.rs2));
			else
			{
				mov(ecx, op.rs2._imm);
				mul(ecx);
			}
			mov(mapRegister(op.rd), eax);
			break;
		case shop_mul_u64:
			mov(eax, mapRegister(op.rs1));
			if (op.rs2.is_reg())
				mov(edx, mapRegister(op.rs2));
			else
				mov(edx, op.rs2._imm);
			if (ArchX64)
			{
#ifndef XBYAK32
				mul(rdx);
				mov(mapRegister(op.rd), eax);
				shr(rax, 32);
				mov(mapRegister(op.rd2), eax);
#endif
			}
			else
			{
				mul(edx);
				mov(mapRegister(op.rd), eax);
				mov(mapRegister(op.rd2), edx);
			}
			break;

		case shop_ext_s8:
			mov(eax, mapRegister(op.rs1));
			movsx(mapRegister(op.rd), al);
			break;
		case shop_ext_s16:
			movsx(mapRegister(op.rd), mapRegister(op.rs1).cvt16());
			break;

		case shop_xtrct:
			{
				Xbyak::Reg32 rd = mapRegister(op.rd);
				Xbyak::Reg32 rs1 = ecx;
				if (op.rs1.is_reg())
					rs1 = mapRegister(op.rs1);
				else
					mov(rs1, op.rs1.imm_value());
				Xbyak::Reg32 rs2 = eax;
				if (op.rs2.is_reg())
					rs2 = mapRegister(op.rs2);
				else
					mov(rs2, op.rs2.imm_value());
				if (rd == rs2)
				{
					shl(rd, 16);
					mov(eax, rs1);
					shr(eax, 16);
					or_(rd, eax);
					break;
				}
				else if (rd != rs1)
				{
					mov(rd, rs1);
				}
				shr(rd, 16);
				mov(eax, rs2);
				shl(eax, 16);
				or_(rd, eax);
			}
			break;

		//
		// FPU
		//

		case shop_fadd:
			genBinaryFOp(op, &BaseXbyakRec::addss);
			break;
		case shop_fsub:
			genBinaryFOp(op, &BaseXbyakRec::subss);
			break;
		case shop_fmul:
			genBinaryFOp(op, &BaseXbyakRec::mulss);
			break;
		case shop_fdiv:
			genBinaryFOp(op, &BaseXbyakRec::divss);
			break;

		case shop_fabs:
			movd(eax, mapXRegister(op.rs1));
			and_(eax, 0x7FFFFFFF);
			movd(mapXRegister(op.rd), eax);
			break;
		case shop_fneg:
			movd(eax, mapXRegister(op.rs1));
			xor_(eax, 0x80000000);
			movd(mapXRegister(op.rd), eax);
			break;

		case shop_fsqrt:
			sqrtss(mapXRegister(op.rd), mapXRegister(op.rs1));
			break;

		case shop_fmac:
			{
				Xbyak::Xmm rs1 = mapXRegister(op.rs1);
				Xbyak::Xmm rs2 = mapXRegister(op.rs2);
				Xbyak::Xmm rs3 = mapXRegister(op.rs3);
				Xbyak::Xmm rd = mapXRegister(op.rd);
				if (rd == rs2)
				{
					movss(xmm1, rs2);
					rs2 = xmm1;
				}
				if (rd == rs3)
				{
					movss(xmm2, rs3);
					rs3 = xmm2;
				}
				if (op.rs1.is_imm())
				{
					mov(eax, op.rs1._imm);
					movd(rd, eax);
				}
				else if (rd != rs1)
				{
					movss(rd, rs1);
				}
				//if (cpu.has(Xbyak::util::Cpu::tFMA))
				//	vfmadd231ss(rd, rs2, rs3);
				//else
				{
					movss(xmm0, rs2);
					mulss(xmm0, rs3);
					addss(rd, xmm0);
				}
			}
			break;

		case shop_fsrra:
			// RSQRTSS has an |error| <= 1.5*2^-12 where the SH4 FSRRA needs |error| <= 2^-21
			sqrtss(xmm0, mapXRegister(op.rs1));
			if (ArchX64)
			{
				mov(eax, 0x3f800000);	// 1.0
				movd(mapXRegister(op.rd), eax);
			}
			else
			{
				static float one = 1.f;
				movss(mapXRegister(op.rd), dword[&one]);
			}
			divss(mapXRegister(op.rd), xmm0);
			break;

		case shop_fsetgt:
		case shop_fseteq:
			ucomiss(mapXRegister(op.rs1), mapXRegister(op.rs2));
			if (op.op == shop_fsetgt)
			{
				seta(al);
			}
			else
			{
#ifdef __FAST_MATH__
				sete(al);
#else
				mov(edx, 0);
				setnp(al);	// Parity means unordered (NaN), ZF is set too
				cmovne(eax, edx);
#endif
			}
			movzx(mapRegister(op.rd), al);
			break;

		case shop_fsca:
			if (op.rs1.is_imm())
				mov(eax, op.rs1._imm & 0xFFFF);
			else
				movzx(eax, mapRegister(op.rs1).cvt16());
			if (ArchX64)
			{
#ifndef XBYAK32
				mov(rcx, (uintptr_t)&sin_table);
				mov(rcx, qword[rcx + rax * 8]);
#if ALLOC_F64 == false
				mov(rdx, (uintptr_t)op.rd.reg_ptr());
				mov(qword[rdx], rcx);
#else
				movd(mapXRegister(op.rd, 0), ecx);
				shr(rcx, 32);
				movd(mapXRegister(op.rd, 1), ecx);
#endif
#endif
			}
			else
			{
#if ALLOC_F64 == true
				movss(mapXRegister(op.rd, 0), dword[(size_t)&sin_table + eax * 8]);
				movss(mapXRegister(op.rd, 1), dword[(size_t)&sin_table[0].u[1] + eax * 8]);
#else
				verify(!isAllocAny(op.rd));
				mov(ecx, dword[(size_t)&sin_table + eax * 8]);
				mov(edx, dword[(size_t)&sin_table[0].u[1] + eax * 8]);
				mov(dword[op.rd.reg_ptr()], ecx);
				mov(dword[op.rd.reg_ptr() + 1], edx);
#endif
			}
			break;

		case shop_cvt_f2i_t:
			{
				Xbyak::Reg32 rd = mapRegister(op.rd);
		        Xbyak::Label done;

		        cvttss2si(edx, mapXRegister(op.rs1));
		        mov(rd, 0x7fffffff);
		        cmp(edx, 0x7fffff80);
		        jg(done, T_SHORT);
		        mov(rd, edx);
		        cmp(rd, 0x80000000);	// indefinite integer
		        jne(done, T_SHORT);
		        xor_(eax, eax);
		        pxor(xmm0, xmm0);
		        ucomiss(mapXRegister(op.rs1), xmm0);
		        setb(al);
		        add(eax, 0x7fffffff);
		        mov(rd, eax);
		        L(done);
			}
			break;
		case shop_cvt_i2f_n:
		case shop_cvt_i2f_z:
			cvtsi2ss(mapXRegister(op.rd), mapRegister(op.rs1));
			break;

		default:
			return false;
		}
		return true;
	}

	// uses eax/rax
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
			if (isAllocf(param))
			{
				if (param.is_r32f() || param.is_r64f())
				{
					Xbyak::Xmm sreg = mapXRegister(param, 0);
					if (!reg.isXMM())
						movd(reg.cvt32(), sreg);
					else if (reg != sreg)
						movss((const Xbyak::Xmm &)reg, sreg);
#ifndef XBYAK32
					if (param.is_r64f())
					{
						sreg = mapXRegister(param, 1);
						verify(reg != rax);
						movd(eax, sreg);
						shl(rax, 32);
						or_(reg, rax);
					}
#endif
				}
				else
				{
					verify(!reg.isXMM());
					if (ArchX64)
					{
#ifndef XBYAK32
						mov(rax, (size_t)param.reg_ptr());
						mov(reg.cvt32(), dword[rax]);
#endif
					}
					else
					{
						mov(reg.cvt32(), dword[param.reg_ptr()]);
					}
				}
			}
			else if (isAllocg(param))
			{
				Xbyak::Reg32 sreg = mapRegister(param);
				if (reg.isXMM())
					movd((const Xbyak::Xmm &)reg, sreg);
				else if (reg != sreg)
					mov(reg.cvt32(), sreg);
			}
			else
			{
				if (ArchX64)
				{
#ifndef XBYAK32
					mov(rax, (size_t)param.reg_ptr());
					if (!reg.isXMM())
						mov(reg.cvt32(), dword[rax]);
					else
						movss((const Xbyak::Xmm &)reg, dword[rax]);
#endif
				}
				else
				{
					if (!reg.isXMM())
						mov(reg.cvt32(), dword[param.reg_ptr()]);
					else
						movss((const Xbyak::Xmm &)reg, dword[param.reg_ptr()]);
				}
			}
		}
		else
		{
			verify(param.is_null());
		}
	}

	// 64-bit: uses rax
	void host_reg_to_shil_param(const shil_param& param, const Xbyak::Reg& reg)
	{
		if (isAllocg(param))
		{
			Xbyak::Reg32 sreg = mapRegister(param);
			if (!reg.isXMM())
				mov(sreg, reg.cvt32());
			else if (reg != sreg)
				movd(sreg, (const Xbyak::Xmm &)reg);
		}
		else if (isAllocf(param))
		{
			Xbyak::Xmm sreg = mapXRegister(param, 0);
			if (!reg.isXMM())
				movd(sreg, reg.cvt32());
			else if (reg != sreg)
				movss(sreg, (const Xbyak::Xmm &)reg);
#ifndef XBYAK32
			if (param.is_r64f())
			{
				sreg = mapXRegister(param, 1);
				shr(reg, 32);
				movd(sreg, reg.cvt32());
			}
#endif
		}
		else
		{
			if (ArchX64)
			{
#ifndef XBYAK32
				mov(rax, (size_t)param.reg_ptr());
				if (!reg.isXMM())
					mov(dword[rax], reg.cvt32());
				else
					movss(dword[rax], (const Xbyak::Xmm &)reg);
#endif
			}
			else
			{
				if (!reg.isXMM())
					mov(dword[param.reg_ptr()], reg.cvt32());
				else
					movss(dword[param.reg_ptr()], (const Xbyak::Xmm &)reg);
			}
		}
	}

private:
	Xbyak::Reg32 mapRegister(const shil_param& param) {
		return static_cast<T*>(this)->regalloc.MapRegister(param);
	}

	Xbyak::Xmm mapXRegister(const shil_param& param, int index = 0) {
		return static_cast<T*>(this)->regalloc.MapXRegister(param, index);
	}

	int mapg(const shil_param& param) {
		return (int)static_cast<T*>(this)->regalloc.mapg(param);
	}

	int mapf(const shil_param& param, int index = 0) {
		return (int)static_cast<T*>(this)->regalloc.mapf(param, index);
	}

	bool isAllocg(const shil_param& param) {
		return static_cast<T*>(this)->regalloc.IsAllocg(param);
	}

	bool isAllocf(const shil_param& param) {
		return static_cast<T*>(this)->regalloc.IsAllocf(param);
	}

	bool isAllocAny(const shil_param& param) {
		return static_cast<T*>(this)->regalloc.IsAllocAny(param);
	}
};
