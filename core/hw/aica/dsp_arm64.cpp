/*
	Copyright 2019 flyinghead

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

#if HOST_CPU == CPU_ARM64 && FEAT_DSPREC != DYNAREC_NONE

#include "dsp.h"
#include "aica.h"
#include "aica_if.h"
#include "deps/vixl/aarch64/macro-assembler-aarch64.h"
using namespace vixl::aarch64;

extern void vmem_platform_flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end);

class DSPAssembler : public MacroAssembler
{
public:
	DSPAssembler(u8 *code_buffer, size_t size) : MacroAssembler(code_buffer, size), aica_ram_lit(NULL) {}

	void Compile(struct dsp_t *DSP)
	{
		this->DSP = DSP;
		DEBUG_LOG(AICA_ARM, "DSPAssembler::DSPCompile recompiling for arm64 at %p", GetBuffer()->GetStartAddress<void*>());

		Instruction* instr_start = GetBuffer()->GetStartAddress<Instruction*>();

		Stp(x29, x30, MemOperand(sp, -96, PreIndex));
		Stp(x21, x22, MemOperand(sp, 16));
		Stp(x23, x24, MemOperand(sp, 32));
		Stp(x25, x26, MemOperand(sp, 48));
		Stp(x27, x28, MemOperand(sp, 64));
		Stp(x19, x20, MemOperand(sp, 80));
		Mov(x28, (uintptr_t)&DSP->TEMP[0]);		// x28 points to TEMP, right after the code
		Mov(x27, (uintptr_t)DSPData);			// x27 points to DSPData
		const Register& INPUTS = w25;	// 24 bits
		const Register& ACC = w19;		// 26 bits - saved
		const Register& B = w26;		// 26 bits - saved
		const Register& X = w10;		// 24 bits
		const Register& Y = w9;			// 13 bits
		const Register& FRC_REG = w20;	// 13 bits - saved
		const Register& Y_REG = w21;	// 24 bits - saved
		const Register& SHIFTED = w24;	// 24 bits
		const Register& ADRS_REG = w22;	// 13 bits unsigned - saved
		const Register& MDEC_CT = w23;	// saved

		Mov(ACC, 0);
		Mov(B, 0);
		Mov(FRC_REG, 0);
		Mov(Y_REG, 0);
		Mov(ADRS_REG, 0);
		Ldr(MDEC_CT, dsp_operand(&DSP->regs.MDEC_CT));

#if 0
		Instruction* instr_cur = GetBuffer()->GetEndAddress<Instruction*>();
		DEBUG_LOG(AICA_ARM, "DSP PROLOGUE");
		Disassemble(instr_start, instr_cur);
		instr_start = instr_cur;
#endif
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
				Ldr(w1, dsp_operand(DSP->MEMVAL, step & 3));
				Str(w1, dsp_operand(DSP->MEMS, op.IWA));
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
					//B = DSP->TEMP[(TRA + DSP->regs.MDEC_CT) & 0x7F];
					if (op.TRA)
						Add(w1, MDEC_CT, op.TRA);
					else
						Mov(w1, MDEC_CT);
					Bfc(w1, 7, 25);
					Ldr(B, dsp_operand(DSP->TEMP, x1));
				}
				if (op.NEGB)
					//B = 0 - B;
					Neg(B, B);
			}

			// X
			const Register* X_alias = &X;
			if (op.XSEL)
				//X = INPUTS;
				X_alias = &INPUTS;
			else
			{
				//X = DSP->TEMP[(TRA + DSP->regs.MDEC_CT) & 0x7F];
				if (!op.ZERO && !op.BSEL)
					X_alias = &B;
				else
				{
					if (op.TRA)
						Add(w1, MDEC_CT, op.TRA);
					else
						Mov(w1, MDEC_CT);
					Bfc(w1, 7, 25);
					Ldr(X, dsp_operand(DSP->TEMP, x1));
				}
			}

			// Y
			if (op.YSEL == 0)
			{
				//Y = FRC_REG;
				Mov(Y, FRC_REG);
			}
			else if (op.YSEL == 1)
			{
				//Y = DSPData->COEF[COEF] >> 3;	//COEF is 16 bits
				Ldr(Y, dspdata_operand(DSPData->COEF, COEF));
				Sbfx(Y, Y, 3, 13);
			}
			else if (op.YSEL == 2)
				//Y = Y_REG >> 11;
				Asr(Y, Y_REG, 11);
			else if (op.YSEL == 3)
				//Y = (Y_REG >> 4) & 0x0FFF;
				Ubfx(Y, Y_REG, 4, 12);

			if (op.YRL)
				//Y_REG = INPUTS;
				Mov(Y_REG, INPUTS);

			if (op.TWT || op.FRCL || op.MWT || (op.ADRL && op.SHIFT == 3) || op.EWT)
			{
				// Shifter
				// There's a 1-step delay at the output of the X*Y + B adder. So we use the ACC value from the previous step.
				if (op.SHIFT == 0)
				{
					// SHIFTED = clamp(ACC, -0x800000, 0x7FFFFF)
					Mov(w0, 0x800000);
					Neg(w1, w0);
					Cmp(ACC, w1);
					Csel(SHIFTED, w1, ACC, lt);
					Sub(w0, w0, 1);
					Cmp(SHIFTED, w0);
					Csel(SHIFTED, w0, SHIFTED, gt);
				}
				else if (op.SHIFT == 1)
				{
					//SHIFTED = ACC << 1;	// x2 scale
					Lsl(SHIFTED, ACC, 1);
					// SHIFTED = clamp(SHIFTED, -0x800000, 0x7FFFFF)
					Mov(w0, 0x800000);
					Neg(w1, w0);
					Cmp(SHIFTED, w1);
					Csel(SHIFTED, w1, SHIFTED, lt);
					Sub(w0, w0, 1);
					Cmp(SHIFTED, w0);
					Csel(SHIFTED, w0, SHIFTED, gt);
				}
				else if (op.SHIFT == 2)
				{
					//SHIFTED = ACC << 1;	// x2 scale
					Lsl(SHIFTED, ACC, 1);
				}
				else if (op.SHIFT == 3)
				{
					//SHIFTED = ACC;
					Mov(SHIFTED, ACC);
				}
			}

			// ACCUM
			//ACC = (((s64)X * (s64)Y) >> 12) + B;
			const Register& X64 = Register::GetXRegFromCode(X_alias->GetCode());
			const Register& Y64 = Register::GetXRegFromCode(Y.GetCode());
			Sxtw(X64, *X_alias);
			Sxtw(Y64, Y);
			Mul(x0, X64, Y64);
			Asr(x0, x0, 12);
			if (op.ZERO)
				Mov(ACC, w0);
			else
				Add(ACC, w0, B);

			if (op.TWT)
			{
				//DSP->TEMP[(op.TWA + DSP->regs.MDEC_CT) & 0x7F] = SHIFTED;
				if (op.TWA)
					Add(w1, MDEC_CT, op.TWA);
				else
					Mov(w1, MDEC_CT);
				Bfc(w1, 7, 25);
				Str(SHIFTED, dsp_operand(DSP->TEMP, x1));
			}

			if (op.FRCL)
			{
				if (op.SHIFT == 3)
					//FRC_REG = SHIFTED & 0x0FFF;
					Ubfx(FRC_REG, SHIFTED, 0, 12);
				else
					//FRC_REG = SHIFTED >> 11;
					Asr(FRC_REG, SHIFTED, 11);
			}

			if (step & 1)
			{
				const Register& ADDR = w11;
				if (op.MRD)			// memory only allowed on odd. DoA inserts NOPs on even
				{
					//MEMVAL[(step + 2) & 3] = UNPACK(*(u16 *)&aica_ram[ADDR & ARAM_MASK]);
					CalculateADDR(ADDR, op, ADRS_REG, MDEC_CT);
					Ldr(x1, GetAicaRam());
					MemOperand aram_op(x1, Register::GetXRegFromCode(ADDR.GetCode()));
					Ldrh(w0, aram_op);
					GenCallRuntime(UNPACK);
					Mov(w2, w0);
					Str(w2, dsp_operand(DSP->MEMVAL, (step + 2) & 3));
				}
				if (op.MWT)
				{
					// *(u16 *)&aica_ram[ADDR & ARAM_MASK] = PACK(SHIFTED);
					Mov(w0, SHIFTED);
					GenCallRuntime(PACK);
					Mov(w2, w0);

					CalculateADDR(ADDR, op, ADRS_REG, MDEC_CT);
					Ldr(x1, GetAicaRam());
					MemOperand aram_op(x1, Register::GetXRegFromCode(ADDR.GetCode()));
					Strh(w2, aram_op);
				}
			}

			if (op.ADRL)
			{
				if (op.SHIFT == 3)
					//ADRS_REG = SHIFTED >> 12;
					Asr(ADRS_REG, SHIFTED, 12);
				else
					//ADRS_REG = INPUTS >> 16;
					Asr(ADRS_REG, INPUTS, 16);
			}

			if (op.EWT)
			{
				//DSPData->EFREG[op.EWA] = SHIFTED >> 8;
				MemOperand mem_operand = dspdata_operand(DSPData->EFREG, op.EWA);
				Asr(w1, SHIFTED, 8);
				Str(w1, mem_operand);
			}
#if 0
			instr_cur = GetBuffer()->GetEndAddress<Instruction*>();
			DEBUG_LOG(AICA_ARM, "DSP STEP %d: %04x %04x %04x %04x", step, mpro[0], mpro[1], mpro[2], mpro[3]);
			Disassemble(instr_start, instr_cur);
			instr_start = instr_cur;
#endif
		}
		// DSP->regs.MDEC_CT--
		Subs(MDEC_CT, MDEC_CT, 1);
		//if (dsp.regs.MDEC_CT == 0)
		//	dsp.regs.MDEC_CT = dsp.RBL + 1;			// RBL is ring buffer length - 1
		Mov(w0, dsp.RBL + 1);
		Csel(MDEC_CT, w0, MDEC_CT, eq);
		Str(MDEC_CT, dsp_operand(&DSP->regs.MDEC_CT));

		Ldp(x21, x22, MemOperand(sp, 16));
		Ldp(x23, x24, MemOperand(sp, 32));
		Ldp(x25, x26, MemOperand(sp, 48));
		Ldp(x27, x28, MemOperand(sp, 64));
		Ldp(x19, x20, MemOperand(sp, 80));
		Ldp(x29, x30, MemOperand(sp, 96, PostIndex));
		Ret();
#if 0
		instr_cur = GetBuffer()->GetEndAddress<Instruction*>();
		DEBUG_LOG(AICA_ARM, "DSP EPILOGUE");
		Disassemble(instr_start, instr_cur);
		instr_start = instr_cur;
#endif
		FinalizeCode();

		vmem_platform_flush_cache(
			GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>(),
			GetBuffer()->GetStartAddress<void*>(), GetBuffer()->GetEndAddress<void*>());
	}

private:
	MemOperand dsp_operand(void *data, int index = 0, u32 element_size = 4)
	{
		ptrdiff_t offset = ((u8*)data - (u8*)DSP) - offsetof(dsp_t, TEMP) + index  * element_size;
		if (offset < 16384)
			return MemOperand(x28, offset);
		Mov(x0, offset);
		return MemOperand(x28, x0);
	}

	MemOperand dsp_operand(void *data, const Register& offset_reg, u32 element_size = 4)
	{
		ptrdiff_t offset = ((u8*)data - (u8*)DSP) - offsetof(dsp_t, TEMP);
		if (offset == 0)
			return MemOperand(x28, offset_reg, LSL, element_size == 4 ? 2 : element_size == 2 ? 1 : 0);

		Mov(x0, offset);
		Add(x0, x0, Operand(offset_reg, LSL, element_size == 4 ? 2 : element_size == 2 ? 1 : 0));
		return MemOperand(x28, x0);
	}

	MemOperand dspdata_operand(void *data, int index = 0, u32 element_size = 4)
	{
		ptrdiff_t offset = ((u8*)data - (u8*)DSPData) + index  * element_size;
		if (offset < 16384)
			return MemOperand(x27, offset);
		Mov(x0, offset);
		return MemOperand(x27, x0);
	}

	template <typename R, typename... P>
	void GenCallRuntime(R (*function)(P...))
	{
		ptrdiff_t offset = reinterpret_cast<uintptr_t>(function) - GetBuffer()->GetStartAddress<uintptr_t>();
		verify(offset >= -128 * 1024 * 1024 && offset <= 128 * 1024 * 1024);
		verify((offset & 3) == 0);
		Label function_label;
		BindToOffset(&function_label, offset);
		Bl(&function_label);
	}

	void CalculateADDR(const Register& ADDR, const _INST& op, const Register& ADRS_REG, const Register& MDEC_CT)
	{
		//u32 ADDR = DSPData->MADRS[op.MASA];
		Ldr(ADDR, dspdata_operand(DSPData->MADRS, op.MASA));
		if (op.ADREB)
		{
			//ADDR += ADRS_REG & 0x0FFF;
			Ubfx(w0, ADRS_REG, 0, 12);
			Add(ADDR, ADDR, w0);
		}
		if (op.NXADR)
			//ADDR++;
			Add(ADDR, ADDR, 1);
		if (!op.TABLE)
		{
			//ADDR += DSP->regs.MDEC_CT;
			Add(ADDR, ADDR, MDEC_CT);
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

	Literal<u8*> *GetAicaRam()
	{
		if (aica_ram_lit == NULL)
			aica_ram_lit = new Literal<u8*>(&aica_ram[0], GetLiteralPool(), RawLiteral::kDeletedOnPoolDestruction);
		return aica_ram_lit;
	}

	void Disassemble(Instruction* instr_start, Instruction* instr_end)
	{
		Decoder decoder;
		Disassembler disasm;
		decoder.AppendVisitor(&disasm);
		Instruction* instr;
		for (instr = instr_start; instr < instr_end; instr += kInstructionSize) {
			decoder.Decode(instr);
			DEBUG_LOG(AICA_ARM, "    %p:\t%s", reinterpret_cast<void*>(instr), disasm.GetOutput());
		}
	}

	struct dsp_t *DSP;
	Literal<u8*> *aica_ram_lit;
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
	DSPAssembler assembler(&dsp.DynCode[0], sizeof(dsp.DynCode));
	assembler.Compile(&dsp);
}

void dsp_init()
{
	memset(&dsp, 0, sizeof(dsp));
	dsp.RBL = 0x8000 - 1;
	dsp.RBP=0;
	dsp.regs.MDEC_CT = 1;
	dsp.dyndirty = true;

	if (!mem_region_set_exec(dsp.DynCode, sizeof(dsp.DynCode)))
	{
		perror("Couldn’t mprotect DSP code");
		die("mprotect failed in arm64 dsp");
	}
}

void dsp_step()
{
	if (dsp.dyndirty)
	{
		dsp.dyndirty = false;
		dsp_recompile();
	}
	if (dsp.Stopped)
		return;
	((void (*)())&dsp.DynCode)();
}

void dsp_writenmem(u32 addr)
{
	if (addr >= 0x3400 && addr < 0x3C00)
	{
		dsp.dyndirty = true;
	}
	else if (addr >= 0x4000 && addr < 0x4400)
	{
		// TODO proper sharing of memory with sh4 through DSPData
		memset(dsp.TEMP, 0, sizeof(dsp.TEMP));
	}
	else if (addr >= 0x4400 && addr < 0x4500)
	{
		// TODO proper sharing of memory with sh4 through DSPData
		memset(dsp.MEMS, 0, sizeof(dsp.MEMS));
	}
}

void dsp_term()
{
}
#endif
