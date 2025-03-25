#pragma once
#include "types.h"
#include "sh4_if.h"
#include <cstring>

#define sh4op(str) void  DYNACALL str (Sh4Context *ctx, u32 op)

typedef void (DYNACALL OpCallFP) (Sh4Context *ctx, u32 op);
extern OpCallFP* OpPtr[0x10000];

typedef void OpDissasmFP(char* out,const char* const FormatString,u32 pc,u16 opcode);

enum sh4_eu
{
	MT,
	EX,
	BR,
	LS,
	FE,
	CO,
};

enum OpcodeType
{
	//basic
	Normal       = 0,   // Heh , nothing special :P
	ReadsPC      = 1,   // PC must be set upon calling it
	WritesPC     = 2,   // It will write PC (branch)
	Delayslot    = 4,   // Has a delayslot opcode , valid only when WritesPC is set

	WritesSR     = 8,   // Writes to SR , and UpdateSR needs to be called
	WritesFPSCR  = 16,  // Writes to FPSCR , and UpdateSR needs to be called
	Invalid      = 128, // Invalid

	UsesFPU      = 2048, // Floating point op
	FWritesFPSCR = UsesFPU | WritesFPSCR,

	// Heh, not basic :P
	ReadWritePC  = ReadsPC|WritesPC,     // Read and writes pc :P
	WritesSRRWPC = WritesSR|ReadsPC|WritesPC,

	// Branches (not delay slot):
	Branch_dir   = ReadWritePC,          // Direct (eg , pc=r[xx]) -- this one is ReadWritePC b/c the delayslot may use pc ;)
	Branch_rel   = ReadWritePC,          // Relative (rg pc+=10);

	// Delay slot
	Branch_dir_d = Delayslot|Branch_dir, // Direct (eg , pc=r[xx])
	Branch_rel_d = Delayslot|Branch_rel, // Relative (rg pc+=10);
};

std::string disassemble_op(const char* tx1, u32 pc, u16 opcode);

typedef void ( RecOpCallFP) (u32 op);
struct sh4_opcodelistentry
{
	RecOpCallFP* rec_oph;
	OpCallFP* oph;
	u32 mask;
	u32 rez;
	u32 type;
	const char* diss;
	u8 IssueCycles;
	u8 LatencyCycles;
	sh4_eu unit;
	u8 ex_type;
	u64 decode;

	void Disassemble(char* strout, u32 pc, u16 op) const
	{
		const char *disOp = diss;
		if (!strcmp(disOp, "missing"))
		{
			static char tmp[6];
			snprintf(tmp, sizeof(tmp), "?%04X", op);
			disOp = tmp;
		}

		std::string text = disassemble_op(disOp, pc, op);
		strcpy(strout, text.c_str());
	}

	bool SetPC() const
	{
		return (type & WritesPC)!=0;
	}

	bool NeedPC() const
	{
		return (type & ReadsPC)!=0;
	}

	bool SetSR() const
	{
		return (type & WritesSR)!=0;
	}

	bool SetFPSCR() const
	{
		return (type & WritesFPSCR)!=0;
	}

	bool IsFloatingPoint() const
	{
		return (type & UsesFPU) != 0;
	}
};

extern sh4_opcodelistentry* OpDesc[0x10000];

void DissasembleOpcode(u16 opcode,u32 pc,char* Dissasm);
enum DecParam
{
	// Constants
	PRM_PC_D8_x2,
	PRM_PC_D8_x4,
	PRM_ZERO,
	PRM_ONE,
	PRM_TWO,
	PRM_TWO_INV,
	PRM_ONE_F32,
	
	// imms
	PRM_SIMM8,
	PRM_UIMM8,

	// Direct registers
	PRM_R0,
	PRM_RN,
	PRM_RM,
	PRM_FRN,
	PRM_FRN_SZ, //single/double, selected bank
	PRM_FRM,
	PRM_FRM_SZ,
	PRM_FPN,    //float pair, 3 bits
	PRM_FVN,    //float quad, 2 bits
	PRM_FVM,    //float quad, 2 bits
	PRM_XMTRX,  //float matrix, 0 bits
	PRM_FRM_FR0,
	PRM_FPUL,
	PRM_SR_T,
	PRM_SR_STATUS,

	PRM_SREG,   //FPUL/FPSCR/MACH/MACL/PR/DBR/SGR
	PRM_CREG,   //SR/GBR/VBR/SSR/SPC/<RM_BANK>
	
	//reg/imm reg/reg
	PRM_RN_D4_x1,
	PRM_RN_D4_x2,
	PRM_RN_D4_x4,
	PRM_RN_R0,

	PRM_RM_R0,
	PRM_RM_D4_x1,
	PRM_RM_D4_x2,
	PRM_RM_D4_x4,

	PRM_GBR_D8_x1,
	PRM_GBR_D8_x2,
	PRM_GBR_D8_x4,
};

enum DecMode
{
	DM_ReadSRF,
	DM_BinaryOp,    //d=d op s
	DM_UnaryOp,     //d= op s
	DM_ReadM,       //d=readm(s);s+=e
	DM_WriteM,      //s-=e;writem(s,d);
	DM_fiprOp,
	DM_WriteTOp,    //T=d op s
	DM_DT,          //special case for dt
	DM_Shift,
	DM_Rot,
	DM_EXTOP,
	DM_MUL,
	DM_DIV0,
	DM_ADC,
	DM_NEGC,
};
