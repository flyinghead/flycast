/*
	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.

    Some PLFO and FEG code from Highly_Theoretical lib by Neill Corlett
    (https://gitlab.com/kode54/highly_theoretical)
 */

#include "sgc_if.h"
#include "aica.h"
#include "aica_if.h"
#include "aica_mem.h"
#include "dsp.h"
#include "audio/audiostream.h"
#include "hw/gdrom/gdrom_if.h"
#include "cfg/option.h"
#include "serialize.h"

#include <algorithm>
#include <cmath>

#undef FAR

//#define CLIP_WARN
#define key_printf(...) DEBUG_LOG(AICA, __VA_ARGS__)
#define aeg_printf(...) DEBUG_LOG(AICA, __VA_ARGS__)
#define feg_printf(...) DEBUG_LOG(AICA, __VA_ARGS__)
#define step_printf(...) DEBUG_LOG(AICA, __VA_ARGS__)

#ifdef CLIP_WARN
#define clip_verify(x) verify(x)
#else
#define clip_verify(x)
#endif

namespace aica::sgc
{
//Sound generation, mixin, and channel regs emulation
//x.15
static s32 volume_lut[16];
//255 -> mute
//Converts Send levels to TL-compatible values (DISDL, etc)
static const u32 SendLevel[16] =
{
	255, 14 << 3, 13 << 3, 12 << 3, 11 << 3, 10 << 3, 9 << 3, 8 << 3,
	7 << 3, 6 << 3, 5 << 3, 4 << 3, 3 << 3, 2 << 3, 1 << 3, 0 << 3
};
static s32 tl_lut[256 + 768];	//xx.15 format. >=255 is muted

//in ms :)
static const double AEG_Attack_Time[64] =
{
	-1,-1,8100.0,6900.0,6000.0,4800.0,4000.0,3400.0,3000.0,2400.0,2000.0,1700.0,1500.0,
	1200.0,1000.0,860.0,760.0,600.0,500.0,430.0,380.0,300.0,250.0,220.0,190.0,150.0,130.0,110.0,95.0,
	76.0,63.0,55.0,47.0,38.0,31.0,27.0,24.0,19.0,15.0,13.0,12.0,9.4,7.9,6.8,6.0,4.7,3.8,3.4,3.0,2.4,
	2.0,1.8,1.6,1.3,1.1,0.93,0.85,0.65,0.53,0.44,0.40,0.35,0.0,0.0
};
static const double AEG_DSR_Time[64] =
{
	-1,-1,118200.0,101300.0,88600.0,70900.0,59100.0,50700.0,44300.0,35500.0,29600.0,25300.0,22200.0,17700.0,
	14800.0,12700.0,11100.0,8900.0,7400.0,6300.0,5500.0,4400.0,3700.0,3200.0,2800.0,2200.0,1800.0,1600.0,1400.0,1100.0,
	920.0,790.0,690.0,550.0,460.0,390.0,340.0,270.0,230.0,200.0,170.0,140.0,110.0,98.0,85.0,68.0,57.0,49.0,43.0,34.0,
	28.0,25.0,22.0,18.0,14.0,12.0,11.0,8.5,7.1,6.1,5.4,4.3,3.6,3.1
};

static const float PLFOS_Scale[8] = { 0.f, 3.61f, 7.22f, 14.44f, 28.88f, 57.75f, 115.5f, 231.f };
static int PLFO_Scales[8][256];

#define EG_STEP_BITS (16)
#define AEG_ATTACK_SHIFT 16

//Steps per sample
static u32 AEG_ATT_SPS[64];
static u32 AEG_DSR_SPS[64];
static u32 FEG_SPS[64];

static const char* stream_names[]=
{
	"0: 16-bit PCM (two's complement format)",
	"1: 8-bit PCM (two's complement format)",
	"2: 4-bit ADPCM (Yamaha format)",
	"3: 4-bit ADPCM long stream"
};

//x.8 format
static const s32 adpcm_qs[8] =
{
	0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266,
};
//x.3 format
static const s32 adpcm_scale[16] =
{
	1,3,5,7,9,11,13,15,
	-1,-3,-5,-7,-9,-11,-13,-15,
};

static const s32 qtable[32] = {
0x0E00,0x0E80,0x0F00,0x0F80,
0x1000,0x1080,0x1100,0x1180,
0x1200,0x1280,0x1300,0x1380,
0x1400,0x1480,0x1500,0x1580,
0x1600,0x1680,0x1700,0x1780,
0x1800,0x1880,0x1900,0x1980,
0x1A00,0x1A80,0x1B00,0x1B80,
0x1C00,0x1D00,0x1E00,0x1F00
};

//Remove the fractional part by chopping..
static SampleType FPs(SampleType a, int bits) {
	return a >> bits;
}

//Fixed point mul w/ rounding :)
template<typename T>
static T FPMul(T a, T b, int bits) {
	return (a * b) >> bits;
}

static void VolumePan(SampleType value, u32 vol, u32 pan, SampleType& outl, SampleType& outr)
{
	SampleType temp = FPMul(value, volume_lut[vol], 15);
	SampleType Sc = FPMul(temp, volume_lut[0xF - (pan & 0xF)], 15);
	if (pan & 0x10)
	{
		outl += temp;
		outr += Sc;
	}
	else
	{
		outl += Sc;
		outr += temp;
	}
}

class VmuBeep
{
public:
	void init()
	{
		active = false;
		att = 256;
		waveIdx.full = 0;
		waveStep = 0;
	}

	void update(int period, int on)
	{
		if (on == 0 || period == 0 || on >= period) {
			active = false;
		}
		else
		{
			// on (duty cycle) is ignored
			active = true;
			// 6 MHz clock
			int freq = 6000000 / 6 / period;
			waveStep = freq * 1024 / 2698;
		}
	}

	SampleType getSample()
	{
		if (!active && att >= 256)
			return 0;

		waveIdx.full += waveStep;
		waveIdx.ip %= std::size(wave);
		int nextIdx = (waveIdx.ip + 1) % std::size(wave);
		SampleType s = (FPMul(wave[waveIdx.ip], (int)(1024 - waveIdx.fp), 10) + FPMul(wave[nextIdx], (int)waveIdx.fp, 10)) * 2;

		s = FPMul(s, tl_lut[att], 15);
		if (active)
			att = std::max(att - 2, 0);
		else
			att = std::min(att + 2, 256);
		return s;
	}

	void serialize(Serializer& ser)
	{
		ser << active;
		ser << att;
		ser << waveIdx;
		ser << waveStep;
	}

