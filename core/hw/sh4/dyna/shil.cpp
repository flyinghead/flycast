#include "types.h"
#if FEAT_SHREC != DYNAREC_NONE
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_mmr.h"
#include "ngen.h"
#include "ssa.h"
#include <sstream>
#include <locale>

void AnalyseBlock(RuntimeBlockInfo* blk)
{
	SSAOptimizer optim(blk);
	optim.Optimize();
}

u32 getRegOffset(Sh4RegType reg)
{
	if (reg >= reg_r0 && reg <= reg_r15) {
		const size_t regofs = (reg - reg_r0) * sizeof(u32);
		return offsetof(Sh4Context, r[0]) + regofs;
	}
	if (reg >= reg_r0_Bank && reg <= reg_r7_Bank) {
		const size_t regofs = (reg - reg_r0_Bank) * sizeof(u32);
		return offsetof(Sh4Context, r_bank[0]) + regofs;
	}
	if (reg >= reg_fr_0 && reg <= reg_fr_15) {
		const size_t regofs = (reg - reg_fr_0) * sizeof(float);
		return offsetof(Sh4Context, fr[0]) + regofs;
	}
	if (reg >= reg_xf_0 && reg <= reg_xf_15) {
		const size_t regofs = (reg - reg_xf_0) * sizeof(float);
		return offsetof(Sh4Context, xf[0]) + regofs;
	}
	switch (reg)
	{
	case reg_gbr: return offsetof(Sh4Context, gbr);
	case reg_vbr: return offsetof(Sh4Context, vbr);
	case reg_ssr: return offsetof(Sh4Context, ssr);
	case reg_spc: return offsetof(Sh4Context, spc);
	case reg_sgr: return offsetof(Sh4Context, sgr);
	case reg_dbr: return offsetof(Sh4Context, dbr);
	case reg_mach: return offsetof(Sh4Context, mac.h);
	case reg_macl: return offsetof(Sh4Context, mac.l);
	case reg_pr: return offsetof(Sh4Context, pr);
	case reg_fpul: return offsetof(Sh4Context, fpul);
	case reg_nextpc: return offsetof(Sh4Context, pc);
	case reg_sr_status: return offsetof(Sh4Context, sr.status);
	case reg_sr_T: return offsetof(Sh4Context, sr.T);
	case reg_old_fpscr: return offsetof(Sh4Context, old_fpscr.full);
	case reg_fpscr: return offsetof(Sh4Context, fpscr.full);
	case reg_pc_dyn: return offsetof(Sh4Context, jdyn);
	case reg_temp: return offsetof(Sh4Context, temp_reg);
	case reg_sq_buffer: return offsetof(Sh4Context, sq_buffer);
	default:
		ERROR_LOG(SH4, "Unknown register ID %d", reg);
		die("Invalid reg");
		return 0;
	}
}

u32* GetRegPtr(Sh4Context& ctx, u32 reg)
{
	return (u32 *)((u8 *)&ctx + getRegOffset((Sh4RegType)reg));
}

std::string name_reg(Sh4RegType reg)
{
	std::ostringstream ss;
	ss.imbue(std::locale::classic());

	if (reg >= reg_fr_0 && reg <= reg_xf_15)
		ss << "f" << (reg - reg_fr_0);
	else if (reg <= reg_r15)
		ss << "r" << reg;
	else if (reg <= reg_r7_Bank)
		ss << "r" << (reg - reg_r0_Bank) << "b";
	else
	{
		switch (reg)
		{
		case reg_sr_T:
			ss << "sr.T";
			break;
		case reg_fpscr:
			ss << "fpscr";
			break;
		case reg_sr_status:
			ss << "sr";
			break;
		case reg_pc_dyn:
			ss << "pc_dyn";
			break;
		case reg_macl:
			ss << "macl";
			break;
		case reg_mach:
			ss << "mach";
			break;
		case reg_pr:
			ss << "pr";
			break;
		case reg_gbr:
			ss << "gbr";
			break;
		case reg_nextpc:
			ss << "pc";
			break;
		case reg_fpul:
			ss << "fpul";
			break;
		case reg_old_fpscr:
			ss << "old_fpscr";
			break;
		case reg_ssr:
			ss << "ssr";
			break;
		case reg_temp:
			ss << "temp";
			break;
		default:
			ss << "s" << reg;
			break;
		}
	}

	return ss.str();
}

static std::string dissasm_param(const shil_param& prm, bool comma)
{
	std::ostringstream ss;
	ss.imbue(std::locale::classic());

	if (!prm.is_null() && comma)
			ss << ", ";

	if (prm.is_imm())
	{	
		if (prm.is_imm_s8())
			ss  << (s32)prm._imm ;
		else
			ss << "0x" << std::hex << prm._imm;
	}
	else if (prm.is_reg())
	{
		ss << name_reg(prm._reg);

		if (prm.count() > 1)
		{
			ss << "v" << prm.count();
		}
		ss << "." << prm.version[0];
	}

	return ss.str();
}

#include "hw/sh4/sh4_core.h"

#define SHIL_MODE 1
#include "shil_canonical.h"

#define SHIL_MODE 4
#include "shil_canonical.h"

//#define SHIL_MODE 2
//#include "shil_canonical.h"

#if FEAT_SHREC != DYNAREC_NONE
#define SHIL_MODE 3
#include "shil_canonical.h"
#endif

std::string shil_opcode::dissasm() const
{
	std::ostringstream ss;
	ss.imbue(std::locale::classic());
	ss << shilop_str[op] << " " << dissasm_param(rd,false) << dissasm_param(rd2,true) << " <- " << dissasm_param(rs1,false) << dissasm_param(rs2,true) << dissasm_param(rs3,true);
	return ss.str();
}

const char* shil_opcode_name(int op)
{
	return shilop_str[op];
}

#endif	// FEAT_SHREC != DYNAREC_NONE
