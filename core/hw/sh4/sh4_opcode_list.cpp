#include "types.h"
#include "interpr/sh4_opcodes.h"
#include "sh4_opcode_list.h"
#include "dyna/decoder.h"
#include "dyna/decoder_opcodes.h"
#include "hw/sh4/dyna/shil.h"
#include "reios/reios.h"

OpCallFP* OpPtr[0x10000];
sh4_opcodelistentry* OpDesc[0x10000];

#define Mask_n_m 0xF00F
#define Mask_n_m_imm4 0xF000
#define Mask_n 0xF0FF
#define Mask_none 0xFFFF
#define Mask_imm8 0xFF00
#define Mask_n_imm8 0xF000
#define Mask_n_ml3bit 0xF08F
#define Mask_nh3bit 0xF1FF
#define Mask_nh2bit 0xF3FF

static u64 dec_Fill(DecMode mode,DecParam d,DecParam s,shilop op,u32 extra=0)
{
	return (((u64)extra)<<32)|(mode<<24)|(d<<16)|(s<<8)|op;
}
static u64 dec_Un_rNrM(shilop op)
{
	return dec_Fill(DM_UnaryOp,PRM_RN,PRM_RM,op);
}
static u64 dec_Un_frNfrN(shilop op)
{
	return dec_Fill(DM_UnaryOp,PRM_FRN,PRM_FRN,op);
}
static u64 dec_Bin_frNfrM(shilop op, u32 haswrite=1)
{
	return dec_Fill(DM_BinaryOp,PRM_FRN,PRM_FRM,op,haswrite);
}
static u64 dec_Bin_rNrM(shilop op, u32 haswrite=1)
{
	return dec_Fill(DM_BinaryOp,PRM_RN,PRM_RM,op,haswrite);
}
static u64 dec_mul(u32 type)
{
	return dec_Fill(DM_MUL,PRM_RN,PRM_RM,shop_mul_s16,type);
}
static u64 dec_Bin_S8R(shilop op, u32 haswrite=1)
{
	return dec_Fill(DM_BinaryOp,PRM_RN,PRM_SIMM8,op,haswrite);
}
static u64 dec_Bin_r0u8(shilop op, u32 haswrite=1)
{
	return dec_Fill(DM_BinaryOp,PRM_R0,PRM_UIMM8,op,haswrite);
}
static u64 dec_shft(s32 offs,bool arithm)
{
	if (offs>0)
	{
		//left shift
		return dec_Fill(DM_Shift,PRM_RN,PRM_RN,shop_shl,offs);
	}
	else
	{
		return dec_Fill(DM_Shift,PRM_RN,PRM_RN,arithm?shop_sar:shop_shr,-offs);
	}
}

static u64 dec_cmp(shilop op, DecParam s1,DecParam s2)
{
	return dec_Fill(DM_WriteTOp,s1,s2,op);
}

static u64 dec_LD(DecParam d)  { return dec_Fill(DM_UnaryOp,d,PRM_RN,shop_mov32); }
static u64 dec_LDM(DecParam d) { return dec_Fill(DM_ReadM,d,PRM_RN,shop_readm,-4); }
static u64 dec_ST(DecParam d)  { return dec_Fill(DM_UnaryOp,PRM_RN,d,shop_mov32); }
static u64 dec_STSRF(DecParam d)   { return dec_Fill(DM_ReadSRF,PRM_RN,d,shop_mov32); }
static u64 dec_STM(DecParam d) { return dec_Fill(DM_WriteM,PRM_RN,d,shop_writem,-4); }

//d=reg to read into
static u64 dec_MRd(DecParam d,DecParam s,u32 sz) { return dec_Fill(DM_ReadM,d,s,shop_readm,sz); }
//d= reg to read from
static u64 dec_MWt(DecParam d,DecParam s,u32 sz) { return dec_Fill(DM_WriteM,d,s,shop_writem,sz); }

static sh4_opcodelistentry missing_opcode = {dec_illegalOp, iNotImplemented, 0, 0, ReadWritePC, "missing", 0, 0, CO, 1 };