	void deserialize(Deserializer& deser)
	{
		if (deser.version() < Deserializer::V49)
		{
			if (deser.version() >= Deserializer::V22)
			{
				deser.skip<int>();	// beepOn
				deser.skip<int>();	// beepPeriod
				deser.skip<int>();	// beepCounter
			}
			init();
		}
		else
		{
			deser >> active;
			deser >> att;
			deser >> waveIdx;
			deser >> waveStep;
		}
	}

private:
	bool active = false;
	int att = 256;
	fp_22_10 waveIdx {};
	int waveStep = 0;

	// 2698 Hz
	static constexpr SampleType wave[] = {   503, -3519,
		-7540, -8214, -8209, -8214, -8209, -5199, -1172,  2843,
		 6873,  8207,  8214,  8209,  8212,  5866,  1840, -2175,
		-6203, -8210, -8215, -8209, -8214, -6533, -2516,  1507,
		 5526,  8212,  8210,  8212,  8210,  7206,  3187,  -841,
		-4856, -8215, -8209, -8214, -8208, -7882, -3850,   162,
		 4187,  8213,  8208,  8213,  8209,  8211,  4525 };
};
static VmuBeep beep;

#pragma pack(push, 1)
//All regs are 16b , aligned to 32b (upper bits 0?)
struct ChannelCommonData
{
	//+00 [0]
	//SA is half at reg 0 and the rest at reg 1
	u32 SA_hi:7;
	u32 PCMS:2;
	u32 LPCTL:1;
	u32 SSCTL:1;
	u32 :3;
	u32 KYONB:1;
	u32 KYONEX:1;

	u32 :16;

	//+04 [1]
	//SA (defined above)
	u32 SA_low:16;

	u32 :16;

	//+08 [2]
	u32 LSA:16;

	u32 :16;

	//+0C [3]
	u32 LEA:16;

	u32 :16;

	//+10 [4]
	u32 AR:5;
	u32 :1;
	u32 D1R:5;
	u32 D2R:5;

	u32 :16;

	//+14 [5]
	u32 RR:5;
	u32 DL:5;
	u32 KRS:4;
	u32 LPSLNK:1;
	u32 :1;

	u32 :16;

	//+18[6]
	u32 FNS:10;
	u32 :1;
	u32 OCT:4;
	u32 :1;

	u32 :16;

	//+1C	RE	LFOF[4:0]	PLFOWS	PLFOS[2:0]	ALFOWS	ALFOS[2:0]
	u32 ALFOS:3;
	u32 ALFOWS:2;

	u32 PLFOS:3;
	u32 PLFOWS:2;

	u32 LFOF:5;
	u32 LFORE:1;

	u32 :16;

	//+20	--	IMXL[3:0]	ISEL[3:0]
	u32 ISEL:4;
	u32 IMXL:4;
	u32 :8;

	u32 :16;

	//+24	--	DISDL[3:0]	--	DIPAN[4:0]
	u32 DIPAN:5;
	u32 :3;
	
	u32 DISDL:4;
	u32 :4;

	u32 :16;
	

	//+28	TL[7:0]	--	Q[4:0]
	u32 Q:5;
	u32 LPOFF:1;		// confirmed but not documented: 0: LPF enabled, 1: LPF disabled
	u32 VOFF:1;			// unconfirmed: 0: attenuation enabled, 1: attenuation disabled (TL, AEG, ALFO)
	u32 :1;

	u32 TL:8;

	u32 :16;

	//+2C	--	FLV0[12:0]
	u32 FLV0:13;
	u32 :3;

	u32 :16;

	//+30	--	FLV1[12:0]
	u32 FLV1:13;
	u32 :3;
	
	u32 :16;

	//+34	--	FLV2[12:0]
	u32 FLV2:13;
	u32 :3;
	
	u32 :16;

	//+38	--	FLV3[12:0]
	u32 FLV3:13;
	u32 :3;
	
	u32 :16;

	//+3C	--	FLV4[12:0]
	u32 FLV4:13;
	u32 :3;
	
	u32 :16;
	
	//+40	--	FAR[4:0]	--	FD1R[4:0]
	u32 FD1R:5;
	u32 :3;
	u32 FAR:5;
	u32 :3;

	u32 :16;

	//+44	--	FD2R[4:0]	--	FRR[4:0]
	u32 FRR:5;
	u32 :3;
	u32 FD2R:5;
	u32 :3;

	u32 :16;
};
#pragma pack(pop)


enum _EG_state
{
	EG_Attack = 0,
	EG_Decay1 = 1,
	EG_Decay2 = 2,
	EG_Release = 3
};

/*
	KEY_OFF->KEY_ON : Resets everything, and starts playback (EG: A)
	KEY_ON->KEY_ON  : nothing
	KEY_ON->KEY_OFF : Switches to RELEASE state (does not disable channel)

*/

struct ChannelEx;

static void (* STREAM_STEP_LUT[5][2][2])(ChannelEx* ch);
static void (* STREAM_INITAL_STEP_LUT[5])(ChannelEx* ch);
static void (* AEG_STEP_LUT[4])(ChannelEx* ch);
static void (* FEG_STEP_LUT[4])(ChannelEx* ch);
static void (* ALFOWS_CALC[4])(ChannelEx* ch);
static void (* PLFOWS_CALC[4])(ChannelEx* ch);

struct ChannelEx
{
	static ChannelEx Chans[64];

	ChannelCommonData* ccd;

	u8* SA;
	u32 CA;
	fp_22_10 step;
	u32 update_rate;

	SampleType s0,s1;

	struct
	{
		u32 LSA;
		u32 LEA;

		u8 looped;
	} loop;
	
	struct
	{
		//used in adpcm decoding
		s32 last_quant;
		// Saved quantization and previous sample used in PCMS mode 2 (yamaha ADPCM)
		s32 loopstart_quant;
		SampleType loopstart_prev_sample;
		bool in_loop;

		void Reset(ChannelEx* ch)
		{
			last_quant=127;
			loopstart_quant = 0;
			loopstart_prev_sample = 0;
			in_loop = false;

			ch->s0=0;
		}
	} adpcm;

	u32 noise_state;//for Noise generator

	struct
	{
		u32 DLAtt;
		u32 DRAtt;
		u32 DSPAtt;
		SampleType* DSPOut;
	} VolMix;
	
	void (* StepAEG)(ChannelEx* ch);
	void (* StepFEG)(ChannelEx* ch);
	void (* StepStream)(ChannelEx* ch);
	void (* StepStreamInitial)(ChannelEx* ch);
	
