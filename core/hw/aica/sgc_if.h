#pragma once
#include "types.h"

void AICA_Sample();
void AICA_Sample32();

void WriteChannelReg(u32 channel, u32 reg, int size);

void sgc_Init();
void sgc_Term();

union fp_22_10
{
	struct
	{
		u32 fp:10;
		u32 ip:22;
	};
	u32 full;
};
union fp_s_22_10
{
	struct
	{
		u32 fp:10;
		s32 ip:22;
	};
	s32 full;
};
union fp_20_12
{
	struct
	{
		u32 fp:12;
		u32 ip:20;
	};
	u32 full;
};

struct DSP_OUT_VOL_REG
{
	//--	EFSDL[3:0]	--	EFPAN[4:0]

	u32 EFPAN:5;
	u32 res_1:3;

	u32 EFSDL:4;
	u32 res_2:4;

	u32 pad:16;
};

typedef s32 SampleType;

void ReadCommonReg(u32 reg,bool byte);
void WriteCommonReg8(u32 reg,u32 data);
void channel_serialize(Serializer& ctx);
void channel_deserialize(Deserializer& ctx);
void vmuBeep(int on, int period);
