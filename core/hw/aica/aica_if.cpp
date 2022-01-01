/*
	aica interface
		Handles RTC, Display mode reg && arm reset reg !
	arm7 is handled on a separate arm plugin now
*/

#include "aica_if.h"
#include "aica_mem.h"
#include "hw/holly/sb.h"
#include "hw/holly/holly_intc.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "profiler/profiler.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/arm7/arm7.h"

#include <ctime>

VArray2 aica_ram;
u32 VREG;
u32 ARMRST;
u32 rtc_EN;
int dma_sched_id = -1;
u32 RealTimeClock;
int rtc_schid = -1;
u32 SB_ADST;

u32 GetRTC_now()
{
	// rtc kept static for netplay when savestate is not loaded
	if (config::GGPOEnable)
		// 1/1/70 00:00:00
		return (20 * 365 + 5) * 24 * 60 * 60;

	// The Dreamcast Epoch time is 1/1/50 00:00 but without support for time zone or DST.
	// We compute the TZ/DST current time offset and add it to the result
	// as if we were in the UTC time zone (as well as the DC Epoch)
	time_t rawtime = time(NULL);
	struct tm localtm, gmtm;
	localtm = *localtime(&rawtime);
	gmtm = *gmtime(&rawtime);
	gmtm.tm_isdst = -1;
	time_t time_offset = mktime(&localtm) - mktime(&gmtm);
	// 1/1/50 to 1/1/70 is 20 years and 5 leap days
	return (20 * 365 + 5) * 24 * 60 * 60 + rawtime + time_offset;
}

u32 ReadMem_aica_rtc(u32 addr, u32 sz)
{
	switch (addr & 0xFF)
	{
	case 0:
		return RealTimeClock >> 16;
	case 4:
		return RealTimeClock & 0xFFFF;
	case 8:
		return 0;
	}

	WARN_LOG(AICA, "ReadMem_aica_rtc: invalid address %x sz %d", addr, sz);
	return 0;
}

void WriteMem_aica_rtc(u32 addr, u32 data, u32 sz)
{
	switch (addr & 0xFF)
	{
	case 0:
		if (rtc_EN)
		{
			RealTimeClock &= 0xFFFF;
			RealTimeClock |= (data & 0xFFFF) << 16;
			rtc_EN = 0;
		}
		break;
	case 4:
		if (rtc_EN)
		{
			RealTimeClock &= 0xFFFF0000;
			RealTimeClock |= data & 0xFFFF;
			//TODO: Clean the internal timer ?
		}
		break;
	case 8:
		rtc_EN = data & 1;
		break;

	default:
		WARN_LOG(AICA, "WriteMem_aica_rtc: invalid address %x sz %d data %x", addr, sz, data);
		break;
	}
}

u32 ReadMem_aica_reg(u32 addr, u32 sz)
{
	addr &= 0x7FFF;
	if (sz == 1)
	{
		switch (addr)
		{
		case 0x2C00:
			return ARMRST;
		case 0x2C01:
			return VREG;
		default:
			break;
		}
	}
	else if (addr == 0x2C00)
		return (VREG << 8) | ARMRST;

	return libAICA_ReadReg(addr, sz);
}

static void ArmSetRST()
{
	ARMRST &= 1;
	aicaarm::enable(ARMRST == 0);
}

void WriteMem_aica_reg(u32 addr,u32 data,u32 sz)
{
	addr &= 0x7FFF;

	if (sz == 1)
	{
		switch (addr)
		{
		case 0x2C00:
			ARMRST = data;
			INFO_LOG(AICA_ARM, "ARMRST = %02X", ARMRST);
			ArmSetRST();
			return;
		case 0x2C01:
			VREG = data;
			INFO_LOG(AICA_ARM, "VREG = %02X", VREG);
			return;
		default:
			break;
		}
	}
	else if (addr == 0x2C00)
	{
		VREG = (data >> 8) & 0xFF;
		ARMRST = data & 0xFF;
		INFO_LOG(AICA_ARM, "VREG = %02X ARMRST %02X", VREG, ARMRST);
		ArmSetRST();
		return;
	}
	libAICA_WriteReg(addr, data, sz);
}

static int DreamcastSecond(int tag, int c, int j)
{
	RealTimeClock++;

	prof_periodical();

#if FEAT_SHREC != DYNAREC_NONE
	bm_Periodical_1s();
#endif

	return SH4_MAIN_CLOCK;
}

