/*
	Ugly, hacky, bad code
	It decodes sh4 opcodes too
*/

#include "types.h"

#if FEAT_SHREC != DYNAREC_NONE

#include "decoder.h"
#include "shil.h"
#include "ngen.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_cycles.h"
#include "hw/sh4/modules/mmu.h"
#include "decoder_opcodes.h"
#include "cfg/option.h"

#define BLOCK_MAX_SH_OPS_SOFT 500
#define BLOCK_MAX_SH_OPS_HARD 511

static RuntimeBlockInfo* blk;
static Sh4Cycles cycleCounter;

static inline shil_param mk_imm(u32 immv)
{
	return shil_param(immv);
}

static inline shil_param mk_reg(Sh4RegType reg)
{
	return shil_param(reg);
}

static inline shil_param mk_regi(int reg)
{
	return mk_reg((Sh4RegType)reg);
}

static state_t state;

static void Emit(shilop op, shil_param rd = shil_param(), shil_param rs1 = shil_param(), shil_param rs2 = shil_param(),
		u32 size = 0, shil_param rs3 = shil_param(), shil_param rd2 = shil_param())
{
	shil_opcode sp;

	sp.size = size;
	sp.op = op;
	sp.rd = rd;
	sp.rd2 = rd2;
	sp.rs1 = rs1;
	sp.rs2 = rs2;
	sp.rs3 = rs3;
	sp.guest_offs = state.cpu.rpc - blk->vaddr;
	sp.delay_slot = state.cpu.is_delayslot;

	blk->oplist.push_back(sp);
}

static void dec_fallback(u32 op)
{
	shil_opcode opcd;
	opcd.op = shop_ifb;

	opcd.rs1 = shil_param(OpDesc[op]->NeedPC());

	opcd.rs2 = shil_param(state.cpu.rpc + 2);
	opcd.rs3 = shil_param(op);

	opcd.guest_offs = state.cpu.rpc - blk->vaddr;
	opcd.delay_slot = state.cpu.is_delayslot;
	blk->oplist.push_back(opcd);
}

static void dec_DynamicSet(u32 regbase,u32 offs=0)
{
	if (offs==0)
		Emit(shop_jdyn,reg_pc_dyn,mk_reg((Sh4RegType)regbase));
	else
		Emit(shop_jdyn,reg_pc_dyn,mk_reg((Sh4RegType)regbase),mk_imm(offs));
}

static void dec_End(u32 dst, BlockEndType flags, bool delaySlot)
{
	state.BlockType = flags;
	state.NextOp = delaySlot ? NDO_Delayslot : NDO_End;
	state.DelayOp = NDO_End;
	state.JumpAddr = dst;
	if (flags != BET_StaticCall && flags != BET_StaticJump)
		state.NextAddr = state.cpu.rpc + 2 + (delaySlot ? 2 : 0);
	else
		verify(state.JumpAddr != NullAddress);
}

#define SR_STATUS_MASK STATUS_MASK
#define SR_T_MASK 1