	struct
	{
		s32 val;
		s32 GetValue() { return val >> EG_STEP_BITS;}
		void SetValue(u32 aegb) { val = aegb << EG_STEP_BITS; }

		_EG_state state=EG_Attack;

		u32 AttackRate;
		u32 Decay1Rate;
		u32 Decay2Value;
		u32 Decay2Rate;
		u32 ReleaseRate;
	} AEG;
	
	struct
	{
		u32 value;
		u32 GetValue() { return value >> EG_STEP_BITS;}
		void SetValue(u32 fegb) { value = fegb << EG_STEP_BITS; }

		_EG_state state = EG_Attack;

		SampleType prev1;
		SampleType prev2;
		s32 q;
		u32 AttackRate;
		u32 Decay1Rate;
		u32 Decay2Rate;
		u32 ReleaseRate;
		bool active = false;
	} FEG;
	
	struct 
	{
		u32 counter;
		u32 start_value;
		u8 state;
		u8 alfo;
		u8 alfo_shft;
		fp_22_10 plfo_step;
		int *plfo_scale;
		void (* alfo_calc)(ChannelEx* ch);
		void (* plfo_calc)(ChannelEx* ch);
		void Step(ChannelEx* ch) { counter--;if (counter==0) { state++; counter=start_value; alfo_calc(ch);plfo_calc(ch); } }
		void Reset(ChannelEx* ch) { state=0; counter=start_value; alfo_calc(ch); plfo_calc(ch); }
		void SetStartValue(u32 nv) { start_value = nv;}
	} lfo;

	bool enabled;	//set to false to 'freeze' the channel
	bool quiet;
	int ChannelNumber;

	void Init(int cn,u8* ccd_raw)
	{
		ccd=(ChannelCommonData*)&ccd_raw[cn*0x80];
		ChannelNumber = cn;
		quiet = true;
		for (u32 i = 0; i < 0x80; i += 2)
			RegWrite(i, 2);
		quiet = false;
		disable();
	}

	void disable()
	{
		enabled=false;
		SetAegState(EG_Release);
		AEG.SetValue(0x3FF);
		CA = 0;
	}

	void enable()
	{
		enabled=true;
	}

	SampleType InterpolateSample()
	{
		SampleType rv;
		u32 fp=step.fp;
		rv=FPMul(s0,(s32)(1024-fp),10);
		rv+=FPMul(s1,(s32)(fp),10);

		return rv;
	}

	bool Step(SampleType& oLeft, SampleType& oRight, SampleType& oDsp)
	{
		if (!enabled)
		{
			oLeft=oRight=oDsp=0;
			return false;
		}
		else
		{
			SampleType sample = InterpolateSample();

			// Low-pass filter
			if (FEG.active)
			{
				u32 fv = FEG.GetValue();
				s32 f = (((fv & 0x1FF) | 0x200) << 3) >> ((fv >> 9) ^ 0xF);
				if (f == 0) {
					sample = 0;
				}
				else
				{
					sample = f * sample + (0x2000 - f + FEG.q) * FEG.prev1 - FEG.q * FEG.prev2;
					sample >>= 13;
					sample = std::clamp(sample, -32768, 32767);
				}
				FEG.prev2 = FEG.prev1;
				FEG.prev1 = sample;
			}

			//Volume & Mixer processing
			//All attenuations are added together then applied and mixed :)

			//offset is up to 511
			//*Att is up to 511
			//logtable handles up to 1024, anything >=255 is mute

			u32 ofsatt;
			if (ccd->VOFF == 1)
			{
				ofsatt = 0;
			}
			else
			{
				ofsatt = lfo.alfo + (AEG.GetValue() >> 2);
				ofsatt = std::min(ofsatt, (u32)255); // make sure it never gets more 255 -- it can happen with some alfo/aeg combinations
			}
			u32 const max_att = ((16 << 4) - 1) - ofsatt;
			
			s32* logtable = ofsatt + tl_lut;

			u32 dl = std::min(VolMix.DLAtt, max_att);
			u32 dr = std::min(VolMix.DRAtt, max_att);
			u32 ds = std::min(VolMix.DSPAtt, max_att);

			oLeft = FPMul(sample, logtable[dl], 15);
			oRight = FPMul(sample, logtable[dr], 15);
			oDsp = FPMul(sample, logtable[ds], 11);	// 20 bits

			clip_verify(((s16)oLeft)==oLeft);
			clip_verify(((s16)oRight)==oRight);
			clip_verify((oDsp << 12) >> 12 == oDsp);
			clip_verify(sample*oLeft>=0);
			clip_verify(sample*oRight>=0);
			clip_verify((s64)sample*oDsp>=0);

			StepAEG(this);
			if (enabled)
			{
				StepFEG(this);
				StepStream(this);
				lfo.Step(this);
			}
			return true;
		}
	}

	void Step(SampleType& mixl, SampleType& mixr)
	{
		SampleType oLeft,oRight,oDsp;

		Step(oLeft, oRight, oDsp);

		*VolMix.DSPOut += oDsp;
		if (oLeft + oRight == 0 && !config::DSPEnabled)
			oLeft = oRight = oDsp >> 4;

		mixl+=oLeft;
		mixr+=oRight;
	}

	static void StepAll(SampleType& mixl, SampleType& mixr)
	{
		for (ChannelEx& channel : Chans)
			channel.Step(mixl, mixr);
	}

	void SetAegState(_EG_state newstate)
	{
		StepAEG=AEG_STEP_LUT[newstate];
		AEG.state=newstate;
		if (newstate==EG_Release)
			ccd->KYONB=0;
	}

	void SetFegState(_EG_state newstate)
	{
		StepFEG = FEG_STEP_LUT[newstate];
		FEG.state = newstate;
		if (newstate == EG_Attack)
		{
			FEG.SetValue(ccd->FLV0);
			FEG.prev1 = 0;
			FEG.prev2 = 0;
		}
	}

