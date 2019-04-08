/*
	aica interface
		Handles RTC, Display mode reg && arm reset reg !
	arm7 is handled on a separate arm plugin now
*/
#include "types.h"
#include "aica_if.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/holly/sb.h"
#include "types.h"
#include "hw/holly/holly_intc.h"
#include "hw/sh4/sh4_sched.h"

#include <time.h>

VArray2 aica_ram;
u32 VREG;//video reg =P
u32 ARMRST;//arm reset reg
u32 rtc_EN=0;
int dma_sched_id;
u32 RealTimeClock;

u32 GetRTC_now()
{
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

u32 ReadMem_aica_rtc(u32 addr,u32 sz)
{
	switch( addr & 0xFF )
	{
	case 0:
		return RealTimeClock>>16;
	case 4:
		return RealTimeClock &0xFFFF;
	case 8:
		return 0;
	}

	printf("ReadMem_aica_rtc : invalid address\n");
	return 0;
}

void WriteMem_aica_rtc(u32 addr,u32 data,u32 sz)
{
	switch( addr & 0xFF )
	{
	case 0:
		if (rtc_EN)
		{
			RealTimeClock&=0xFFFF;
			RealTimeClock|=(data&0xFFFF)<<16;
			rtc_EN=0;
		}
		return;
	case 4:
		if (rtc_EN)
		{
			RealTimeClock&=0xFFFF0000;
			RealTimeClock|= data&0xFFFF;
			//TODO: Clean the internal timer ?
		}
		return;
	case 8:
		rtc_EN=data&1;
		return;
	}

	return;
}
u32 ReadMem_aica_reg(u32 addr,u32 sz)
{
	addr&=0x7FFF;
	if (sz==1)
	{
		if (addr==0x2C01)
		{
			return VREG;
		}
		else if (addr==0x2C00)
		{
			return ARMRST;
		}
		else
		{
			return libAICA_ReadReg(addr, sz);
		}
	}
	else
	{
		if (addr==0x2C00)
		{
			return (VREG<<8) | ARMRST;
		}
		else
		{
			return libAICA_ReadReg(addr, sz);
		}
	}
}

void ArmSetRST()
{
	ARMRST&=1;
	libARM_SetResetState(ARMRST);
}
void WriteMem_aica_reg(u32 addr,u32 data,u32 sz)
{
	addr&=0x7FFF;

	if (sz==1)
	{
		if (addr==0x2C01)
		{
			VREG=data;
			printf("VREG = %02X\n",VREG);
		}
		else if (addr==0x2C00)
		{
			ARMRST=data;
			printf("ARMRST = %02X\n",ARMRST);
			ArmSetRST();
		}
		else
		{
			libAICA_WriteReg(addr,data,sz);
		}
	}
	else
	{
		if (addr==0x2C00)
		{
			VREG=(data>>8)&0xFF;
			ARMRST=data&0xFF;
			printf("VREG = %02X ARMRST %02X\n",VREG,ARMRST);
			ArmSetRST();
		}
		else
		{
			libAICA_WriteReg(addr,data,sz);
		}
	}
}
//Init/res/term
void aica_Init()
{
	RealTimeClock = GetRTC_now();
}

void aica_Reset(bool Manual)
{
	aica_Init();
}

void aica_Term()
{

}

int dma_end_sched(int tag, int cycl, int jitt)
{
	u32 len=SB_ADLEN & 0x7FFFFFFF;

	if (SB_ADLEN & 0x80000000)
		SB_ADEN=1;//
	else
		SB_ADEN=0;//

	SB_ADSTAR+=len;
	SB_ADSTAG+=len;
	SB_ADST = 0x00000000;//dma done
	SB_ADLEN = 0x00000000;

	// indicate that dma is not happening, or has been paused
	SB_ADSUSP |= 0x10;

	asic_RaiseInterrupt(holly_SPU_DMA);

	return 0;
}

void Write_SB_ADST(u32 addr, u32 data)
{
	//0x005F7800	SB_ADSTAG	RW	AICA:G2-DMA G2 start address 
	//0x005F7804	SB_ADSTAR	RW	AICA:G2-DMA system memory start address 
	//0x005F7808	SB_ADLEN	RW	AICA:G2-DMA length 
	//0x005F780C	SB_ADDIR	RW	AICA:G2-DMA direction 
	//0x005F7810	SB_ADTSEL	RW	AICA:G2-DMA trigger select 
	//0x005F7814	SB_ADEN	RW	AICA:G2-DMA enable 
	//0x005F7818	SB_ADST	RW	AICA:G2-DMA start 
	//0x005F781C	SB_ADSUSP	RW	AICA:G2-DMA suspend 
	
	if (data&1)
	{
		if (SB_ADEN&1)
		{
			u32 src=SB_ADSTAR;
			u32 dst=SB_ADSTAG;
			u32 len=SB_ADLEN & 0x7FFFFFFF;

			if ((SB_ADDIR&1)==1)
			{
				//swap direction
				u32 tmp=src;
				src=dst;
				dst=tmp;
				printf("**AICA DMA : SB_ADDIR==1: Not sure this works, please report if broken/missing sound or crash\n**");
			}

			WriteMemBlock_nommu_dma(dst,src,len);
			/*
			for (u32 i=0;i<len;i+=4)
			{
				u32 data=ReadMem32_nommu(src+i);
				WriteMem32_nommu(dst+i,data);
			}
			*/

			// idicate that dma is in progress
			SB_ADSUSP &= ~0x10;

			if (!settings.aica.OldSyncronousDma)
			{

				// Schedule the end of DMA transfer interrupt
				int cycles = len * (SH4_MAIN_CLOCK / 2 / 25000000);       // 16 bits @ 25 MHz
				if (cycles < 4096)
					dma_end_sched(0, 0, 0);
				else
					sh4_sched_request(dma_sched_id, cycles);
			}
			else
			{
				dma_end_sched(0, 0, 0);
			}
		}
	}
}

void Write_SB_E1ST(u32 addr, u32 data)
{
	//0x005F7800	SB_ADSTAG	RW	AICA:G2-DMA G2 start address 
	//0x005F7804	SB_ADSTAR	RW	AICA:G2-DMA system memory start address 
	//0x005F7808	SB_ADLEN	RW	AICA:G2-DMA length 
	//0x005F780C	SB_ADDIR	RW	AICA:G2-DMA direction 
	//0x005F7810	SB_ADTSEL	RW	AICA:G2-DMA trigger select 
	//0x005F7814	SB_ADEN	RW	AICA:G2-DMA enable 
	//0x005F7818	SB_ADST	RW	AICA:G2-DMA start 
	//0x005F781C	SB_ADSUSP	RW	AICA:G2-DMA suspend 
	
	if (data&1)
	{
		if (SB_E1EN&1)
		{
			u32 src=SB_E1STAR;
			u32 dst=SB_E1STAG;
			u32 len=SB_E1LEN & 0x7FFFFFFF;

			if (SB_E1DIR==1)
			{
				u32 t=src;
				src=dst;
				dst=t;
				printf("G2-EXT1 DMA : SB_E1DIR==1 DMA Read to 0x%X from 0x%X %d bytes\n",dst,src,len);
			}
			else
				printf("G2-EXT1 DMA : SB_E1DIR==0:DMA Write to 0x%X from 0x%X %d bytes\n",dst,src,len);

			WriteMemBlock_nommu_dma(dst,src,len);

			/*
			for (u32 i=0;i<len;i+=4)
			{
				u32 data=ReadMem32_nommu(src+i);
				WriteMem32_nommu(dst+i,data);
			}*/

			if (SB_E1LEN & 0x80000000)
				SB_E1EN=1;//
			else
				SB_E1EN=0;//

			SB_E1STAR+=len;
			SB_E1STAG+=len;
			SB_E1ST = 0x00000000;//dma done
			SB_E1LEN = 0x00000000;

			
			asic_RaiseInterrupt(holly_EXT_DMA1);
		}
	}
}

void Write_SB_E2ST(u32 addr, u32 data)
{
	if ((data & 1) && (SB_E2EN & 1))
	{
		u32 src=SB_E2STAR;
		u32 dst=SB_E2STAG;
		u32 len=SB_E2LEN & 0x7FFFFFFF;

		if (SB_E2DIR==1)
		{
			u32 t=src;
			src=dst;
			dst=t;
			printf("G2-EXT2 DMA : SB_E2DIR==1 DMA Read to 0x%X from 0x%X %d bytes\n",dst,src,len);
		}
		else
			printf("G2-EXT2 DMA : SB_E2DIR==0:DMA Write to 0x%X from 0x%X %d bytes\n",dst,src,len);

		WriteMemBlock_nommu_dma(dst,src,len);

		if (SB_E2LEN & 0x80000000)
			SB_E2EN=1;
		else
			SB_E2EN=0;

		SB_E2STAR+=len;
		SB_E2STAG+=len;
		SB_E2ST = 0x00000000;//dma done
		SB_E2LEN = 0x00000000;


		asic_RaiseInterrupt(holly_EXT_DMA2);
	}
}


void Write_SB_DDST(u32 addr, u32 data)
{
	if (data & 1)
		die("SB_DDST DMA not implemented");
}

void aica_sb_Init()
{
	//NRM
	//6
	sb_rio_register(SB_ADST_addr,RIO_WF,0,&Write_SB_ADST);
	//sb_regs[((SB_ADST_addr-SB_BASE)>>2)].flags=REG_32BIT_READWRITE | REG_READ_DATA;
	//sb_regs[((SB_ADST_addr-SB_BASE)>>2)].writeFunction=Write_SB_ADST;

	//I really need to implement G2 dma (and rest dmas actually) properly
	//THIS IS NOT AICA, its G2-EXT (BBA)

	sb_rio_register(SB_E1ST_addr,RIO_WF,0,&Write_SB_E1ST);
	sb_rio_register(SB_E2ST_addr,RIO_WF,0,&Write_SB_E2ST);
	sb_rio_register(SB_DDST_addr,RIO_WF,0,&Write_SB_DDST);

	//sb_regs[((SB_E1ST_addr-SB_BASE)>>2)].flags=REG_32BIT_READWRITE | REG_READ_DATA;
	//sb_regs[((SB_E1ST_addr-SB_BASE)>>2)].writeFunction=Write_SB_E1ST;
	dma_sched_id = sh4_sched_register(0, &dma_end_sched);
}

void aica_sb_Reset(bool Manual)
{
}

void aica_sb_Term()
{
}