//Init/res/term
void aica_Init()
{
	RealTimeClock = GetRTC_now();
	if (rtc_schid == -1)
		rtc_schid = sh4_sched_register(0, &DreamcastSecond);
}

void aica_Reset(bool hard)
{
	if (hard)
	{
		aica_Init();
		sh4_sched_request(rtc_schid, SH4_MAIN_CLOCK);
	}
	VREG = 0;
	ARMRST = 0;
}

void aica_Term()
{
	sh4_sched_unregister(rtc_schid);
	rtc_schid = -1;
}

static int dma_end_sched(int tag, int cycl, int jitt)
{
	u32 len = SB_ADLEN & 0x7FFFFFFF;

	if (SB_ADLEN & 0x80000000)
		SB_ADEN = 0;
	else
		SB_ADEN = 1;

	SB_ADSTAR += len;
	SB_ADSTAG += len;
	SB_ADST = 0;	// dma done
	SB_ADLEN = 0;

	// indicate that dma is not happening, or has been paused
	SB_ADSUSP |= 0x10;

	asic_RaiseInterrupt(holly_SPU_DMA);

	return 0;
}

static bool check_STAG(u32 addr)
{
#ifdef STRICT_MODE
	const u32 area = (addr >> 26) & 7;
	// Aica and G2 Ext dev #1 and #2 on area 0
	// G2 Ext dev #3 on area 5
	return area == 0 || area == 5;
#else
	return true;
#endif
}

static bool check_STAR(u32 addr)
{
#ifdef STRICT_MODE
	const u32 area = (addr >> 26) & 7;
	// System RAM and VRAM
	return area == 3 || area == 1;
#else
	return true;
#endif
}

static bool check_STAR_overrun(u32 addr)
{
#ifdef STRICT_MODE
	u32 bottom = (((SB_G2APRO >> 8) & 0x7f) << 20) | 0x08000000;
	u32 top = ((SB_G2APRO & 0x7f) << 20) | 0x080fffe0;

	return addr >= bottom && addr <= top;
#else
	return true;
#endif
}

template<u32 ENABLE, u32 START, u32 SRC, u32 DEST, u32 LEN, u32 DIR,
	HollyInterruptID interrupt, HollyInterruptID iainterrupt, HollyInterruptID ovinterrupt,
	const char *LogTag>
void Write_DmaStart(u32 addr, u32 data)
{
	u32& enableReg = SB_REGN_32(ENABLE);
	u32& startReg = SB_REGN_32(START);
	u32& sourceReg = SB_REGN_32(SRC);
	u32& destReg = SB_REGN_32(DEST);
	u32& lenReg = SB_REGN_32(LEN);
	const u32 dirReg = SB_REGN_32(DIR);

	if (!(data & 1) || enableReg == 0)
		return;
	u32 src = sourceReg;
	u32 dst = destReg;
	u32 len = lenReg & 0x7FFFFFFF;

	// STAR
	if (!check_STAR(src))
	{
		INFO_LOG(AICA, "%s: Invalid src address %08x", LogTag, src);
		startReg = 0;
		enableReg = 0;
		asic_RaiseInterrupt(iainterrupt);
		return;
	}
	// STAG
	if (!check_STAG(dst))
	{
		INFO_LOG(AICA, "%s: Invalid dst address %08x", LogTag, dst);
		startReg = 0;
		enableReg = 0;
		asic_RaiseInterrupt(iainterrupt);
		return;
	}
	// Overrun
	if (!check_STAR_overrun(src) || !check_STAR_overrun(src + len - 1))
	{
		INFO_LOG(AICA, "%s: Overrun address %08x len %x", LogTag, src, len);
		startReg = 0;
		enableReg = 0;
		asic_RaiseInterrupt(ovinterrupt);
		return;
	}

	if (dirReg == 1)
		std::swap(src, dst);
	DEBUG_LOG(AICA, "%s: DMA Write to %X from %X %d bytes", LogTag, dst, src, len);

	WriteMemBlock_nommu_dma(dst, src, len);

	if (lenReg & 0x80000000)
		enableReg = 0;
	else
		enableReg = 1;

	sourceReg += len;
	destReg += len;
	startReg = 0; // dma done
	lenReg = 0;

	asic_RaiseInterrupt(interrupt);
}

