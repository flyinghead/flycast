/*
	Basic gdrom syscall emulation
	Adapted from some (very) old pre-nulldc hle code
*/

#include <stdio.h>
#include "types.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"

#include "gdrom_hle.h"


void GDROM_HLE_ReadSES(u32 addr)
{
	u32 s = ReadMem32(addr + 0);
	u32 b = ReadMem32(addr + 4);
	u32 ba = ReadMem32(addr + 8);
	u32 bb = ReadMem32(addr + 12);

	printf("GDROM_HLE_ReadSES: %d, %d, %d, %d\n", s, b, ba, bb);
}
void GDROM_HLE_ReadTOC(u32 Addr)
{
	u32 s = ReadMem32(Addr + 0);
	u32 b = ReadMem32(Addr + 4);

	u8 * pDst = GetMemPtr(b, 0);

	//
	//printf("GDROM READ TOC : %X %X \n\n", s, b);

	libGDR_GetToc((u32*)pDst, s);
}

void read_sectors_to(u32 addr, u32 sector, u32 count) {
	u8 * pDst = GetMemPtr(addr, 0);

	if (pDst) {
		libGDR_ReadSector(pDst, sector, count, 2048);
	}
	else {
		u8 temp[2048];

		while (count > 0) {
			libGDR_ReadSector(temp, sector, 1, 2048);

			for (int i = 0; i < 2048 / 4; i += 4) {
				WriteMem32(addr, temp[i]);
				addr += 4;
			}

			sector++;
			count--;
		}
	}
	
}

void GDROM_HLE_ReadDMA(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	

	//printf("GDROM:\tPIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X\n", s, n, b, u);
	read_sectors_to(b, s, n);
}

void GDROM_HLE_ReadPIO(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	//printf("GDROM:\tPIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X\n", s, n, b, u);

	read_sectors_to(b, s, n);
}

void GDCC_HLE_GETSCD(u32 addr) {
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	printf("GDROM:\tGETSCD [0]=%d, [1]=%d, [2]=0x%08X, [3]=0x%08X\n", s, n, b, u);
}

#define r Sh4cntx.r


u32 SecMode[4];

void GD_HLE_Command(u32 cc, u32 prm)
{
	switch(cc)
	{
	case GDCC_GETTOC:
		printf("GDROM:\t*FIXME* CMD GETTOC PRM:%X\n",cc,prm);
		break;

	case GDCC_GETTOC2:
		GDROM_HLE_ReadTOC(r[5]);
		break;

	case GDCC_GETSES:
		printf("GDROM:\tGETSES PRM:%X\n", cc, prm);
		GDROM_HLE_ReadSES(r[5]);
		break;

	case GDCC_INIT:
		printf("GDROM:\tCMD INIT PRM:%X\n",cc,prm);
		break;

	case GDCC_PIOREAD:
		GDROM_HLE_ReadPIO(r[5]);
		break;

	case GDCC_DMAREAD:
		//printf("GDROM:\tCMD DMAREAD PRM:%X\n",cc,prm);
		GDROM_HLE_ReadDMA(r[5]);
		break;


	case GDCC_PLAY_SECTOR:
		printf("GDROM:\tCMD PLAYSEC? PRM:%X\n",cc,prm);
		break;

	case GDCC_RELEASE:
		printf("GDROM:\tCMD RELEASE? PRM:%X\n",cc,prm);
		break;

	case GDCC_STOP:	printf("GDROM:\tCMD STOP PRM:%X\n",cc,prm);		break;
	case GDCC_SEEK:	printf("GDROM:\tCMD SEEK PRM:%X\n",cc,prm);	break;
	case GDCC_PLAY:	printf("GDROM:\tCMD PLAY PRM:%X\n",cc,prm);	break;
	case GDCC_PAUSE:printf("GDROM:\tCMD PAUSE PRM:%X\n",cc,prm);	break;

	case GDCC_READ:
		printf("GDROM:\tCMD READ PRM:%X\n",cc,prm);
		break;

	case GDCC_GETSCD:
		printf("GDROM:\tGETSCD PRM:%X\n",cc,prm);
		GDCC_HLE_GETSCD(r[5]);
		break;

	default: printf("GDROM:\tUnknown GDROM CC:%X PRM:%X\n",cc,prm); break;
	}
}

void gdrom_hle_op()
{
	static bool bFlip=false;			// only works for last cmd, might help somewhere
	static u32 dwReqID=0xFFFFFFFF;		// ReqID, starting w/ high val

	if( SYSCALL_GDROM == r[6] )		// GDROM SYSCALL
	{
		switch(r[7])				// COMMAND CODE
		{
			// *FIXME* NEED RET
		case GDROM_SEND_COMMAND:	// SEND GDROM COMMAND RET: - if failed + req id
			//printf("\nGDROM:\tHLE SEND COMMAND CC:%X  param ptr: %X\n",r[4],r[5]);
			GD_HLE_Command(r[4],r[5]);
			r[0] = --dwReqID;		// RET Request ID
			bFlip= true;			// COMMAND IS COMPLETE 
		break;

		case GDROM_CHECK_COMMAND:	// 
			//printf("\nGDROM:\tHLE CHECK COMMAND REQID:%X  param ptr: %X\n",r[4],r[5]);
			r[0] = bFlip ? 2 : 0;	// RET Finished : Invalid
			bFlip= false;			// INVALIDATE CHECK CMD
		break;

			// NO return, NO params
		case GDROM_MAIN:	
			//printf("\nGDROM:\tHLE GDROM_MAIN\n");	
			break;

		case GDROM_INIT:	printf("\nGDROM:\tHLE GDROM_INIT\n");	break;
		case GDROM_RESET:	printf("\nGDROM:\tHLE GDROM_RESET\n");	break;

		case GDROM_CHECK_DRIVE:		// 
//			printf("\nGDROM:\tHLE GDROM_CHECK_DRIVE r4:%X\n",r[4],r[5]);
			WriteMem32(r[4]+0,0x02);	// STANDBY
			WriteMem32(r[4]+4,0x80);	// CDROM | 0x80 for GDROM
			r[0]=0;					// RET SUCCESS
		break;

		case GDROM_ABORT_COMMAND:	// 
			printf("\nGDROM:\tHLE GDROM_ABORT_COMMAND r4:%X\n",r[4],r[5]);
			r[0]=-1;				// RET FAILURE
		break;


		case GDROM_SECTOR_MODE:		// 
			printf("GDROM:\tHLE GDROM_SECTOR_MODE PTR_r4:%X\n",r[4]);
			for(int i=0; i<4; i++) {
				SecMode[i] = ReadMem32(r[4]+(i<<2));
				printf("%08X%s",SecMode[i],((3==i) ? "\n" : "\t"));
			}
			r[0]=0;					// RET SUCCESS
		break;

		default: printf("\nGDROM:\tUnknown SYSCALL: %X\n",r[7]); break;
		}
	}
	else							// MISC 
	{
		printf("SYSCALL:\tSYSCALL: %X\n",r[7]);
	}
}