static u32 dec_jump_simm8(u32 op)
{
	return state.cpu.rpc + GetSImm8(op)*2 + 4;
}
static u32 dec_jump_simm12(u32 op)
{
	return state.cpu.rpc + GetSImm12(op)*2 + 4;
}
static u32 dec_set_pr()
{
	u32 retaddr=state.cpu.rpc + 4;
	Emit(shop_mov32,reg_pr,mk_imm(retaddr));
	return retaddr;
}
static void dec_write_sr(shil_param src)
{
	Emit(shop_and,mk_reg(reg_sr_status),src,mk_imm(SR_STATUS_MASK));
	Emit(shop_and,mk_reg(reg_sr_T),src,mk_imm(SR_T_MASK));
}
//bf <bdisp8>
sh4dec(i1000_1011_iiii_iiii)
{
	dec_End(dec_jump_simm8(op),BET_Cond_0,false);
}
//bf.s <bdisp8>
sh4dec(i1000_1111_iiii_iiii)
{
	blk->has_jcond=true;
	Emit(shop_jcond,reg_pc_dyn,reg_sr_T);
	dec_End(dec_jump_simm8(op),BET_Cond_0,true);
}
//bt <bdisp8>
sh4dec(i1000_1001_iiii_iiii)
{
	dec_End(dec_jump_simm8(op),BET_Cond_1,false);
}
//bt.s <bdisp8>
sh4dec(i1000_1101_iiii_iiii)
{
	blk->has_jcond=true;
	Emit(shop_jcond,reg_pc_dyn,reg_sr_T);
	dec_End(dec_jump_simm8(op),BET_Cond_1,true);
}
//bra <bdisp12>
sh4dec(i1010_iiii_iiii_iiii)
{
	dec_End(dec_jump_simm12(op),BET_StaticJump,true);
}
//braf <REG_N>
sh4dec(i0000_nnnn_0010_0011)
{
	u32 n = GetN(op);

	dec_DynamicSet(reg_r0+n,state.cpu.rpc + 4);
	dec_End(NullAddress, BET_DynamicJump, true);
}
//jmp @<REG_N>
sh4dec(i0100_nnnn_0010_1011)
{
	u32 n = GetN(op);

	dec_DynamicSet(reg_r0+n);
	dec_End(NullAddress, BET_DynamicJump, true);
}
//bsr <bdisp12>
sh4dec(i1011_iiii_iiii_iiii)
{
	dec_set_pr();
	dec_End(dec_jump_simm12(op), BET_StaticCall, true);
}
//bsrf <REG_N>
sh4dec(i0000_nnnn_0000_0011)
{
	u32 n = GetN(op);
	u32 retaddr=dec_set_pr();
	dec_DynamicSet(reg_r0+n,retaddr);
	dec_End(NullAddress, BET_DynamicCall, true);
}
//jsr @<REG_N>
sh4dec(i0100_nnnn_0000_1011) 
{
	u32 n = GetN(op);

	dec_set_pr();
	dec_DynamicSet(reg_r0+n);
	dec_End(NullAddress, BET_DynamicCall, true);
}
//rts
sh4dec(i0000_0000_0000_1011)
{
	dec_DynamicSet(reg_pr);
	dec_End(NullAddress, BET_DynamicRet, true);
}
//rte
sh4dec(i0000_0000_0010_1011)
{
	//TODO: Write SR, Check intr
	dec_write_sr(reg_ssr);
	Emit(shop_sync_sr);
	dec_DynamicSet(reg_spc);
	dec_End(NullAddress, BET_DynamicIntr, true);
}
//trapa #<imm>
sh4dec(i1100_0011_iiii_iiii)
{
	//TODO: ifb
	dec_fallback(op);
	dec_DynamicSet(reg_nextpc);
	dec_End(NullAddress, BET_DynamicJump, false);
}
//sleep
sh4dec(i0000_0000_0001_1011)
{
	//TODO: ifb
	dec_fallback(op);
	dec_DynamicSet(reg_nextpc);
	dec_End(NullAddress, BET_DynamicJump, false);
}

// illegal instruction
void dec_illegalOp(u32 op)
{
	INFO_LOG(DYNAREC, "illegal instuction %04x at pc %x", op, state.cpu.rpc);
	Emit(shop_illegal, shil_param(), mk_imm(state.cpu.rpc), mk_imm(state.cpu.is_delayslot));
	dec_DynamicSet(reg_nextpc);
	dec_End(NullAddress, BET_DynamicJump, false);
}

//ldc <REG_N>,SR
sh4dec(i0100_nnnn_0000_1110)
{
	dec_write_sr((Sh4RegType)(reg_r0 + GetN(op)));
	Emit(shop_sync_sr);
	if (!state.cpu.is_delayslot)
		dec_End(state.cpu.rpc + 2, BET_StaticIntr, false);
}

//ldc.l @<REG_N>+,SR
sh4dec(i0100_nnnn_0000_0111)
{
	shil_param rn = mk_regi(reg_r0 + GetN(op));
	state.info.has_readm = true;
	Emit(shop_readm, reg_temp, rn, shil_param(), 4);
	Emit(shop_add, rn, rn, mk_imm(4));
	dec_write_sr(reg_temp);
	Emit(shop_sync_sr);
	if (!state.cpu.is_delayslot)
		dec_End(state.cpu.rpc + 2, BET_StaticIntr, false);
}

//ldc.l <REG_N>,FPSCR
sh4dec(i0100_nnnn_0110_1010)
{
	Emit(shop_mov32, reg_fpscr, mk_regi(reg_r0 + GetN(op)));
	Emit(shop_sync_fpscr);
	if (!state.cpu.is_delayslot)
		dec_End(state.cpu.rpc + 2, BET_StaticJump, false);
}