static void Write_SB_ADST(u32 addr, u32 data)
{
	//0x005F7800	SB_ADSTAG	RW	AICA:G2-DMA G2 start address 
	//0x005F7804	SB_ADSTAR	RW	AICA:G2-DMA system memory start address 
	//0x005F7808	SB_ADLEN	RW	AICA:G2-DMA length 
	//0x005F780C	SB_ADDIR	RW	AICA:G2-DMA direction 
	//0x005F7810	SB_ADTSEL	RW	AICA:G2-DMA trigger select 
	//0x005F7814	SB_ADEN		RW	AICA:G2-DMA enable
	//0x005F7818	SB_ADST		RW	AICA:G2-DMA start
	//0x005F781C	SB_ADSUSP	RW	AICA:G2-DMA suspend 
	
	if ((data & 1) == 1 && (SB_ADST & 1) == 0)
	{
		if (SB_ADEN == 1)
		{
			u32 src = SB_ADSTAR;
			u32 dst = SB_ADSTAG;
			u32 len = SB_ADLEN & 0x7FFFFFFF;

			if (!check_STAR(src))
			{
				INFO_LOG(AICA, "AICA-DMA : Invalid src address %08x", src);
				SB_ADST = 0;
				SB_ADEN = 0;
				asic_RaiseInterrupt(holly_AICA_ILLADDR);
				return;
			}
			if (!check_STAG(dst))
			{
				INFO_LOG(AICA, "AICA-DMA : Invalid dst address %08x", dst);
				SB_ADST = 0;
				SB_ADEN = 0;
				asic_RaiseInterrupt(holly_AICA_ILLADDR);
				return;
			}
			// Overrun
			if (!check_STAR_overrun(src) || !check_STAR_overrun(src + len - 1))
			{
				INFO_LOG(AICA, "AICA-DMA : Overrun address %08x len %x", src, len);
				SB_ADST = 0;
				SB_ADEN = 0;
				asic_RaiseInterrupt(holly_AICA_OVERRUN);
				return;
			}

			if (SB_ADDIR == 1)
			{
				//swap direction
				std::swap(src, dst);
				DEBUG_LOG(AICA, "AICA-DMA : SB_ADDIR==1 DMA Read to 0x%X from 0x%X %x bytes", dst, src, SB_ADLEN);
			}
			else
				DEBUG_LOG(AICA, "AICA-DMA : SB_ADDIR==0:DMA Write to 0x%X from 0x%X %x bytes", dst, src, SB_ADLEN);

			WriteMemBlock_nommu_dma(dst, src, len);

			// indicate that dma is in progress
			SB_ADST = 1;
			SB_ADSUSP &= ~0x10;

			// Schedule the end of DMA transfer interrupt
			int cycles = len * (SH4_MAIN_CLOCK / 2 / 25000000);       // 16 bits @ 25 MHz
			if (cycles < 4096)
				dma_end_sched(0, 0, 0);
			else
				sh4_sched_request(dma_sched_id, cycles);
		}
	}
}

u32 Read_SB_ADST(u32 addr)
{
	// Le Mans and Looney Tunes sometimes send the same dma transfer twice after checking SB_ADST == 0.
	// To avoid this, we pretend SB_ADST is still set when there is a pending aica-dma interrupt.
	// This is only done once.
	if ((SB_ISTNRM & (1 << (u8)holly_SPU_DMA)) && !(SB_ADST & 2))
	{
		SB_ADST |= 2;
		return 1;
	}
	else
	{
		SB_ADST &= ~2;
		return SB_ADST;
	}
}

template<u32 STAG, HollyInterruptID iainterrupt, const char *LogTag>
void Write_SB_STAG(u32 addr, u32 data)
{
	u32& stagReg = sb_regs[(STAG - SB_BASE) / 4].data32;
	stagReg = data & 0x1FFFFFE0;

	if (!check_STAG(data))
	{
		INFO_LOG(AICA, "%s Write_SB_STAG: Invalid address %08x", LogTag, data);
		asic_RaiseInterrupt(iainterrupt);
	}
}

template<u32 STAR, HollyInterruptID iainterrupt, const char *LogTag>
void Write_SB_STAR(u32 addr, u32 data)
{
	u32& starReg = sb_regs[(STAR - SB_BASE) / 4].data32;
	starReg = data & 0x1FFFFFE0;

	if (!check_STAR(data))
	{
		INFO_LOG(AICA, "%s Write_SB_STAR: Invalid address %08x", LogTag, data);
		asic_RaiseInterrupt(iainterrupt);
	}
}

