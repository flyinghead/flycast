/*
	Basic gdrom syscall emulation
	Adapted from some (very) old pre-nulldc hle code
	Bits and pieces from redream (https://github.com/inolen/redream)
*/

#include <stdio.h>
#include "types.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"

#include "gdrom_hle.h"
#include "hw/gdrom/gdromv3.h"
#include "reios.h"

#define SWAP32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))

#define debugf(...) DEBUG_LOG(REIOS, __VA_ARGS__)

// FIXME Serialize
u32 LastCommandId = 0xFFFFFFFF;
u32 NextCommandId = 1;
u32 bios_result[4];				// BIOS result vector
u32 cur_sector;

void GDROM_HLE_ReadSES(u32 addr)
{
	u32 s = ReadMem32(addr + 0);
	u32 b = ReadMem32(addr + 4);
	u32 ba = ReadMem32(addr + 8);
	u32 bb = ReadMem32(addr + 12);

	WARN_LOG(REIOS, "GDROM_HLE_ReadSES: doing nothing w/ %d, %d, %d, %d", s, b, ba, bb);
}
void GDROM_HLE_ReadTOC(u32 Addr)
{
	u32 area = ReadMem32(Addr + 0);
	u32 dest = ReadMem32(Addr + 4);

	u32* pDst = (u32*)GetMemPtr(dest, 0);

	debugf("GDROM READ TOC : %X %X", area, dest);
	if (area == DoubleDensity && libGDR_GetDiscType() != GdRom)
	{
		// Only GD-ROM has a high-density area but no error is reported
		LastCommandId = 0xFFFFFFFF;
		return;
	}

	libGDR_GetToc(pDst, area);

	// Swap results to LE
	for (int i = 0; i < 102; i++) {
		pDst[i] = SWAP32(pDst[i]);
	}
}

void read_sectors_to(u32 addr, u32 sector, u32 count) {
	u8 * pDst = GetMemPtr(addr, 0);

	if (pDst) {
		libGDR_ReadSector(pDst, sector, count, 2048);
		cur_sector = sector + count - 1;
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
		cur_sector = sector - 1;
	}
}

void GDROM_HLE_ReadDMA(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	debugf("GDROM: DMA READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X", s, n, b, u);
	read_sectors_to(b, s, n);
	bios_result[2] = n * 2048;
	bios_result[3] = -n * 2048;
}

void GDROM_HLE_ReadPIO(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	debugf("GDROM: PIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X", s, n, b, u);

	read_sectors_to(b, s, n);
	bios_result[2] = n * 2048;
	bios_result[3] = -n * 2048;
}

void GDCC_HLE_GETSCD(u32 addr) {
	u32 format = ReadMem32(addr + 0x00);
	u32 size = ReadMem32(addr + 0x04);
	u32 dest = ReadMem32(addr + 0x08);

	INFO_LOG(REIOS, "GDROM: GETSCD format %x size %x dest %08x", format, size, dest);

	u8 scd[100];
	gd_get_subcode(format, cur_sector, scd);
	verify(scd[3] == size);

	memcpy(GetMemPtr(dest, size), scd, size);

	// record size of pio transfer to gdrom
	bios_result[2] = size;
}


u32 SecMode[4];

