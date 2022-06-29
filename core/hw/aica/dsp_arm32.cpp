/*
	Copyright 2021 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "build.h"

#if HOST_CPU == CPU_ARM && FEAT_DSPREC != DYNAREC_NONE

#include "dsp.h"
#include "aica.h"
#include "aica_if.h"
#include "hw/mem/_vmem.h"
#include <aarch32/macro-assembler-aarch32.h>
using namespace vixl::aarch32;

namespace dsp
{

constexpr size_t CodeSize = 4096 * 8;	//32 kb, 8 pages

#if defined(__unix__)
alignas(4096) static u8 DynCode[CodeSize] __attribute__((section(".text")));
#else
#error "Unsupported platform for arm32 DSP dynarec"
#endif

class DSPAssembler : public MacroAssembler
{
public:
	DSPAssembler(u8 *code_buffer, size_t size) : MacroAssembler(code_buffer, size, A32) {}

	void compile(DSPState *DSP)
	{
		this->DSP = DSP;
		DEBUG_LOG(AICA_ARM, "DSPAssembler::compile recompiling for arm32 at %p", GetBuffer()->GetStartAddress<void*>());

		RegisterList regList = RegisterList::Union(
				RegisterList(r4, r5, r6, r7),
				RegisterList(r8, r9, r10, lr));
		Push(regList);
		Mov(r8, (uintptr_t)&DSP->TEMP[0]);	// r8 points to TEMP, right after the code
		Mov(r7, (uintptr_t)DSPData);		// r7 points to DSPData
		const Register& INPUTS = r4;		// 24 bits
		const Register& ACC = r5;			// 26 bits - saved
		const Register& B = r10;			// 26 bits - saved
		const Register& X = r6;				// 24 bits
		const Register& Y = r9;				// 13 bits

		Mov(ACC, 0);
		Mov(B, 0);
		Str(B, dsp_operand(&DSP->FRC_REG));
		Str(B, dsp_operand(&DSP->Y_REG));
		Str(B, dsp_operand(&DSP->ADRS_REG));

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
					Ldr(INPUTS, dsp_operand(DSP->MEMS, op.IRA));
				else if (op.IRA <= 0x2F)
				{
					//INPUTS = DSP->MIXS[op.IRA - 0x20] << 4;		// MIXS is 20 bit
					Ldr(INPUTS, dsp_operand(DSP->MIXS, op.IRA - 0x20));
					Lsl(INPUTS, INPUTS, 4);
				}
				else if (op.IRA <= 0x31)
				{
					//INPUTS = DSPData->EXTS[op.IRA - 0x30] << 8;	// EXTS is 16 bits
					Ldr(INPUTS, dspdata_operand(DSPData->EXTS, op.IRA - 0x30));
					Lsl(INPUTS, INPUTS, 8);
				}
				else
				{
					Mov(INPUTS, 0);
				}
			}

			if (op.IWT)
			{
				//DSP->MEMS[op.IWA] = MEMVAL[step & 3];	// MEMVAL was selected in previous MRD
				Ldr(r1, dsp_operand(DSP->MEMVAL, step & 3));
				Str(r1, dsp_operand(DSP->MEMS, op.IWA));
			}

			// Operand sel
			// B
			if (!op.ZERO)
			{
				if (op.BSEL)
					//B = ACC;
					Mov(B, ACC);
				else
				{
					//B = DSP->TEMP[(TRA + DSP->MDEC_CT) & 0x7F];
					Ldr(r1, dsp_operand(&DSP->MDEC_CT));
					if (op.TRA)
						Add(r1, r1, op.TRA);
					Bfc(r1, 7, 25);
					Ldr(B, dsp_operand(DSP->TEMP, r1));
				}
				if (op.NEGB)
					//B = 0 - B;
					Rsb(B, B, 0);
			}

			// X
			const Register* X_alias = &X;
			if (op.XSEL)
				//X = INPUTS;
				X_alias = &INPUTS;
			else
			{
				//X = DSP->TEMP[(TRA + DSP->MDEC_CT) & 0x7F];
				if (!op.ZERO && !op.BSEL && !op.NEGB)
					X_alias = &B;
				else
				{
					Ldr(r1, dsp_operand(&DSP->MDEC_CT));
					if (op.TRA)
						Add(r1, r1, op.TRA);
					Bfc(r1, 7, 25);
					Ldr(X, dsp_operand(DSP->TEMP, r1));
				}
			}

			// Y
			if (op.YSEL == 0)
			{
				//Y = FRC_REG;
				Ldr(Y, dsp_operand(&DSP->FRC_REG));
			}
			else if (op.YSEL == 1)
			{
				//Y = DSPData->COEF[COEF] >> 3;	//COEF is 16 bits
				Ldr(Y, dspdata_operand(DSPData->COEF, COEF));
				Sbfx(Y, Y, 3, 13);
			}
			else if (op.YSEL == 2)
			{
				//Y = Y_REG >> 11;
				Ldr(r1, dsp_operand(&DSP->Y_REG));
				Asr(Y, r1, 11);
			}
			else if (op.YSEL == 3)
			{
				//Y = (Y_REG >> 4) & 0x0FFF;
				Ldr(r1, dsp_operand(&DSP->Y_REG));
				Ubfx(Y, r1, 4, 12);
			}

			if (op.YRL)
				//Y_REG = INPUTS;
				Str(INPUTS, dsp_operand(&DSP->Y_REG));

			if (op.TWT || op.FRCL || op.MWT || (op.ADRL && op.SHIFT == 3) || op.EWT)
			{
				// Shifter
				// There's a 1-step delay at the output of the X*Y + B adder. So we use the ACC value from the previous step.
				if (op.SHIFT == 0)
				{
					// SHIFTED = clamp(ACC, -0x800000, 0x7FFFFF)
					Ssat(r2, 24, ACC);
				}
				else if (op.SHIFT == 1)
				{
					//SHIFTED = ACC << 1;	// x2 scale
					// SHIFTED = clamp(SHIFTED, -0x800000, 0x7FFFFF)
					Ssat(r2, 24, Operand(ACC, LSL, 1));
				}
				else if (op.SHIFT == 2)
				{
					//SHIFTED = ACC << 1;	// x2 scale
					Lsl(r2, ACC, 1);
				}
				else if (op.SHIFT == 3)
				{
					//SHIFTED = ACC;
					Mov(r2, ACC);
				}
				Str(r2, dsp_operand(&DSP->SHIFTED));
			}

			// ACCUM
			//ACC = (((s64)X * (s64)Y) >> 12) + B;
			Smull(r0, r1, *X_alias, Y);
			Lsr(r0, r0, 12);
			Orr(ACC, r0, Operand(r1, LSL, 20));
			if (!op.ZERO)
				Add(ACC, ACC, B);

			if (op.TWT)
			{
				//DSP->TEMP[(op.TWA + DSP->MDEC_CT) & 0x7F] = SHIFTED;
				Ldr(r2, dsp_operand(&DSP->SHIFTED));
				Ldr(r1, dsp_operand(&DSP->MDEC_CT));
				if (op.TWA)
					Add(r1, r1, op.TWA);
				Bfc(r1, 7, 25);
				Str(r2, dsp_operand(DSP->TEMP, r1));
			}

			if (op.FRCL)
			{
				Ldr(r2, dsp_operand(&DSP->SHIFTED));
				if (op.SHIFT == 3)
					//FRC_REG = SHIFTED & 0x0FFF;
					Ubfx(r1, r2, 0, 12);
				else
					//FRC_REG = SHIFTED >> 11;
					Asr(r1, r2, 11);
				Str(r1, dsp_operand(&DSP->FRC_REG));
			}

			if (step & 1)
			{
				const Register& ADDR = r3;
				if (op.MRD)			// memory only allowed on odd. DoA inserts NOPs on even
				{
					//MEMVAL[(step + 2) & 3] = UNPACK(*(u16 *)&aica_ram[ADDR & ARAM_MASK]);
					calculateADDR(ADDR, op);
					Mov(r1, getAicaRam());
					Ldrh(r0, MemOperand(r1, ADDR));
					genCallRuntime(UNPACK);
					Mov(r2, r0);
					Str(r2, dsp_operand(DSP->MEMVAL, (step + 2) & 3));
				}
				if (op.MWT)
				{
					// *(u16 *)&aica_ram[ADDR & ARAM_MASK] = PACK(SHIFTED);
					Ldr(r0, dsp_operand(&DSP->SHIFTED));
					genCallRuntime(PACK);
					Mov(r2, r0);

					calculateADDR(ADDR, op);
					Mov(r1, getAicaRam());
					Strh(r2, MemOperand(r1, ADDR));
				}
			}

			if (op.ADRL)
			{
				if (op.SHIFT == 3)
				{
					//ADRS_REG = SHIFTED >> 12;
					Ldr(r2, dsp_operand(&DSP->SHIFTED));
					Asr(r1, r2, 12);
				}
				else
				{
					//ADRS_REG = INPUTS >> 16;
					Asr(r1, INPUTS, 16);
				}
				Str(r1, dsp_operand(&DSP->ADRS_REG));
			}

			if (op.EWT)
			{
				//DSPData->EFREG[op.EWA] = SHIFTED >> 8;
				Ldr(r1, dsp_operand(&DSP->SHIFTED));
				Asr(r1, r1, 8);
				Str(r1, dspdata_operand(DSPData->EFREG, op.EWA));
			}
		}
		Ldr(r1, dsp_operand(&DSP->MDEC_CT));
		// DSP->MDEC_CT--
		Subs(r1, r1, 1);
		//if (dsp.MDEC_CT == 0)
		//	dsp.MDEC_CT = dsp.RBL + 1;			// RBL is ring buffer length - 1
		Mov(eq, r1, DSP->RBL + 1);
		Str(r1, dsp_operand(&DSP->MDEC_CT));

		Pop(regList);
		Mov(pc, lr);

		FinalizeCode();

		vmem_platform_flush_cache(
			GetBuffer()->GetStartAddress<char*>(), GetBuffer()->GetEndAddress<char*>(),
			GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>());
	}

private:
	MemOperand dsp_operand(void *data, int index = 0, u32 element_size = 4)
	{
		ptrdiff_t offset = ((u8*)data - (u8*)DSP) - offsetof(DSPState, TEMP) + index  * element_size;
		if (offset <= 4095)
			return MemOperand(r8, offset);
		Mov(r0, offset);
		return MemOperand(r8, r0);
	}

	MemOperand dsp_operand(void *data, const Register& offset_reg, u32 element_size = 4)
	{
		ptrdiff_t offset = ((u8*)data - (u8*)DSP) - offsetof(DSPState, TEMP);
		if (offset == 0)
			return MemOperand(r8, offset_reg, LSL, element_size == 4 ? 2 : element_size == 2 ? 1 : 0);

		Mov(r0, offset);
		Add(r0, r0, Operand(offset_reg, LSL, element_size == 4 ? 2 : element_size == 2 ? 1 : 0));
		return MemOperand(r8, r0);
	}

	MemOperand dspdata_operand(void *data, int index = 0, u32 element_size = 4)
	{
		ptrdiff_t offset = ((u8*)data - (u8*)DSPData) + index  * element_size;
		if (offset <= 4095)
			return MemOperand(r7, offset);
		Mov(r0, offset);
		return MemOperand(r7, r0);
	}

	template <typename R, typename... P>
	void genCallRuntime(R (*function)(P...))
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(function) - GetBuffer()->GetStartAddress<uintptr_t>();
		verify((offset & 3) == 0);
		if (offset >= -32 * 1024 * 1024 && offset <= 32 * 1024 * 1024)
		{
			Label function_label(offset);
			Bl(&function_label);
		}
		else
		{
			Mov(r3, reinterpret_cast<uintptr_t>(function));
			Blx(r3);
		}
	}

	void calculateADDR(const Register& ADDR, const Instruction& op)
	{
		//u32 ADDR = DSPData->MADRS[op.MASA];
		Ldr(ADDR, dspdata_operand(DSPData->MADRS, op.MASA));
		if (op.ADREB)
		{
			//ADDR += ADRS_REG & 0x0FFF;
			Ldr(r1, dsp_operand(&DSP->ADRS_REG));
			Ubfx(r0, r1, 0, 12);
			Add(ADDR, ADDR, r0);
		}
		if (op.NXADR)
			//ADDR++;
			Add(ADDR, ADDR, 1);
		if (!op.TABLE)
		{
			//ADDR += DSP->MDEC_CT;
			Ldr(r1, dsp_operand(&DSP->MDEC_CT));
			Add(ADDR, ADDR, r1);
			//ADDR &= DSP->RBL;
			// RBL is constant for this program
			And(ADDR, ADDR, DSP->RBL);
		}
		else
			//ADDR &= 0xFFFF;
			Bfc(ADDR, 16, 16);

		//ADDR <<= 1;				// Word -> byte address
		Lsl(ADDR, ADDR, 1);
		//ADDR += DSP->RBP;			// RBP is already a byte address
		// RBP is constant for this program
		Add(ADDR, ADDR, DSP->RBP);
		// ADDR & ARAM_MASK
		if (ARAM_SIZE == 2*1024*1024)
			Bfc(ADDR, 21, 11);
		else if (ARAM_SIZE == 8*1024*1024)
			Bfc(ADDR, 23, 9);
		else
			die("Unsupported ARAM_SIZE");
	}

	uintptr_t getAicaRam()
	{
		return reinterpret_cast<uintptr_t>(&aica_ram[0]);
	}

	DSPState *DSP = nullptr;
};

void recompile()
{
	JITWriteProtect(false);
	DSPAssembler assembler(DynCode, CodeSize);
	assembler.compile(&state);
	JITWriteProtect(true);
}

void recInit()
{
	u8 *pCodeBuffer;
	verify(vmem_platform_prepare_jit_block(DynCode, CodeSize, (void**)&pCodeBuffer));
}

void runStep()
{
	((void (*)())DynCode)();
}

}
#endif