static sh4_opcodelistentry opcodes[]=
{
	//HLE
	{0, reios_trap, Mask_none, REIOS_OPCODE, Branch_dir, "reios_trap", 100, 100, CO, 1 },
	//CPU
	{dec_i0000_nnnn_0010_0011   ,i0000_nnnn_0010_0011   ,Mask_n         ,0x0023 ,Branch_rel_d   ,"braf <REG_N>"                         ,2,3,CO,4},  //braf <REG_N>
	{dec_i0000_nnnn_0000_0011   ,i0000_nnnn_0000_0011   ,Mask_n         ,0x0003 ,Branch_rel_d   ,"bsrf <REG_N>"                         ,2,3,CO,24}, //bsrf <REG_N>
	{0                          ,i0000_nnnn_1100_0011   ,Mask_n         ,0x00C3 ,Normal         ,"movca.l R0, @<REG_N>"                 ,1,4,LS,12   ,dec_MWt(PRM_RN,PRM_R0,4)}, //movca.l R0, @<REG_N>
	{dec_i0000_0000_0000_1001   ,i0000_nnnn_1001_0011   ,Mask_n         ,0x0093 ,Normal         ,"ocbi @<REG_N>"                        ,1,2,LS,10}, //ocbi @<REG_N>
	{dec_i0000_0000_0000_1001   ,i0000_nnnn_1010_0011   ,Mask_n         ,0x00A3 ,Normal         ,"ocbp @<REG_N>"                        ,1,3,LS,11}, //ocbp @<REG_N>
	{dec_i0000_0000_0000_1001   ,i0000_nnnn_1011_0011   ,Mask_n         ,0x00B3 ,Normal         ,"ocbwb @<REG_N>"                       ,1,3,LS,11}, //ocbwb @<REG_N>
	{0                          ,i0000_nnnn_1000_0011   ,Mask_n         ,0x0083 ,Normal         ,"pref @<REG_N>"                        ,1,1,LS,2    ,dec_Fill(DM_UnaryOp,PRM_RN,PRM_ONE,shop_pref,1)},  //pref @<REG_N>
	{0                          ,i0000_nnnn_mmmm_0111   ,Mask_n_m       ,0x0007 ,Normal         ,"mul.l <REG_M>,<REG_N>"                ,2,4,CO,34   ,dec_mul(-32)}, //mul.l <REG_M>,<REG_N>
	{0                          ,i0000_0000_0010_1000   ,Mask_none      ,0x0028 ,Normal         ,"clrmac"                               ,1,3,CO,28}, //clrmac
	{0                          ,i0000_0000_0100_1000   ,Mask_none      ,0x0048 ,Normal         ,"clrs"                                 ,1,1,CO,1    ,dec_Fill(DM_BinaryOp, PRM_SR_STATUS, PRM_TWO_INV, shop_and, 1) }, //clrs
	{0                          ,i0000_0000_0000_1000   ,Mask_none      ,0x0008 ,Normal         ,"clrt"                                 ,1,1,MT,1    ,dec_Fill(DM_UnaryOp,PRM_SR_T,PRM_ZERO,shop_mov32)},    //clrt
	{0                          ,i0000_0000_0011_1000   ,Mask_none      ,0x0038 ,Normal         ,"ldtlb"                                ,1,1,CO,2}   ,//ldtlb
	{0                          ,i0000_0000_0101_1000   ,Mask_none      ,0x0058 ,Normal         ,"sets"                                 ,1,1,CO,1    ,dec_Fill(DM_BinaryOp, PRM_SR_STATUS, PRM_TWO, shop_or, 1) },  //sets
	{0                          ,i0000_0000_0001_1000   ,Mask_none      ,0x0018 ,Normal         ,"sett"                                 ,1,1,MT,1    ,dec_Fill(DM_UnaryOp,PRM_SR_T,PRM_ONE,shop_mov32)}, //sett
	{0                          ,i0000_0000_0001_1001   ,Mask_none      ,0x0019 ,Normal         ,"div0u"                                ,1,1,EX,1    ,dec_Fill(DM_DIV0,PRM_RN,PRM_RM,shop_or,1)},//div0u
	{0                          ,i0000_nnnn_0010_1001   ,Mask_n         ,0x0029 ,Normal         ,"movt <REG_N>"                         ,1,1,EX,1    ,dec_Fill(DM_UnaryOp,PRM_RN,PRM_SR_T,shop_mov32)},  //movt <REG_N>
	{dec_i0000_0000_0000_1001   ,i0000_0000_0000_1001   ,Mask_none      ,0x0009 ,Normal         ,"nop"                                  ,1,0,MT,1}   ,//nop
	{dec_i0000_0000_0000_1001   ,i0000_0000_0000_1001   ,Mask_none      ,0x0000 ,Normal         ,"nop0"                                 ,1,0,MT,1}   ,//nop0 Looney Tunes: Space Race, Samba de Amigo 2000


	{dec_i0000_0000_0010_1011   ,i0000_0000_0010_1011   ,Mask_none      ,0x002B ,Branch_dir_d   ,"rte"                                  ,5,5,CO,8},  //rte
	{dec_i0000_0000_0000_1011   ,i0000_0000_0000_1011   ,Mask_none      ,0x000B ,Branch_dir_d   ,"rts"                                  ,2,3,CO,4},  //rts
	{dec_i0000_0000_0001_1011   ,i0000_0000_0001_1011   ,Mask_none      ,0x001B ,ReadWritePC    ,"sleep"                                ,4,4,CO,9},  //sleep


	{0                          ,i0000_nnnn_mmmm_1111   ,Mask_n_m       ,0x000F ,Normal         ,"mac.l @<REG_M>+,@<REG_N>+"            ,2,4,CO,35}, //mac.l @<REG_M>+,@<REG_N>+

	{0                          ,i0010_nnnn_mmmm_0111   ,Mask_n_m       ,0x2007 ,Normal         ,"div0s <REG_M>,<REG_N>"                ,1,1,EX,1    ,dec_Fill(DM_DIV0,PRM_RN,PRM_RM,shop_or,-1)},   // div0s <REG_M>,<REG_N>
	{0                          ,i0010_nnnn_mmmm_1000   ,Mask_n_m       ,0x2008 ,Normal         ,"tst <REG_M>,<REG_N>"                  ,1,1,MT,1    ,dec_cmp(shop_test,PRM_RN,PRM_RM)}, // tst <REG_M>,<REG_N>
	{0                          ,i0010_nnnn_mmmm_1001   ,Mask_n_m       ,0x2009 ,Normal         ,"and <REG_M>,<REG_N>"                  ,1,1,EX,1    ,dec_Bin_rNrM(shop_and)},   //and <REG_M>,<REG_N>
	{0                          ,i0010_nnnn_mmmm_1010   ,Mask_n_m       ,0x200A ,Normal         ,"xor <REG_M>,<REG_N>"                  ,1,1,EX,1    ,dec_Bin_rNrM(shop_xor)},   //xor <REG_M>,<REG_N>
	{0                          ,i0010_nnnn_mmmm_1011   ,Mask_n_m       ,0x200B ,Normal         ,"or <REG_M>,<REG_N>"                   ,1,1,EX,1    ,dec_Bin_rNrM(shop_or)},    //or <REG_M>,<REG_N>
	{0                          ,i0010_nnnn_mmmm_1100   ,Mask_n_m       ,0x200C ,Normal         ,"cmp/str <REG_M>,<REG_N>"              ,1,1,MT,1    ,dec_cmp(shop_setpeq,PRM_RN,PRM_RM)},   //cmp/str <REG_M>,<REG_N>
	{0                          ,i0010_nnnn_mmmm_1101   ,Mask_n_m       ,0x200D ,Normal         ,"xtrct <REG_M>,<REG_N>"                ,1,1,EX,1	 ,dec_Bin_rNrM(shop_xtrct)},  //xtrct <REG_M>,<REG_N>
	{0                          ,i0010_nnnn_mmmm_1110   ,Mask_n_m       ,0x200E ,Normal         ,"mulu.w <REG_M>,<REG_N>"               ,2,4,CO,34   ,dec_mul(16)},  //mulu.w <REG_M>,<REG_N>
	{0                          ,i0010_nnnn_mmmm_1111   ,Mask_n_m       ,0x200F ,Normal         ,"muls.w <REG_M>,<REG_N>"               ,2,4,CO,34   ,dec_mul(-16)}, //muls.w <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_0000   ,Mask_n_m       ,0x3000 ,Normal         ,"cmp/eq <REG_M>,<REG_N>"               ,1,1,MT,1    ,dec_cmp(shop_seteq,PRM_RN,PRM_RM)},    // cmp/eq <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_0010   ,Mask_n_m       ,0x3002 ,Normal         ,"cmp/hs <REG_M>,<REG_N>"               ,1,1,MT,1    ,dec_cmp(shop_setae,PRM_RN,PRM_RM)},    // cmp/hs <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_0011   ,Mask_n_m       ,0x3003 ,Normal         ,"cmp/ge <REG_M>,<REG_N>"               ,1,1,MT,1    ,dec_cmp(shop_setge,PRM_RN,PRM_RM)},    //cmp/ge <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_0100   ,Mask_n_m       ,0x3004 ,Normal         ,"div1 <REG_M>,<REG_N>"                 ,1,1,EX,1    ,dec_Fill(DM_ADC,PRM_RN,PRM_RM,shop_div1,0)},  //div1 <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_0101   ,Mask_n_m       ,0x3005 ,Normal         ,"dmulu.l <REG_M>,<REG_N>"              ,2,4,CO,34   ,dec_mul(64)},  //dmulu.l <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_0110   ,Mask_n_m       ,0x3006 ,Normal         ,"cmp/hi <REG_M>,<REG_N>"               ,1,1,MT,1    ,dec_cmp(shop_setab,PRM_RN,PRM_RM)},    // cmp/hi <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_0111   ,Mask_n_m       ,0x3007 ,Normal         ,"cmp/gt <REG_M>,<REG_N>"               ,1,1,MT,1    ,dec_cmp(shop_setgt,PRM_RN,PRM_RM)},    //cmp/gt <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_1000   ,Mask_n_m       ,0x3008 ,Normal         ,"sub <REG_M>,<REG_N>"                  ,1,1,EX,1    ,dec_Bin_rNrM(shop_sub)},   // sub <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_1010   ,Mask_n_m       ,0x300A ,Normal         ,"subc <REG_M>,<REG_N>"                 ,1,1,EX,1,   dec_Fill(DM_ADC,PRM_RN,PRM_RM,shop_sbc,-1)},    //subc <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_1011   ,Mask_n_m       ,0x300B ,Normal         ,"subv <REG_M>,<REG_N>"                 ,1,1,EX,1},  //subv <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_1100   ,Mask_n_m       ,0x300C ,Normal         ,"add <REG_M>,<REG_N>"                  ,1,1,EX,1    ,dec_Bin_rNrM(shop_add)},   //add <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_1101   ,Mask_n_m       ,0x300D ,Normal         ,"dmuls.l <REG_M>,<REG_N>"              ,2,4,CO,34   ,dec_mul(-64)}, //dmuls.l <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_1110   ,Mask_n_m       ,0x300E ,Normal         ,"addc <REG_M>,<REG_N>"                 ,1,1,EX,1,   dec_Fill(DM_ADC,PRM_RN,PRM_RM,shop_adc,-1)},    //addc <REG_M>,<REG_N>
	{0                          ,i0011_nnnn_mmmm_1111   ,Mask_n_m       ,0x300F ,Normal         ,"addv <REG_M>,<REG_N>"                 ,1,1,EX,1},  // addv <REG_M>,<REG_N>

	//Normal readm/writem
	{0                          ,i0000_nnnn_mmmm_0100   ,Mask_n_m       ,0x0004 ,Normal         ,"mov.b <REG_M>,@(R0,<REG_N>)"          ,1,1,LS,2    ,dec_MWt(PRM_RN_R0,PRM_RM,1)},  //mov.b <REG_M>,@(R0,<REG_N>)
	{0                          ,i0000_nnnn_mmmm_0101   ,Mask_n_m       ,0x0005 ,Normal         ,"mov.w <REG_M>,@(R0,<REG_N>)"          ,1,1,LS,2    ,dec_MWt(PRM_RN_R0,PRM_RM,2)},  //mov.w <REG_M>,@(R0,<REG_N>)
	{0                          ,i0000_nnnn_mmmm_0110   ,Mask_n_m       ,0x0006 ,Normal         ,"mov.l <REG_M>,@(R0,<REG_N>)"          ,1,1,LS,2    ,dec_MWt(PRM_RN_R0,PRM_RM,4)},  //mov.l <REG_M>,@(R0,<REG_N>)
	{0                          ,i0000_nnnn_mmmm_1100   ,Mask_n_m       ,0x000C ,Normal         ,"mov.b @(R0,<REG_M>),<REG_N>"          ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM_R0,1)},  //mov.b @(R0,<REG_M>),<REG_N>
	{0                          ,i0000_nnnn_mmmm_1101   ,Mask_n_m       ,0x000D ,Normal         ,"mov.w @(R0,<REG_M>),<REG_N>"          ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM_R0,2)},  //mov.w @(R0,<REG_M>),<REG_N>
	{0                          ,i0000_nnnn_mmmm_1110   ,Mask_n_m       ,0x000E ,Normal         ,"mov.l @(R0,<REG_M>),<REG_N>"          ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM_R0,4)},  //mov.l @(R0,<REG_M>),<REG_N>
	{0                          ,i0001_nnnn_mmmm_iiii   ,Mask_n_imm8    ,0x1000 ,Normal         ,"mov.l <REG_M>,@(<disp4dw>,<REG_N>)"   ,1,1,LS,2    ,dec_MWt(PRM_RN_D4_x4,PRM_RM,4)},   //mov.l <REG_M>,@(<disp>,<REG_N>)
	{0                          ,i0101_nnnn_mmmm_iiii   ,Mask_n_m_imm4  ,0x5000,Normal          ,"mov.l @(<disp4dw>,<REG_M>),<REG_N>"   ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM_D4_x4,4)},   //mov.l @(<disp>,<REG_M>),<REG_N>

	{0                          ,i0010_nnnn_mmmm_0000   ,Mask_n_m       ,0x2000 ,Normal         ,"mov.b <REG_M>,@<REG_N>"               ,1,1,LS,2    ,dec_MWt(PRM_RN,PRM_RM,1)}, //mov.b <REG_M>,@<REG_N>
	{0                          ,i0010_nnnn_mmmm_0001   ,Mask_n_m       ,0x2001 ,Normal         ,"mov.w <REG_M>,@<REG_N>"               ,1,1,LS,2    ,dec_MWt(PRM_RN,PRM_RM,2)}, // mov.w <REG_M>,@<REG_N>
	{0                          ,i0010_nnnn_mmmm_0010   ,Mask_n_m       ,0x2002 ,Normal         ,"mov.l <REG_M>,@<REG_N>"               ,1,1,LS,2    ,dec_MWt(PRM_RN,PRM_RM,4)}, // mov.l <REG_M>,@<REG_N>
	{0                          ,i0110_nnnn_mmmm_0000   ,Mask_n_m       ,0x6000 ,Normal         ,"mov.b @<REG_M>,<REG_N>"               ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM,1)}, //mov.b @<REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_0001   ,Mask_n_m       ,0x6001 ,Normal         ,"mov.w @<REG_M>,<REG_N>"               ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM,2)}, //mov.w @<REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_0010   ,Mask_n_m       ,0x6002 ,Normal         ,"mov.l @<REG_M>,<REG_N>"               ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM,4)}, //mov.l @<REG_M>,<REG_N>

	{0                          ,i0010_nnnn_mmmm_0100   ,Mask_n_m       ,0x2004 ,Normal         ,"mov.b <REG_M>,@-<REG_N>"              ,1,1,LS,2    ,dec_MWt(PRM_RN,PRM_RM,-1)},    // mov.b <REG_M>,@-<REG_N>
	{0                          ,i0010_nnnn_mmmm_0101   ,Mask_n_m       ,0x2005 ,Normal         ,"mov.w <REG_M>,@-<REG_N>"              ,1,1,LS,2    ,dec_MWt(PRM_RN,PRM_RM,-2)},    //mov.w <REG_M>,@-<REG_N>
	{0                          ,i0010_nnnn_mmmm_0110   ,Mask_n_m       ,0x2006 ,Normal         ,"mov.l <REG_M>,@-<REG_N>"              ,1,1,LS,2    ,dec_MWt(PRM_RN,PRM_RM,-4)},    //mov.l <REG_M>,@-<REG_N>
	{0                          ,i0110_nnnn_mmmm_0100   ,Mask_n_m       ,0x6004 ,Normal         ,"mov.b @<REG_M>+,<REG_N>"              ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM,-1)},    //mov.b @<REG_M>+,<REG_N>
	{0                          ,i0110_nnnn_mmmm_0101   ,Mask_n_m       ,0x6005 ,Normal         ,"mov.w @<REG_M>+,<REG_N>"              ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM,-2)},    //mov.w @<REG_M>+,<REG_N>
	{0                          ,i0110_nnnn_mmmm_0110   ,Mask_n_m       ,0x6006 ,Normal         ,"mov.l @<REG_M>+,<REG_N>"              ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_RM,-4)},    //mov.l @<REG_M>+,<REG_N>

	{0                          ,i1000_0000_mmmm_iiii   ,Mask_imm8      ,0x8000 ,Normal         ,"mov.b R0,@(<disp4b>,<REG_M>)"         ,1,1,LS,2    ,dec_MWt(PRM_RM_D4_x1,PRM_R0,1)},   // mov.b R0,@(<disp>,<REG_M>)
	{0                          ,i1000_0001_mmmm_iiii   ,Mask_imm8      ,0x8100 ,Normal         ,"mov.w R0,@(<disp4w>,<REG_M>)"         ,1,1,LS,2    ,dec_MWt(PRM_RM_D4_x2,PRM_R0,2)},   // mov.w R0,@(<disp>,<REG_M>)
	{0                          ,i1000_0100_mmmm_iiii   ,Mask_imm8      ,0x8400 ,Normal         ,"mov.b @(<disp4b>,<REG_M>),R0"         ,1,2,LS,2    ,dec_MRd(PRM_R0,PRM_RM_D4_x1,1)},   // mov.b @(<disp>,<REG_M>),R0
	{0                          ,i1000_0101_mmmm_iiii   ,Mask_imm8      ,0x8500 ,Normal         ,"mov.w @(<disp4w>,<REG_M>),R0"         ,1,2,LS,2    ,dec_MRd(PRM_R0,PRM_RM_D4_x2,2)},   // mov.w @(<disp>,<REG_M>),R0
	{0                          ,i1001_nnnn_iiii_iiii   ,Mask_n_imm8    ,0x9000 ,ReadsPC        ,"mov.w @(<PCdisp8w>),<REG_N>"          ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_PC_D8_x2,2)},   // mov.w @(<disp>,PC),<REG_N>
	{0                          ,i1100_0000_iiii_iiii   ,Mask_imm8      ,0xC000 ,Normal         ,"mov.b R0,@(<disp8b>,GBR)"             ,1,1,LS,3    ,dec_MWt(PRM_GBR_D8_x1,PRM_R0,1)},  // mov.b R0,@(<disp>,GBR)
	{0                          ,i1100_0001_iiii_iiii   ,Mask_imm8      ,0xC100 ,Normal         ,"mov.w R0,@(<disp8w>,GBR)"             ,1,1,LS,3    ,dec_MWt(PRM_GBR_D8_x2,PRM_R0,2)},  // mov.w R0,@(<disp>,GBR)
	{0                          ,i1100_0010_iiii_iiii   ,Mask_imm8      ,0xC200 ,Normal         ,"mov.l R0,@(<disp8dw>,GBR)"            ,1,1,LS,3    ,dec_MWt(PRM_GBR_D8_x4,PRM_R0,4)},  // mov.l R0,@(<disp>,GBR)
	{0                          ,i1100_0100_iiii_iiii   ,Mask_imm8      ,0xC400 ,Normal         ,"mov.b @(<GBRdisp8b>),R0"              ,1,2,LS,3    ,dec_MRd(PRM_R0,PRM_GBR_D8_x1,1)},  // mov.b @(<disp>,GBR),R0
	{0                          ,i1100_0101_iiii_iiii   ,Mask_imm8      ,0xC500 ,Normal         ,"mov.w @(<GBRdisp8w>),R0"              ,1,2,LS,3    ,dec_MRd(PRM_R0,PRM_GBR_D8_x2,2)},  // mov.w @(<disp>,GBR),R0
	{0                          ,i1100_0110_iiii_iiii   ,Mask_imm8      ,0xC600 ,Normal         ,"mov.l @(<GBRdisp8dw>),R0"             ,1,2,LS,3    ,dec_MRd(PRM_R0,PRM_GBR_D8_x4,4)},  // mov.l @(<disp>,GBR),R0
	{0                          ,i1101_nnnn_iiii_iiii   ,Mask_n_imm8    ,0xD000 ,ReadsPC        ,"mov.l @(<PCdisp8d>),<REG_N>"          ,1,2,LS,2    ,dec_MRd(PRM_RN,PRM_PC_D8_x4,4)},   // mov.l @(<disp>,PC),<REG_N>

	//normal mov
	{0                          ,i0110_nnnn_mmmm_0011   ,Mask_n_m       ,0x6003 ,Normal         ,"mov <REG_M>,<REG_N>"                  ,1,0,MT,1    ,dec_Un_rNrM(shop_mov32)},  //mov <REG_M>,<REG_N>
	{0                          ,i1100_0111_iiii_iiii   ,Mask_imm8      ,0xC700 ,ReadsPC        ,"mova @(<PCdisp8d>),R0"                ,1,1,EX,1    ,dec_Fill(DM_UnaryOp,PRM_R0,PRM_PC_D8_x4,shop_mov32)},  // mova @(<disp>,PC),R0
	{0                          ,i1110_nnnn_iiii_iiii   ,Mask_n_imm8    ,0xE000 ,Normal         ,"mov #<simm8hex>,<REG_N>"              ,1,1,EX,1    ,dec_Fill(DM_UnaryOp,PRM_RN,PRM_SIMM8,shop_mov32)}, // mov #<imm>,<REG_N>

	//Special register readm/writem/movs

	//sts : @-rn
	{0                          ,i0100_nnnn_0101_0010   ,Mask_n         ,0x4052 ,UsesFPU        ,"sts.l FPUL,@-<REG_N>"                 ,1,1,CO,2    ,dec_STM(PRM_SREG)},    //sts.l FPUL,@-<REG_N>
	{0                          ,i0100_nnnn_0110_0010   ,Mask_n         ,0x4062 ,UsesFPU        ,"sts.l FPSCR,@-<REG_N>"                ,1,1,CO,2    ,dec_STM(PRM_SREG)},    //sts.l FPSCR,@-<REG_N>
	{0                          ,i0100_nnnn_0000_0010   ,Mask_n         ,0x4002 ,Normal         ,"sts.l MACH,@-<REG_N>"                 ,1,1,CO,31   ,dec_STM(PRM_SREG)},    //sts.l MACH,@-<REG_N>
	{0                          ,i0100_nnnn_0001_0010   ,Mask_n         ,0x4012 ,Normal         ,"sts.l MACL,@-<REG_N>"                 ,1,1,CO,31   ,dec_STM(PRM_SREG)},    //sts.l MACL,@-<REG_N>
	{0                          ,i0100_nnnn_0010_0010   ,Mask_n         ,0x4022 ,Normal         ,"sts.l PR,@-<REG_N>"                   ,2,2,CO,27   ,dec_STM(PRM_SREG)},    //sts.l PR,@-<REG_N>
	{0                          ,i0100_nnnn_1111_0010   ,Mask_n         ,0x40F2 ,Normal         ,"stc.l DBR,@-<REG_N>"                  ,2,2,CO,22   ,dec_STM(PRM_SREG)},    //sts.l DBR,@-<REG_N>
	{0                          ,i0100_nnnn_0011_0010   ,Mask_n         ,0x4032 ,Normal         ,"stc.l SGR,@-<REG_N>"                  ,3,3,CO,23   ,dec_STM(PRM_SREG)},    //sts.l SGR,@-<REG_N>

	//stc : @-rn
	{dec_i0100_nnnn_0000_0011   ,i0100_nnnn_0000_0011   ,Mask_n         ,0x4003 ,Normal         ,"stc.l SR,@-<REG_N>"                   ,2,2,CO,22}, //stc.l SR,@-<REG_N>
	{0                          ,i0100_nnnn_0001_0011   ,Mask_n         ,0x4013 ,Normal         ,"stc.l GBR,@-<REG_N>"                  ,2,2,CO,22   ,dec_STM(PRM_CREG)},    //stc.l GBR,@-<REG_N>
	{0                          ,i0100_nnnn_0010_0011   ,Mask_n         ,0x4023 ,Normal         ,"stc.l VBR,@-<REG_N>"                  ,2,2,CO,22   ,dec_STM(PRM_CREG)},    //stc.l VBR,@-<REG_N>
	{0                          ,i0100_nnnn_0011_0011   ,Mask_n         ,0x4033 ,Normal         ,"stc.l SSR,@-<REG_N>"                  ,2,2,CO,22   ,dec_STM(PRM_CREG)},    //stc.l SSR,@-<REG_N>
	{0                          ,i0100_nnnn_0100_0011   ,Mask_n         ,0x4043 ,Normal         ,"stc.l SPC,@-<REG_N>"                  ,2,2,CO,22   ,dec_STM(PRM_CREG)},    //stc.l SPC,@-<REG_N>
	{0                          ,i0100_nnnn_1mmm_0011   ,Mask_n_ml3bit  ,0x4083,Normal          ,"stc <RM_BANK>,@-<REG_N>"              ,2,2,CO,22   ,dec_STM(PRM_CREG)},    //stc RM_BANK,@-<REG_N>

	//lds : @rn+
	{0                          ,i0100_nnnn_0000_0110   ,Mask_n         ,0x4006 ,Normal         ,"lds.l @<REG_N>+,MACH"                 ,1,3,CO,29   ,dec_LDM(PRM_SREG)},    //lds.l @<REG_N>+,MACH
	{0                          ,i0100_nnnn_0001_0110   ,Mask_n         ,0x4016 ,Normal         ,"lds.l @<REG_N>+,MAC"                  ,1,3,CO,29   ,dec_LDM(PRM_SREG)},    //lds.l @<REG_N>+,MACL
	{0                          ,i0100_nnnn_0010_0110   ,Mask_n         ,0x4026 ,Normal         ,"lds.l @<REG_N>+,PR"                   ,2,3,CO,25   ,dec_LDM(PRM_SREG)},    //lds.l @<REG_N>+,PR
	{0                          ,i0100_nnnn_0011_0110   ,Mask_n         ,0x4036 ,Normal         ,"ldc.l @<REG_N>+,SGR"                  ,3,3,CO,18   ,dec_LDM(PRM_SREG)},    //lds.l @<REG_N>+,SGR
	{0                          ,i0100_nnnn_0101_0110   ,Mask_n         ,0x4056 ,UsesFPU        ,"lds.l @<REG_N>+,FPUL"                 ,1,2,CO,2    ,dec_LDM(PRM_SREG)},    //lds.l @<REG_N>+,FPUL
	{dec_i0100_nnnn_0110_0110   ,i0100_nnnn_0110_0110   ,Mask_n         ,0x4066 ,FWritesFPSCR   ,"lds.l @<REG_N>+,FPSCR"                ,1,4,CO,33}, //lds.l @<REG_N>+,FPSCR
	{0                          ,i0100_nnnn_1111_0110   ,Mask_n         ,0x40F6 ,Normal         ,"ldc.l @<REG_N>+,DBR"                  ,1,3,CO,17   ,dec_LDM(PRM_SREG)},    //lds.l @<REG_N>+,DBR

	//ldc : @rn+
	{dec_i0100_nnnn_0000_0111   ,i0100_nnnn_0000_0111   ,Mask_n         ,0x4007 ,WritesSRRWPC   ,"ldc.l @<REG_N>+,SR"                   ,4,4,CO,19}, //ldc.l @<REG_N>+,SR
	{0                          ,i0100_nnnn_0001_0111   ,Mask_n         ,0x4017 ,Normal         ,"ldc.l @<REG_N>+,GBR"                  ,3,3,CO,18   ,dec_LDM(PRM_CREG)},    //ldc.l @<REG_N>+,GBR
	{0                          ,i0100_nnnn_0010_0111   ,Mask_n         ,0x4027 ,Normal         ,"ldc.l @<REG_N>+,VBR"                  ,1,3,CO,17   ,dec_LDM(PRM_CREG)},    //ldc.l @<REG_N>+,VBR
	{0                          ,i0100_nnnn_0011_0111   ,Mask_n         ,0x4037 ,Normal         ,"ldc.l @<REG_N>+,SSR"                  ,1,3,CO,17   ,dec_LDM(PRM_CREG)},    //ldc.l @<REG_N>+,SSR
	{0                          ,i0100_nnnn_0100_0111   ,Mask_n         ,0x4047 ,Normal         ,"ldc.l @<REG_N>+,SPC"                  ,1,3,CO,17   ,dec_LDM(PRM_CREG)},    //ldc.l @<REG_N>+,SPC
	{0                          ,i0100_nnnn_1mmm_0111   ,Mask_n_ml3bit  ,0x4087 ,Normal         ,"ldc.l @<REG_N>+,RM_BANK"              ,1,3,CO,17   ,dec_LDM(PRM_CREG)},    //ldc.l @<REG_N>+,RM_BANK

	//sts : rn
	{0                          ,i0000_nnnn_0000_0010   ,Mask_n         ,0x0002 ,Normal         ,"stc SR,<REG_N>"                       ,2,2,CO,20   ,dec_STSRF(PRM_CREG)},  //stc SR,<REG_N>
	{0                          ,i0000_nnnn_0001_0010   ,Mask_n         ,0x0012 ,Normal         ,"stc GBR,<REG_N>"                      ,2,2,CO,20   ,dec_ST(PRM_CREG)}, //stc GBR,<REG_N>
	{0                          ,i0000_nnnn_0010_0010   ,Mask_n         ,0x0022 ,Normal         ,"stc VBR,<REG_N>"                      ,2,2,CO,20   ,dec_ST(PRM_CREG)}, //stc VBR,<REG_N>
	{0                          ,i0000_nnnn_0011_0010   ,Mask_n         ,0x0032 ,Normal         ,"stc SSR,<REG_N>"                      ,2,2,CO,20   ,dec_ST(PRM_CREG)}, //stc SSR,<REG_N>
	{0                          ,i0000_nnnn_0100_0010   ,Mask_n         ,0x0042 ,Normal         ,"stc SPC,<REG_N>"                      ,2,2,CO,20   ,dec_ST(PRM_CREG)}, //stc SPC,<REG_N>
	{0                          ,i0000_nnnn_1mmm_0010   ,Mask_n_ml3bit  ,0x0082 ,Normal         ,"stc RM_BANK,<REG_N>"                  ,2,2,CO,20   ,dec_ST(PRM_CREG)}, //stc RM_BANK,<REG_N>

	//stc : rn

	{0                          ,i0000_nnnn_0000_1010   ,Mask_n         ,0x000A ,Normal         ,"sts MACH,<REG_N>"                     ,1,3,CO,30   ,dec_ST(PRM_SREG)}, //sts MACH,<REG_N>
	{0                          ,i0000_nnnn_0001_1010   ,Mask_n         ,0x001A ,Normal         ,"sts MACL,<REG_N>"                     ,1,3,CO,30   ,dec_ST(PRM_SREG)}, //sts MACL,<REG_N>
	{0                          ,i0000_nnnn_0010_1010   ,Mask_n         ,0x002A ,Normal         ,"sts PR,<REG_N>"                       ,2,2,CO,26   ,dec_ST(PRM_SREG)}, //sts PR,<REG_N>
	{0                          ,i0000_nnnn_0011_1010   ,Mask_n         ,0x003A ,Normal         ,"sts SGR,<REG_N>"                      ,3,3,CO,21   ,dec_ST(PRM_SREG)}, //sts SGR,<REG_N>
	{0                          ,i0000_nnnn_0101_1010   ,Mask_n         ,0x005A ,UsesFPU        ,"sts FPUL,<REG_N>"                     ,1,3,LS,1    ,dec_ST(PRM_SREG)}, //sts FPUL,<REG_N>
	{0                          ,i0000_nnnn_0110_1010   ,Mask_n         ,0x006A ,UsesFPU        ,"sts FPSCR,<REG_N>"                    ,1,3,CO,1    ,dec_ST(PRM_SREG)}, //sts FPSCR,<REG_N>
	{0                          ,i0000_nnnn_1111_1010   ,Mask_n         ,0x00FA ,Normal         ,"sts DBR,<REG_N>"                      ,2,2,CO,20   ,dec_ST(PRM_SREG)}, //sts DBR,<REG_N>

	//lds : rn
	{0                          ,i0100_nnnn_0000_1010   ,Mask_n         ,0x400A ,Normal         ,"lds <REG_N>,MACH"                     ,1,3,CO,28   ,dec_LD(PRM_SREG)}, //lds <REG_N>,MACH
	{0                          ,i0100_nnnn_0001_1010   ,Mask_n         ,0x401A ,Normal         ,"lds <REG_N>,MAC"                      ,1,3,CO,28   ,dec_LD(PRM_SREG)}, //lds <REG_N>,MACL
	{0                          ,i0100_nnnn_0010_1010   ,Mask_n         ,0x402A ,Normal         ,"lds <REG_N>,PR"                       ,2,3,CO,24   ,dec_LD(PRM_SREG)}, //lds <REG_N>,PR
	{0                          ,i0100_nnnn_0011_1010   ,Mask_n         ,0x403A ,Normal         ,"ldc <REG_N>,SGR"                      ,3,3,CO,15   ,dec_LD(PRM_SREG)}, //lds <REG_N>,SGR
	{0                          ,i0100_nnnn_0101_1010   ,Mask_n         ,0x405A ,UsesFPU        ,"lds <REG_N>,FPUL"                     ,1,1,LS,1    ,dec_LD(PRM_SREG)}, //lds <REG_N>,FPUL
	{dec_i0100_nnnn_0110_1010   ,i0100_nnnn_0110_1010   ,Mask_n         ,0x406A ,FWritesFPSCR   ,"lds <REG_N>,FPSCR"                    ,1,4,CO,32}, //lds <REG_N>,FPSCR
	{0                          ,i0100_nnnn_1111_1010   ,Mask_n         ,0x40FA ,Normal         ,"ldc <REG_N>,DBR"                      ,1,3,CO,14   ,dec_LD(PRM_SREG)}, //lds <REG_N>,DBR

	//ldc : rn
	{dec_i0100_nnnn_0000_1110   ,i0100_nnnn_0000_1110   ,Mask_n         ,0x400E ,WritesSRRWPC   ,"ldc <REG_N>,SR"                       ,4,4,CO,16}, //ldc <REG_N>,SR
	{0                          ,i0100_nnnn_0001_1110   ,Mask_n         ,0x401E ,Normal         ,"ldc <REG_N>,GBR"                      ,3,3,CO,15   ,dec_LD(PRM_CREG)}, //ldc <REG_N>,GBR
	{0                          ,i0100_nnnn_0010_1110   ,Mask_n         ,0x402E ,Normal         ,"ldc <REG_N>,VBR"                      ,1,3,CO,14   ,dec_LD(PRM_CREG)}, //ldc <REG_N>,VBR
	{0                          ,i0100_nnnn_0011_1110   ,Mask_n         ,0x403E ,Normal         ,"ldc <REG_N>,SSR"                      ,1,3,CO,14   ,dec_LD(PRM_CREG)}, //ldc <REG_N>,SSR
	{0                          ,i0100_nnnn_0100_1110   ,Mask_n         ,0x404E ,Normal         ,"ldc <REG_N>,SPC"                      ,1,3,CO,14   ,dec_LD(PRM_CREG)}, //ldc <REG_N>,SPC
	{0                          ,i0100_nnnn_1mmm_1110   ,Mask_n_ml3bit  ,0x408E ,Normal         ,"ldc <REG_N>,<RM_BANK>"                ,1,3,CO,14   ,dec_LD(PRM_CREG)}, //ldc <REG_N>,<RM_BANK>

	//
	{0                          ,i0100_nnnn_0000_0000   ,Mask_n         ,0x4000 ,Normal         ,"shll <REG_N>"                         ,1,1,EX,1    ,dec_shft(1,false)},    //shll <REG_N>
	{0                          ,i0100_nnnn_0001_0000   ,Mask_n         ,0x4010 ,Normal         ,"dt <REG_N>"                           ,1,1,EX,1    ,dec_Fill(DM_DT,PRM_RN,PRM_ONE,shop_sub)},  //dt <REG_N>
	{0                          ,i0100_nnnn_0010_0000   ,Mask_n         ,0x4020 ,Normal         ,"shal <REG_N>"                         ,1,1,EX,1    ,dec_shft(1,true)},     //shal <REG_N>
	{0                          ,i0100_nnnn_0000_0001   ,Mask_n         ,0x4001 ,Normal         ,"shlr <REG_N>"                         ,1,1,EX,1    ,dec_shft(-1,false)},   //shlr <REG_N>
	{0                          ,i0100_nnnn_0001_0001   ,Mask_n         ,0x4011 ,Normal         ,"cmp/pz <REG_N>"                       ,1,1,MT,1    ,dec_cmp(shop_setge,PRM_RN,PRM_ZERO)},  //cmp/pz <REG_N>
	{0                          ,i0100_nnnn_0010_0001   ,Mask_n         ,0x4021 ,Normal         ,"shar <REG_N>"                         ,1,1,EX,1    ,dec_shft(-1,true)},    //shar <REG_N>
	{dec_i0100_nnnn_0010_0100   ,i0100_nnnn_0010_0100   ,Mask_n         ,0x4024 ,Normal         ,"rotcl <REG_N>"                        ,1,1,EX,1},  //rotcl <REG_N>
	{0                          ,i0100_nnnn_0000_0100   ,Mask_n         ,0x4004 ,Normal         ,"rotl <REG_N>"                         ,1,1,EX,1    ,dec_Fill(DM_Rot,PRM_RN,PRM_RN,shop_ror,-31)},  //rotl <REG_N>
	{0                          ,i0100_nnnn_0001_0101   ,Mask_n         ,0x4015 ,Normal         ,"cmp/pl <REG_N>"                       ,1,1,MT,1    ,dec_cmp(shop_setgt,PRM_RN,PRM_ZERO)},  //cmp/pl <REG_N>
	{dec_i0100_nnnn_0010_0101   ,i0100_nnnn_0010_0101   ,Mask_n         ,0x4025 ,Normal         ,"rotcr <REG_N>"                        ,1,1,EX,1},  //rotcr <REG_N>
	{0                          ,i0100_nnnn_0000_0101   ,Mask_n         ,0x4005 ,Normal         ,"rotr <REG_N>"                         ,1,1,EX,1    ,dec_Fill(DM_Rot,PRM_RN,PRM_RN,shop_ror,1)},    //rotr <REG_N>
	{0                          ,i0100_nnnn_0000_1000   ,Mask_n         ,0x4008 ,Normal         ,"shll2 <REG_N>"                        ,1,1,EX,1    ,dec_shft(2,false)},    //shll2 <REG_N>
	{0                          ,i0100_nnnn_0001_1000   ,Mask_n         ,0x4018 ,Normal         ,"shll8 <REG_N>"                        ,1,1,EX,1    ,dec_shft(8,false)},    //shll8 <REG_N>
	{0                          ,i0100_nnnn_0010_1000   ,Mask_n         ,0x4028 ,Normal         ,"shll16 <REG_N>"                       ,1,1,EX,1    ,dec_shft(16,false)},   //shll16 <REG_N>
	{0                          ,i0100_nnnn_0000_1001   ,Mask_n         ,0x4009 ,Normal         ,"shlr2 <REG_N>"                        ,1,1,EX,1    ,dec_shft(-2,false)},   //shlr2 <REG_N>
	{0                          ,i0100_nnnn_0001_1001   ,Mask_n         ,0x4019 ,Normal         ,"shlr8 <REG_N>"                        ,1,1,EX,1    ,dec_shft(-8,false)},   //shlr8 <REG_N>
	{0                          ,i0100_nnnn_0010_1001   ,Mask_n         ,0x4029 ,Normal         ,"shlr16 <REG_N>"                       ,1,1,EX,1    ,dec_shft(-16,false)},  //shlr16 <REG_N>
	{dec_i0100_nnnn_0010_1011   ,i0100_nnnn_0010_1011   ,Mask_n         ,0x402B ,Branch_dir_d   ,"jmp @<REG_N>"                         ,2,3,CO,4},  //jmp @<REG_N>
	{dec_i0100_nnnn_0000_1011   ,i0100_nnnn_0000_1011   ,Mask_n         ,0x400B ,Branch_dir_d   ,"jsr @<REG_N>"                         ,2,3,CO,24}, //jsr @<REG_N>
	{dec_i0100_nnnn_0001_1011   ,i0100_nnnn_0001_1011   ,Mask_n         ,0x401B ,Normal         ,"tas.b @<REG_N>"                       ,5,5,CO,7},  //tas.b @<REG_N>
	{0                          ,i0100_nnnn_mmmm_1100   ,Mask_n_m       ,0x400C ,Normal         ,"shad <REG_M>,<REG_N>"                 ,1,1,EX,1    ,dec_Bin_rNrM(shop_shad)},  //shad <REG_M>,<REG_N>
	{0                          ,i0100_nnnn_mmmm_1101   ,Mask_n_m       ,0x400D ,Normal         ,"shld <REG_M>,<REG_N>"                 ,1,1,EX,1    ,dec_Bin_rNrM(shop_shld)},  //shld <REG_M>,<REG_N>
	{0                          ,i0100_nnnn_mmmm_1111   ,Mask_n_m       ,0x400F ,Normal         ,"mac.w @<REG_M>+,@<REG_N>+"            ,2,4,CO,35}, //mac.w @<REG_M>+,@<REG_N>+
	{0                          ,i0110_nnnn_mmmm_0111   ,Mask_n_m       ,0x6007 ,Normal         ,"not <REG_M>,<REG_N>"                  ,1,1,EX,1    ,dec_Un_rNrM(shop_not)},    //not <REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_1000   ,Mask_n_m       ,0x6008 ,Normal         ,"swap.b <REG_M>,<REG_N>"               ,1,1,EX,1    ,dec_Un_rNrM(shop_swaplb)}, //swap.b <REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_1001   ,Mask_n_m       ,0x6009 ,Normal         ,"swap.w <REG_M>,<REG_N>"               ,1,1,EX,1    ,dec_Fill(DM_Rot,PRM_RN,PRM_RM,shop_ror,16|0x1000)},    //swap.w <REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_1010   ,Mask_n_m       ,0x600A ,Normal         ,"negc <REG_M>,<REG_N>"                 ,1,1,EX,1	 ,dec_Fill(DM_NEGC,PRM_RN,PRM_RM,shop_negc)},  //negc <REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_1011   ,Mask_n_m       ,0x600B ,Normal         ,"neg <REG_M>,<REG_N>"                  ,1,1,EX,1    ,dec_Un_rNrM(shop_neg)},    //neg <REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_1100   ,Mask_n_m       ,0x600C ,Normal         ,"extu.b <REG_M>,<REG_N>"               ,1,1,EX,1    ,dec_Fill(DM_EXTOP,PRM_RN,PRM_RM,shop_and,1)},  //extu.b <REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_1101   ,Mask_n_m       ,0x600D ,Normal         ,"extu.w <REG_M>,<REG_N>"               ,1,1,EX,1    ,dec_Fill(DM_EXTOP,PRM_RN,PRM_RM,shop_and,2)},  //extu.w <REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_1110   ,Mask_n_m       ,0x600E ,Normal         ,"exts.b <REG_M>,<REG_N>"               ,1,1,EX,1    ,dec_Un_rNrM(shop_ext_s8)}, //exts.b <REG_M>,<REG_N>
	{0                          ,i0110_nnnn_mmmm_1111   ,Mask_n_m       ,0x600F ,Normal         ,"exts.w <REG_M>,<REG_N>"               ,1,1,EX,1    ,dec_Un_rNrM(shop_ext_s16)},//exts.w <REG_M>,<REG_N>
	{0                          ,i0111_nnnn_iiii_iiii   ,Mask_n_imm8    ,0x7000 ,Normal         ,"add #<simm8>,<REG_N>"                 ,1,1,EX,1    ,dec_Bin_S8R(shop_add)},    //add #<imm>,<REG_N>
	{dec_i1000_1011_iiii_iiii   ,i1000_1011_iiii_iiii   ,Mask_imm8      ,0x8B00 ,Branch_rel     ,"bf <bdisp8>"                          ,1,2,BR,1},  // bf <bdisp8>
	{dec_i1000_1111_iiii_iiii   ,i1000_1111_iiii_iiii   ,Mask_imm8      ,0x8F00 ,Branch_rel_d   ,"bf.s <bdisp8>"                        ,1,2,BR,1},  // bf.s <bdisp8>
	{dec_i1000_1001_iiii_iiii   ,i1000_1001_iiii_iiii   ,Mask_imm8      ,0x8900 ,Branch_rel     ,"bt <bdisp8>"                          ,1,2,BR,1},  // bt <bdisp8>
	{dec_i1000_1101_iiii_iiii   ,i1000_1101_iiii_iiii   ,Mask_imm8      ,0x8D00 ,Branch_rel_d   ,"bt.s <bdisp8>"                        ,1,2,BR,1},  // bt.s <bdisp8>
	{0                          ,i1000_1000_iiii_iiii   ,Mask_imm8      ,0x8800 ,Normal         ,"cmp/eq #<simm8hex>,R0"                ,1,1,MT,1    ,dec_cmp(shop_seteq,PRM_R0,PRM_SIMM8)}, // cmp/eq #<imm>,R0
	{dec_i1010_iiii_iiii_iiii   ,i1010_iiii_iiii_iiii   ,Mask_n_imm8    ,0xA000 ,Branch_rel_d   ,"bra <bdisp12>"                        ,1,2,BR,1},  // bra <bdisp12>
	{dec_i1011_iiii_iiii_iiii   ,i1011_iiii_iiii_iiii   ,Mask_n_imm8    ,0xB000 ,Branch_rel_d   ,"bsr <bdisp12>"                        ,1,2,BR,14}, // bsr <bdisp12>

	{dec_i1100_0011_iiii_iiii   ,i1100_0011_iiii_iiii   ,Mask_imm8      ,0xC300 ,ReadWritePC    ,"trapa #<imm8>"                        ,7,7,CO,13}, // trapa #<imm>

	{0                          ,i1100_1000_iiii_iiii   ,Mask_imm8      ,0xC800 ,Normal         ,"tst #<imm8>,R0"                       ,1,1,MT,1    ,dec_cmp(shop_test,PRM_R0,PRM_UIMM8)},  // tst #<imm>,R0
	{0                          ,i1100_1001_iiii_iiii   ,Mask_imm8      ,0xC900 ,Normal         ,"and #<imm8>,R0"                       ,1,1,EX,1    ,dec_Bin_r0u8(shop_and)},   // and #<imm>,R0
	{0                          ,i1100_1010_iiii_iiii   ,Mask_imm8      ,0xCA00 ,Normal         ,"xor #<imm8>,R0"                       ,1,1,EX,1    ,dec_Bin_r0u8(shop_xor)},   // xor #<imm>,R0
	{0                          ,i1100_1011_iiii_iiii   ,Mask_imm8      ,0xCB00 ,Normal         ,"or #<imm8>,R0"                        ,1,1,EX,1    ,dec_Bin_r0u8(shop_or)},    // or #<imm>,R0

	{0                          ,i1100_1100_iiii_iiii   ,Mask_imm8      ,0xCC00 ,Normal         ,"tst.b #<imm8>,@(R0,GBR)"              ,3,3,CO,5},  // tst.b #<imm>,@(R0,GBR)
	{0                          ,i1100_1101_iiii_iiii   ,Mask_imm8      ,0xCD00 ,Normal         ,"and.b #<imm8>,@(R0,GBR)"              ,4,4,CO,6},  // and.b #<imm>,@(R0,GBR)
	{0                          ,i1100_1110_iiii_iiii   ,Mask_imm8      ,0xCE00 ,Normal         ,"xor.b #<imm8>,@(R0,GBR)"              ,4,4,CO,6},  // xor.b #<imm>,@(R0,GBR)
	{0                          ,i1100_1111_iiii_iiii   ,Mask_imm8      ,0xCF00 ,Normal         ,"or.b #<imm8>,@(R0,GBR)"               ,4,4,CO,6},  // or.b #<imm>,@(R0,GBR)

	//and here are the new ones :D
	{0                          ,i1111_nnnn_mmmm_0000   ,Mask_n_m       ,0xF000,UsesFPU         ,"fadd <FREG_M_SD_F>,<FREG_N>"          ,1,4,FE,36   ,dec_Bin_frNfrM(shop_fadd)},    //fadd <FREG_M>,<FREG_N>
	{0                          ,i1111_nnnn_mmmm_0001   ,Mask_n_m       ,0xF001,UsesFPU         ,"fsub <FREG_M_SD_F>,<FREG_N>"          ,1,4,FE,36   ,dec_Bin_frNfrM(shop_fsub)},    //fsub <FREG_M>,<FREG_N>
	{0                          ,i1111_nnnn_mmmm_0010   ,Mask_n_m       ,0xF002,UsesFPU         ,"fmul <FREG_M_SD_F>,<FREG_N>"          ,1,4,FE,36   ,dec_Bin_frNfrM(shop_fmul)},    //fmul <FREG_M>,<FREG_N>
	{0                          ,i1111_nnnn_mmmm_0011   ,Mask_n_m       ,0xF003,UsesFPU         ,"fdiv <FREG_M_SD_F>,<FREG_N>"          ,1,13,FE,37  ,dec_Bin_frNfrM(shop_fdiv)},//fdiv <FREG_M>,<FREG_N>
	{0                          ,i1111_nnnn_mmmm_0100   ,Mask_n_m       ,0xF004,UsesFPU         ,"fcmp/eq <FREG_M>,<FREG_N>"            ,1,4,FE,36   ,dec_cmp(shop_fseteq,PRM_FRN,PRM_FRM)}, //fcmp/eq <FREG_M>,<FREG_N>
	{0                          ,i1111_nnnn_mmmm_0101   ,Mask_n_m       ,0xF005,UsesFPU         ,"fcmp/gt <FREG_M>,<FREG_N>"            ,1,4,FE,36   ,dec_cmp(shop_fsetgt,PRM_FRN,PRM_FRM)}, //fcmp/gt <FREG_M>,<FREG_N>
	{0                          ,i1111_nnnn_mmmm_0110   ,Mask_n_m       ,0xF006,UsesFPU         ,"fmov.s @(R0,<REG_M>),<FREG_N>"        ,1,2,LS,2    ,dec_MRd(PRM_FRN_SZ,PRM_RM_R0,4)},  //fmov.s @(R0,<REG_M>),<FREG_N>
	{0                          ,i1111_nnnn_mmmm_0111   ,Mask_n_m       ,0xF007,UsesFPU         ,"fmov.s <FREG_M>,@(R0,<REG_N>)"        ,1,1,LS,2    ,dec_MWt(PRM_RN_R0,PRM_FRM_SZ,4)},  //fmov.s <FREG_M>,@(R0,<REG_N>)
	{0                          ,i1111_nnnn_mmmm_1000   ,Mask_n_m       ,0xF008,UsesFPU         ,"fmov.s @<REG_M>,<FREG_N>"             ,1,2,LS,2    ,dec_MRd(PRM_FRN_SZ,PRM_RM,4)}, //fmov.s @<REG_M>,<FREG_N>
	{0                          ,i1111_nnnn_mmmm_1001   ,Mask_n_m       ,0xF009,UsesFPU         ,"fmov.s @<REG_M>+,<FREG_N>"            ,1,2,LS,2    ,dec_MRd(PRM_FRN_SZ,PRM_RM,-4)},    //fmov.s @<REG_M>+,<FREG_N>
	{0                          ,i1111_nnnn_mmmm_1010   ,Mask_n_m       ,0xF00A,UsesFPU         ,"fmov.s <FREG_M>,@<REG_N>"             ,1,1,LS,2    ,dec_MWt(PRM_RN,PRM_FRM_SZ,4)}, //fmov.s <FREG_M>,@<REG_N>
	{0                          ,i1111_nnnn_mmmm_1011   ,Mask_n_m       ,0xF00B,UsesFPU         ,"fmov.s <FREG_M>,@-<REG_N>"            ,1,1,LS,2    ,dec_MWt(PRM_RN,PRM_FRM_SZ,-4)},    //fmov.s <FREG_M>,@-<REG_N>
	{0                          ,i1111_nnnn_mmmm_1100   ,Mask_n_m       ,0xF00C,UsesFPU         ,"fmov <FREG_M>,<FREG_N>"               ,1,0,LS,1    ,dec_Fill(DM_UnaryOp,PRM_FRN_SZ,PRM_FRM_SZ,shop_mov32)},    //fmov <FREG_M>,<FREG_N>
	{0                          ,i1111_nnnn_0101_1101   ,Mask_n         ,0xF05D,UsesFPU         ,"fabs <FREG_N>"                        ,1,0,LS,1    ,dec_Un_frNfrN(shop_fabs)}, //fabs <FREG_N>
	{0                          ,i1111_nnn0_1111_1101   ,Mask_nh3bit    ,0xF0FD,UsesFPU         ,"FSCA FPUL, <DR_N>"                    ,1,3,FE,44   ,dec_Fill(DM_UnaryOp,PRM_FPN,PRM_FPUL,shop_fsca)},  //FSCA FPUL, DRn//F0FD//1111_nnnn_1111_1101
	{0                          ,i1111_nnnn_1011_1101   ,Mask_n         ,0xF0BD,UsesFPU         ,"fcnvds <DR_N>,FPUL"                   ,1,4,FE,38}, //fcnvds <DR_N>,FPUL
	{0                          ,i1111_nnnn_1010_1101   ,Mask_n         ,0xF0AD,UsesFPU         ,"fcnvsd FPUL,<DR_N>"                   ,1,4,FE,38}, //fcnvsd FPUL,<DR_N>
	{0                          ,i1111_nnmm_1110_1101   ,Mask_n         ,0xF0ED,UsesFPU         ,"fipr <FV_M>,<FV_N>"                   ,1,5,FE,42   ,dec_Fill(DM_fiprOp,PRM_FVN,PRM_FVM,shop_fipr)},    //fipr <FV_M>,<FV_N>
	{0                          ,i1111_nnnn_1000_1101   ,Mask_n         ,0xF08D,UsesFPU         ,"fldi0 <FREG_N>"                       ,1,0,LS,1    ,dec_Fill(DM_UnaryOp,PRM_FRN,PRM_ZERO,shop_mov32)}, //fldi0 <FREG_N>
	{0                          ,i1111_nnnn_1001_1101   ,Mask_n         ,0xF09D,UsesFPU         ,"fldi1 <FREG_N>"                       ,1,0,LS,1    ,dec_Fill(DM_UnaryOp,PRM_FRN,PRM_ONE_F32,shop_mov32)},  //fldi1 <FREG_N>
	{0                          ,i1111_nnnn_0001_1101   ,Mask_n         ,0xF01D,UsesFPU         ,"flds <FREG_N>,FPUL"                   ,1,0,LS,1    ,dec_Fill(DM_UnaryOp,PRM_FPUL,PRM_FRN,shop_mov32)}, //flds <FREG_N>,FPUL
	{0                          ,i1111_nnnn_0010_1101   ,Mask_n         ,0xF02D,UsesFPU         ,"float FPUL,<FREG_N>"                  ,1,4,FE,36   ,dec_Fill(DM_UnaryOp,PRM_FRN,PRM_FPUL,shop_cvt_i2f_n)}, //float FPUL,<FREG_N>
	{0                          ,i1111_nnnn_0100_1101   ,Mask_n         ,0xF04D,UsesFPU         ,"fneg <FREG_N> "                       ,1,0,LS,1    ,dec_Un_frNfrN(shop_fneg)}, //fneg <FREG_N>
	{dec_i1111_1011_1111_1101   ,i1111_1011_1111_1101   ,Mask_none      ,0xFBFD,FWritesFPSCR    ,"frchg"                                ,1,4,FE,36}, //frchg
	{dec_i1111_0011_1111_1101   ,i1111_0011_1111_1101   ,Mask_none      ,0xF3FD,FWritesFPSCR    ,"fschg"                                ,1,4,FE,36}, //fschg
	{0                          ,i1111_nnnn_0110_1101   ,Mask_n         ,0xF06D,UsesFPU         ,"fsqrt <FREG_N>"                       ,1,23,FE,41  ,dec_Un_frNfrN(shop_fsqrt)},//fsqrt <FREG_N>
	{0                          ,i1111_nnnn_0011_1101   ,Mask_n         ,0xF03D,UsesFPU         ,"ftrc <FREG_N>, FPUL"                  ,1,4,FE,38   ,dec_Fill(DM_UnaryOp,PRM_FPUL,PRM_FRN,shop_cvt_f2i_t)},  //ftrc <FREG_N>, FPUL  //  ,dec_Fill(DM_UnaryOp,PRM_FPUL,PRM_FRN,shop_cvt)
	{0                          ,i1111_nnnn_0000_1101   ,Mask_n         ,0xF00D,UsesFPU         ,"fsts FPUL,<FREG_N>"                   ,1,0,LS,1    ,dec_Fill(DM_UnaryOp,PRM_FRN,PRM_FPUL,shop_mov32)}, //fsts FPUL,<FREG_N>
	{0                          ,i1111_nn01_1111_1101   ,Mask_nh2bit    ,0xF1FD,UsesFPU         ,"ftrv xmtrx,<FV_N>"                    ,1,8,FE,43   ,dec_Fill(DM_BinaryOp,PRM_FVN,PRM_XMTRX,shop_ftrv,1)},  //ftrv xmtrx,<FV_N>
	{0                          ,i1111_nnnn_mmmm_1110   ,Mask_n_m       ,0xF00E,UsesFPU         ,"fmac <FREG_0>,<FREG_M>,<FREG_N>"      ,1,4,FE,36   ,dec_Fill(DM_BinaryOp,PRM_FRN,PRM_FRM_FR0,shop_fmac,1)},    //fmac <FREG_0>,<FREG_M>,<FREG_N>
	{0                          ,i1111_nnnn_0111_1101   ,Mask_n         ,0xF07D,UsesFPU         ,"FSRRA <FREG_N>"                       ,1,4,FE,36   ,dec_Un_frNfrN(shop_fsrra)},    //FSRRA <FREG_N> (1111nnnn 01111101)

	//HLE ops

	//end of list
	{0,0,0,0,ReadWritePC}//Branch in order to stop the block and save PC ect :)
};

