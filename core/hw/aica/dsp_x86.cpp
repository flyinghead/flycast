/*
	Copyright 2021 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "build.h"

#if HOST_CPU == CPU_X86 && FEAT_DSPREC != DYNAREC_NONE

#include <xbyak/xbyak.h>
#include "dsp.h"
#include "aica.h"
#include "aica_if.h"
#include "hw/mem/_vmem.h"

#define CC_RW2RX(ptr) (ptr)
#define CC_RX2RW(ptr) (ptr)

alignas(4096) static u8 CodeBuffer[32 * 1024]
#if defined(_WIN32)
	;
#elif defined(__unix__)
	__attribute__((section(".text")));
#elif defined(__APPLE__)
	__attribute__((section("__TEXT,.text")));
#else
	#error CodeBuffer code section unknown
#endif
static u8 *pCodeBuffer;

class X86DSPAssembler : public Xbyak::CodeGenerator
{
public:
	X86DSPAssembler(u8 *code_buffer, size_t size) : Xbyak::CodeGenerator(size, code_buffer) {}

	void Compile(struct dsp_t *DSP)
	{
		this->DSP = DSP;
		DEBUG_LOG(AICA_ARM, "X86DSPAssembler::Compile recompiling for x86 at %p", this->getCode());

		push(esi);
		push(edi);
		push(ebp);
		push(ebx);
#ifndef _WIN32
		// 16-byte alignment
		sub(esp, 12);
#endif
		const Xbyak::Reg32 INPUTS = esi;	// 24 bits
		const Xbyak::Reg32 ACC = edi;		// 26 bits - saved
		const Xbyak::Reg32 X = ebp;			// 24 bits
		const Xbyak::Reg32 Y = ebx;			// 13 bits

		xor_(ACC, ACC);
		mov(dword[&DSP->FRC_REG], 0);
		mov(dword[&DSP->Y_REG], 0);
		mov(dword[&DSP->ADRS_REG], 0);

		for (int step = 0; step < 128; ++step)
		{
			u32 *mpro = &DSPData->MPRO[step * 4];
			_INST op;
			DecodeInst(mpro, &op);
			const u32 COEF = step;

			if (op.XSEL || op.YRL || (op.ADRL && op.SHIFT != 3))
			{
				if (op.IRA <= 0x1f)
					//INPUTS = DSP->MEMS[op.IRA];
					mov(INPUTS, dword[&DSP->MEMS[op.IRA]]);
				else if (op.IRA <= 0x2F)
				{
					//INPUTS = DSP->MIXS[op.IRA - 0x20] << 4;		// MIXS is 20 bit
					mov(INPUTS, dword[&DSP->MIXS[op.IRA - 0x20]]);
					shl(INPUTS, 4);
				}
				else if (op.IRA <= 0x31)
				{
					//INPUTS = DSPData->EXTS[op.IRA - 0x30] << 8;	// EXTS is 16 bits
					mov(INPUTS, dword[&DSPData->EXTS[op.IRA - 0x30]]);
					shl(INPUTS, 8);
				}
				else
				{
					xor_(INPUTS, INPUTS);
				}
			}

			if (op.IWT)
			{
				//DSP->MEMS[op.IWA] = MEMVAL[step & 3];	// MEMVAL was selected in previous MRD
				mov(eax, dword[&DSP->MEMVAL[step & 3]]);
				mov(dword[&DSP->MEMS[op.IWA]], eax);
			}

			// Operand sel
			// B
			if (!op.ZERO)
			{
				if (op.BSEL)
					//B = ACC;
					mov(dword[&DSP->B], ACC);
				else
				{
					//B = DSP->TEMP[(TRA + DSP->regs.MDEC_CT) & 0x7F];
					mov(eax, dword[&DSP->regs.MDEC_CT]);
					if (op.TRA)
						add(eax, op.TRA);
					and_(eax, 0x7f);
					mov(eax, dword[(size_t)&DSP->TEMP + eax * 4]);
					mov(dword[&DSP->B], eax);
				}
				if (op.NEGB)
					//B = 0 - B;
					neg(dword[&DSP->B]);
			}

			// X
			Xbyak::Reg32 X_alias = X;
			if (op.XSEL)
				//X = INPUTS;
				X_alias = INPUTS;
			else
			{
				//X = DSP->TEMP[(TRA + DSP->regs.MDEC_CT) & 0x7F];
				if (!op.ZERO && !op.BSEL && !op.NEGB)
					mov(X, dword[&DSP->B]);
				else
				{
					mov(eax, dword[&DSP->regs.MDEC_CT]);
					if (op.TRA)
						add(eax, op.TRA);
					and_(eax, 0x7f);
					mov(X, dword[(size_t)&DSP->TEMP + eax * 4]);
				}
			}

			// Y
			if (op.YSEL == 0)
			{
				//Y = FRC_REG;
				mov(Y, dword[&DSP->FRC_REG]);
			}
			else if (op.YSEL == 1)
			{
				//Y = DSPData->COEF[COEF] >> 3;	//COEF is 16 bits
				movsx(Y, word[&DSPData->COEF[COEF]]);
				sar(Y, 3);
			}
			else if (op.YSEL == 2)
			{
				//Y = Y_REG >> 11;
				mov(Y, dword[&DSP->Y_REG]);
				sar(Y, 11);
			}
			else if (op.YSEL == 3)
			{
				//Y = (Y_REG >> 4) & 0x0FFF;
				mov(Y, dword[&DSP->Y_REG]);
				sar(Y, 4);
				and_(Y, 0x0fff);
			}

			if (op.YRL)
				//Y_REG = INPUTS;
				mov(dword[&DSP->Y_REG], INPUTS);

			if (op.TWT || op.FRCL || op.MWT || (op.ADRL && op.SHIFT == 3) || op.EWT)
			{
				// Shifter
				// There's a 1-step delay at the output of the X*Y + B adder. So we use the ACC value from the previous step.
				if (op.SHIFT == 0)
				{
					// SHIFTED = clamp(ACC, -0x80000, 0x7FFFF)
			        cmp(ACC, 0xFF800000);
			        mov(ecx, 0xFF800000);
			        mov(eax, 0x007FFFFF);
			        cmovge(ecx, ACC);
			        cmp(ecx, 0x007FFFFF);
			        cmovg(ecx, eax);
				}
				else if (op.SHIFT == 1)
				{
					//SHIFTED = ACC << 1;	// x2 scale
					mov(edx, ACC);
					shl(edx, 1);
					// SHIFTED = clamp(SHIFTED, -0x80000, 0x7FFFF)
			        cmp(edx, 0xFF800000);
			        mov(ecx, 0xFF800000);
			        mov(eax, 0x007FFFFF);
			        cmovge(ecx, edx);
			        cmp(ecx, 0x007FFFFF);
			        cmovg(ecx, eax);
				}
				else if (op.SHIFT == 2)
				{
					//SHIFTED = ACC << 1;	// x2 scale
					mov(ecx, ACC);
					shl(ecx, 1);
				}
				else if (op.SHIFT == 3)
				{
					//SHIFTED = ACC;
					mov(ecx, ACC);
				}
				// ecx contains SHIFTED
			}

			// ACCUM
			//ACC = (((s64)X * (s64)Y) >> 12) + B;
			mov(eax, X_alias);
			imul(Y);
			shl(edx, 20);
			shr(eax, 12);
			or_(eax, edx);
			mov(ACC, eax);
			if (!op.ZERO)
				add(ACC, dword[&DSP->B]);

			if (op.TWT)
			{
				//DSP->TEMP[(op.TWA + DSP->regs.MDEC_CT) & 0x7F] = SHIFTED;
				mov(edx, dword[&DSP->regs.MDEC_CT]);
				if (op.TWA)
					add(edx, op.TWA);
				and_(edx, 0x7f);
				mov(dword[(size_t)&DSP->TEMP + edx * 4], ecx);
			}

			if (op.FRCL)
			{
				mov(edx, ecx);
				if (op.SHIFT == 3)
					//FRC_REG = SHIFTED & 0x0FFF;
					and_(edx, 0xFFF);
				else
					//FRC_REG = SHIFTED >> 11;
					sar(edx, 11);
				mov(dword[&DSP->FRC_REG], edx);
			}

			if (step & 1)
			{
				if (op.MRD || op.MWT)
				{
					if ((op.ADRL && op.SHIFT == 3) || op.EWT)
						push(ecx);
				}
				const Xbyak::Reg32 ADDR = Y;
				if (op.MWT)
				{
					// *(u16 *)&aica_ram[ADDR & ARAM_MASK] = PACK(SHIFTED);
					// SHIFTED is in ecx
					call(CC_RW2RX((const void *)PACK));

					CalculateADDR(ADDR, op);
					mov(ecx, (uintptr_t)&aica_ram[0]);
					mov(word[ecx + ADDR], ax);
				}
				if (op.MRD)			// memory only allowed on odd. DoA inserts NOPs on even
				{
					//MEMVAL[(step + 2) & 3] = UNPACK(*(u16 *)&aica_ram[ADDR & ARAM_MASK]);
					CalculateADDR(ADDR, op);
					mov(ecx, (uintptr_t)&aica_ram[0]);
					movzx(ecx, word[ecx + ADDR]);
					call(CC_RW2RX((const void *)UNPACK));
					mov(dword[&DSP->MEMVAL[(step + 2) & 3]], eax);
				}
				if (op.MRD || op.MWT)
				{
					if ((op.ADRL && op.SHIFT == 3) || op.EWT)
						pop(ecx);
				}
			}

			if (op.ADRL)
			{
				if (op.SHIFT == 3)
				{
					//ADRS_REG = SHIFTED >> 12;
					mov(dword[&DSP->ADRS_REG], ecx);	// SHIFTED
					sar(dword[&DSP->ADRS_REG], 12);
				}
				else
				{
					//ADRS_REG = INPUTS >> 16;
					mov(dword[&DSP->ADRS_REG], INPUTS);
					sar(dword[&DSP->ADRS_REG], 16);
				}
			}

			if (op.EWT)
			{
				//DSPData->EFREG[op.EWA] = SHIFTED >> 8;
				sar(ecx, 8);	// SHIFTED
				mov(dword[&DSPData->EFREG[op.EWA]], ecx);
			}
		}
		// DSP->regs.MDEC_CT--
		mov(eax, dsp.RBL + 1);
		mov(ecx, dword[&DSP->regs.MDEC_CT]);
		sub(ecx, 1);
		//if (dsp.regs.MDEC_CT == 0)
		//	dsp.regs.MDEC_CT = dsp.RBL + 1;			// RBL is ring buffer length - 1
		cmove(ecx, eax);
		mov(dword[&DSP->regs.MDEC_CT], ecx);

#ifndef _WIN32
		// 16-byte alignment
		add(esp, 12);
#endif
		pop(ebx);
		pop(ebp);
		pop(edi);
		pop(esi);

		ret();
		ready();
	}

private:
	void CalculateADDR(const Xbyak::Reg32 ADDR, const _INST& op)
	{
		//u32 ADDR = DSPData->MADRS[op.MASA];
		mov(ADDR, dword[&DSPData->MADRS[op.MASA]]);
		if (op.ADREB)
		{
			//ADDR += ADRS_REG & 0x0FFF;
			mov(ecx, dword[&DSP->ADRS_REG]);
			and_(ecx, 0x0FFF);
			add(ADDR, ecx);
		}
		if (op.NXADR)
			//ADDR++;
			add(ADDR, 1);
		if (!op.TABLE)
		{
			//ADDR += DSP->regs.MDEC_CT;
			add(ADDR, dword[&DSP->regs.MDEC_CT]);
			//ADDR &= DSP->RBL;
			// RBL is constant for this program
			and_(ADDR, DSP->RBL);
		}
		else
			//ADDR &= 0xFFFF;
			and_(ADDR, 0xFFFF);

		//ADDR <<= 1;				// Word -> byte address
		shl(ADDR, 1);
		//ADDR += DSP->RBP;			// RBP is already a byte address
		// RBP is constant for this program
		add(ADDR, DSP->RBP);
		// ADDR & ARAM_MASK
		and_(ADDR, ARAM_MASK);
	}

	struct dsp_t *DSP = nullptr;
};

void dsp_recompile()
{
	dsp.Stopped = true;
	for (int i = 127; i >= 0; --i)
	{
		u32 *IPtr = DSPData->MPRO + i * 4;

		if (IPtr[0] != 0 || IPtr[1] != 0 || IPtr[2 ]!= 0 || IPtr[3] != 0)
		{
			dsp.Stopped = false;
			break;
		}
	}
	X86DSPAssembler assembler(pCodeBuffer, sizeof(CodeBuffer));
	assembler.Compile(&dsp);
}

void dsp_rec_init()
{
	if (!vmem_platform_prepare_jit_block(CodeBuffer, sizeof(CodeBuffer), (void**)&pCodeBuffer))
		die("mprotect failed in x86 dsp");
}

void dsp_rec_step()
{
	((void (*)())&pCodeBuffer[0])();
}
#endif