	void KEY_ON()
	{
		if (AEG.state != EG_Release)
			return;

		enable();

		// reset AEG
		SetAegState(EG_Attack);
		AEG.SetValue(0x280);	// start value taken from HT

		// reset FEG
		SetFegState(EG_Attack);

		// reset sampling state
		CA = 0;
		step.full = 0;

		loop.looped = false;
		if (loop.LEA <= loop.LSA && ccd->LPCTL == 1)
			// Legacy of Kain
			loop.LEA = 0xffff;

		adpcm.Reset(this);

		StepStreamInitial(this);
		key_printf("[%d] KEY_ON %s @ %f Hz, loop %d - AEG AR %d DC1R %d DC2V %d DC2R %d RR %d - KRS %d OCT %d FNS %d - PFLOS %d PFLOWS %d - SA %x LSA %x LEA %x",
				ChannelNumber, stream_names[ccd->PCMS], (44100.0 * update_rate) / 1024, ccd->LPCTL,
				ccd->AR, ccd->D1R, ccd->DL << 5, ccd->D2R, ccd->RR,
				ccd->KRS, ccd->OCT, ccd->FNS,
				ccd->PLFOS, ccd->PLFOWS, (int)(SA - &aica_ram[0]), ccd->LSA, ccd->LEA);
	}

	void KEY_OFF()
	{
		if (AEG.state == EG_Release)
			return;
		key_printf("[%d] KEY_OFF -> Release", ChannelNumber);
		SetAegState(EG_Release);
		SetFegState(EG_Release);
	}

	//PCMS,SSCTL,LPCTL,LPSLNK
	void UpdateStreamStep()
	{
		s32 fmt=ccd->PCMS;
		if (ccd->SSCTL)
			fmt=4;

		StepStream=STREAM_STEP_LUT[fmt][ccd->LPCTL][ccd->LPSLNK];
		StepStreamInitial=STREAM_INITAL_STEP_LUT[fmt];
	}
	//SA,PCMS
	void UpdateSA()
	{
		u32 addr = (ccd->SA_hi << 16) | ccd->SA_low;
		if (ccd->PCMS == 0)
			addr &= ~1; //0: 16 bit
		
		SA = &aica_ram[addr & ARAM_MASK];
	}
	//LSA,LEA
	void UpdateLoop()
	{
		loop.LSA = ccd->LSA;
		loop.LEA = ccd->LEA;
	}

	s32 EG_BaseRate()
	{
		s32 effrate = 0;
		if (ccd->KRS < 0xF)
		{
		    effrate += (ccd->FNS >> 9) & 1;
		    effrate += ccd->KRS * 2;
		    effrate += (ccd->OCT ^ 8) - 8;
		}

		return effrate;
	}

	u32 EG_EffRate(s32 base_rate, u32 rate)
	{
		s32 rv = base_rate + rate * 2;
		return std::clamp(rv, 0, 0x3f);
	}

	//D2R,D1R,AR,DL,RR,KRS, [OCT,FNS] for now
	void UpdateAEG()
	{
		s32 base_rate = EG_BaseRate();
		AEG.AttackRate = AEG_ATT_SPS[EG_EffRate(base_rate, ccd->AR)];
		AEG.Decay1Rate = AEG_DSR_SPS[EG_EffRate(base_rate, ccd->D1R)];
		AEG.Decay2Value = ccd->DL<<5;
		AEG.Decay2Rate = AEG_DSR_SPS[EG_EffRate(base_rate, ccd->D2R)];
		AEG.ReleaseRate = AEG_DSR_SPS[EG_EffRate(base_rate, ccd->RR)];
	}
	//OCT,FNS
	void UpdatePitch()
	{
		u32 oct=ccd->OCT;

		u32 update_rate = 1024 | ccd->FNS;
		if (oct& 8)
			update_rate>>=(16-oct);
		else
			update_rate<<=oct;

		this->update_rate=update_rate;
	}

	//LFORE,LFOF,PLFOWS,PLFOS,ALFOWS,ALFOS
	void UpdateLFO(bool derivedState)
	{
		{
			int N=ccd->LFOF;
			int S = N >> 2;
			int M = (~N) & 3;
			int G = 128>>S;
			int L = (G-1)<<2;
			int O = L + G * (M+1);
			lfo.SetStartValue(O);
			if (!derivedState)
				lfo.counter = O;
		}

		lfo.alfo_shft=8-ccd->ALFOS;

		lfo.alfo_calc=ALFOWS_CALC[ccd->ALFOWS];
		lfo.plfo_calc=PLFOWS_CALC[ccd->PLFOWS];
		lfo.plfo_scale = PLFO_Scales[ccd->PLFOS];

		if (ccd->LFORE && !derivedState)
		{
			lfo.Reset(this);
		}
		else
		{
			lfo.alfo_calc(this);
			lfo.plfo_calc(this);
		}
	}

	//ISEL
	void UpdateDSPMIX()
	{
		VolMix.DSPOut = &dsp::state.MIXS[ccd->ISEL];
	}
	//TL,DISDL,DIPAN,IMXL
	void UpdateAtts()
	{
		u32 total_level = ccd->VOFF ? 0 : ccd->TL;
		u32 attFull = total_level + SendLevel[ccd->DISDL];
		u32 attPan = attFull + SendLevel[(~ccd->DIPAN) & 0xF];

		//0x1* -> R decreases
		if (ccd->DIPAN&0x10)
		{
			VolMix.DLAtt=attFull;
			VolMix.DRAtt=attPan;
		}
		else //0x0* -> L decreases
		{
			VolMix.DLAtt=attPan;
			VolMix.DRAtt=attFull;
		}

		VolMix.DSPAtt = total_level + SendLevel[ccd->IMXL];
	}

	//Q,FLV0,FLV1,FLV2,FLV3,FLV4,FAR,FD1R,FD2R,FRR, LPOFF
	void UpdateFEG()
	{
		FEG.active = ccd->LPOFF == 0
				&& (ccd->FLV0 < 0x1ff7 || ccd->FLV1 < 0x1ff7
						|| ccd->FLV2 < 0x1ff7 || ccd->FLV3 < 0x1ff7
						|| ccd->FLV4 < 0x1ff7);
		if (!FEG.active)
			return;
		if (!quiet)
			feg_printf("FEG active channel %d Q %d FLV: %05x %05x %05x %05x %05x AR %02x FD1R %02x FD2R %02x FRR %02x",
					ChannelNumber, ccd->Q,
					ccd->FLV0, ccd->FLV1, ccd->FLV2, ccd->FLV3, ccd->FLV4,
					ccd->FAR, ccd->FD1R, ccd->FD2R, ccd->FRR);
		FEG.q = qtable[ccd->Q];
		s32 base_rate = EG_BaseRate();
		FEG.AttackRate = FEG_SPS[EG_EffRate(base_rate, ccd->FAR)];
		FEG.Decay1Rate = FEG_SPS[EG_EffRate(base_rate, ccd->FD1R)];
		FEG.Decay2Rate = FEG_SPS[EG_EffRate(base_rate, ccd->FD2R)];
		FEG.ReleaseRate = FEG_SPS[EG_EffRate(base_rate, ccd->FRR)];
	}
	