static void BuildOpcodeTables()
{

	for (int i=0;i<0x10000;i++)
	{
		OpPtr[i]=iNotImplemented;
		OpDesc[i]=&missing_opcode;
	}

	for (int i2=0;opcodes[i2].oph;i2++)
	{
		if (opcodes[i2].diss==0)
			opcodes[i2].diss="Unknown Opcode";

		u32 shft;
		u32 count;
		u32 mask=~opcodes[i2].mask;
		u32 base=opcodes[i2].rez;
		switch(opcodes[i2].mask)
		{
			case Mask_none:
				count=1;
				shft=0;
				break;
			case Mask_n:
				count=16;
				shft=8;
				break;
			case Mask_n_m:
				count=256;
				shft=4;
				break;
			case Mask_n_m_imm4:
				count=256*16;
				shft=0;
				break;

			case Mask_imm8:
				count=256;
				shft=0;
				break;

			case Mask_n_ml3bit:
				count=256;
				shft=4;
				break;

			case Mask_nh3bit:
				count=8;
				shft=9;
				break;

			case Mask_nh2bit:
				count=4;
				shft=10;
				break;
			default:
				die("Error");
				return;
		}
		for (u32 i=0;i<count;i++)
		{
			u32 idx=((i<<shft)&mask)+base;

			OpPtr[idx]=opcodes[i2].oph;

			OpDesc[idx]=&opcodes[i2];
		}
	}
}