//ldc.l @<REG_N>+,FPSCR
sh4dec(i0100_nnnn_0110_0110)
{
	shil_param rn = mk_regi(reg_r0 + GetN(op));
	state.info.has_readm = true;
	Emit(shop_readm, reg_fpscr, rn, shil_param(), 4);
	Emit(shop_add, rn, rn, mk_imm(4));
	Emit(shop_sync_fpscr);
	if (!state.cpu.is_delayslot)
		dec_End(state.cpu.rpc + 2, BET_StaticJump, false);
}

//stc.l SR,@-<REG_N>
sh4dec(i0100_nnnn_0000_0011)
{
	Emit(shop_mov32, reg_temp, reg_sr_status);
	Emit(shop_or, reg_temp, reg_temp, reg_sr_T);
	shil_param rn = mk_regi(reg_r0 + GetN(op));
	state.info.has_writem = true;
	Emit(shop_writem, shil_param(), rn, reg_temp, 4, mk_imm(-4));
	Emit(shop_add, rn, rn, mk_imm(-4));
}

// tas.b <REG_N>
sh4dec(i0100_nnnn_0001_1011)
{
	shil_param rn = mk_regi(reg_r0 + GetN(op));
	state.info.has_readm = true;
	state.info.has_writem = true;
	Emit(shop_readm, reg_temp, rn, shil_param(), 1);
	Emit(shop_seteq, reg_sr_T, reg_temp, mk_imm(0));
	Emit(shop_or, reg_temp, reg_temp, mk_imm(0x80));
	Emit(shop_writem, shil_param(), rn, reg_temp, 1);
}

//nop !
sh4dec(i0000_0000_0000_1001)
{
}

//fschg
sh4dec(i1111_0011_1111_1101)
{
	//fpscr.SZ is bit 20
	Emit(shop_xor,reg_fpscr,reg_fpscr,mk_imm(1<<20));
	state.cpu.FSZ64=!state.cpu.FSZ64;
}

//frchg
sh4dec(i1111_1011_1111_1101)
{
	Emit(shop_xor,reg_fpscr,reg_fpscr,mk_imm(1<<21));
	Emit(shop_mov32,reg_old_fpscr,reg_fpscr);
	shil_param rmn;//null param
	Emit(shop_frswap,regv_xmtrx,regv_fmtrx,regv_xmtrx,0,rmn,regv_fmtrx);
}

//rotcl
sh4dec(i0100_nnnn_0010_0100)
{
	u32 n = GetN(op);
	Sh4RegType rn=(Sh4RegType)(reg_r0+n);
	
	Emit(shop_rocl,rn,rn,reg_sr_T,0,shil_param(),reg_sr_T);
}

//rotcr
sh4dec(i0100_nnnn_0010_0101)
{
	u32 n = GetN(op);
	Sh4RegType rn=(Sh4RegType)(reg_r0+n);

	Emit(shop_rocr,rn,rn,reg_sr_T,0,shil_param(),reg_sr_T);
}

static const Sh4RegType SREGS[] =
{
	reg_mach,
	reg_macl,
	reg_pr,
	reg_sgr,
	NoReg,
	reg_fpul,
	reg_fpscr,
	NoReg,

	NoReg,
	NoReg,
	NoReg,
	NoReg,
	NoReg,
	NoReg,
	NoReg,
	reg_dbr,
};

static const Sh4RegType CREGS[] =
{
	reg_sr_status,
	reg_gbr,
	reg_vbr,
	reg_ssr,
	reg_spc,
	NoReg,
	NoReg,
	NoReg,

	reg_r0_Bank,
	reg_r1_Bank,
	reg_r2_Bank,
	reg_r3_Bank,
	reg_r4_Bank,
	reg_r5_Bank,
	reg_r6_Bank,
	reg_r7_Bank,
};