void GD_HLE_Command(u32 cc, u32 prm)
{
	switch(cc)
	{
	case GDCC_GETTOC:
		WARN_LOG(REIOS, "GDROM: *FIXME* CMD GETTOC PRM:%X", prm);
		break;

	case GDCC_GETTOC2:
		GDROM_HLE_ReadTOC(prm);
		break;

	case GDCC_GETSES:
		GDROM_HLE_ReadSES(prm);
		break;

	case GDCC_INIT:
		INFO_LOG(REIOS, "GDROM: CMD INIT PRM:%X", prm);
		break;

	case GDCC_PIOREAD:
		GDROM_HLE_ReadPIO(prm);
		break;

	case GDCC_DMAREAD:
		GDROM_HLE_ReadDMA(prm);
		break;


	case GDCC_PLAY_SECTOR:
		WARN_LOG(REIOS, "GDROM: CMD PLAYSEC? PRM:%X", prm);
		break;

	case GDCC_RELEASE:
		WARN_LOG(REIOS, "GDROM: CMD RELEASE? PRM:%X", prm);
		break;

	case GDCC_STOP:
		INFO_LOG(REIOS, "GDROM: CMD STOP PRM:%X", prm);
		cdda.playing = false;
		SecNumber.Status = GD_STANDBY;
		break;

	case GDCC_SEEK:
		INFO_LOG(REIOS, "GDROM: CMD SEEK PRM:%X", prm);
		cdda.playing = false;
		SecNumber.Status = GD_PAUSE;
		break;

	case GDCC_PLAY:
		{

			u32 first_track = ReadMem32(prm);
			u32 last_track = ReadMem32(prm + 4);
			u32 repeats = ReadMem32(prm + 8);
			u32 start_fad, end_fad, dummy;
			libGDR_GetTrack(first_track, start_fad, dummy);
			libGDR_GetTrack(last_track, dummy, end_fad);
			INFO_LOG(REIOS, "GDROM: CMD PLAY first_track %x last_track %x repeats %x start_fad %x end_fad %x param4 %x", first_track, last_track, repeats,
					start_fad, end_fad, ReadMem32(prm + 12));
			cdda.playing = true;
			cdda.StartAddr.FAD = start_fad;
			cdda.EndAddr.FAD = end_fad;
			cdda.repeats = repeats;
			if (SecNumber.Status != GD_PAUSE || cdda.CurrAddr.FAD < start_fad || cdda.CurrAddr.FAD > end_fad)
				cdda.CurrAddr.FAD = start_fad;
			SecNumber.Status = GD_PLAY;
		}
		break;

	case GDCC_PAUSE:
		INFO_LOG(REIOS, "GDROM: CMD PAUSE");
		cdda.playing = false;
		SecNumber.Status = GD_PAUSE;
		break;

	case GDCC_READ:
		{
			u32 s = ReadMem32(prm + 0x00);
			u32 n = ReadMem32(prm + 0x04);
			u32 b = ReadMem32(prm + 0x08);
			u32 u = ReadMem32(prm + 0x0C);

			WARN_LOG(REIOS, "GDROM: CMD READ PRM:%X Sector=%d, Num=%d, Buffer=0x%08X, Unk01=%08x", prm, s, n, b, u);
		}
		break;

	case GDCC_GETSCD:
		GDCC_HLE_GETSCD(prm);
		break;

	case GDCC_REQ_MODE:
		{
			debugf("GDROM: REQ_MODE PRM:%X", prm);
			u32 dest = ReadMem32(prm);
			u32 *out = (u32 *)GetMemPtr(dest, 16);
			out[0] = GD_HardwareInfo.speed;
			out[1] = (GD_HardwareInfo.standby_hi << 8) | GD_HardwareInfo.standby_lo;
			out[2] = GD_HardwareInfo.read_flags;
			out[3] = GD_HardwareInfo.read_retry;

			// record size of pio transfer to gdrom
			bios_result[2] = 0xa;
		}
		break;

	case GDCC_SET_MODE:
		{
			u32 speed = ReadMem32(prm);
			u32 standby = ReadMem32(prm + 4);
			u32 read_flags = ReadMem32(prm + 8);
			u32 read_retry = ReadMem32(prm + 12);

			debugf("GDROM: SET_MODE PRM:%X speed %x standby %x read_flags %x read_retry %x", prm, speed, standby, read_flags, read_retry);

			GD_HardwareInfo.speed = speed;
			GD_HardwareInfo.standby_hi = (standby & 0xff00) >> 8;
			GD_HardwareInfo.standby_lo = standby & 0xff;
			GD_HardwareInfo.read_flags = read_flags;
			GD_HardwareInfo.read_retry = read_retry;

			// record size of pio transfer to gdrom
			bios_result[2] = 0xa;
		}
		break;

	case GDCC_GET_VER:
		{
			u32 dest = ReadMem32(prm);

			debugf("GDROM: GDCC_GET_VER dest %x", dest);

			char ver[] = "GDC Version 1.10 1999-03-31 ";
			u32 len = (u32)strlen(ver);

			// 0x8c0013b8 (offset 0xd0 in the gdrom state struct) is then loaded and
			// overwrites the last byte. no idea what this is, but seems to be hard
			// coded to 0x02 on boot
			ver[len - 1] = 0x02;

			memcpy(GetMemPtr(dest, len), ver, len);
		}
		break;

	case GDCC_REQ_STAT:
		{
			// odd, but this function seems to get passed 4 unique pointers
			u32 dst0 = ReadMem32(prm);
			u32 dst1 = ReadMem32(prm + 4);
			u32 dst2 = ReadMem32(prm + 8);
			u32 dst3 = ReadMem32(prm + 12);

			debugf("GDROM: GDCC_REQ_STAT dst0=%08x dst1=%08x dst2=%08x dst3=%08x", dst0, dst1, dst2, dst3);

			// bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0
			// byte  |     |     |     |     |     |     |     |
			// ------------------------------------------------------
			// 0     |  0  |  0  |  0  |  0  | status
			// ------------------------------------------------------
			// 1     |  0  |  0  |  0  |  0  | repeat count
			// ------------------------------------------------------
			// 2-3   |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0
			WriteMem32(dst0, (cdda.repeats << 8) | SecNumber.Status);

			// bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0
			// byte  |     |     |     |     |     |     |     |
			// ------------------------------------------------------
			// 0     | subcode q track number
			// ------------------------------------------------------
			// 1-3   |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0
			u32 elapsed;
			u32 tracknum = libGDR_GetTrackNumber(cur_sector, elapsed);
			WriteMem32(dst1, tracknum);

			// bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0
			// byte  |     |     |     |     |     |     |     |
			// ------------------------------------------------------
			// 0-2  | fad (little-endian)
			// ------------------------------------------------------
			// 3    | address                | control
			// FIXME address/control
			u32 out = ((0x4) << 28) | ((0x1) << 24) | (cur_sector & 0x00ffffff);
			WriteMem32(dst2, out);

			// bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0
			// byte  |     |     |     |     |     |     |     |
			// ------------------------------------------------------
			// 0     | subcode q index number
			// ------------------------------------------------------
			// 1-3   |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0
			WriteMem32(dst3, 1);

			// record pio transfer size
			bios_result[2] = 0xa;
		}
		break;
	default:
		WARN_LOG(REIOS, "GDROM: Unknown GDROM CC:%X PRM:%X", cc, prm);
		break;
	}
}

