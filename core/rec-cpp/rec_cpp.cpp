
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
	dst->InterpreterFallback = false;
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

struct CC_PS
{
	CanonicalParamType type;
	shil_param* prm;
};

typedef vector<CC_PS> CC_pars_t;

struct opcode_cc_aBaCbC : public opcodeExec {
	void* fn;
	u32* rs1;
	u32 rs2;
	u32* rd;
	void execute()  {
		*rd = ((u32(*)(u32, u32))fn)(*rs1, rs2);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs2 = prms[0].prm->imm_value();
		rs1 = prms[1].prm->reg_ptr();
		rd = prms[2].prm->reg_ptr();
		verify(prms.size() == 3);
	}
};

struct opcode_cc_aCaCbC : public opcodeExec {
	void* fn;
	u32* rs1;
	u32* rs2;
	u32* rd;
	void execute()  {
		*rd = ((u32(*)(u32, u32))fn)(*rs1, *rs2);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs2 = prms[0].prm->reg_ptr();
		rs1 = prms[1].prm->reg_ptr();
		rd = prms[2].prm->reg_ptr();
		verify(prms.size() == 3);
	}
};

struct opcode_cc_aCbC : public opcodeExec {
	void* fn;
	u32* rs1;
	u32* rd;
	void execute()  {
		*rd = ((u32(*)(u32))fn)(*rs1);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs1 = prms[0].prm->reg_ptr();
		rd = prms[1].prm->reg_ptr();
		verify(prms.size() == 2);
	}
};

struct opcode_cc_aC : public opcodeExec {
	void* fn;
	u32* rs1;
	void execute()  {
		((void(*)(u32))fn)(*rs1);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs1 = prms[0].prm->reg_ptr();
		verify(prms.size() == 1);
	}
};

struct opcode_cc_aCaCaCbC : public opcodeExec {
	void* fn;
	u32* rs1;
	u32* rs2;
	u32* rs3;
	u32* rd;
	void execute()  {
		*rd = ((u32(*)(u32, u32, u32))fn)(*rs1, *rs2, *rs3);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs3 = prms[0].prm->reg_ptr();
		rs2 = prms[1].prm->reg_ptr();
		rs1 = prms[2].prm->reg_ptr();
		rd = prms[3].prm->reg_ptr();
		verify(prms.size() == 4);
	}
};

//split this to two cases, u64 and u64L/u32H
struct opcode_cc_aCaCaCcCdC : public opcodeExec {
	void* fn;
	u32* rs1;
	u32* rs2;
	u32* rs3;
	u32* rd;
	u32* rd2;
	void execute()  {
		auto rv = ((u64(*)(u32, u32, u32))fn)(*rs1, *rs2, *rs3);

		*rd = rv;
		*rd2 = rv >> 32;
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs3 = prms[0].prm->reg_ptr();
		rs2 = prms[1].prm->reg_ptr();
		rs1 = prms[2].prm->reg_ptr();
		rd = prms[3].prm->reg_ptr();
		rd2 = prms[4].prm->reg_ptr();

		//verify((u64*)(rd2 - 1) == rd);
		verify(prms.size() == 5);
	}
};


struct opcode_cc_aCaCcCdC : public opcodeExec {
	void* fn;
	u32* rs1;
	u32* rs2;
	u32* rd;
	u32* rd2;
	void execute()  {
		auto rv = ((u64(*)(u32, u32))fn)(*rs1, *rs2);
		*rd = rv;
		*rd2 = rv >> 32;
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs2 = prms[0].prm->reg_ptr();
		rs1 = prms[1].prm->reg_ptr();
		rd = prms[2].prm->reg_ptr();
		rd2 = prms[3].prm->reg_ptr();

		verify(prms.size() == 4);
	}
};


struct opcode_cc_eDeDeDfD : public opcodeExec {
	void* fn;
	f32* rs1;
	f32* rs2;
	f32* rs3;
	f32* rd;
	void execute()  {
		*rd = ((f32(*)(f32, f32, f32))fn)(*rs1, *rs2, *rs3);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs3 = (f32*)prms[0].prm->reg_ptr();
		rs2 = (f32*)prms[1].prm->reg_ptr();
		rs1 = (f32*)prms[2].prm->reg_ptr();
		rd = (f32*)prms[3].prm->reg_ptr();
	}
};