static void dec_param(DecParam p,shil_param& r1,shil_param& r2, u32 op)
{
	switch(p)
	{
		//constants
	case PRM_PC_D8_x2:
		r1=mk_imm((state.cpu.rpc+4)+(GetImm8(op)<<1));
		break;

	case PRM_PC_D8_x4:
		r1=mk_imm(((state.cpu.rpc+4)&0xFFFFFFFC)+(GetImm8(op)<<2));
		break;
	
	case PRM_ZERO:
		r1= mk_imm(0);
		break;

	case PRM_ONE:
		r1= mk_imm(1);
		break;

	case PRM_TWO:
		r1= mk_imm(2);
		break;

	case PRM_TWO_INV:
		r1= mk_imm(~2);
		break;

	case PRM_ONE_F32:
		r1= mk_imm(0x3f800000);
		break;

	//imms
	case PRM_SIMM8:
		r1=mk_imm(GetSImm8(op));
		break;
	case PRM_UIMM8:
		r1=mk_imm(GetImm8(op));
		break;

	//direct registers
	case PRM_R0:
		r1=mk_reg(reg_r0);
		break;

	case PRM_RN:
		r1=mk_regi(reg_r0+GetN(op));
		break;

	case PRM_RM:
		r1=mk_regi(reg_r0+GetM(op));
		break;

	case PRM_FRN_SZ:
		if (state.cpu.FSZ64)
		{
			int rx=GetN(op)/2;
			if (GetN(op)&1)
				rx+=regv_xd_0;
			else
				rx+=regv_dr_0;

			r1=mk_regi(rx);
			break;
		}
	case PRM_FRN:
		r1=mk_regi(reg_fr_0+GetN(op));
		break;

	case PRM_FRM_SZ:
		if (state.cpu.FSZ64)
		{
			int rx=GetM(op)/2;
			if (GetM(op)&1)
				rx+=regv_xd_0;
			else
				rx+=regv_dr_0;

			r1=mk_regi(rx);
			break;
		}
	case PRM_FRM:
		r1=mk_regi(reg_fr_0+GetM(op));
		break;

	case PRM_FPUL:
		r1=mk_regi(reg_fpul);
		break;

	case PRM_FPN:	//float pair, 3 bits
		r1=mk_regi(regv_dr_0+GetN(op)/2);
		break;

	case PRM_FVN:	//float quad, 2 bits
		r1=mk_regi(regv_fv_0+GetN(op)/4);
		break;

	case PRM_FVM:	//float quad, 2 bits
		r1=mk_regi(regv_fv_0+(GetN(op)&0x3));
		break;

	case PRM_XMTRX:	//float matrix, 0 bits
		r1=mk_regi(regv_xmtrx);
		break;

	case PRM_FRM_FR0:
		r1=mk_regi(reg_fr_0+GetM(op));
		r2=mk_regi(reg_fr_0);
		break;

	case PRM_SR_T:
		r1=mk_regi(reg_sr_T);
		break;

	case PRM_SR_STATUS:
		r1=mk_regi(reg_sr_status);
		break;

	case PRM_SREG:	//FPUL/FPSCR/MACH/MACL/PR/DBR/SGR
		r1=mk_regi(SREGS[GetM(op)]);
		break;
	case PRM_CREG:	//SR/GBR/VBR/SSR/SPC/<RM_BANK>
		r1=mk_regi(CREGS[GetM(op)]);
		break;
	
	//reg/imm reg/reg
	case PRM_RN_D4_x1:
	case PRM_RN_D4_x2:
	case PRM_RN_D4_x4:
		{
			u32 shft=p-PRM_RN_D4_x1;
			r1=mk_regi(reg_r0+GetN(op));
			r2=mk_imm(GetImm4(op)<<shft);
		}
		break;

	case PRM_RN_R0:
		r1=mk_regi(reg_r0+GetN(op));
		r2=mk_regi(reg_r0);
		break;

	case PRM_RM_D4_x1:
	case PRM_RM_D4_x2:
	case PRM_RM_D4_x4:
		{
			u32 shft=p-PRM_RM_D4_x1;
			r1=mk_regi(reg_r0+GetM(op));
			r2=mk_imm(GetImm4(op)<<shft);
		}
		break;

	case PRM_RM_R0:
		r1=mk_regi(reg_r0+GetM(op));
		r2=mk_regi(reg_r0);
		break;

	case PRM_GBR_D8_x1:
	case PRM_GBR_D8_x2:
	case PRM_GBR_D8_x4:
		{
			u32 shft=p-PRM_GBR_D8_x1;
			r1=mk_regi(reg_gbr);
			r2=mk_imm(GetImm8(op)<<shft);
		}
		break;

	default:
		die("Non-supported parameter used");
	}
}

#define MASK_N_M 0xF00F
#define MASK_N   0xF0FF
#define MASK_NONE   0xFFFF

#define DIV0U_KEY 0x0019
#define DIV0S_KEY 0x2007
#define DIV1_KEY 0x3004
#define ROTCL_KEY 0x4024

static Sh4RegType div_som_reg1;
static Sh4RegType div_som_reg2;
static Sh4RegType div_som_reg3;

