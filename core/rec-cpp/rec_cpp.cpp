#include "types.h"

#if FEAT_SHREC == DYNAREC_CPP
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"

#define SHIL_MODE 2
#include "hw/sh4/dyna/shil_canonical.h"

#include <algorithm>
#include <map>

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

void ngen_mainloop(void* v_cntx)
{
	Sh4RCB* ctx = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4RCB));

	cycle_counter = 0;

	while (sh4_int_bCpuRun)
	{
		cycle_counter = SH4_TIMESLICE;
		do {
			DynarecCodeEntryPtr rcb = bm_GetCodeByVAddr(ctx->cntx.pc);
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

RuntimeBlockInfo* ngen_AllocateBlock()
{
	return new DynaRBI();
}

static void ngen_blockcheckfail(u32 pc) {
	INFO_LOG(DYNAREC, "REC CPP: SMC invalidation at %08X", pc);
	rdv_BlockCheckFail(pc);
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

typedef std::vector<CC_PS> CC_pars_t;


struct opcode_cc_aBaCbC {
	template <typename T>
	struct opex2 : public opcodeExec  {
		
		u32 rs2;
		const u32* rs1;
		u32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = prms[0].prm->imm_value();
			rs1 = prms[1].prm->reg_ptr();
			rd = prms[2].prm->reg_ptr();
			verify(prms.size() == 3);
		}

		void execute()  {
			*rd = ((u32(*)(u32, u32))&T::impl)(*rs1, rs2);
		}
	};
};

struct opcode_cc_aCaBbC {
	struct opex : public opcodeExec  {
		void* fn;
		const u32 *rs2;
		u32 rs1;
		u32* rd;

		void execute()  {
			*rd = ((u32(*)(u32, u32))fn)(rs1, *rs2);
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs2 = prms[0].prm->reg_ptr();
			rs1 = prms[1].prm->imm_value();
			rd = prms[2].prm->reg_ptr();
			verify(prms.size() == 3);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((u32(*)(u32, u32))&T::impl)(rs1, *rs2);
		}
	};
};

struct opcode_cc_aCaCbC {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
		const u32* rs2;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((u32(*)(u32, u32))&T::impl)(*rs1, *rs2);
		}
	};
};

struct opcode_cc_aCbC {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((u32(*)(u32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_aC {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
		void execute()  {
			((void(*)(u32))fn)(*rs1);
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs1 = prms[0].prm->reg_ptr();
			verify(prms.size() == 1);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			((void(*)(u32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_aB {
	struct opex : public opcodeExec {
		void* fn;
		u32 rs1;
		void execute()  {
			((void(*)(u32))fn)(rs1);
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs1 = prms[0].prm->imm_value();
			verify(prms.size() == 1);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			((void(*)(u32))&T::impl)(rs1);
		}
	};
};

struct opcode_cc_aCaCaCbC {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
		const u32* rs2;
		const u32* rs3;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((u32(*)(u32, u32, u32))&T::impl)(*rs1, *rs2, *rs3);
		}
	};
};

struct opcode_cc_aCaCaCcCdC {
	//split this to two cases, u64 and u64L/u32H
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
		const u32* rs2;
		const u32* rs3;
		u32* rd;
		u32* rd2;
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))fn)(*rs1, *rs2, *rs3);

			*rd = (u32)rv;
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
	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))&T::impl)(*rs1, *rs2, *rs3);

			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};

};

struct opcode_cc_aCaCcCdC {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
		const u32* rs2;
		u32* rd;
		u32* rd2;
		void execute()  {
			auto rv = ((u64(*)(u32, u32))fn)(*rs1, *rs2);
			*rd = (u32)rv;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			auto rv = ((u64(*)(u32, u32))&T::impl)(*rs1, *rs2);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};
};