#define r Sh4cntx.r

void gdrom_hle_op()
{
	if( SYSCALL_GDROM == r[6] )		// GDROM SYSCALL
	{
		switch(r[7])				// COMMAND CODE
		{
		case GDROM_SEND_COMMAND:	// SEND GDROM COMMAND RET: - if failed + req id
			//debugf("GDROM: HLE SEND COMMAND CC:%X  param ptr: %X", r[4], r[5]);
			memset(bios_result, 0, sizeof(bios_result));
			LastCommandId = r[0] = NextCommandId++;		// Request Id
			GD_HLE_Command(r[4], r[5]);
			break;

		case GDROM_CHECK_COMMAND:
			if (r[4] != LastCommandId)
			{
				r[0] = -1;	// Error (examine extended status information for cause of failure)
				bios_result[0] = 5;	// Invalid command id
				bios_result[1] = 0;
				bios_result[2] = 0;
				bios_result[3] = 0;
			}
			else
			{
				r[0] = 2;	// Finished
			}
			//debugf("GDROM: HLE CHECK COMMAND REQID:%X  param ptr: %X -> %X", r[4], r[5], r[0]);
			LastCommandId = 0xFFFFFFFF;			// INVALIDATE CHECK CMD
			WriteMem32(r[5], bios_result[0]);
			WriteMem32(r[5] + 4, bios_result[1]);
			WriteMem32(r[5] + 8, bios_result[2]);
			WriteMem32(r[5] + 12, bios_result[3]);
			break;

		case GDROM_MAIN:
			//debugf("GDROM: HLE GDROM_MAIN");
			break;

		case GDROM_INIT:
			INFO_LOG(REIOS, "GDROM: HLE GDROM_INIT");
			LastCommandId = 0xFFFFFFFF;
			break;

		case GDROM_RESET:
			INFO_LOG(REIOS, "GDROM: HLE GDROM_RESET");
			break;

		case GDROM_CHECK_DRIVE:
			//debugf("GDROM: HLE GDROM_CHECK_DRIVE r4:%X", r[4]);
			WriteMem32(r[4] + 0, 0x02);	// STANDBY
			if (strstr(reios_device_info, "GD-ROM") != NULL)
				WriteMem32(r[4] + 4, GdRom);
			else
				WriteMem32(r[4] + 4, libGDR_GetDiscType());
			r[0] = 0;					// Success
			break;

		case GDROM_ABORT_COMMAND:
			INFO_LOG(REIOS, "GDROM: HLE GDROM_ABORT_COMMAND r4:%X",r[4]);
			r[0] = -1;					// Failure
			break;


		case GDROM_SECTOR_MODE:
			WARN_LOG(REIOS, "GDROM: HLE GDROM_SECTOR_MODE PTR_r4:%X",r[4]);
			for(int i=0; i<4; i++) {
				SecMode[i] = ReadMem32(r[4]+(i<<2));
				INFO_LOG(REIOS, "%08X", SecMode[i]);
			}
			r[0] = 0;					// Success
			break;

		default:
			WARN_LOG(REIOS, "GDROM: Unknown SYSCALL: %X",r[7]);
			break;
		}
	}
	else							// MISC 
	{
		switch(r[7])
		{
		case MISC_INIT:
			WARN_LOG(REIOS, "GDROM: MISC_INIT not implemented");
			break;

		case MISC_SETVECTOR:
			WARN_LOG(REIOS, "GDROM: MISC_SETVECTOR not implemented");
			break;

		default:
			WARN_LOG(REIOS, "GDROM: Unknown MISC command %x", r[7]);
			break;
		}
	}
}