static u32 MatchDiv32(u32 pc , Sh4RegType &reg1,Sh4RegType &reg2 , Sh4RegType &reg3)
{

	u32 v_pc=pc;
	u32 match=1;
	for (int i=0;i<32;i++)
	{
		u16 opcode=IReadMem16(v_pc);
		v_pc+=2;
		if ((opcode&MASK_N)==ROTCL_KEY)
		{
			if (reg1==NoReg)
				reg1=(Sh4RegType)GetN(opcode);
			else if (reg1!=(Sh4RegType)GetN(opcode))
				break;
			match++;
		}
		else
		{
			//printf("DIV MATCH BROKEN BY: %s\n",OpDesc[opcode]->diss);
			break;
		}
		
		opcode=IReadMem16(v_pc);
		v_pc+=2;
		if ((opcode&MASK_N_M)==DIV1_KEY)
		{
			if (reg2==NoReg)
				reg2=(Sh4RegType)GetM(opcode);
			else if (reg2!=(Sh4RegType)GetM(opcode))
				break;
			
			if (reg2==reg1)
				break;

			if (reg3==NoReg)
				reg3=(Sh4RegType)GetN(opcode);
			else if (reg3!=(Sh4RegType)GetN(opcode))
				break;
			
			if (reg3==reg1)
				break;

			match++;
		}
		else
			break;
	}
	
	return match;
}

static bool MatchDiv32u(u32 op,u32 pc)
{
	div_som_reg1 = NoReg;
	div_som_reg2 = NoReg;
	div_som_reg3 = NoReg;

	u32 match = MatchDiv32(pc + 2, div_som_reg1, div_som_reg2, div_som_reg3);

	return match == 65;
}

static bool MatchDiv32s(u32 op,u32 pc)
{
	u32 n = GetN(op);
	u32 m = GetM(op);

	div_som_reg1 = NoReg;
	div_som_reg2 = (Sh4RegType)m;
	div_som_reg3 = (Sh4RegType)n;

	u32 match = MatchDiv32(pc + 2, div_som_reg1, div_som_reg2, div_som_reg3);
	
	return match == 65;
}