struct opcode_cc_eDeDfD : public opcodeExec {
	void* fn;
	f32* rs1;
	f32* rs2;
	f32* rd;
	void execute()  {
		*rd = ((f32(*)(f32, f32))fn)(*rs1, *rs2);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs2 = (f32*)prms[0].prm->reg_ptr();
		rs1 = (f32*)prms[1].prm->reg_ptr();
		rd = (f32*)prms[2].prm->reg_ptr();
	}
};

struct opcode_cc_eDeDbC : public opcodeExec {
	void* fn;
	f32* rs1;
	f32* rs2;
	u32* rd;
	void execute()  {
		*rd = ((u32(*)(f32, f32))fn)(*rs1, *rs2);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs2 = (f32*)prms[0].prm->reg_ptr();
		rs1 = (f32*)prms[1].prm->reg_ptr();
		rd = (u32*)prms[2].prm->reg_ptr();
	}
};

struct opcode_cc_eDbC : public opcodeExec {
	void* fn;
	f32* rs1;
	u32* rd;
	void execute()  {
		*rd = ((u32(*)(f32))fn)(*rs1);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs1 = (f32*)prms[0].prm->reg_ptr();
		rd = (u32*)prms[1].prm->reg_ptr();
	}
};

struct opcode_cc_aCfD : public opcodeExec {
	void* fn;
	u32* rs1;
	f32* rd;
	void execute()  {
		*rd = ((f32(*)(u32))fn)(*rs1);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs1 = (u32*)prms[0].prm->reg_ptr();
		rd = (f32*)prms[1].prm->reg_ptr();
	}
};

struct opcode_cc_eDfD : public opcodeExec {
	void* fn;
	f32* rs1;
	f32* rd;
	void execute()  {
		*rd = ((f32(*)(f32))fn)(*rs1);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs1 = (f32*)prms[0].prm->reg_ptr();
		rd = (f32*)prms[1].prm->reg_ptr();
	}
};

struct opcode_cc_aCgE : public opcodeExec {
	void* fn;
	u32* rs1;
	f32* rd;
	void execute()  {
		((void(*)(f32*, u32))fn)(rd, *rs1);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs1 = (u32*)prms[0].prm->reg_ptr();
		rd = (f32*)prms[1].prm->reg_ptr();
	}
};

struct opcode_cc_gJgHgH : public opcodeExec {
	void* fn;
	f32* rs2;
	f32* rs1;
	f32* rd;
	void execute()  {
		((void(*)(f32*, f32*, f32*))fn)(rd, rs1, rs2);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs2 = (f32*)prms[0].prm->reg_ptr();
		rs1 = (f32*)prms[1].prm->reg_ptr();
		rd = (f32*)prms[2].prm->reg_ptr();
	}
};

struct opcode_cc_gHgHfD : public opcodeExec {
	void* fn;
	f32* rs2;
	f32* rs1;
	f32* rd;
	void execute()  {
		*rd = ((f32(*)(f32*, f32*))fn)(rs1, rs2);
	}

	void setup(const CC_pars_t& prms, void* fun) {
		fn = fun;
		rs2 = (f32*)prms[0].prm->reg_ptr();
		rs1 = (f32*)prms[1].prm->reg_ptr();
		rd = (f32*)prms[2].prm->reg_ptr();
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

struct opcode_mov64 : public opcodeExec {
	u64* src;
	u64* dst;

	void execute()  {
		*dst = *src;
	}
};

#define do_readm(d, a, sz) do { if (sz == 1) { *d = (s32)(s8)ReadMem8(a); } else if (sz == 2) { *d = (s32)(s16)ReadMem16(a); } \
								else if (sz == 4) { *d = ReadMem32(a);} else if (sz == 8) { *(u64*)d = ReadMem64(a); } \
							  } while(0)
template <int sz>
struct opcode_readm : public opcodeExec {
	u32* src;
	u32* dst;