static OnLoad ol_bot(&BuildOpcodeTables);

static bool strcmp2(const char* &str1_o,const char * str2)
{
	u32 sz=0;
	const char* str1=str1_o;
	while (*str2!='\0')
	{
		if (*str1!=*str2)
		{
			return false;// nope , theyre diferent
		}
		sz++;
		str1++;str2++;//next char
	}

	str1_o+=sz;
	return true;
}

std::string disassemble_op(const char* tx1, u32 pc, u16 opcode)
{
	std::string regs;
	std::string text;
	char buf[256];

	while (*tx1!='\0')
	{
		if (*tx1=='<')
		{
			tx1++;
			if (strcmp2(tx1,"REG_N>"))
			{
				snprintf(buf, sizeof(buf), "R%d=%x ", GetN(opcode), p_sh4rcb->cntx.r[GetN(opcode)]);
				regs += buf;
				snprintf(buf, sizeof(buf), "R%d",GetN(opcode));
			}
			else if (strcmp2(tx1,"REG_M>") )
			{
				snprintf(buf, sizeof(buf), "R%d=%x ", GetM(opcode), p_sh4rcb->cntx.r[GetM(opcode)]);
				regs += buf;
				snprintf(buf, sizeof(buf), "R%d",GetM(opcode));
			}
			else if (strcmp2(tx1,"FREG_N>"))
			{
				snprintf(buf, sizeof(buf), "FR%d=%f ", GetN(opcode), p_sh4rcb->cntx.fr[GetN(opcode)]);
				regs += buf;
				snprintf(buf, sizeof(buf), "FR%d",GetN(opcode));
			}
			else if (strcmp2(tx1,"FREG_M>"))
			{
				snprintf(buf, sizeof(buf), "FR%d=%f ", GetM(opcode), p_sh4rcb->cntx.fr[GetM(opcode)]);
				regs += buf;
				snprintf(buf, sizeof(buf), "FR%d",GetM(opcode));
			}
			else if (strcmp2(tx1, "FREG_M_SD_F>"))
			{
				snprintf(buf, sizeof(buf), "FR%d=%f ", GetM(opcode), p_sh4rcb->cntx.fr[GetM(opcode)]);
				regs += buf;
				snprintf(buf, sizeof(buf), "FR%d", GetM(opcode));
			}
			else if (strcmp2(tx1,"FREG_0>"))
			{
				snprintf(buf, sizeof(buf), "FR0=%f ", p_sh4rcb->cntx.fr[0]);
				regs += buf;
				snprintf(buf, sizeof(buf), "FR0");
			}
			else if (strcmp2(tx1,"RM_BANK>"))
			{
				snprintf(buf, sizeof(buf), "R%d_BANK",GetM(opcode)&0x7);
			}
			else if (strcmp2(tx1,"DFREG_N>"))
			{
				snprintf(buf, sizeof(buf), "DR%d",GetN(opcode)>>1);
			}
			else if (strcmp2(tx1,"DFREG_M>"))
			{
				snprintf(buf, sizeof(buf), "DR%d",GetM(opcode)>>1);
			}
			else if (strcmp2(tx1,"XDFREG_N>"))
			{
				u32 t=GetN(opcode);
				if (t & 0x1)
					snprintf(buf, sizeof(buf), "XD%d",t>>1);
				else
					snprintf(buf, sizeof(buf), "DR%d",t>>1);
			}
			else if (strcmp2(tx1,"XDFREG_M>"))
			{
				u32 t=GetM(opcode);
				if (t & 0x1)
					snprintf(buf, sizeof(buf), "XD%d",t>>1);
				else
					snprintf(buf, sizeof(buf), "DR%d",t>>1);
			}
			else if (strcmp2(tx1,"disp4b>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm4(opcode)*1);
			}
			else if (strcmp2(tx1,"disp4w>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm4(opcode)*2);
			}
			else if (strcmp2(tx1,"disp4dw>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm4(opcode)*4);
			}
			else if (strcmp2(tx1,"PCdisp8w>"))
			{
				snprintf(buf, sizeof(buf), "0x%X[PC]",(pc)+4+(GetImm8(opcode)<<1));
			}
			else if (strcmp2(tx1,"PCdisp8d>"))
			{
				snprintf(buf, sizeof(buf), "0x%X[PC]",(pc&0xFFFFFFFC)+4+(GetImm8(opcode)<<2));
			}
			else if (strcmp2(tx1,"disp8b>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm8(opcode)*1);
			}
			else if (strcmp2(tx1,"disp8w>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm8(opcode)*2);
			}
			else if (strcmp2(tx1,"disp8dw>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm8(opcode)*4);
			}
			else if (strcmp2(tx1,"GBRdisp8b>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm8(opcode)*1 + p_sh4rcb->cntx.gbr);
			}
			else if (strcmp2(tx1,"GBRdisp8w>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm8(opcode)*2 + p_sh4rcb->cntx.gbr);
			}
			else if (strcmp2(tx1,"GBRdisp8dw>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm8(opcode)*4 + p_sh4rcb->cntx.gbr);
			}
			else if (strcmp2(tx1,"bdisp8>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",((GetSImm8(opcode))*2 + 4 + pc));
			}
			else if (strcmp2(tx1,"bdisp12>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",((  ((s16)((GetImm12(opcode))<<4)) >>3)  + pc + 4));
			}
			else if (strcmp2(tx1,"imm8>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetImm8(opcode));
			}
			else if (strcmp2(tx1,"simm8>"))
			{
				snprintf(buf, sizeof(buf), "%d",GetSImm8(opcode));
			}
			else if (strcmp2(tx1,"simm8hex>"))
			{
				snprintf(buf, sizeof(buf), "0x%X",GetSImm8(opcode));
			}
			else
			{
				u32 ti=0;
				while (tx1[ti]!='\0')
				{
					if (tx1[ti]=='>')
						break;
					else
						ti++;
				}

				DEBUG_LOG(SH4, "Sh4Dissasm : Tag not known\"%s\"", tx1);

				buf[0] = '<';
				buf[1] = *tx1;
				buf[2] = '\0';
				tx1++;
			}
			text += buf;
		}
		else
		{
			text += *tx1;
			tx1++;
		}
	}

	return text + " ; " + regs;
}

