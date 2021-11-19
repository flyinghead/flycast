/*
	Copyright 2020 flyinghead

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

#if HOST_CPU == CPU_X64 && FEAT_DSPREC != DYNAREC_NONE

#include <xbyak/xbyak.h>
#include "dsp.h"
#include "aica.h"
#include "aica_if.h"
#include "hw/mem/_vmem.h"

namespace dsp
{

constexpr size_t CodeBufferSize = 32 * 1024;
#if defined(_WIN32)
static u8 *CodeBuffer;
#else
alignas(4096) static u8 CodeBuffer[CodeBufferSize]
	#if defined(__unix__)
		__attribute__((section(".text")));
	#elif defined(__APPLE__)
		__attribute__((section("__TEXT,.text")));
	#else
		#error CodeBuffer code section unknown
	#endif
#endif
static u8 *pCodeBuffer;
static ptrdiff_t rx_offset;

class X64DSPAssembler : public Xbyak::CodeGenerator
{
public:
	X64DSPAssembler(u8 *code_buffer, size_t size) : Xbyak::CodeGenerator(size, code_buffer) {}

	void Compile(DSPState *DSP)
	{
		this->DSP = DSP;
		DEBUG_LOG(AICA_ARM, "DSPAssembler::DSPCompile recompiling for x86/64 at %p", this->getCode());

		push(rbx);
		push(rbp);
		push(r12);
		push(r13);
		push(r14);
		push(r15);
#ifdef _WIN32
		sub(rsp, 40);	// 32-byte shadow space + 8 bytes for 16-byte stack alignment
#else
		sub(rsp, 8);	// 16-byte stack alignment
#endif
		mov(rbx, (uintptr_t)&DSP->TEMP[0]);	// rbx points to TEMP, right after the code
		mov(rbp, (uintptr_t)DSPData);		// rbp points to DSPData
		const Xbyak::Reg32 INPUTS = r8d;	// 24 bits
		const Xbyak::Reg32 ACC = r12d;		// 26 bits - saved
		const Xbyak::Reg32 B = r9d;			// 26 bits
		const Xbyak::Reg32 X = r10d;		// 24 bits
		const Xbyak::Reg32 Y = r11d;		// 13 bits
		const Xbyak::Reg32 Y_REG = r13d;	// 24 bits - saved
		const Xbyak::Reg32 ADRS_REG = r14d;	// 13 bits unsigned - saved
		const Xbyak::Reg32 MDEC_CT = r15d;	// saved
#ifdef _WIN32
		const Xbyak::Reg32 call_arg0 = ecx;
#else
		const Xbyak::Reg32 call_arg0 = edi;
#endif

		xor_(ACC, ACC);
		mov(dword[rbx + dsp_operand(&DSP->FRC_REG)], 0);
		xor_(Y_REG, Y_REG);
		xor_(ADRS_REG, ADRS_REG);
		mov(MDEC_CT, dword[rbx + dsp_operand(&DSP->MDEC_CT)]);

		for (int step = 0; step < 128; ++step)
		{
			u32 *mpro = &DSPData->MPRO[step * 4];
			Instruction op;
			DecodeInst(mpro, &op);
			const u32 COEF = step;

			if (op.XSEL || op.YRL || (op.ADRL && op.SHIFT != 3))
			{
				if (op.IRA <= 0x1f)
					//INPUTS = DSP->MEMS[op.IRA];
					mov(INPUTS, dword[rbx + dsp_operand(DSP->MEMS, op.IRA)]);
				else if (op.IRA <= 0x2F)
				{
					//INPUTS = DSP->MIXS[op.IRA - 0x20] << 4;		// MIXS is 20 bit
					mov(INPUTS, dword[rbx + dsp_operand(DSP->MIXS, op.IRA - 0x20)]);
					shl(INPUTS, 4);
				}
				else if (op.IRA <= 0x31)
				{
					//INPUTS = DSPData->EXTS[op.IRA - 0x30] << 8;	// EXTS is 16 bits
					mov(INPUTS, dword[rbx + dsp_operand(DSPData->EXTS, op.IRA - 0x30)]);
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
				mov(eax, dword[rbx + dsp_operand(DSP->MEMVAL, step & 3)]);
				mov(dword[rbx + dsp_operand(DSP->MEMS, op.IWA)], eax);
			}

			// Operand sel
			// B
			if (!op.ZERO)
			{
				if (op.BSEL)
					//B = ACC;
					mov(B, ACC);
				else
				{
					//B = DSP->TEMP[(TRA + DSP->MDEC_CT) & 0x7F];
					mov(eax, MDEC_CT);
					if (op.TRA)
						add(eax, op.TRA);
					and_(eax, 0x7f);
					mov(B, dword[rbx + rax * 4]);
				}
				if (op.NEGB)
					//B = 0 - B;
					neg(B);
			}

			// X
			Xbyak::Reg32 X_alias = X;
			if (op.XSEL)
				//X = INPUTS;
				X_alias = INPUTS;
			else
			{
				//X = DSP->TEMP[(TRA + DSP->MDEC_CT) & 0x7F];
				if (!op.ZERO && !op.BSEL && !op.NEGB)
					X_alias = B;
				else
				{
					mov(eax, MDEC_CT);
					if (op.TRA)
						add(eax, op.TRA);
					and_(eax, 0x7f);
					mov(X, dword[rbx + rax * 4]);
				}
			}

			// Y
			if (op.YSEL == 0)
			{
				//Y = FRC_REG;
				mov(Y, dword[rbx + dsp_operand(&DSP->FRC_REG)]);
			}
			else if (op.YSEL == 1)
			{
				//Y = DSPData->COEF[COEF] >> 3;	//COEF is 16 bits
				movsx(Y, word[rbp + dspdata_operand(DSPData->COEF, COEF)]);
				sar(Y, 3);
			}
			else if (op.YSEL == 2)
			{
				//Y = Y_REG >> 11;
				mov(Y, Y_REG);
				sar(Y, 11);
			}
			else if (op.YSEL == 3)
			{
				//Y = (Y_REG >> 4) & 0x0FFF;
				mov(Y, Y_REG);
				sar(Y, 4);
				and_(Y, 0x0fff);
			}

			if (op.YRL)
				//Y_REG = INPUTS;
				mov(Y_REG, INPUTS);

			if (op.TWT || op.FRCL || op.MWT || (op.ADRL && op.SHIFT == 3) || op.EWT)
			{
				// Shifter
				// There's a 1-step delay at the output of the X*Y + B adder. So we use the ACC value from the previous step.
				if (op.SHIFT == 0)
				{
					// SHIFTED = clamp(ACC, -0x80000, 0x7FFFF)
			        cmp(ACC, 0xFF800000);
			        mov(edx, 0xFF800000);
			        mov(eax, 0x007FFFFF);
			        cmovge(edx, ACC);
			        cmp(edx, 0x007FFFFF);
			        cmovg(edx, eax);
				}
				else if (op.SHIFT == 1)
				{
					//SHIFTED = ACC << 1;	// x2 scale
					mov(ecx, ACC);
					shl(ecx, 1);
					// SHIFTED = clamp(SHIFTED, -0x80000, 0x7FFFF)
			        cmp(ecx, 0xFF800000);
			        mov(edx, 0xFF800000);
			        mov(eax, 0x007FFFFF);
			        cmovge(edx, ecx);
			        cmp(edx, 0x007FFFFF);
			        cmovg(edx, eax);
				}
				else if (op.SHIFT == 2)
				{
					//SHIFTED = ACC << 1;	// x2 scale
					mov(edx, ACC);
					shl(edx, 1);
				}
				else if (op.SHIFT == 3)
				{
					//SHIFTED = ACC;
					mov(edx, ACC);
				}
				// edx contains SHIFTED
			}

			// ACCUM
			//ACC = (((s64)X * (s64)Y) >> 12) + B;
			const Xbyak::Reg64 Xlong = X_alias.cvt64();
			movsxd(Xlong, X_alias);
			movsxd(rax, Y);
			imul(rax, Xlong);
			sar(rax, 12);
			mov(ACC, eax);
			if (!op.ZERO)
				add(ACC, B);

			if (op.TWT)
			{
				//DSP->TEMP[(op.TWA + DSP->MDEC_CT) & 0x7F] = SHIFTED;
				mov(ecx, MDEC_CT);
				if (op.TWA)
					add(ecx, op.TWA);
				and_(ecx, 0x7f);
				mov(dword[rbx + rcx * 4], edx);
			}

			if (op.FRCL)
			{
				mov(ecx, edx);
				if (op.SHIFT == 3)
					//FRC_REG = SHIFTED & 0x0FFF;
					and_(ecx, 0xFFF);
				else
					//FRC_REG = SHIFTED >> 11;
					sar(ecx, 11);
				mov(dword[rbx + dsp_operand(&DSP->FRC_REG)], ecx);
			}

			if (step & 1)
			{
				if (op.MRD || op.MWT)
				{
					if ((op.ADRL && op.SHIFT == 3) || op.EWT)
						push(rdx);
					if (op.ADRL && op.SHIFT != 3)
						push(INPUTS.cvt64());
				}
				const Xbyak::Reg32 ADDR = Y;
				if (op.MRD)			// memory only allowed on odd. DoA inserts NOPs on even
				{
					//MEMVAL[(step + 2) & 3] = UNPACK(*(u16 *)&aica_ram[ADDR & ARAM_MASK]);
					CalculateADDR(ADDR, op, ADRS_REG, MDEC_CT);
					mov(rcx, (uintptr_t)&aica_ram[0]);
					movzx(call_arg0, word[rcx + ADDR.cvt64()]);
					GenCall(UNPACK);
					mov(dword[rbx + dsp_operand(&DSP->MEMVAL[(step + 2) & 3])], eax);
				}
				if (op.MWT)
				{
					// *(u16 *)&aica_ram[ADDR & ARAM_MASK] = PACK(SHIFTED);
					mov(call_arg0, edx);	// SHIFTED
					GenCall(PACK);

					CalculateADDR(ADDR, op, ADRS_REG, MDEC_CT);
					mov(rcx, (uintptr_t)&aica_ram[0]);
					mov(word[rcx + ADDR.cvt64()], ax);
				}
				if (op.MRD || op.MWT)
				{
					if (op.ADRL && op.SHIFT != 3)
						pop(INPUTS.cvt64());
					if ((op.ADRL && op.SHIFT == 3) || op.EWT)
						pop(rdx);
				}
			}

			if (op.ADRL)
			{
				if (op.SHIFT == 3)
				{
					//ADRS_REG = SHIFTED >> 12;
					mov(ADRS_REG, edx);	// SHIFTED
					sar(ADRS_REG, 12);
				}
				else
				{
					//ADRS_REG = INPUTS >> 16;
					mov(ADRS_REG, INPUTS);
					sar(ADRS_REG, 16);
				}
			}

			if (op.EWT)
			{
				//DSPData->EFREG[op.EWA] = SHIFTED >> 8;
				sar(edx, 8);	// SHIFTED
				mov(dword[rbp + dspdata_operand(DSPData->EFREG, op.EWA)], edx);
			}
		}
		// DSP->MDEC_CT--
		mov(eax, DSP->RBL + 1);
		sub(MDEC_CT, 1);
		//if (dsp.MDEC_CT == 0)
		//	dsp.MDEC_CT = dsp.RBL + 1;			// RBL is ring buffer length - 1
		cmove(MDEC_CT, eax);
		mov(dword[rbx + dsp_operand(&DSP->MDEC_CT)], MDEC_CT);

#ifdef _WIN32
		add(rsp, 40);
#else
		add(rsp, 8);
#endif
		pop(r15);
		pop(r14);
		pop(r13);
		pop(r12);
		pop(rbp);
		pop(rbx);

		ret();
		ready();
	}

private:
	ptrdiff_t dsp_operand(void *data, int index = 0, u32 element_size = 4)
	{
		return ((u8*)data - (u8*)DSP) - offsetof(DSPState, TEMP) + index  * element_size;
	}
	ptrdiff_t dspdata_operand(void *data, int index = 0, u32 element_size = 4)
	{
		return ((u8*)data - (u8*)DSPData) + index  * element_size;
	}

	void CalculateADDR(const Xbyak::Reg32 ADDR, const Instruction& op, const Xbyak::Reg32 ADRS_REG, const Xbyak::Reg32 MDEC_CT)
	{
		//u32 ADDR = DSPData->MADRS[op.MASA];
		mov(ADDR, dword[rbp + dspdata_operand(DSPData->MADRS, op.MASA)]);
		if (op.ADREB)
		{
			//ADDR += ADRS_REG & 0x0FFF;
			mov(ecx, ADRS_REG);
			and_(ecx, 0x0FFF);
			add(ADDR, ecx);
		}
		if (op.NXADR)
			//ADDR++;
			add(ADDR, 1);
		if (!op.TABLE)
		{
			//ADDR += DSP->MDEC_CT;
			add(ADDR, MDEC_CT);
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

	template<class Ret, class... Params>
	void GenCall(Ret(*function)(Params...))
	{
		call(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(function) - rx_offset));
	}

	DSPState *DSP = nullptr;
};

void recompile()
{
	vmem_platform_jit_set_exec(pCodeBuffer, CodeBufferSize, false);
	X64DSPAssembler assembler(pCodeBuffer, CodeBufferSize);
	assembler.Compile(&state);
	vmem_platform_jit_set_exec(pCodeBuffer, CodeBufferSize, true);
}

void recInit()
{
#ifdef FEAT_NO_RWX_PAGES
	if (!vmem_platform_prepare_jit_block(CodeBuffer, CodeBufferSize, (void**)&pCodeBuffer, &rx_offset))
#else
	if (!vmem_platform_prepare_jit_block(CodeBuffer, CodeBufferSize, (void**)&pCodeBuffer))
#endif
		die("vmem_platform_prepare_jit_block failed in x64 dsp");
}

void runStep()
{
	((void (*)())&pCodeBuffer[0])();
}

}
#endif