	void RegWrite(u32 offset, int size)
	{
		switch (offset)
		{
		case 0x00: // PCMS, SA
		case 0x01: // KYONEX, KYONB, SSCTL, LPCTL, PCMS
			UpdateStreamStep();
			if (offset == 0 || size == 2)
				UpdateSA();
			if ((offset == 1 || size == 2) && ccd->KYONEX)
			{
				ccd->KYONEX=0;
				for (ChannelEx& channel : Chans)
				{
					if (channel.ccd->KYONB)
						channel.KEY_ON();
					else
						channel.KEY_OFF();
				}
			}
			break;

		case 0x04: // SA
		case 0x05: // SA
			UpdateSA();
			break;

		case 0x08://LSA
		case 0x09://LSA
		case 0x0C://LEA
		case 0x0D://LEA
			UpdateLoop();
			break;

		case 0x10://D1R,AR
		case 0x11://D2R,D1R
			UpdateAEG();
			break;

		case 0x14://RR,DL
		case 0x15://DL,KRS,LPSLINK
			UpdateStreamStep();
			UpdateAEG();
			break;

		case 0x18://FNS
		case 0x19://FNS,OCT
			UpdatePitch();
			UpdateAEG();
			UpdateFEG();
			break;

		case 0x1C://ALFOS,ALFOWS,PLFOS
		case 0x1D://PLFOWS,LFOF,LFORE
			UpdateLFO(false);
			break;

		case 0x20://ISEL,IMXL
		//case 0x21://nothing here !
			UpdateDSPMIX();
			UpdateAtts();
			break;

		case 0x24://DIPAN
		case 0x25://DISDL
			UpdateAtts();
			break;

		case 0x28://Q, LPOFF
		case 0x29://TL
			if (size == 2 || offset == 0x28)
				UpdateFEG();
			if (size == 2 || offset == 0x29)
				UpdateAtts();
			break;

		case 0x2C: //FLV0
		case 0x2D: //FLV0
		case 0x30: //FLV1
		case 0x31: //FLV1
		case 0x34: //FLV2
		case 0x35: //FLV2
		case 0x38: //FLV3
		case 0x39: //FLV3
		case 0x3C: //FLV4
		case 0x3D: //FLV4
		case 0x40: //FD1R
		case 0x41: //FAR
		case 0x44: //FRR
		case 0x45: //FD2R
			UpdateFEG();
			break;

		}
	} 