struct opcode_cc_aCaBcCdC {
	struct opex : public opcodeExec {
		void* fn;
		u32 rs1;
		const u32* rs2;
		u32* rd;
		u32* rd2;
		void execute()  {
			auto rv = ((u64(*)(u32, u32))fn)(rs1, *rs2);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs2 = prms[0].prm->reg_ptr();
			rs1 = prms[1].prm->imm_value();
			rd = prms[2].prm->reg_ptr();
			rd2 = prms[3].prm->reg_ptr();

			verify(prms.size() == 4);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			auto rv = ((u64(*)(u32, u32))&T::impl)(rs1, *rs2);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};
};

struct opcode_cc_aBaCcCdC {
	struct opex : public opcodeExec {
		void* fn;
		const u32 *rs1;
		u32 rs2;
		u32* rd;
		u32* rd2;
		void execute()  {
			auto rv = ((u64(*)(u32, u32))fn)(*rs1, rs2);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs2 = prms[0].prm->imm_value();
			rs1 = prms[1].prm->reg_ptr();
			rd = prms[2].prm->reg_ptr();
			rd2 = prms[3].prm->reg_ptr();

			verify(prms.size() == 4);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			auto rv = ((u64(*)(u32, u32))&T::impl)(*rs1, rs2);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};
};

struct opcode_cc_aBaCaCcCdC {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
		const u32* rs2;
		u32 rs3;
		u32* rd;
		u32* rd2;
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))fn)(*rs1, *rs2, rs3);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs3 = prms[0].prm->imm_value();
			rs2 = prms[1].prm->reg_ptr();
			rs1 = prms[2].prm->reg_ptr();
			rd = prms[3].prm->reg_ptr();
			rd2 = prms[4].prm->reg_ptr();

			verify(prms.size() == 5);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))&T::impl)(*rs1, *rs2, rs3);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};
};

struct opcode_cc_aCaBaCcCdC {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
		u32 rs2;
		const u32 *rs3;
		u32* rd;
		u32* rd2;
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))fn)(*rs1, rs2, *rs3);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs3 = prms[0].prm->reg_ptr();
			rs2 = prms[1].prm->imm_value();
			rs1 = prms[2].prm->reg_ptr();
			rd = prms[3].prm->reg_ptr();
			rd2 = prms[4].prm->reg_ptr();

			verify(prms.size() == 5);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))&T::impl)(*rs1, rs2, *rs3);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};
};

struct opcode_cc_aCaBaBcCdC {
	struct opex : public opcodeExec {
		void* fn;
		u32 rs1;
		u32 rs2;
		const u32* rs3;
		u32* rd;
		u32* rd2;
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))fn)(rs1, rs2, *rs3);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs3 = prms[0].prm->reg_ptr();
			rs2 = prms[1].prm->imm_value();
			rs1 = prms[2].prm->imm_value();
			rd = prms[3].prm->reg_ptr();
			rd2 = prms[4].prm->reg_ptr();

			verify(prms.size() == 5);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))&T::impl)(rs1, rs2, *rs3);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};
};

struct opcode_cc_aBaBaBcCdC {
	struct opex : public opcodeExec {
		void* fn;
		u32 rs1;
		u32 rs2;
		u32 rs3;
		u32* rd;
		u32* rd2;
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))fn)(rs1, rs2, rs3);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs3 = prms[0].prm->imm_value();
			rs2 = prms[1].prm->imm_value();
			rs1 = prms[2].prm->imm_value();
			rd = prms[3].prm->reg_ptr();
			rd2 = prms[4].prm->reg_ptr();

			verify(prms.size() == 5);
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			auto rv = ((u64(*)(u32, u32, u32))&T::impl)(rs1, rs2, rs3);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};
};

struct opcode_cc_eDeDeDfD {
	struct opex : public opcodeExec {
		void* fn;
		const f32* rs1;
		const f32* rs2;
		const f32* rs3;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((f32(*)(f32, f32, f32))&T::impl)(*rs1, *rs2, *rs3);
		}
	};

};

struct opcode_cc_eDeDfD {
	struct opex : public opcodeExec {
		void* fn;
		const f32* rs1;
		const f32* rs2;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((f32(*)(f32, f32))&T::impl)(*rs1, *rs2);
		}
	};
};

