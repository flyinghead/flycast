
#include "types.h"

#include <map>

#if FEAT_SHREC == DYNAREC_CPP
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/dyna/regalloc.h"
#include "emitter/x86_emitter.h"
#include "profiler/profiler.h"
#include "oslib/oslib.h"


struct DynaRBI : RuntimeBlockInfo
{
	virtual u32 Relink() {
		//verify(false);
		return 0;
	}

	virtual void Relocate(void* dst) {
		verify(false);
	}
};



int cycle_counter;

void ngen_FailedToFindBlock_internal() {
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

void(*ngen_FailedToFindBlock)() = &ngen_FailedToFindBlock_internal;

void ngen_mainloop(void* v_cntx)
{
	Sh4RCB* ctx = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4RCB));

	cycle_counter = 0;

	for (;;) {
		cycle_counter = SH4_TIMESLICE;
		do {
			DynarecCodeEntryPtr rcb = bm_GetCode(ctx->cntx.pc);
			rcb();
		} while (cycle_counter > 0);

		if (UpdateSystem()) {
			rdv_DoInterrupts_pc(ctx->cntx.pc);
		}
	}
}

void ngen_init()
{
}


void ngen_GetFeatures(ngen_features* dst)
{
	dst->InterpreterFallback = true;
	dst->OnlyDynamicEnds = true;
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	return new DynaRBI();
}

u32* GetRegPtr(u32 reg)
{
	return Sh4_int_GetRegisterPtr((Sh4RegType)reg);
}

class opcodeExec {
	public:
	virtual void execute() = 0;
};

class opcodeDie : public opcodeExec {
	void execute()  {
		die("death opcode");
	}
};

struct opcode_ifb_pc : public opcodeExec {
	OpCallFP* oph;
	u32 pc;
	u16 opcode;
	 
	void execute()  {
		next_pc = pc;
		oph(opcode);
	}
};

struct opcode_ifb : public opcodeExec {
	OpCallFP* oph;
	u16 opcode;

	void execute()  {
		oph(opcode);
	}
};

struct opcode_jdyn : public opcodeExec {
	u32* src;
	void execute()  {
		next_pc = *src;
	}
};

struct opcode_jdyn_imm : public opcodeExec {
	u32* src;
	u32 imm;
	void execute()  {
		next_pc = *src + imm;
	}
};

struct opcode_mov32 : public opcodeExec {
	u32* src;
	u32* dst;
	
	void execute()  {
		*dst = *src;
	}
};

struct opcode_mov32_imm : public opcodeExec {
	u32 src;
	u32* dst;

	void execute()  {
		*dst = src;
	}
};

template <int cnt>
class fnblock {
public:
	opcodeExec* ops[cnt];
	int cc;
	void execute() {
		cycle_counter -= cc;
		for (int i = 0; i < cnt; i++) {
			ops[i]->execute();
		}
	}

	static void runner(void* fnb) {
		((fnblock<cnt>*)fnb)->execute();
	}
};

template <>
class fnblock<0> {
	void execute() {
		die("WHATNOT");
	}
};

struct fnrv {
	void* fnb;
	void(*runner)(void* fnb);
	opcodeExec** ptrs;
};

template<int opcode_slots>
fnrv fnnCtor(int cycles) {
	auto rv = new fnblock<opcode_slots>();
	rv->cc = cycles;
	fnrv rvb = { rv, &fnblock<opcode_slots>::runner, rv->ops };
	return rvb;
}

template<>
fnrv fnnCtor<0>(int cycles) {
	fnrv rvb = { 0, 0, 0 };
	return rvb;
}

template <typename CTR>
opcodeExec* createType() {
	return new CTR();
}

map< const char*, opcodeExec*(*)()> unmap = {
	{ "uru", &createType<opcodeDie> },
};

struct {
	void* fnb;
	void(*runner)(void* fnb);
} dispatchb[8192];