	static void initAll() {
		for (std::size_t i = 0; i < std::size(Chans); i++)
			Chans[i].Init(i, aica_reg);
	}
};

static SampleType DecodeADPCM(u32 sample,s32 prev,s32& quant)
{
	s32 sign=1-2*(sample/8);

	u32 data=sample&7;

	/*(1 - 2 * L4) * (L3 + L2/2 +L1/4 + 1/8) * quantized width (Dn) + decode value (Xn - 1) */
	SampleType rv = (quant * adpcm_scale[data]) >> 3;
	if (rv > 0x7FFF)
		rv = 0x7FFF;
	rv = sign * rv + prev;

	quant = (quant * adpcm_qs[data])>>8;
	quant = std::clamp(quant, 127, 24576);

	return std::clamp(rv, -32768, 32767);
}

template<s32 PCMS,bool last>
void StepDecodeSample(ChannelEx* ch,u32 CA)
{
	if (!last && PCMS<2)
		return ;

	// TODO bound checking of sample addresses
	s16* sptr16=(s16*)ch->SA;
	s8* sptr8=(s8*)sptr16;
	u8* uptr8=(u8*)sptr16;
	u32 next_addr = CA + 1;
	if (next_addr >= ch->loop.LEA)
		next_addr = ch->loop.LSA;

	SampleType s0,s1;
	switch(PCMS)
	{
	case -1:
		ch->noise_state = ch->noise_state*16807 + 0xbeef;	//beef is good

		s0=ch->noise_state;
		s0>>=16;
		
		s1=ch->noise_state*16807 + 0xbeef;
		s1>>=16;
		break;

	case 0:
		s0 = sptr16[CA];
		s1 = sptr16[next_addr];
		break;

	case 1:
		s0 = sptr8[CA] << 8;
		s1 = sptr8[next_addr] << 8;
		break;

	case 2:
	case 3:
		{
			u8 ad1 = uptr8[CA >> 1];
			u8 ad2 = uptr8[next_addr >> 1];

			ad1 >>= (CA & 1) * 4;
			ad2 >>= (next_addr & 1) * 4;

			ad1 &= 0xF;
			ad2 &= 0xF;

			s32 q = ch->adpcm.last_quant;
			if (PCMS == 2 && CA == ch->loop.LSA)
			{
				if (!ch->adpcm.in_loop)
				{
					ch->adpcm.in_loop = true;
					ch->adpcm.loopstart_quant = q;
					ch->adpcm.loopstart_prev_sample = ch->s0;
				}
				else
				{
					q = ch->adpcm.loopstart_quant;
					ch->s0 = ch->adpcm.loopstart_prev_sample;
				}
			}
			s0 = DecodeADPCM(ad1, ch->s0, q);
			ch->adpcm.last_quant = q;
			if (last)
			{
				SampleType prev = s0;
				if (PCMS == 2 && next_addr == ch->loop.LSA && ch->adpcm.in_loop)
				{
					q = ch->adpcm.loopstart_quant;
					prev = ch->adpcm.loopstart_prev_sample;
				}
				s1 = DecodeADPCM(ad2, prev, q);
			}
			else
				s1 = 0;
		}
		break;
	}
	
	ch->s0=s0;
	ch->s1=s1;
}



template<s32 PCMS>
void StepDecodeSampleInitial(ChannelEx* ch)
{
	StepDecodeSample<PCMS,true>(ch,0);
}
template<s32 PCMS,u32 LPCTL,u32 LPSLNK>
void StreamStep(ChannelEx* ch)
{
	ch->step.full += (ch->update_rate * ch->lfo.plfo_step.full) >> 10;
	fp_22_10 sp=ch->step;
	ch->step.ip=0;

	while(sp.ip>0)
	{
		sp.ip--;

		u32 CA=ch->CA + 1;

		u32 ca_t=CA;
		if (PCMS==3)
			ca_t&=~3;	// in adpcm "stream" mode, LEA and LSA are supposed to be 4-sample aligned
						// but some games don't respect this rule

		if (LPSLNK)
		{
			if ((ch->AEG.state==EG_Attack) && (CA>=ch->loop.LSA))
			{
				step_printf("[%d]LPSLNK : Switching to EG_Decay1 %X", ch->ChannelNumber, ch->AEG.GetValue());
				ch->SetAegState(EG_Decay1);
			}
		}

		if (ca_t >= ch->loop.LEA)
		{
			ch->loop.looped = 1;
			if (LPCTL == 0)
			{
				CA = 0;
				ch->disable();
			}
			else
			{
				CA = ch->loop.LSA;
				key_printf("[%d]LPCTL : Looping LSA %x LEA %x AEG %x", ch->ChannelNumber, ch->loop.LSA, ch->loop.LEA, ch->AEG.GetValue());
			}
		}

		ch->CA=CA;

		//keep adpcm up to date
		if (sp.ip==0)
			StepDecodeSample<PCMS,true>(ch,CA);
		else
			StepDecodeSample<PCMS,false>(ch,CA);
	}


}

enum class LFOType
{
	Sawtooth,
	Square,
	Triangle,
	Random
};

template<LFOType Type>
void CalcAlfo(ChannelEx* ch)
{
	u32 rv;
	switch(Type)
	{
	case LFOType::Sawtooth:
		rv=ch->lfo.state;
		break;

	case LFOType::Square:
		rv=ch->lfo.state&0x80?255:0;
		break;

	case LFOType::Triangle:
		rv=(ch->lfo.state&0x7f)^(ch->lfo.state&0x80 ? 0x7F:0);
		rv<<=1;
		break;

	case LFOType::Random: // ... not so much
		rv=(ch->lfo.state>>3)^(ch->lfo.state<<3)^(ch->lfo.state&0xE3);
		break;
	}
	ch->lfo.alfo=rv>>ch->lfo.alfo_shft;
}

template<LFOType Type>
void CalcPlfo(ChannelEx* ch)
{
	u32 rv;
	switch(Type)
	{
	case LFOType::Sawtooth:
		rv = ch->lfo.state;
		break;

	case LFOType::Square:
		rv = ch->lfo.state & 0x80 ? 0xff : 0;
		break;

	case LFOType::Triangle:
		rv = (ch->lfo.state & 0x7f) ^ (ch->lfo.state & 0x80 ? 0x7F : 0);
		rv <<= 1;
		break;

	case LFOType::Random:
		rv = (ch->lfo.state >> 3) ^ (ch->lfo.state << 3) ^ (ch->lfo.state & 0xE3);
		break;
	}
	ch->lfo.plfo_step.full = ch->lfo.plfo_scale[(u8)rv];
}

template<u32 state>
void AegStep(ChannelEx* ch)
{
	switch(state)
	{
	case EG_Attack:
		if (ch->AEG.AttackRate != 0)
		{
			ch->AEG.val -= (((u64)ch->AEG.val << AEG_ATTACK_SHIFT) / ch->AEG.AttackRate) + 1;
			if (ch->AEG.GetValue() <= 0)
			{
				if (!ch->ccd->LPSLNK)
				{
					aeg_printf("[%d]AEG_step : Switching to EG_Decay1", ch->ChannelNumber);
					ch->SetAegState(EG_Decay1);
				}
				ch->AEG.SetValue(0);
			}
		}
		break;
	case EG_Decay1:
		ch->AEG.val += ch->AEG.Decay1Rate;
		if (((u32)ch->AEG.GetValue()) >= ch->AEG.Decay2Value)
		{
			aeg_printf("[%d]AEG_step : Switching to EG_Decay2", ch->ChannelNumber);
			ch->SetAegState(EG_Decay2);
		}
		break;
	case EG_Decay2:
		ch->AEG.val += ch->AEG.Decay2Rate;
		if (ch->AEG.GetValue() >= 0x3FF)
		{
			aeg_printf("[%d]AEG_step : Switching to EG_Release", ch->ChannelNumber);
			ch->AEG.SetValue(0x3FF);
			ch->SetAegState(EG_Release);
		}
		break;
	case EG_Release: //only on key_off
		ch->AEG.val += ch->AEG.ReleaseRate;
		if (ch->AEG.GetValue() >= 0x3FF)
		{
			aeg_printf("[%d]AEG_step : EG_Release End @ %x", ch->ChannelNumber, ch->AEG.GetValue());
			ch->disable();
		}
		break;
	}
}
template<u32 state>
void FegStep(ChannelEx* ch)
{
	if (!ch->FEG.active)
		return;
	u32 delta;
	u32 target;
	switch(state)
	{
	case EG_Attack:
		delta = ch->FEG.AttackRate;
		target = ch->ccd->FLV1;
		break;
	case EG_Decay1:
		delta = ch->FEG.Decay1Rate;
		target = ch->ccd->FLV2;
		break;
	case EG_Decay2:
		delta = ch->FEG.Decay2Rate;
		target = ch->ccd->FLV3;
		break;
	case EG_Release:
		delta = ch->FEG.ReleaseRate;
		target = ch->ccd->FLV4;
		break;
	}
	target <<= EG_STEP_BITS;
	if (ch->FEG.value < target)
	{
		u32 maxd = target - ch->FEG.value;
		if (delta > maxd)
			delta = maxd;
		ch->FEG.value += delta;
	}
	else if (ch->FEG.value > target)
	{
		u32 maxd = ch->FEG.value - target;
		if (delta > maxd)
			delta = maxd;
		ch->FEG.value -= delta;
	}
	else if (ch->FEG.state < EG_Decay2)
	{
		feg_printf("[%d]FEG_step : Switching to next state: %d Freq %x", ch->ChannelNumber, (int)ch->FEG.state + 1, target >> EG_STEP_BITS);
		ch->SetFegState((_EG_state)((int)ch->FEG.state + 1));
	}
}

static u32 CalcEgSteps(float t)
{
	const double eg_allsteps = 1024 * (1 << EG_STEP_BITS) - 1;

	if (t < 0)
		return 0;
	if (t == 0)
		return (u32)eg_allsteps;

	//44.1*ms = samples
	double scnt = 44.1 * t;
	double steps = eg_allsteps / scnt;
	return (u32)lround(steps);
}
static u32 CalcAttackEgSteps(float t)
{
	if (t < 0)
		return 0;
	if (t == 0)
		return 1 << AEG_ATTACK_SHIFT;

	//44.1*ms = samples
	double scnt = 44.1 * t;
	double factor = (1.0 / (1.0 - 1.0 / pow(0x280, 1.0 / scnt))) * (1 << AEG_ATTACK_SHIFT);

	return (u32)lround(factor);
}

static void staticinitialise()
{
	STREAM_STEP_LUT[0][0][0]=&StreamStep<0,0,0>;
	STREAM_STEP_LUT[1][0][0]=&StreamStep<1,0,0>;
	STREAM_STEP_LUT[2][0][0]=&StreamStep<2,0,0>;
	STREAM_STEP_LUT[3][0][0]=&StreamStep<3,0,0>;
	STREAM_STEP_LUT[4][0][0]=&StreamStep<-1,0,0>;

	STREAM_STEP_LUT[0][0][1]=&StreamStep<0,0,1>;
	STREAM_STEP_LUT[1][0][1]=&StreamStep<1,0,1>;
	STREAM_STEP_LUT[2][0][1]=&StreamStep<2,0,1>;
	STREAM_STEP_LUT[3][0][1]=&StreamStep<3,0,1>;
	STREAM_STEP_LUT[4][0][1]=&StreamStep<-1,0,1>;

	STREAM_STEP_LUT[0][1][0]=&StreamStep<0,1,0>;
	STREAM_STEP_LUT[1][1][0]=&StreamStep<1,1,0>;
	STREAM_STEP_LUT[2][1][0]=&StreamStep<2,1,0>;
	STREAM_STEP_LUT[3][1][0]=&StreamStep<3,1,0>;
	STREAM_STEP_LUT[4][1][0]=&StreamStep<-1,1,0>;

	STREAM_STEP_LUT[0][1][1]=&StreamStep<0,1,1>;
	STREAM_STEP_LUT[1][1][1]=&StreamStep<1,1,1>;
	STREAM_STEP_LUT[2][1][1]=&StreamStep<2,1,1>;
	STREAM_STEP_LUT[3][1][1]=&StreamStep<3,1,1>;
	STREAM_STEP_LUT[4][1][1]=&StreamStep<-1,1,1>;

	STREAM_INITAL_STEP_LUT[0]=&StepDecodeSampleInitial<0>;
	STREAM_INITAL_STEP_LUT[1]=&StepDecodeSampleInitial<1>;
	STREAM_INITAL_STEP_LUT[2]=&StepDecodeSampleInitial<2>;
	STREAM_INITAL_STEP_LUT[3]=&StepDecodeSampleInitial<3>;
	STREAM_INITAL_STEP_LUT[4]=&StepDecodeSampleInitial<-1>;

	AEG_STEP_LUT[EG_Attack] = &AegStep<EG_Attack>;
	AEG_STEP_LUT[EG_Decay1] = &AegStep<EG_Decay1>;
	AEG_STEP_LUT[EG_Decay2] = &AegStep<EG_Decay2>;
	AEG_STEP_LUT[EG_Release] = &AegStep<EG_Release>;

	FEG_STEP_LUT[EG_Attack] = &FegStep<EG_Attack>;
	FEG_STEP_LUT[EG_Decay1] = &FegStep<EG_Decay1>;
	FEG_STEP_LUT[EG_Decay2] = &FegStep<EG_Decay2>;
	FEG_STEP_LUT[EG_Release] = &FegStep<EG_Release>;

	ALFOWS_CALC[(int)LFOType::Sawtooth] = &CalcAlfo<LFOType::Sawtooth>;
	ALFOWS_CALC[(int)LFOType::Square] = &CalcAlfo<LFOType::Square>;
	ALFOWS_CALC[(int)LFOType::Triangle] = &CalcAlfo<LFOType::Triangle>;
	ALFOWS_CALC[(int)LFOType::Random] = &CalcAlfo<LFOType::Random>;

	PLFOWS_CALC[(int)LFOType::Sawtooth] = &CalcPlfo<LFOType::Sawtooth>;
	PLFOWS_CALC[(int)LFOType::Square] = &CalcPlfo<LFOType::Square>;
	PLFOWS_CALC[(int)LFOType::Triangle] = &CalcPlfo<LFOType::Triangle>;
	PLFOWS_CALC[(int)LFOType::Random] = &CalcPlfo<LFOType::Random>;

	for (std::size_t i = 1; i < std::size(volume_lut); i++)
		volume_lut[i] = (s32)((1 << 15) / pow(2.0, (15 - i) / 2.0));

	for (int i = 0; i < 256; i++)
		tl_lut[i] = (s32)((1 << 15) / pow(2.0, i / 16.0));
	//tl entries 256 to 1023 are 0

	for (int i = 0; i < 64; i++)
	{
		AEG_ATT_SPS[i] = CalcAttackEgSteps(AEG_Attack_Time[i]);
		AEG_DSR_SPS[i] = CalcEgSteps(AEG_DSR_Time[i]);
		// The AEG range is 1024, while the FEG range is 8912.
		// So decay times are x8 greater for FEG than AEG
		// instead of x4 as mentioned in the doc.
		// However it sounds better this way.
		FEG_SPS[i] = AEG_DSR_SPS[i];
	}

	for (int s = 0; s < 8; s++)
	{
		float limit = PLFOS_Scale[s];
		for (int i = -128; i < 128; i++)
			PLFO_Scales[s][i + 128] = (u32)((1 << 10) * powf(2.0f, limit * i / 128.0f / 1200.0f));
	}
}
static OnLoad staticInit(staticinitialise);

ChannelEx ChannelEx::Chans[64];

#define Chans ChannelEx::Chans

void init()
{
	ChannelEx::initAll();
	beep.init();
	dsp::init();
}

void term()
{
	dsp::term();
}

void WriteChannelReg(u32 channel, u32 reg, int size)
{
	Chans[channel].RegWrite(reg, size);
}

void ReadCommonReg(u32 reg,bool byte)
{
	switch(reg)
	{
	case 0x2808:
	case 0x2809:
		if (!midiSendBuffer.empty())
		{
			if (!byte || reg == 0x2808)
			{
				CommonData->MIBUF = midiSendBuffer.front();
				midiSendBuffer.pop_front();
			}
			CommonData->MIEMP = 0;
			CommonData->MIFUL = 1;
		}
		else
		{
			CommonData->MIEMP = 1;
			CommonData->MIFUL = 0;
		}
		CommonData->MIOVF = 0;
		CommonData->MOEMP = 1;
		CommonData->MOFUL = 0;
		break;
	case 0x2810: // EG, SGC, LP
	case 0x2811:
		{
			u32 chan=CommonData->MSLC;
			
			CommonData->LP=Chans[chan].loop.looped;
			if (CommonData->AFSEL == 1)
				WARN_LOG(AICA, "FEG monitor (AFSEL=1) not supported");
			s32 aeg = Chans[chan].AEG.GetValue();
			if (aeg > 0x3BF)
				CommonData->EG = 0x1FFF;
			else
				CommonData->EG = aeg; //AEG is only 10 bits, FEG is 13 bits
			CommonData->SGC=Chans[chan].AEG.state;

			if (!byte || reg == 0x2811)
				Chans[chan].loop.looped = 0;
		}
		break;
	case 0x2814: //CA
	case 0x2815: //CA
		{
			u32 chan=CommonData->MSLC;
			CommonData->CA = Chans[chan].CA;
			//printf("[%d] CA read %d\n",chan,Chans[chan].CA);
		}
		break;
	}
}

void vmuBeep(int on, int period)
{
	beep.update(on, period);
}

constexpr int CDDA_SIZE = 2352 / 2;
static s16 cdda_sector[CDDA_SIZE];
static u32 cdda_index = CDDA_SIZE;

void AICA_Sample()
{
	SampleType mixl,mixr;
	mixl = 0;
	mixr = 0;
	memset(dsp::state.MIXS, 0, sizeof(dsp::state.MIXS));

	ChannelEx::StepAll(mixl,mixr);
	
	//OK , generated all Channels  , now DSP/ect + final mix ;p
	//CDDA EXTS input
	
	if (cdda_index>=CDDA_SIZE)
	{
		cdda_index=0;
		libCore_CDDA_Sector(cdda_sector);
	}
	s32 EXTS0L=cdda_sector[cdda_index];
	s32 EXTS0R=cdda_sector[cdda_index+1];
	cdda_index+=2;

	//Final MIX ..
	//Add CDDA / DSP effect(s)

	//CDDA
	VolumePan(EXTS0L, dsp_out_vol[16].EFSDL, dsp_out_vol[16].EFPAN, mixl, mixr);
	VolumePan(EXTS0R, dsp_out_vol[17].EFSDL, dsp_out_vol[17].EFPAN, mixl, mixr);

	DSPData->EXTS[0] = EXTS0L;
	DSPData->EXTS[1] = EXTS0R;

	if (config::DSPEnabled)
	{
		dsp::step();

		for (int i=0;i<16;i++)
			VolumePan(*(s16*)&DSPData->EFREG[i], dsp_out_vol[i].EFSDL, dsp_out_vol[i].EFPAN, mixl, mixr);
	}

#ifdef LIBRETRO
	if (settings.aica.muteAudio)
#else
	if (settings.input.fastForwardMode || settings.aica.muteAudio)
#endif
		return;

	if (config::VmuSound)
	{
		SampleType b = beep.getSample();
		mixl += b;
		mixr += b;
	}

	// Mono
	if (CommonData->Mono)
		mixl = mixr = FPs(mixl + mixr, 1);
	
	//MVOL !
	//we want to make sure mix* is *At least* 23 bits wide here, so 64 bit mul !
	u32 mvol=CommonData->MVOL;
	s32 val=volume_lut[mvol];
	mixl = (s32)FPMul<s64>(mixl, val, 15);
	mixr = (s32)FPMul<s64>(mixr, val, 15);

	if (CommonData->DAC18B)
	{
		//If 18 bit output , make it 16b :p
		mixl=FPs(mixl,2);
		mixr=FPs(mixr,2);
	}

	//Sample is ready ! clip/saturate and store :}

#ifdef CLIP_WARN
	if (((s16)mixl) != mixl || ((s16)mixr) != mixr)
		printf("Clipped mixl %d mixr %d\n", mixl, mixr);
#endif

	mixl = std::clamp(mixl, -32768, 32767);
	mixr = std::clamp(mixr, -32768, 32767);

	WriteSample(mixr,mixl);
}

void serialize(Serializer& ser)
{
	for (const ChannelEx& channel : Chans)
	{
		u32 addr = channel.SA - &aica_ram[0];
		ser << addr;

		ser << channel.CA;
		ser << channel.step;
		ser << channel.s0;
		ser << channel.s1;
		ser << channel.loop.looped;
		ser << channel.adpcm.last_quant;
		ser << channel.adpcm.loopstart_quant;
		ser << channel.adpcm.loopstart_prev_sample;
		ser << channel.adpcm.in_loop;
		ser << channel.noise_state;

		ser << channel.AEG.val;
		ser << channel.AEG.state;
		ser << channel.FEG.value;
		ser << channel.FEG.state;
		ser << channel.FEG.prev1;
		ser << channel.FEG.prev2;

		ser << channel.lfo.counter;
		ser << channel.lfo.state;
		ser << channel.enabled;
	}
	beep.serialize(ser);
	ser << cdda_sector;
	ser << cdda_index;
	ser << (u32)midiSendBuffer.size();
	for (u8 b : midiSendBuffer)
		ser << b;
}

void deserialize(Deserializer& deser)
{
	for (ChannelEx& channel : Chans)
	{
		channel.quiet = true;
		u32 addr;
		deser >> addr;
		channel.SA = addr + &aica_ram[0];

		deser >> channel.CA;
		deser >> channel.step;
		channel.UpdatePitch();
		deser >> channel.s0;
		deser >> channel.s1;
		deser >> channel.loop.looped;
		channel.UpdateLoop();
		deser >> channel.adpcm.last_quant;
		deser >> channel.adpcm.loopstart_quant;
		deser >> channel.adpcm.loopstart_prev_sample;
		deser >> channel.adpcm.in_loop;
		deser >> channel.noise_state;
		channel.UpdateAtts();
		channel.UpdateDSPMIX();

		deser >> channel.AEG.val;
		deser >> channel.AEG.state;
		channel.SetAegState(channel.AEG.state);
		channel.UpdateAEG();
		deser >> channel.FEG.value;
		deser >> channel.FEG.state;
		deser >> channel.FEG.prev1;
		deser >> channel.FEG.prev2;
		channel.SetFegState(channel.FEG.state);
		channel.UpdateFEG();
		channel.UpdateStreamStep();

		deser >> channel.lfo.counter;
		deser >> channel.lfo.state;
		channel.UpdateLFO(true);
		deser >> channel.enabled;
		channel.quiet = false;
	}
	beep.deserialize(deser);
	deser >> cdda_sector;
	deser >> cdda_index;
	midiSendBuffer.clear();
	if (deser.version() >= Deserializer::V28)
	{
		u32 size;
		deser >> size;
		for (u32 i = 0; i < size; i++)
		{
			u8 b;
			deser >> b;
			midiSendBuffer.push_back(b);
		}
	}
}

} // namespace aica::sgc