	void execute()  {
		auto a = *src;
		do_readm(dst, a, sz);
	}
};

template <int sz>
struct opcode_readm_imm : public opcodeExec {
	u32 src;
	u32* dst;

	void execute()  {
		auto a = src;
		do_readm(dst, a, sz);
	}
};

template <int sz>
struct opcode_readm_offs : public opcodeExec {
	u32* src;
	u32* dst;
	u32* offs;

	void execute()  {
		auto a = *src + *offs;
		do_readm(dst, a, sz);
	}
};

template <int sz>
struct opcode_readm_offs_imm : public opcodeExec {
	u32* src;
	u32* dst;
	u32 offs;

	void execute()  {
		auto a = *src + offs;
		do_readm(dst, a, sz);
	}
};

#define do_writem(d, a, sz) do { if (sz == 1) { WriteMem8(a, *d);} else if (sz == 2) { WriteMem16(a, *d); } \
										else if (sz == 4) { WriteMem32(a, *d);} else if (sz == 8) { WriteMem64(a, *(u64*)d); } \
							  } while(0)
template <int sz>
struct opcode_writem : public opcodeExec {
	u32* src;
	u32* src2;

	void execute()  {
		auto a = *src;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_imm : public opcodeExec {
	u32 src;
	u32* src2;

	void execute()  {
		auto a = src;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_offs : public opcodeExec {
	u32* src;
	u32* src2;
	u32* offs;

	void execute()  {
		auto a = *src + *offs;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_offs_imm : public opcodeExec {
	u32* src;
	u32* src2;
	u32 offs;

	void execute()  {
		auto a = *src + offs;
		do_writem(src2, a, sz);
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
opcodeExec* createType(const CC_pars_t& prms, void* fun) {
	auto rv = new CTR();

	rv->setup(prms, fun);
	return rv;
}

map< string, opcodeExec*(*)(const CC_pars_t& prms, void* fun)> unmap = {
	{ "aBaCbC", &createType<opcode_cc_aBaCbC> },
	{ "aCaCbC", &createType<opcode_cc_aCaCbC> },
	{ "aCbC", &createType<opcode_cc_aCbC> },
	{ "aC", &createType<opcode_cc_aC> },

	{ "eDeDeDfD", &createType<opcode_cc_eDeDeDfD> },
	{ "eDeDfD", &createType<opcode_cc_eDeDfD> },

	{ "aCaCaCbC", &createType<opcode_cc_aCaCaCbC> },
	{ "aCaCcCdC", &createType<opcode_cc_aCaCcCdC> },
	{ "aCaCaCcCdC", &createType<opcode_cc_aCaCaCcCdC> },

	{ "eDbC", &createType<opcode_cc_eDbC> },
	{ "aCfD", &createType<opcode_cc_aCfD> },

	{ "eDeDbC", &createType<opcode_cc_eDeDbC> },
	{ "eDfD", &createType<opcode_cc_eDfD> },

	{ "aCgE", &createType<opcode_cc_aCgE> },
	{ "gJgHgH", &createType<opcode_cc_gJgHgH> },
	{ "gHgHfD", &createType<opcode_cc_gHgHfD> },
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
	if (n >= 8192)
		return 0;
	else
		return FNS[n];
}

FNAFB fnnCtor_forreal(size_t n) {
	if (n > 512)
		return 0;
	else
		return FNA[n];
}

class BlockCompiler {
public:

	size_t opcode_index;
	opcodeExec** ptrsg;
	void compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise) {
		
		auto ptrs = fnnCtor_forreal(block->oplist.size())(block->guest_cycles);

		ptrsg = ptrs.ptrs;

		dispatchb[idxnxx].fnb = ptrs.fnb;
		dispatchb[idxnxx].runner = ptrs.runner;

		block->code = getndpn_forreal(idxnxx++);

		if (getndpn_forreal(idxnxx) == 0) {
			emit_Skip(emit_FreeSpace()-16);
		}

		for (size_t i = 0; i < block->oplist.size(); i++) {
			opcode_index = i;
			shil_opcode& op = block->oplist[i];
			switch (op.op) {

			case shop_ifb:
			{
				if (op.rs1.imm_value()) {
					auto opc = new opcode_ifb_pc();
					ptrs.ptrs[i] = opc;
					
					opc->pc = op.rs2.imm_value();
					opc->opcode = op.rs3.imm_value();

					opc->oph = OpDesc[op.rs3.imm_value()]->oph;
				}
				else {
					auto opc = new opcode_ifb();
					ptrs.ptrs[i] = opc;

					opc->opcode = op.rs3.imm_value();

					opc->oph = OpDesc[op.rs3.imm_value()]->oph;
				}
			}
			break;
				
			case shop_jdyn:
			{
				if (op.rs2.is_imm()) {
					auto opc = new opcode_jdyn_imm();
					ptrs.ptrs[i] = opc;

					opc->src = op.rs1.reg_ptr();
					opc->imm = op.rs2.imm_value();
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

					opc->src = op.rs1.imm_value();
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

			case shop_mov64:
			{
				verify(op.rd.is_reg());

				verify(op.rs1.is_reg());

				auto opc = new opcode_mov64();
				ptrs.ptrs[i] = opc;

				opc->src = (u64*) op.rs1.reg_ptr();
				opc->dst = (u64*)op.rd.reg_ptr();
			}
			break;

			case shop_readm:
			{
				u32 size = op.flags & 0x7f;
				if (op.rs1.is_imm()) {
					verify(op.rs2.is_null() && op.rs3.is_null());

					if (size == 1)
					{
						auto opc = new opcode_readm_imm<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_readm_imm<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_readm_imm<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_readm_imm<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->dst = op.rd.reg_ptr();
					}
				}
				else if (op.rs3.is_imm()) {
					verify(op.rs2.is_null());
					if (size == 1)
					{
						auto opc = new opcode_readm_offs_imm<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_readm_offs_imm<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_readm_offs_imm<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_readm_offs_imm<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->dst = op.rd.reg_ptr();
					}
				}
				else if (op.rs3.is_reg()) {
					verify(op.rs2.is_null());
					if (size == 1)
					{
						auto opc = new opcode_readm_offs<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_readm_offs<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_readm_offs<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_readm_offs<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
				}
				else {
					verify(op.rs2.is_null() && op.rs3.is_null());
					if (size == 1)
					{
						auto opc = new opcode_readm<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_readm<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_readm<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_readm<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
				}
			}
			break;

			case shop_writem:
			{
				u32 size = op.flags & 0x7f;
				
				if (op.rs1.is_imm()) {
					verify(op.rs3.is_null());
					if (size == 1)
					{
						auto opc = new opcode_writem_imm<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_imm<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_imm<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_imm<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
				}
				else if (op.rs3.is_imm()) {
					if (size == 1)
					{
						auto opc = new opcode_writem_offs_imm<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_offs_imm<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_offs_imm<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_offs_imm<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
				}
				else if (op.rs3.is_reg()) {
					if (size == 1)
					{
						auto opc = new opcode_writem_offs<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_offs<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_offs<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_offs<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
				}
				else {
					verify(op.rs3.is_null());
					if (size == 1)
					{
						auto opc = new opcode_writem<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
				}
			}
			break;
			
			default:
				shil_chf[op.op](&op);
				break;
			}
		}

		verify(block->BlockType == BET_DynamicJump);

		//emit_Skip(getSize());
	}

	CC_pars_t CC_pars;
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
		string nm = "";
		for (auto m : CC_pars) {
			nm += (char)(m.type + 'a');
			nm += (char)(m.prm->type + 'A');
		}
		
		if (unmap.count(nm)) {
			ptrsg[opcode_index] = unmap[nm](CC_pars, ccfn);
		}
		else {
			printf("IMPLEMENT CC_CALL CLASS: %s\n", nm.c_str());
			ptrsg[opcode_index] = new opcodeDie();
		}
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