void Write_SB_G2APRO(u32 addr, u32 data)
{
	if ((data >> 16) == 0x4659)
		SB_G2APRO = data & 0x00007f7f;
}

extern const char AICA_TAG[] = "G2-AICA DMA";
extern const char EXT1_TAG[] = "G2-EXT1 DMA";
extern const char EXT2_TAG[] = "G2-EXT2 DMA";
extern const char DDEV_TAG[] = "G2-DDev DMA";

void aica_sb_Init()
{
	// G2-DMA registers

	// AICA
	sb_rio_register(SB_ADST_addr, RIO_FUNC, &Read_SB_ADST, &Write_SB_ADST);
#ifdef STRICT_MODE
	sb_rio_register(SB_ADSTAR_addr, RIO_WF, nullptr, &Write_SB_STAR<SB_ADSTAR_addr, holly_AICA_ILLADDR, AICA_TAG>);
	sb_rio_register(SB_ADSTAG_addr, RIO_WF, nullptr, &Write_SB_STAG<SB_ADSTAG_addr, holly_AICA_ILLADDR, AICA_TAG>);
#endif

	// G2 Ext device #1
	sb_rio_register(SB_E1ST_addr, RIO_WF, nullptr, &Write_DmaStart<SB_E1EN_addr, SB_E1ST_addr, SB_E1STAR_addr, SB_E1STAG_addr, SB_E1LEN_addr,
			SB_E1DIR_addr, holly_EXT_DMA1, holly_EXT1_ILLADDR, holly_EXT1_OVERRUN, EXT1_TAG>);
	sb_rio_register(SB_E1STAR_addr, RIO_WF, nullptr, &Write_SB_STAR<SB_E1STAR_addr, holly_EXT1_ILLADDR, EXT1_TAG>);
	sb_rio_register(SB_E1STAG_addr, RIO_WF, nullptr, &Write_SB_STAG<SB_E1STAG_addr, holly_EXT1_ILLADDR, EXT1_TAG>);

	// G2 Ext device #2
	sb_rio_register(SB_E2ST_addr, RIO_WF, nullptr, &Write_DmaStart<SB_E2EN_addr, SB_E2ST_addr, SB_E2STAR_addr, SB_E2STAG_addr, SB_E2LEN_addr,
			SB_E2DIR_addr, holly_EXT_DMA2, holly_EXT2_ILLADDR, holly_EXT2_OVERRUN, EXT2_TAG>);
	sb_rio_register(SB_E2STAR_addr, RIO_WF, nullptr, &Write_SB_STAR<SB_E2STAR_addr, holly_EXT2_ILLADDR, EXT2_TAG>);
	sb_rio_register(SB_E2STAG_addr, RIO_WF, nullptr, &Write_SB_STAG<SB_E2STAG_addr, holly_EXT2_ILLADDR, EXT2_TAG>);

	// G2 Ext device #3
	sb_rio_register(SB_DDST_addr, RIO_WF, nullptr, &Write_DmaStart<SB_DDEN_addr, SB_DDST_addr, SB_DDSTAR_addr, SB_DDSTAG_addr, SB_DDLEN_addr,
			SB_DDDIR_addr, holly_DEV_DMA, holly_DEV_ILLADDR, holly_DEV_OVERRUN, DDEV_TAG>);
	sb_rio_register(SB_DDSTAR_addr, RIO_WF, nullptr, &Write_SB_STAR<SB_DDSTAR_addr, holly_DEV_ILLADDR, DDEV_TAG>);
	sb_rio_register(SB_DDSTAG_addr, RIO_WF, nullptr, &Write_SB_STAG<SB_DDSTAG_addr, holly_DEV_ILLADDR, DDEV_TAG>);

	sb_rio_register(SB_G2APRO_addr, RIO_WO_FUNC, nullptr, &Write_SB_G2APRO);

	dma_sched_id = sh4_sched_register(0, &dma_end_sched);
}

void aica_sb_Reset(bool hard)
{
	if (hard)
		SB_ADST = 0;
}

void aica_sb_Term()
{
	sh4_sched_unregister(dma_sched_id);
	dma_sched_id = -1;
}
