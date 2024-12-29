#pragma once
#include "hw/sh4/sh4_if.h"

struct shil_opcode;
typedef void shil_chfp(shil_opcode* op);
extern shil_chfp* shil_chf[];

enum Sh4RegType
{
	//GPRs
	reg_r0,
	reg_r1,
	reg_r2,
	reg_r3,
	reg_r4,
	reg_r5,
	reg_r6,
	reg_r7,
	reg_r8,
	reg_r9,
	reg_r10,
	reg_r11,
	reg_r12,
	reg_r13,
	reg_r14,
	reg_r15,

	//FPU, bank 0
	reg_fr_0,
	reg_fr_1,
	reg_fr_2,
	reg_fr_3,
	reg_fr_4,
	reg_fr_5,
	reg_fr_6,
	reg_fr_7,
	reg_fr_8,
	reg_fr_9,
	reg_fr_10,
	reg_fr_11,
	reg_fr_12,
	reg_fr_13,
	reg_fr_14,
	reg_fr_15,

	//FPU, bank 1
	reg_xf_0,
	reg_xf_1,
	reg_xf_2,
	reg_xf_3,
	reg_xf_4,
	reg_xf_5,
	reg_xf_6,
	reg_xf_7,
	reg_xf_8,
	reg_xf_9,
	reg_xf_10,
	reg_xf_11,
	reg_xf_12,
	reg_xf_13,
	reg_xf_14,
	reg_xf_15,

	//GPR Interrupt bank
	reg_r0_Bank,
	reg_r1_Bank,
	reg_r2_Bank,
	reg_r3_Bank,
	reg_r4_Bank,
	reg_r5_Bank,
	reg_r6_Bank,
	reg_r7_Bank,

	//Misc regs
	reg_gbr,
	reg_ssr,
	reg_spc,
	reg_sgr,
	reg_dbr,
	reg_vbr,
	reg_mach,
	reg_macl,
	reg_pr,
	reg_fpul,
	reg_nextpc,
	reg_sr_status,     //Only the status bits
	reg_sr_T,          //Only T
	reg_old_fpscr,
	reg_fpscr,

	reg_pc_dyn,        //Write only, for dynarec only (dynamic block exit address)
	reg_temp,

	sh4_reg_count,

	/*
		These are virtual registers, used by the dynarec decoder
	*/
	regv_dr_0,
	regv_dr_2,
	regv_dr_4,
	regv_dr_6,
	regv_dr_8,
	regv_dr_10,
	regv_dr_12,
	regv_dr_14,

	regv_xd_0,
	regv_xd_2,
	regv_xd_4,
	regv_xd_6,
	regv_xd_8,
	regv_xd_10,
	regv_xd_12,
	regv_xd_14,

	regv_fv_0,
	regv_fv_4,
	regv_fv_8,
	regv_fv_12,

	regv_xmtrx,
	regv_fmtrx,

	reg_sq_buffer,

	NoReg=-1
};

u32 getRegOffset(Sh4RegType reg);
u32* GetRegPtr(Sh4Context& ctx, u32 reg);

enum shil_param_type
{
	FMT_NULL,
	FMT_IMM,
	FMT_I32,
	FMT_F32,
	FMT_F64,
	
	FMT_V4,
	FMT_V16,

	FMT_REG_BASE = FMT_I32,
	FMT_VECTOR_BASE = FMT_V4,

	FMT_MASK = 0xFFFF,
};

/*
	formats : 16u 16s 32u 32s, 32f, 64f
	param types: r32, r64
*/

#define SHIL_MODE 0
#include "shil_canonical.h"

struct shil_param
{
	shil_param()
	{
		type = FMT_NULL;
		_imm = 0xFFFFFFFF;
		memset(version, 0, sizeof(version));
	}

	shil_param(u32 imm)
	{
		this->type = FMT_IMM;
		_imm = imm;
		memset(version, 0, sizeof(version));
	}

	shil_param(Sh4RegType reg)
	{
		if (reg >= reg_fr_0 && reg <= reg_xf_15)
		{
			type = FMT_F32;
			_imm = reg;
		}
		else if (reg >= regv_dr_0 && reg <= regv_dr_14)
		{
			type = FMT_F64;
			_imm = (reg - regv_dr_0) * 2 + reg_fr_0;
		}
		else if (reg >= regv_xd_0 && reg <= regv_xd_14)
		{
			type = FMT_F64;
			_imm = (reg - regv_xd_0) * 2 + reg_xf_0;
		}
		else if (reg >= regv_fv_0 && reg <= regv_fv_12)
		{
			type = FMT_V4;
			_imm = (reg - regv_fv_0) * 4 + reg_fr_0;
		}
		else if (reg == regv_xmtrx)
		{
			type = FMT_V16;
			_imm = reg_xf_0;
		}
		else if (reg == regv_fmtrx)
		{
			type = FMT_V16;
			_imm = reg_fr_0;
		}
		else
		{
			type = FMT_I32;
			_reg = reg;
		}
		memset(version, 0, sizeof(version));
	}
	union
	{
		u32 _imm;
		Sh4RegType _reg;
	};
	u32 type;
	u16 version[16];

	bool is_null() const { return type==FMT_NULL; }
	bool is_imm() const { return type==FMT_IMM; }
	bool is_reg() const { return type>=FMT_REG_BASE; }

	bool is_r32i() const { return type==FMT_I32; }
	bool is_r32f() const { return type==FMT_F32; }
	u32 is_r32fv() const { return type >= FMT_VECTOR_BASE ? count() : 0; }
	bool is_r64f() const { return type==FMT_F64; }

	bool is_r32() const { return is_r32i() || is_r32f(); }

	bool is_imm_s8() const { return is_imm() && (int8_t)_imm == (int32_t)_imm; }

	u32* reg_ptr(Sh4Context& ctx) const { verify(is_reg()); return GetRegPtr(ctx, _reg); }
	u32  reg_offset() const { verify(is_reg()); return getRegOffset(_reg); }
	s32  reg_nofs() const { verify(is_reg()); return (int)getRegOffset(_reg) - sizeof(Sh4Context); }

	u32 imm_value() const { verify(is_imm()); return _imm; }

	u32 count() const { return  type == FMT_F64 ? 2 :
								type == FMT_V4 ? 4 :
								type == FMT_V16 ? 16 : 1; }	//count of hardware regs

	/*	
		Imms:
		is_imm
		
		regs:
		integer regs            : is_r32i,is_r32,count=1
		fpu regs, single view   : is_r32f,is_r32,count=1
		fpu regs, double view   : is_r64f,count=2
		fpu regs, quad view     : is_r32fv=4, count=4
		fpu regs, matrix view   : is_r32fv=16, count=16
	*/
};

struct shil_opcode
{
	shilop op;
	u32 size;	// memory access size

	shil_param rd, rd2;
	shil_param rs1, rs2, rs3;

	u16 host_offs;
	u16 guest_offs;
	bool delay_slot;

    std::string dissasm() const;
};

const char* shil_opcode_name(int op);

std::string name_reg(Sh4RegType reg);