static bool dec_generic(u32 op)
{
	DecMode mode;DecParam d;DecParam s;shilop natop;u32 e;
	if (OpDesc[op]->decode==0)
		return false;
	
	u64 inf=OpDesc[op]->decode;

	e=(u32)(inf>>32);
	mode=(DecMode)((inf>>24)&0xFF);
	d=(DecParam)((inf>>16)&0xFF);
	s=(DecParam)((inf>>8)&0xFF);
	natop=(shilop)((inf>>0)&0xFF);

	bool transfer_64=false;
	if (op>=0xF000)
	{
		state.info.has_fpu=true;
		if (state.cpu.FPR64) {
			// fallback to interpreter for double float ops
			// except fmov, flds and fsts that don't depend on PR
			if (((op & 0xf) < 6 || (op & 0xf) > 0xc)	// fmov
					&& (op & 0xef) != 0x0d)				// flds, flts
				return false;
		}

		if (state.cpu.FSZ64 && (d==PRM_FRN_SZ || d==PRM_FRM_SZ || s==PRM_FRN_SZ || s==PRM_FRM_SZ))
			transfer_64 = true;
	}

	shil_param rs1,rs2,rs3,rd;

	dec_param(s,rs2,rs3,op);
	dec_param(d,rs1,rs3,op);

	switch(mode)
	{
	case DM_ReadSRF:
		Emit(shop_mov32,rs1,reg_sr_status);
		Emit(shop_or,rs1,rs1,reg_sr_T);
		break;

	case DM_WriteTOp:
		Emit(natop,reg_sr_T,rs1,rs2);
		break;

	case DM_DT:
		verify(natop==shop_sub);
		Emit(natop,rs1,rs1,rs2);
		Emit(shop_seteq,mk_reg(reg_sr_T),rs1,mk_imm(0));
		break;

	case DM_Shift:
		if (natop==shop_shl && e==1)
			Emit(shop_shr,mk_reg(reg_sr_T),rs1,mk_imm(31));
		else if (e==1)
			Emit(shop_and,mk_reg(reg_sr_T),rs1,mk_imm(1));

		Emit(natop,rs1,rs1,mk_imm(e));
		break;

	case DM_Rot:
		if (!(((s32)e>=0?e:-e)&0x1000))
		{
			if ((s32)e<0)
			{
				//left rotate
				Emit(shop_shr,mk_reg(reg_sr_T),rs2,mk_imm(31));
				e=-e;
			}
			else
			{
				//right rotate
				Emit(shop_and,mk_reg(reg_sr_T),rs2,mk_imm(1));
			}
		}
		e&=31;

		Emit(natop,rs1,rs2,mk_imm(e));
		break;

	case DM_BinaryOp://d=d op s
		if (e&1)
			Emit(natop,rs1,rs1,rs2,0,rs3);
		else
			Emit(natop,shil_param(),rs1,rs2,0,rs3);
		break;

	case DM_UnaryOp: //d= op s
		if (transfer_64 && natop==shop_mov32) 
			natop=shop_mov64;

		if (natop==shop_cvt_i2f_n && state.cpu.RoundToZero)
			natop=shop_cvt_i2f_z;

		if (e&1)
			Emit(natop,shil_param(),rs1);
		else
			Emit(natop,rs1,rs2);
		break;

	case DM_WriteM: //write(d,s)
		{
			//0 has no effect, so get rid of it
			if (rs3.is_imm() && rs3._imm==0)
				rs3=shil_param();

			state.info.has_writem=true;
			if (transfer_64) e=(s32)e*2;
			bool update_after=false;
			if ((s32)e<0)
			{
				if (rs1._reg!=rs2._reg && !mmu_enabled()) //reg shouldn't be updated if its written
				{
					Emit(shop_sub,rs1,rs1,mk_imm(-e));
				}
				else
				{
					verify(rs3.is_null());
					rs3=mk_imm(e);
					update_after=true;
				}
			}

			Emit(shop_writem,shil_param(),rs1,rs2,(s32)e<0?-e:e,rs3);

			if (update_after)
			{
				Emit(shop_sub,rs1,rs1,mk_imm(-e));
			}
		}
		break;

	case DM_ReadM:
		//0 has no effect, so get rid of it
		if (rs3.is_imm() && rs3._imm==0)
				rs3=shil_param();

		state.info.has_readm=true;
		if (transfer_64) e=(s32)e*2;

		Emit(shop_readm,rs1,rs2,shil_param(),(s32)e<0?-e:e,rs3);
		if ((s32)e<0)
		{
			if (rs1._reg!=rs2._reg)//the reg shouldn't be updated if it was just read.
				Emit(shop_add,rs2,rs2,mk_imm(-e));
		}
		break;

	case DM_fiprOp:
		{
			shil_param rdd=mk_regi(rs1._reg+3);
			Emit(natop,rdd,rs1,rs2);
		}
		break;

	case DM_EXTOP:
		{
			Emit(natop,rs1,rs2,mk_imm(e==1?0xFF:0xFFFF));
		}
		break;
	
	case DM_MUL:
		{
			shilop op;
			shil_param rd=mk_reg(reg_macl);
			shil_param rd2=shil_param();

			switch((s32)e)
			{
				case 16:  op=shop_mul_u16; break;
				case -16: op=shop_mul_s16; break;

				case -32: op=shop_mul_i32; break;

				case 64:  op=shop_mul_u64; rd2 = mk_reg(reg_mach); break;
				case -64: op=shop_mul_s64; rd2 = mk_reg(reg_mach); break;

				default:
					die("DM_MUL: Failed to classify opcode");
					return false;
			}

			Emit(op,rd,rs1,rs2,0,shil_param(),rd2);
		}
		break;

	case DM_DIV0:
		{
			if (e==1)
			{
				if (MatchDiv32u(op,state.cpu.rpc))
				{
					verify(!state.cpu.is_delayslot);
					//div32u
					Emit(shop_div32u, mk_reg(div_som_reg1), mk_reg(div_som_reg1), mk_reg(div_som_reg2), 0, mk_reg(div_som_reg3), mk_reg(div_som_reg3));
					
					Emit(shop_and, mk_reg(reg_sr_T), mk_reg(div_som_reg1), mk_imm(1));
					Emit(shop_shr, mk_reg(div_som_reg1), mk_reg(div_som_reg1), mk_imm(1));

					Emit(shop_div32p2, mk_reg(div_som_reg3), mk_reg(div_som_reg3), mk_reg(div_som_reg2), 0, mk_reg(reg_sr_T));
					
					for (int i = 1; i <= 64; i++)
					{
						u16 op = IReadMem16(state.cpu.rpc + i * 2);
						blk->guest_cycles += cycleCounter.countCycles(op);
					}
					//skip the aggregated opcodes
					state.cpu.rpc += 128;
				}
				else
				{
					//clear QM (bits 8,9)
					u32 qm=(1<<8)|(1<<9);
					Emit(shop_and,mk_reg(reg_sr_status),mk_reg(reg_sr_status),mk_imm(~qm));
					//clear T !
					Emit(shop_mov32,mk_reg(reg_sr_T),mk_imm(0));
				}
			}
			else
			{
				if (MatchDiv32s(op,state.cpu.rpc))
				{
					verify(!state.cpu.is_delayslot);
					//div32s
					Emit(shop_xor, mk_reg(reg_sr_T), mk_reg(div_som_reg3), mk_reg(div_som_reg2));	// get quotient sign
					Emit(shop_and, mk_reg(reg_sr_T), mk_reg(reg_sr_T), mk_imm(1 << 31));			// isolate sign bit

					Emit(shop_div32s, mk_reg(div_som_reg1), mk_reg(div_som_reg1), mk_reg(div_som_reg2), 0, mk_reg(div_som_reg3), mk_reg(div_som_reg3));

					Emit(shop_and, mk_reg(reg_temp), mk_reg(div_som_reg1), mk_imm(1));				// set quotient lsb in temp reg
					Emit(shop_sar, mk_reg(div_som_reg1), mk_reg(div_som_reg1), mk_imm(1));			// shift quotient right
					Emit(shop_or, mk_reg(reg_sr_T), mk_reg(reg_sr_T), mk_reg(reg_temp));			// store quotient lsb in T

					Emit(shop_div32p2, mk_reg(div_som_reg3), mk_reg(div_som_reg3), mk_reg(div_som_reg2), 0, mk_reg(reg_sr_T));
					
					Emit(shop_and, mk_reg(reg_sr_T), mk_reg(reg_sr_T), mk_imm(1));					// clean up T

					for (int i = 1; i <= 64; i++)
					{
						u16 op = IReadMem16(state.cpu.rpc + i * 2);
						blk->guest_cycles += cycleCounter.countCycles(op);
					}
					//skip the aggregated opcodes
					state.cpu.rpc += 128;
				}
				else
				{
					//Clear Q & M
					Emit(shop_and, mk_reg(reg_sr_status), mk_reg(reg_sr_status), mk_imm(~((1 << 8) | (1 << 9))));

					//sr.Q=r[n]>>31;
					Emit(shop_sar, mk_reg(reg_sr_T),rs1,mk_imm(31));
					Emit(shop_and, mk_reg(reg_sr_T), mk_reg(reg_sr_T), mk_imm(1 << 8));
					Emit(shop_or, mk_reg(reg_sr_status), mk_reg(reg_sr_status), mk_reg(reg_sr_T));

					//sr.M=r[m]>>31;
					Emit(shop_sar, mk_reg(reg_sr_T), rs2, mk_imm(31));
					Emit(shop_and, mk_reg(reg_sr_T), mk_reg(reg_sr_T), mk_imm(1 << 9));
					Emit(shop_or, mk_reg(reg_sr_status), mk_reg(reg_sr_status), mk_reg(reg_sr_T));

					//sr.T=sr.M^sr.Q;
					Emit(shop_xor, mk_reg(reg_sr_T), rs1, rs2);
					Emit(shop_shr, mk_reg(reg_sr_T), mk_reg(reg_sr_T), mk_imm(31));
				}
			}
		}
		break;

	case DM_ADC:
		{
			Emit(natop,rs1,rs1,rs2,0,mk_reg(reg_sr_T),mk_reg(reg_sr_T));
		}
		break;

	case DM_NEGC:
		Emit(natop, rs1, rs2, mk_reg(reg_sr_T), 0, shil_param(), mk_reg(reg_sr_T));
		break;

	default:
		verify(false);
	}

	return true;
}