template<int n>
void disaptchn() {
	dispatchb[n].runner(dispatchb[n].fnb);
}

int idxnxx = 0;
//&disaptchn
#define REP_1(x, phrase) phrase < x >
#define REP_2(x, phrase) REP_1(x, phrase), REP_1(x+1, phrase)
#define REP_4(x, phrase) REP_2(x, phrase), REP_2(x+2, phrase)
#define REP_8(x, phrase) REP_4(x, phrase), REP_4(x+4, phrase)
#define REP_16(x, phrase) REP_8(x, phrase), REP_8(x+8, phrase)
#define REP_32(x, phrase) REP_16(x, phrase), REP_16(x+16, phrase)
#define REP_64(x, phrase) REP_32(x, phrase), REP_32(x+32, phrase)
#define REP_128(x, phrase) REP_64(x, phrase), REP_64(x+64, phrase)
#define REP_256(x, phrase) REP_128(x, phrase), REP_128(x+128, phrase)
#define REP_512(x, phrase) REP_256(x, phrase), REP_256(x+256, phrase)
#define REP_1024(x, phrase) REP_512(x, phrase), REP_512(x+512, phrase)
#define REP_2048(x, phrase) REP_1024(x, phrase), REP_1024(x+1024, phrase)
#define REP_4096(x, phrase) REP_2048(x, phrase), REP_2048(x+2048, phrase)
#define REP_8192(x, phrase) REP_4096(x, phrase), REP_4096(x+4096, phrase)


DynarecCodeEntryPtr FNS[] = { REP_8192(0, &disaptchn) };

typedef fnrv(*FNAFB)(int cycles);

FNAFB FNA[] = { REP_512(0, &fnnCtor) };

DynarecCodeEntryPtr getndpn_forreal(int n) {
	if (n > 8192)
		return 0;
	else
		return FNS[n];
}

FNAFB fnnCtor_forreal(int n) {
	if (n > 512)
		return 0;
	else
		return FNA[n];
}

class BlockCompiler {
public:

