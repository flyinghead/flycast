#include "mmu.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "types.h"


#include "hw/mem/_vmem.h"

//SQ fast remap , mainly hackish , assumes 1MB pages
//max 64MB can be remapped on SQ
u32 sq_remap[64];

TLB_Entry UTLB[64];
TLB_Entry ITLB[4];

//Sync memory mapping to MMU , suspend compiled blocks if needed.entry is a UTLB entry # , -1 is for full sync
void UTLB_Sync(u32 entry)
{	
	if ((UTLB[entry].Address.VPN & (0xFC000000 >> 10)) == (0xE0000000 >> 10))
	{
		u32 vpn_sq = ((UTLB[entry].Address.VPN & 0x7FFFF) >> 10) & 0x3F;//upper bits are always known [0xE0/E1/E2/E3]
		sq_remap[vpn_sq] = UTLB[entry].Data.PPN << 10;
		printf("SQ remap %d : 0x%X to 0x%X\n", entry, UTLB[entry].Address.VPN << 10, UTLB[entry].Data.PPN << 10);
	}
	else
	{
		printf("MEM remap %d : 0x%X to 0x%X\n", entry, UTLB[entry].Address.VPN << 10, UTLB[entry].Data.PPN << 10);
	}
}
//Sync memory mapping to MMU, suspend compiled blocks if needed.entry is a ITLB entry # , -1 is for full sync
void ITLB_Sync(u32 entry)
{
	printf("ITLB MEM remap %d : 0x%X to 0x%X\n",entry,ITLB[entry].Address.VPN<<10,ITLB[entry].Data.PPN<<10);
}

void MMU_init()
{

}

void MMU_reset()
{
	memset(UTLB,0,sizeof(UTLB));
	memset(ITLB,0,sizeof(ITLB));
}

void MMU_term()
{
}