static void state_Setup(u32 rpc,fpscr_t fpu_cfg)
{
	state.cpu.rpc=rpc;
	state.cpu.is_delayslot=false;
	state.cpu.FPR64=fpu_cfg.PR;
	state.cpu.FSZ64=fpu_cfg.SZ;
	state.cpu.RoundToZero=fpu_cfg.RM==1;
	//verify(fpu_cfg.RM<2);	// Happens with many wince games (set to 3)
	//what about fp/fs ?

	state.NextOp = NDO_NextOp;
	state.BlockType = BET_SCL_Intr;
	state.JumpAddr = NullAddress;
	state.NextAddr = NullAddress;

	state.info.has_readm=false;
	state.info.has_writem=false;
	state.info.has_fpu=false;
}

void dec_updateBlockCycles(RuntimeBlockInfo *block, u16 op)
{
	block->guest_cycles += cycleCounter.countCycles(op);
}

bool dec_DecodeBlock(RuntimeBlockInfo* rbi,u32 max_cycles)
{
	blk=rbi;
	state_Setup(blk->vaddr, blk->fpu_cfg);
	
	blk->guest_opcodes = 0;
	cycleCounter.reset();
	// If full MMU, don't allow the block to extend past the end of the current 4K page
	u32 max_pc = mmu_enabled() ? ((state.cpu.rpc >> 12) + 1) << 12 : 0xFFFFFFFF;
	
	for(;;)
	{
		switch(state.NextOp)
		{
		case NDO_Delayslot:
			state.NextOp=state.DelayOp;
			state.cpu.is_delayslot=true;
			//there is no break here by design
		case NDO_NextOp:
			{
				if ((blk->oplist.size() >= BLOCK_MAX_SH_OPS_SOFT || blk->guest_cycles >= max_cycles || state.cpu.rpc >= max_pc)
						&& !state.cpu.is_delayslot)
				{
					dec_End(state.cpu.rpc,BET_StaticJump,false);
				}
				else
				{
					u32 op = IReadMem16(state.cpu.rpc);

					blk->guest_opcodes++;
					dec_updateBlockCycles(blk, op);

					if (OpDesc[op]->IsFloatingPoint())
					{
						if (sr.FD == 1)
						{
							// We need to know FPSCR to compile the block, so let the exception handler run first
							// as it may change the fp registers
							Do_Exception(next_pc, Sh4Ex_FpuDisabled);
							return false;
						}
						blk->has_fpu_op = true;
					}

					if (state.cpu.is_delayslot && OpDesc[op]->SetPC())
						throw FlycastException("Fatal: SH4 branch instruction in delay slot");
					if (!OpDesc[op]->rec_oph)
					{
						if (!dec_generic(op))
						{
							dec_fallback(op);
							if (OpDesc[op]->SetPC())
							{
								dec_DynamicSet(reg_nextpc);
								dec_End(NullAddress, BET_DynamicJump, false);
							}
							else if (OpDesc[op]->SetFPSCR() && !state.cpu.is_delayslot)
							{
								dec_End(state.cpu.rpc + 2, BET_StaticJump, false);
							}
						}
					}
					else
					{
						OpDesc[op]->rec_oph(op);
					}
					state.cpu.rpc+=2;
				}
			}
			break;

		case NDO_End:
			// Disabled for now since we need to know if the block is read-only,
			// which isn't determined until after the decoding.
			// This is a relatively rare optimization anyway
#if 0
			// detect if calling an empty subroutine and skip it
			if (state.BlockType == BET_StaticCall && blk->read_only)
			{
				if ((state.JumpAddr >> 12) == (blk->vaddr >> 12)
						|| (state.JumpAddr >> 12) == ((blk->vaddr + (blk->guest_opcodes - 1) * 2) >> 12))
				{
					u32 op = IReadMem16(state.JumpAddr);
					if (op == 0x000B)	// rts
					{
						u16 delayOp = IReadMem16(state.JumpAddr + 2);
						if (delayOp == 0x0000 || delayOp == 0x0009)	// nop
						{
							state.NextOp = NDO_NextOp;
							state.cpu.is_delayslot = false;
							dec_updateBlockCycles(blk, op);
							dec_updateBlockCycles(blk, delayOp);
							continue;
						}
					}
				}
			}
#endif
			goto _end;
		}
	}

_end:
	blk->sh4_code_size=state.cpu.rpc-blk->vaddr;
	blk->NextBlock=state.NextAddr;
	blk->BranchBlock=state.JumpAddr;
	blk->BlockType=state.BlockType;

	verify(blk->oplist.size() <= BLOCK_MAX_SH_OPS_HARD);
	
	blk->guest_cycles = std::round(blk->guest_cycles * 200.f / std::max(1.f, (float)config::Sh4Clock));

	//make sure we don't use wayy-too-few cycles
	blk->guest_cycles = std::max(1U, blk->guest_cycles);
	blk = nullptr;

	return true;
}

#endif