	void compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise) {
		
		auto ptrs = fnnCtor_forreal(block->oplist.size())(block->guest_cycles);

		dispatchb[idxnxx].fnb = ptrs.fnb;
		dispatchb[idxnxx].runner = ptrs.runner;

		block->code = getndpn_forreal(idxnxx++);

		for (size_t i = 0; i < block->oplist.size(); i++) {
			shil_opcode& op = block->oplist[i];
			switch (op.op) {

			case shop_ifb:
			{
				if (op.rs1._imm) {
					auto opc = new opcode_ifb_pc();
					ptrs.ptrs[i] = opc;
					
					opc->pc = op.rs2._imm;
					opc->opcode = op.rs3._imm;

					opc->oph = OpDesc[op.rs3._imm]->oph;
				}
				else {
					auto opc = new opcode_ifb();
					ptrs.ptrs[i] = opc;

					opc->opcode = op.rs3._imm;

					opc->oph = OpDesc[op.rs3._imm]->oph;
				}
			}
			break;
				
			case shop_jdyn:
			{
				if (op.rs2.is_imm()) {
					auto opc = new opcode_jdyn_imm();
					ptrs.ptrs[i] = opc;

					opc->src = op.rs1.reg_ptr();
					opc->imm = op.rs2._imm;
				}
				else {
					auto opc = new opcode_jdyn();
					ptrs.ptrs[i] = opc;

					opc->src = op.rs1.reg_ptr();
				}
				
			}
			break;

			case shop_mov32:
			{
				verify(op.rd.is_reg());

				verify(op.rs1.is_reg() || op.rs1.is_imm());

			
				if (op.rs1.is_imm()) {
					auto opc = new opcode_mov32_imm();
					ptrs.ptrs[i] = opc;

					opc->src = op.rs1._imm;
					opc->dst = op.rd.reg_ptr();
				}
				else {
					auto opc = new opcode_mov32();
					ptrs.ptrs[i] = opc;

					opc->src = op.rs1.reg_ptr();
					opc->dst = op.rd.reg_ptr();
				}
				

			}
			break;

			/*
			case shop_mov32:
			{
				verify(op.rd.is_reg());

				verify(op.rs1.is_reg() || op.rs1.is_imm());

				sh_to_reg(op.rs1, mov, ecx);

				reg_to_sh(op.rd, ecx);
			}
			break;

			case shop_mov64:
			{
				verify(op.rd.is_reg());

				verify(op.rs1.is_reg() || op.rs1.is_imm());

				sh_to_reg(op.rs1, mov, rcx);

				reg_to_sh(op.rd, rcx);
			}
			break;

			case shop_readm:
			{
				sh_to_reg(op.rs1, mov, call_regs[0]);
				sh_to_reg(op.rs3, add, call_regs[0]);

				u32 size = op.flags & 0x7f;

				if (size == 1) {
					call(ReadMem8);
					movsx(rcx, al);
				}
				else if (size == 2) {
					call(ReadMem16);
					movsx(rcx, ax);
				}
				else if (size == 4) {
					call(ReadMem32);
					mov(rcx, rax);
				}
				else if (size == 8) {
					call(ReadMem64);
					mov(rcx, rax);
				}
				else {
					die("1..8 bytes");
				}

				if (size != 8)
					reg_to_sh(op.rd, ecx);
				else
					reg_to_sh(op.rd, rcx);
			}
			break;

			case shop_writem:
			{
				u32 size = op.flags & 0x7f;
				sh_to_reg(op.rs1, mov, call_regs[0]);
				sh_to_reg(op.rs3, add, call_regs[0]);

				if (size != 8)
					sh_to_reg(op.rs2, mov, call_regs[1]);
				else
					sh_to_reg(op.rs2, mov, call_regs64[1]);

				if (size == 1)
					call(WriteMem8);
				else if (size == 2)
					call(WriteMem16);
				else if (size == 4)
					call(WriteMem32);
				else if (size == 8)
					call(WriteMem64);
				else {
					die("1..8 bytes");
				}
			}
			break;
			*/
			default:
				shil_chf[op.op](&op);
				break;
			}
		}

		verify(block->BlockType == BET_DynamicJump);

		//emit_Skip(getSize());
	}

	struct CC_PS
	{
		CanonicalParamType type;
		shil_param* prm;
	};

	vector<CC_PS> CC_pars;
	void* ccfn;

	void ngen_CC_Start(shil_opcode* op)
	{
		CC_pars.clear();
		ccfn = 0;
	}

	void ngen_CC_param(shil_opcode& op, shil_param& prm, CanonicalParamType tp) {
		CC_PS t = { tp, &prm };
		CC_pars.push_back(t);
	}

	void ngen_CC_Call(shil_opcode*op, void* function)
	{
		ccfn = function;
	}

	void ngen_CC_Finish(shil_opcode* op)
	{
		//lookup
		die("false");
	}

};

BlockCompiler* compiler;

void ngen_Compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
{
	verify(emit_FreeSpace() >= 16 * 1024);

	compiler = new BlockCompiler();


	compiler->compile(block, force_checks, reset, staging, optimise);

	delete compiler;
}



void ngen_CC_Start(shil_opcode* op)
{
	compiler->ngen_CC_Start(op);
}

void ngen_CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
{
	compiler->ngen_CC_param(*op, *par, tp);
}

void ngen_CC_Call(shil_opcode*op, void* function)
{
	compiler->ngen_CC_Call(op, function);
}

void ngen_CC_Finish(shil_opcode* op)
{
	compiler->ngen_CC_Finish(op);
}

void ngen_ResetBlocks()
{
	idxnxx = 0;
	int id = 0;
	/*
	while (dispatchb[id].fnb)
		delete dispatchb[id].fnb;
	*/
}
#endif