struct opcode_cc_eBeDfD {
	struct opex : public opcodeExec {
		void* fn;
		const f32* rs1;
		f32 rs2;
		f32* rd;
		void execute()  {
			*rd = ((f32(*)(f32, f32))fn)(*rs1, rs2);
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs2 = reinterpret_cast<f32&>(prms[0].prm->_imm);
			rs1 = (f32*)prms[1].prm->reg_ptr();
			rd = (f32*)prms[2].prm->reg_ptr();
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((f32(*)(f32, f32))&T::impl)(*rs1, rs2);
		}
	};
};

struct opcode_cc_eDeDbC {
	struct opex : public opcodeExec {
		void* fn;
		const f32* rs1;
		const f32* rs2;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((u32(*)(f32, f32))&T::impl)(*rs1, *rs2);
		}
	};
};

struct opcode_cc_eDbC {
	struct opex : public opcodeExec {
		void* fn;
		const f32* rs1;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((u32(*)(f32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_aCfD {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((f32(*)(u32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_eDfD {
	struct opex : public opcodeExec {
		void* fn;
		const f32* rs1;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((f32(*)(f32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_aCgE {
	struct opex : public opcodeExec {
		void* fn;
		const u32* rs1;
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

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			((void(*)(f32*, u32))&T::impl)(rd, *rs1);
		}
	};
};

struct opcode_cc_gJgHgH {
	struct opex : public opcodeExec {
		void* fn;
		const f32* rs2;
		const f32* rs1;
		f32* rd;
		void execute()  {
			((void(*)(f32*, const f32*, const f32*))fn)(rd, rs1, rs2);
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs2 = (f32*)prms[0].prm->reg_ptr();
			rs1 = (f32*)prms[1].prm->reg_ptr();
			rd = (f32*)prms[2].prm->reg_ptr();
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			((void(*)(f32*, const f32*, const f32*))&T::impl)(rd, rs1, rs2);
		}
	};
};

struct opcode_cc_gHgHfD {
	struct opex : public opcodeExec {
		void* fn;
		const f32* rs2;
		const f32* rs1;
		f32* rd;
		void execute()  {
			*rd = ((f32(*)(const f32*, const f32*))fn)(rs1, rs2);
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs2 = (f32*)prms[0].prm->reg_ptr();
			rs1 = (f32*)prms[1].prm->reg_ptr();
			rd = (f32*)prms[2].prm->reg_ptr();
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			*rd = ((f32(*)(const f32*, const f32*))&T::impl)(rs1, rs2);
		}
	};
};

struct opcode_cc_vV {
	struct opex : public opcodeExec {
		void* fn;
		
		void execute()  {
			((void(*)())fn)();
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			((void(*)())&T::impl)();
		}
	};
};

//u64* fd1,u64* fd2,u64* fs1,u64* fs2
//slightly violates the type, as it's FV4PTR but we pass u64*
struct opcode_cc_gJgJgJgJ {
	struct opex : public opcodeExec {
		void* fn;
		const u64* rs2;
		const u64* rs1;
		u64* rd;
		u64* rd2;
		void execute()  {
			((void(*)(u64*, u64*, const u64*, const u64*))fn)(rd, rd2, rs1, rs2);
		}

		void setup(const CC_pars_t& prms, void* fun) {
			fn = fun;
			rs2 = (u64*)prms[0].prm->reg_ptr();
			rs1 = (u64*)prms[1].prm->reg_ptr();
			rd2 = (u64*)prms[2].prm->reg_ptr();
			rd = (u64*)prms[3].prm->reg_ptr();
		}
	};

	template <typename T>
	struct opex2 : public opex {
		void execute()  {
			((void(*)(u64*, u64*, const u64*, const u64*))&T::impl)(rd, rd2, rs1, rs2);
		}
	};
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
	const u32* src;
	void execute()  {
		Sh4cntx.jdyn = *src;
	}
};

struct opcode_jdyn_imm : public opcodeExec {
	const u32* src;
	u32 imm;
	void execute()  {
		Sh4cntx.jdyn = *src + imm;
	}
};

struct opcode_mov32 : public opcodeExec {
	const u32* src;
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
	const u64* src;
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
	const u32* src;
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
	const u32* src;
	u32* dst;
	const u32* offs;

	void execute()  {
		auto a = *src + *offs;
		do_readm(dst, a, sz);
	}
};

template <int sz>
struct opcode_readm_offs_imm : public opcodeExec {
	const u32* src;
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
	const u32* src;
	const u32* src2;

	void execute()  {
		auto a = *src;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_imm : public opcodeExec {
	u32 src;
	const u32* src2;

	void execute()  {
		auto a = src;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_offs : public opcodeExec {
	const u32* src;
	const u32* src2;
	const u32* offs;

	void execute()  {
		auto a = *src + *offs;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_offs_imm : public opcodeExec {
	const u32* src;
	const u32* src2;
	u32 offs;

	void execute()  {
		auto a = *src + offs;
		do_writem(src2, a, sz);
	}
};

template<int end_type>
struct opcode_blockend : public opcodeExec {
	int next_pc_value;
	int branch_pc_value;
	const u32* jdyn;

	opcodeExec* setup(RuntimeBlockInfo* block) {
		next_pc_value = block->NextBlock;
		branch_pc_value = block->BranchBlock;

		jdyn = &Sh4cntx.jdyn;
		if (!block->has_jcond && BET_GET_CLS(block->BlockType) == BET_CLS_COND) {
			jdyn = &sr.T;
		}
		return this;
	}

	void execute()  {
		//do whatever
		

		switch (end_type) {

		case BET_StaticJump:
		case BET_StaticCall:
			next_pc = branch_pc_value;
			break;

		case BET_Cond_0:
			if (*jdyn != 0)
				next_pc = next_pc_value;
			else
				next_pc = branch_pc_value;
			break;

		case BET_Cond_1:
			if (*jdyn != 1)
				next_pc = next_pc_value;
			else
				next_pc = branch_pc_value;
			break;

		case BET_DynamicJump:
		case BET_DynamicCall:
		case BET_DynamicRet:
			next_pc = *jdyn;
			break;

		case BET_DynamicIntr:
		case BET_StaticIntr:
			if (end_type == BET_DynamicIntr)
				next_pc = *jdyn;
			else
				next_pc = next_pc_value;

			UpdateINTC();
			break;

		default:
			die("NOT GONNA HAPPEN TODAY, ALRIGHY?");
		}
	}
};

template <int sz>
struct opcode_check_block : public opcodeExec {
	RuntimeBlockInfo* block;
	std::vector<u8> code;
	const void* ptr;

	opcodeExec* setup(RuntimeBlockInfo* block) {
		this->block = block;
		ptr = GetMemPtr(block->addr, 4);
		if (ptr != NULL)
		{
			code.resize(sz == -1 ? block->sh4_code_size : sz);
			memcpy(&code[0], ptr, sz == -1 ? block->sh4_code_size : sz);
		}

		return this;
	}

	void execute() {
		if (code.empty())
			return;

		switch (sz)
		{
		case 4:
			if (*(u32 *)ptr != *(u32 *)&code[0])
				ngen_blockcheckfail(block->addr);
			break;
		case 6:
			if (*(u32 *)ptr != *(u32 *)&code[0] || *((u16 *)ptr + 2) != *((u16 *)&code[0] + 2))
				ngen_blockcheckfail(block->addr);
			break;
		case 8:
			if (*(u32 *)ptr != *(u32 *)&code[0] || *((u32 *)ptr + 1) != *((u32 *)&code[0] + 1))
				ngen_blockcheckfail(block->addr);
			break;
		default:
			if (memcmp(ptr, &code[0], block->sh4_code_size) != 0)
				ngen_blockcheckfail(block->addr);
			break;
		}
	}
};

#if !defined(_DEBUG)
	#define DREP_1(x, phrase) if (x < cnt) ops[x]->execute(); else return;
	#define DREP_2(x, phrase) DREP_1(x, phrase) DREP_1(x+1, phrase)
	#define DREP_4(x, phrase) DREP_2(x, phrase) DREP_2(x+2, phrase)
	#define DREP_8(x, phrase) DREP_4(x, phrase) DREP_4(x+4, phrase)
	#define DREP_16(x, phrase) DREP_8(x, phrase) DREP_8(x+8, phrase)
	#define DREP_32(x, phrase) DREP_16(x, phrase) DREP_16(x+16, phrase)
	#define DREP_64(x, phrase) DREP_32(x, phrase) DREP_32(x+32, phrase)
	#define DREP_128(x, phrase) DREP_64(x, phrase) DREP_64(x+64, phrase)
	#define DREP_256(x, phrase) DREP_128(x, phrase) DREP_128(x+128, phrase)
	#define DREP_512(x, phrase) DREP_256(x, phrase) DREP_256(x+256, phrase)
#else
	#define DREP_512(x, phrase) for (int i=0; i<cnt; i++) ops[i]->execute();
#endif

class fnblock_base
{
public:
	virtual ~fnblock_base() {}
};

template <int cnt>
class fnblock : public fnblock_base {
public:
	opcodeExec* ops[cnt];
	int cc;
	void execute() {
		cycle_counter -= cc;

		DREP_512(0, phrase);
	}

	static void runner(fnblock_base* fnb) {
		((fnblock<cnt>*)fnb)->execute();
	}
	~fnblock()
	{
		for (int i = 0; i < cnt; i++)
			delete ops[i];
	}
};

template <>
class fnblock<0> {
	void execute() {
		die("WHATNOT");
	}
};

struct fnrv {
	fnblock_base* fnb;
	void (*runner)(fnblock_base* fnb);
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

template <typename shilop, typename CTR>
opcodeExec* createType2(const CC_pars_t& prms, void* fun) {
	typedef typename CTR::template opex2<shilop> thetype;
	auto rv = new thetype();

	rv->setup(prms, fun);
	return rv;
}


std::map<void*, int> funs;


int funs_id_count;

template <typename CTR>
opcodeExec* createType_fast(const CC_pars_t& prms, void* fun, shil_opcode* opcode) {
	return 0;
}

#define OPCODE_CC(sig) opcode_cc_##sig

#define FAST_sig(sig, ...) \
template <> \
opcodeExec* createType_fast<OPCODE_CC(sig)>(const CC_pars_t& prms, void* fun, shil_opcode* opcode) { \
	typedef OPCODE_CC(sig) CTR; \
	\
	static std::map<void*, opcodeExec* (*)(const CC_pars_t& prms, void* fun)> funsf = {\
		
#define FAST_gis \
};\
	\
	if (funsf.count(fun)) { \
		return funsf[fun](prms, fun); \
	} \
	else { \
		return 0; \
	} \
}

#define FAST_po2(n,fn) { (void*)&shil_opcl_##n::fn::impl, &createType2 < shil_opcl_##n::fn, CTR > },
#define FAST_po(n) FAST_po2(n, f1)

FAST_sig(aCaCbC)
FAST_po(and)
FAST_po(or)
FAST_po(xor)
FAST_po(add)
FAST_po(sub)
FAST_po(ror)
FAST_po(shl)
FAST_po(shr)
FAST_po(sar)
FAST_po(shad)
FAST_po(shld)
FAST_po(test)
FAST_po(seteq)
FAST_po(setge)
FAST_po(setgt)
FAST_po(setae)
FAST_po(setab)
FAST_po(setpeq)
FAST_po(mul_u16)
FAST_po(mul_s16)
FAST_po(mul_i32)
FAST_po(xtrct)
FAST_gis

FAST_sig(aBaCbC)
FAST_po(and)
FAST_po(or)
FAST_po(xor)
FAST_po(add)
FAST_po(sub)
FAST_po(ror)
FAST_po(shl)
FAST_po(shr)
FAST_po(sar)
FAST_po(shad)
FAST_po(shld)
FAST_po(test)
FAST_po(seteq)
FAST_po(setge)
FAST_po(setgt)
FAST_po(setae)
FAST_po(setab)
FAST_po(setpeq)
FAST_po(mul_u16)
FAST_po(mul_s16)
FAST_po(mul_i32)
FAST_gis

FAST_sig(aCaBbC)
FAST_po(shld)
FAST_po(shad)
FAST_gis

FAST_sig(aCaBcCdC)
FAST_po(rocr)
FAST_po(rocl)
FAST_po(negc)
FAST_gis

FAST_sig(aBaCcCdC)
FAST_po(mul_s64)
FAST_po(mul_u64)
FAST_gis

FAST_sig(eDeDfD)
FAST_po(fadd)
FAST_po(fsub)
FAST_po(fmul)
FAST_po(fdiv)
FAST_gis

FAST_sig(eBeDfD)
FAST_po(fadd)
FAST_po(fsub)
FAST_po(fmul)
FAST_po(fdiv)
FAST_gis

FAST_sig(eDfD)
FAST_po(fneg)
FAST_po(fabs)
FAST_po(fsrra)
FAST_po(fsqrt)
FAST_gis


FAST_sig(eDeDbC)
FAST_po(fseteq)
FAST_po(fsetgt)
FAST_gis

FAST_sig(eDeDeDfD)
FAST_po(fmac)
FAST_gis

FAST_sig(gHgHfD)
FAST_po(fipr)
FAST_gis

FAST_sig(aCaCcCdC)
FAST_po(div32u)
FAST_po(div32s)
FAST_po(rocr)
FAST_po(rocl)
FAST_po(negc)
FAST_po(mul_u64)
FAST_po(mul_s64)
FAST_gis

FAST_sig(aCaCaCcCdC)
FAST_po(adc)
FAST_po(sbc)
FAST_gis

FAST_sig(aCaBaBcCdC)
FAST_po(adc)
FAST_po(sbc)
FAST_gis

FAST_sig(aCaBaCcCdC)
FAST_po(adc)
FAST_po(sbc)
FAST_gis

FAST_sig(aBaCaCcCdC)
FAST_po(sbc)
FAST_gis

FAST_sig(aBaBaBcCdC)
FAST_po(sbc)
FAST_gis

FAST_sig(aCaCaCbC)
FAST_po(div32p2)
FAST_gis

FAST_sig(aCbC)
FAST_po(neg)
FAST_po(not)
FAST_po(ext_s8)
FAST_po(ext_s16)
FAST_po(swaplb)
FAST_gis

FAST_sig(aCfD)
FAST_po(cvt_i2f_z)
FAST_po(cvt_i2f_n)
FAST_gis


FAST_sig(aCgE)
FAST_po2(fsca, fsca_table)
FAST_gis

FAST_sig(eDbC)
FAST_po(cvt_f2i_t)
FAST_gis

FAST_sig(gJgHgH)
FAST_po(ftrv)
FAST_gis

FAST_sig(aC)
FAST_po2(pref, f1)
FAST_po2(pref, f2)
FAST_gis

FAST_sig(aB)
FAST_po2(pref, f1)
FAST_po2(pref, f2)
FAST_gis

FAST_sig(vV)
FAST_po(sync_sr)
FAST_po(sync_fpscr)
FAST_gis

FAST_sig(gJgJgJgJ)
FAST_po(frswap)
FAST_gis


typedef opcodeExec*(*foas)(const CC_pars_t& prms, void* fun, shil_opcode* opcode);

std::string getCTN(foas code);

template <typename CTR>
opcodeExec* createType(const CC_pars_t& prms, void* fun, shil_opcode* opcode) {

	auto frv = createType_fast<CTR>(prms, fun, opcode);
	if (frv)
		return frv;

	if (!funs.count(fun)) {
		funs[fun] = funs_id_count++;

		INFO_LOG(DYNAREC, "DEFINE %s: FAST_po(%s)", getCTN(&createType<CTR>).c_str(), shil_opcode_name(opcode->op));
	}

	typedef typename CTR::opex thetype;
	auto rv = new thetype();

	rv->setup(prms, fun);
	return rv;
}

std::map<std::string, foas> unmap = {
	{ "aBaCbC", &createType_fast<opcode_cc_aBaCbC> },
	{ "aCaCbC", &createType<opcode_cc_aCaCbC> },
	{ "aCaBbC", &createType<opcode_cc_aCaBbC> },
	{ "aCbC", &createType<opcode_cc_aCbC> },
	{ "aC", &createType<opcode_cc_aC> },
	{ "aB", &createType<opcode_cc_aB> },

	{ "eDeDeDfD", &createType<opcode_cc_eDeDeDfD> },
	{ "eDeDfD", &createType<opcode_cc_eDeDfD> },

	{ "aCaCaCbC", &createType<opcode_cc_aCaCaCbC> },
	{ "aCaCcCdC", &createType<opcode_cc_aCaCcCdC> },
	{ "aCaBcCdC", &createType<opcode_cc_aCaBcCdC> },
	{ "aBaCcCdC", &createType<opcode_cc_aBaCcCdC> },
	{ "aCaCaCcCdC", &createType<opcode_cc_aCaCaCcCdC> },
	{ "aCaBaCcCdC", &createType<opcode_cc_aCaBaCcCdC> },
	{ "aBaCaCcCdC", &createType<opcode_cc_aBaCaCcCdC> },
	{ "aCaBaBcCdC", &createType<opcode_cc_aCaBaBcCdC> },
	{ "aBaBaBcCdC", &createType<opcode_cc_aBaBaBcCdC> },

	{ "eDbC", &createType<opcode_cc_eDbC> },
	{ "aCfD", &createType<opcode_cc_aCfD> },

	{ "eDeDbC", &createType<opcode_cc_eDeDbC> },
	{ "eDfD", &createType<opcode_cc_eDfD> },
	{ "eBeDfD", &createType<opcode_cc_eBeDfD> },

	{ "aCgE", &createType<opcode_cc_aCgE> },
	{ "gJgHgH", &createType<opcode_cc_gJgHgH> },
	{ "gHgHfD", &createType<opcode_cc_gHgHfD> },
	{ "gJgJgJgJ", &createType<opcode_cc_gJgJgJgJ> },
	{ "vV", &createType<opcode_cc_vV> },
};

std::string getCTN(foas f) {
	auto it = find_if(unmap.begin(), unmap.end(), [f](const std::map<std::string, foas>::value_type& s) { return s.second == f; });

	return it->first;
}

#define CODE_ENTRY_COUNT 16384

struct {
	fnblock_base* fnb;
	void(*runner)(fnblock_base* fnb);
} dispatchb[CODE_ENTRY_COUNT];

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


DynarecCodeEntryPtr FNS[] = { REP_8192(0, &disaptchn), REP_8192(8192, &disaptchn) };

DynarecCodeEntryPtr getndpn_forreal(int n) {
	if (n >= CODE_ENTRY_COUNT)
		return 0;
	else
		return FNS[n];
}

typedef fnrv(*FNAFB)(int cycles);

FNAFB FNA[] = { REP_512(1, &fnnCtor) };

FNAFB fnnCtor_forreal(size_t n) {
	verify(n > 0);
	verify(n <= 512);
	return FNA[n - 1];
}

class BlockCompiler {
	const u32 *get_reg_or_imm(const shil_param& param)
	{
		return param.is_imm() ? &param._imm : param.reg_ptr();
	}

	size_t opcode_index;
	opcodeExec** ptrsg;

public:
	void compile(RuntimeBlockInfo* block, bool smc_checks, bool reset, bool staging, bool optimise)
	{
		//we need an extra one for the end opcode and optionally one more for block check
		auto ptrs = fnnCtor_forreal(block->oplist.size() + 1 + (smc_checks ? 1 : 0))(block->guest_cycles);

		ptrsg = ptrs.ptrs;

		dispatchb[idxnxx].fnb = ptrs.fnb;
		dispatchb[idxnxx].runner = ptrs.runner;

		block->code = getndpn_forreal(idxnxx++);

		if (getndpn_forreal(idxnxx) == 0) {
			emit_Skip(emit_FreeSpace()-16);
		}

		size_t i = 0;
		if (smc_checks)
		{
			opcodeExec* op;
			switch (block->sh4_code_size)
			{
			case 4:
				op = (new opcode_check_block<4>())->setup(block);
				break;
			case 6:
				op = (new opcode_check_block<6>())->setup(block);
				break;
			case 8:
				op = (new opcode_check_block<8>())->setup(block);
				break;
			default:
				op = (new opcode_check_block<-1>())->setup(block);
				break;
			}
			ptrs.ptrs[i++] = op;
		}

		for (size_t opnum = 0; opnum < block->oplist.size(); opnum++, i++) {
			opcode_index = i;
			shil_opcode& op = block->oplist[opnum];
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
			
			case shop_jcond:
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
						auto opc = new opcode_writem_imm<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_imm<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_imm<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_imm<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.imm_value(); opc->src2 = get_reg_or_imm(op.rs2);
					}
				}
				else if (op.rs3.is_imm()) {
					if (size == 1)
					{
						auto opc = new opcode_writem_offs_imm<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_offs_imm<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_offs_imm<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_offs_imm<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = get_reg_or_imm(op.rs2);
					}
				}
				else if (op.rs3.is_reg()) {
					if (size == 1)
					{
						auto opc = new opcode_writem_offs<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_offs<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_offs<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_offs<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = get_reg_or_imm(op.rs2);
					}
				}
				else {
					verify(op.rs3.is_null());
					if (size == 1)
					{
						auto opc = new opcode_writem<1>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem<2>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem<4>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->src2 = get_reg_or_imm(op.rs2);
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem<8>(); ptrs.ptrs[i] = opc; opc->src = op.rs1.reg_ptr(); opc->src2 = get_reg_or_imm(op.rs2);
					}
				}
			}
			break;
			
			default:
				shil_chf[op.op](&op);
				break;
			}
		}

		//Block end opcode
		{
			opcodeExec* op;

			#define CASEWS(n) case n: op = (new opcode_blockend<n>())->setup(block); break

			switch (block->BlockType) {
				CASEWS(BET_StaticJump);
				CASEWS(BET_StaticCall);
				CASEWS(BET_StaticIntr);

				CASEWS(BET_DynamicJump);
				CASEWS(BET_DynamicCall);
				CASEWS(BET_DynamicRet);
				CASEWS(BET_DynamicIntr);

				CASEWS(BET_Cond_0);
				CASEWS(BET_Cond_1);
			}

			ptrs.ptrs[i] = op;
		}

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
		std::string nm = "";
		for (auto m : CC_pars) {
			nm += (char)(m.type + 'a');
			nm += (char)(m.prm->type + 'A');
		}
		if (!nm.size())
			nm = "vV";
		
		if (unmap.count(nm)) {
			ptrsg[opcode_index] = unmap[nm](CC_pars, ccfn, op);
		}
		else {
			ERROR_LOG(DYNAREC, "IMPLEMENT CC_CALL CLASS: %s", nm.c_str());
			ptrsg[opcode_index] = new opcodeDie();
		}
	}

};

BlockCompiler* compiler;

void ngen_Compile(RuntimeBlockInfo* block, bool smc_checks, bool reset, bool staging, bool optimise)
{
	verify(emit_FreeSpace() >= 16 * 1024);

	compiler = new BlockCompiler();


	compiler->compile(block, smc_checks, reset, staging, optimise);

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
	/* FIXME issues when block check fails -> delete current block/op
	for (int i = 0; i < CODE_ENTRY_COUNT && dispatchb[i].fnb != nullptr; i++)
	{
		delete dispatchb[i].fnb;
		dispatchb[i].fnb = nullptr;
	}
	*/
}

void ngen_HandleException(host_context_t &context)
{
	die("rec-cpp exceptions not supported");
}
#endif
