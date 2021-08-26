#pragma once
#include "types.h"

namespace dsp
{

struct DSPState
{
	// buffered DSP state
	s32 TEMP[128];	// 24 bits
	s32 MEMS[32];	// 24 bits
	s32 MIXS[16];	// 20 bits
	
	// RBL/RBP (decoded, from aica common regs)
	u32 RBP;
	u32 RBL;

	u32 MDEC_CT;

	// volatile dsp regs
	int SHIFTED;	// 24 bit
	int B;			// 26 bit
	int MEMVAL[4];
	int FRC_REG;	// 13 bit
	int Y_REG;		// 24 bit
	u32 ADRS_REG;	// 13 bit

	bool stopped;	// DSP program is a no-op
	bool dirty;		// DSP program has changed

	bool serialize(void **data, unsigned int *total_size)
	{
		REICAST_S(TEMP);
		REICAST_S(MEMS);
		REICAST_S(MIXS);
		REICAST_S(RBP);
		REICAST_S(RBL);
		REICAST_S(MDEC_CT);

		return true;
	}

	bool deserialize(void **data, unsigned int *total_size, serialize_version_enum version)
	{
		if (version < V18)
			REICAST_SKIP(4096 * 8);	// DynCode
		REICAST_US(TEMP);
		REICAST_US(MEMS);
		REICAST_US(MIXS);
		REICAST_US(RBP);
		REICAST_US(RBL);
		if (version < V18)
			REICAST_SKIP(44);
		REICAST_US(MDEC_CT);
		if (version < V18)
			REICAST_SKIP(33596 - 4096 * 8 - sizeof(TEMP) - sizeof(MEMS) - sizeof(MIXS) - 4 * 3 - 44);	// other dsp stuff
		dirty = true;

		return true;
	}
};

extern DSPState state;

void init();
void term();
void step();
void writeProg(u32 addr);

void recInit();
void runStep();
void recompile();

struct Instruction
{
	u8 TRA;
	bool TWT;
	u8 TWA;

	bool XSEL;
	u8 YSEL;
	u8 IRA;
	bool IWT;
	u8 IWA;

	bool EWT;
	u8 EWA;
	bool ADRL;
	bool FRCL;
	u8 SHIFT;
	bool YRL;
	bool NEGB;
	bool ZERO;
	bool BSEL;

	bool NOFL;  //MRQ set
	bool TABLE; //MRQ set
	bool MWT;   //MRQ set
	bool MRD;   //MRQ set
	u8 MASA;    //MRQ set
	bool ADREB; //MRQ set
	bool NXADR; //MRQ set
};

void DecodeInst(const u32 *IPtr, Instruction *i);
u16 DYNACALL PACK(s32 val);
s32 DYNACALL UNPACK(u16 val);

}
