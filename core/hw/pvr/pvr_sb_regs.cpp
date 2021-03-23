/*
	PVR-SB handling
	DMA hacks are here
*/

#include "pvr_sb_regs.h"
#include "ta.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/sh4/modules/dmac.h"
#include "hw/sh4/sh4_mem.h"

void RegWrite_SB_C2DST(u32 addr, u32 data)
{
	if(1&data)
	{
		SB_C2DST=1;
		DMAC_Ch2St();
	}
}
//PVR-DMA
void do_pvr_dma()
{
	u32 dmaor  = DMAC_DMAOR.full;

	u32 src = SB_PDSTAR;	// System RAM address
	u32 dst = SB_PDSTAP;	// VRAM address
	u32 len = SB_PDLEN;

	if(0x8201 != (dmaor &DMAOR_MASK))
	{
		INFO_LOG(PVR, "DMAC: DMAOR has invalid settings (%X) !", dmaor);
		return;
	}

	if (len & 0x1F)
	{
		INFO_LOG(PVR, "DMAC: SB_C2DLEN has invalid size (%X) !", len);
		return;
	}

	if (SB_PDDIR)
	{
		//PVR -> System
		WriteMemBlock_nommu_dma(src, dst, len);
	}
	else
	{
		//System -> PVR
		WriteMemBlock_nommu_dma(dst,src,len);
	}

	DMAC_SAR(0) = (src + len);
	DMAC_CHCR(0).TE = 1;
	DMAC_DMATCR(0) = 0x00000000;

	SB_PDST = 0x00000000;

	//TODO : *CHECKME* is that ok here ? the docs don't say here it's used [PVR-DMA , bit 11]
	asic_RaiseInterrupt(holly_PVR_DMA);
}
void RegWrite_SB_PDST(u32 addr, u32 data)
{
	if(1&data)
	{
		SB_PDST=1;
		do_pvr_dma();
	}
}
u32 calculate_start_link_addr()
{
	u8* base = &mem_b[SB_SDSTAW & (RAM_MASK - 31)];
	u32 rv;
	if (SB_SDWLT==0)
	{
		//16b width
		rv=((u16*)base)[SB_SDDIV];
	}
	else
	{
		//32b width
		rv=((u32*)base)[SB_SDDIV];
	}
	SB_SDDIV++; //next index

	return rv;
}
void pvr_do_sort_dma()
{

	SB_SDDIV=0;//index is 0 now :)
	u32 link_addr=calculate_start_link_addr();

	while (link_addr != 2)
	{
		if (SB_SDLAS==1)
			link_addr*=32;

		u32 ea = (SB_SDBAAW + link_addr) & RAM_MASK & ~31;
		u32* ea_ptr=(u32*)&mem_b[ea];

		link_addr=ea_ptr[0x1C>>2];//Next link
		//transfer global param
		ta_vtx_data((const SQBuffer *)ea_ptr, ea_ptr[0x18 >> 2]);
		if (link_addr == 1)
		{
			link_addr=calculate_start_link_addr();
		}
	}

	// End of DMA :)
	SB_SDST=0;
	SB_SDSTAW += 32;
	asic_RaiseInterrupt(holly_PVR_SortDMA);
}
// Auto sort DMA :|
void RegWrite_SB_SDST(u32 addr, u32 data)
{
	if(1&data)
	{
		pvr_do_sort_dma();
	}
}


//Init/Term , global
void pvr_sb_Init()
{
	//0x005F7C18    SB_PDST RW  PVR-DMA start
	sb_rio_register(SB_PDST_addr,RIO_WF,0,&RegWrite_SB_PDST);

	//0x005F6808    SB_C2DST RW  ch2-DMA start 
	sb_rio_register(SB_C2DST_addr,RIO_WF,0,&RegWrite_SB_C2DST);

	//0x005F6820    SB_SDST RW  Sort-DMA start
	sb_rio_register(SB_SDST_addr,RIO_WF,0,&RegWrite_SB_SDST);
}
void pvr_sb_Term()
{
}
//Reset -> Reset - Initialise
void pvr_sb_Reset(bool hard)
{
}
