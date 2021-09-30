/*
	Basic gdrom syscall emulation
	Adapted from some (very) old pre-nulldc hle code
	Bits and pieces from redream (https://github.com/inolen/redream)
*/

#include <cstdio>
#include "types.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_core.h"
#undef r

#include "gdrom_hle.h"
#include "hw/gdrom/gdromv3.h"
#include "hw/holly/holly_intc.h"
#include "reios.h"
#include "imgread/common.h"

#include <algorithm>

#define SWAP32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))

#define debugf(...) DEBUG_LOG(REIOS, __VA_ARGS__)

gdrom_hle_state_t gd_hle_state;

static void GDROM_HLE_ReadSES()
{
	u32 s = gd_hle_state.params[0];
	u32 b = gd_hle_state.params[1];
	u32 ba = gd_hle_state.params[2];
	u32 bb = gd_hle_state.params[3];

	INFO_LOG(REIOS, "GDROM_HLE_ReadSES: doing nothing w/ %d, %d, %d, %d", s, b, ba, bb);
}

static void GDROM_HLE_ReadTOC()
{
	u32 area = gd_hle_state.params[0];
	u32 dest = gd_hle_state.params[1];

	debugf("GDROM READ TOC : %X %X", area, dest);
	if (area == DoubleDensity && libGDR_GetDiscType() != GdRom)
	{
		// Only GD-ROM has a high-density area but no error is reported
		gd_hle_state.status = BIOS_INACTIVE;
		return;
	}

	u32 toc[102];
	libGDR_GetToc(toc, (DiskArea)area);

	// Swap results to LE
	for (int i = 0; i < 102; i++) {
		toc[i] = SWAP32(toc[i]);
	}
	if (!mmu_enabled())
	{
		u32* pDst = (u32*)GetMemPtr(dest, sizeof(toc));
		if (pDst != NULL)
		{
			memcpy(pDst, toc, sizeof(toc));
			return;
		}
	}
	for (int i = 0; i < 102; i++, dest += 4)
		WriteMem32(dest, toc[i]);
}

template<bool virtual_addr>
static void read_sectors_to(u32 addr, u32 sector, u32 count)
{
	gd_hle_state.cur_sector = sector + count - 1;
	if (virtual_addr)
		gd_hle_state.xfer_end_time = 0;
	else if (count > 5 && !config::FastGDRomLoad)
		// Large Transfers: GD-ROM rate (approx. 1.8 MB/s)
		gd_hle_state.xfer_end_time = sh4_sched_now64() + (u64)count * 2048 * 1000000L / 10240;
	else
		// Small transfers: Max G1 bus rate: 50 MHz x 16 bits
		gd_hle_state.xfer_end_time = sh4_sched_now64() + 5 * 2048 * 2;
	if (!virtual_addr || !mmu_enabled())
	{
		u8 * pDst = GetMemPtr(addr, 0);

		if (pDst != NULL)
		{
			libGDR_ReadSector(pDst, sector, count, 2048);
			return;
		}
	}
	u32 temp[2048 / 4];

	while (count > 0)
	{
		libGDR_ReadSector((u8 *)temp, sector, 1, sizeof(temp));

		for (std::size_t i = 0; i < ARRAY_SIZE(temp); i++)
		{
			if (virtual_addr)
				WriteMem32(addr, temp[i]);
			else
				WriteMem32_nommu(addr, temp[i]);
			addr += 4;
		}

		sector++;
		count--;
	}
}

static void GDROM_HLE_ReadDMA()
{
	u32 s = gd_hle_state.params[0];
	u32 n = gd_hle_state.params[1];
	u32 b = gd_hle_state.params[2];
	u32 u = gd_hle_state.params[3];

	debugf("GDROM: DMA READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X", s, n, b, u);

	read_sectors_to<false>(b, s, n);
	gd_hle_state.result[2] = 0;
	gd_hle_state.result[3] = 0;
}

static void GDROM_HLE_ReadPIO()
{
	u32 s = gd_hle_state.params[0];
	u32 n = gd_hle_state.params[1];
	u32 b = gd_hle_state.params[2];
	u32 u = gd_hle_state.params[3];

	debugf("GDROM: PIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X", s, n, b, u);

	read_sectors_to<true>(b, s, n);
	gd_hle_state.result[2] = n * 2048;
	gd_hle_state.result[3] = 0;
}

static void GDCC_HLE_GETSCD() {
	u32 format = gd_hle_state.params[0];
	u32 size = gd_hle_state.params[1];
	u32 dest = gd_hle_state.params[2];

	DEBUG_LOG(REIOS, "GDROM: GETSCD format %x size %x dest %08x", format, size, dest);

	if (libGDR_GetDiscType() == Open || libGDR_GetDiscType() == NoDisk)
	{
		gd_hle_state.status = BIOS_ERROR;
		gd_hle_state.result[0] = 2;	// ?
		gd_hle_state.result[1] = 0;
		gd_hle_state.result[2] = 0;
		gd_hle_state.result[3] = 0;
		return;
	}
	if (sns_asc != 0)
	{
		// Helps D2 detect the disk change
		gd_hle_state.status = BIOS_ERROR;
		gd_hle_state.result[0] = sns_key;
		gd_hle_state.result[1] = sns_asc;
		gd_hle_state.result[2] = 0x18;		// ?
		gd_hle_state.result[3] = sns_ascq;	// ?
		sns_key = 0;
		sns_asc = 0;
		sns_ascq = 0;
		return;
	}
	if (cdda.status == cdda_t::Playing)
		gd_hle_state.cur_sector = cdda.CurrAddr.FAD;
	u8 scd[100];
	gd_get_subcode(format, gd_hle_state.cur_sector, scd);
	verify(scd[3] == size);

	if (!mmu_enabled() && GetMemPtr(dest, size) != NULL)
		memcpy(GetMemPtr(dest, size), scd, size);
	else
	{
		for (u32 i = 0; i < size; i++)
			WriteMem8(dest++, scd[i]);
	}

	// record size of pio transfer to gdrom
	gd_hle_state.result[2] = size;
}
template<bool dma>
static void multi_xfer()
{
	u32 dest = gd_hle_state.params[0];
	u32 size = gd_hle_state.params[1];

	size = std::min(size, gd_hle_state.multi_read_count);
	while (size > 0)
	{
		u8 buf[2048];
		libGDR_ReadSector(buf, gd_hle_state.multi_read_sector, 1, 2048);
		while (size > 0)
		{
			int remaining = 2048 - gd_hle_state.multi_read_offset;
			if (size >= 4 && remaining >= 4 && (dest & 3) == 0)
			{
				if (dma)
					WriteMem32_nommu(dest, *(u32*)&buf[gd_hle_state.multi_read_offset]);
				else
					WriteMem32(dest, *(u32*)&buf[gd_hle_state.multi_read_offset]);
				dest += 4;
				gd_hle_state.multi_read_offset += 4;
				gd_hle_state.multi_read_count -= 4;
				size -= 4;
			}
			else if (size >= 2 && remaining >= 2 && (dest & 1) == 0)
			{
				if (dma)
					WriteMem16_nommu(dest, *(u16*)&buf[gd_hle_state.multi_read_offset]);
				else
					WriteMem16(dest, *(u16*)&buf[gd_hle_state.multi_read_offset]);
				dest += 2;
				gd_hle_state.multi_read_offset += 2;
				gd_hle_state.multi_read_count -= 2;
				size -= 2;
			}
			else
			{
				if (dma)
					WriteMem8_nommu(dest, buf[gd_hle_state.multi_read_offset]);
				else
					WriteMem8(dest, buf[gd_hle_state.multi_read_offset]);
				dest++;
				gd_hle_state.multi_read_offset++;
				gd_hle_state.multi_read_count--;
				size--;
			}
			if (gd_hle_state.multi_read_offset >= 2048)
			{
				verify(gd_hle_state.multi_read_offset == 2048);
				gd_hle_state.multi_read_sector++;
				gd_hle_state.multi_read_offset = 0;
				break;
			}
		}
	}
	if (!dma)
	{
		gd_hle_state.result[2] = gd_hle_state.multi_read_total - gd_hle_state.multi_read_count;
		gd_hle_state.result[3] = 0;	// 1 hangs Bust-a-Move-4
		if (gd_hle_state.multi_callback != 0)
		{
			Sh4cntx.r[4] = gd_hle_state.multi_callback_arg;
			Sh4cntx.pc = gd_hle_state.multi_callback;
		}
	}
	else
	{
		gd_hle_state.result[2] = 2048;
		gd_hle_state.result[3] = gd_hle_state.multi_read_count > 0 ? 1 : 0;
		gd_hle_state.dma_trans_ended = true;
		if (gd_hle_state.multi_read_count == 0)
			gd_hle_state.status = BIOS_COMPLETED;
		asic_RaiseInterrupt(holly_GDROM_DMA);
	}
}

u32 SecMode[4];

static void GD_HLE_Command(u32 cc)
{
	switch(cc)
	{
	case GDCC_GETTOC:
		WARN_LOG(REIOS, "GDROM: *FIXME* CMD GETTOC");
		break;

	case GDCC_GETTOC2:
		GDROM_HLE_ReadTOC();
		break;

	case GDCC_GETSES:
		GDROM_HLE_ReadSES();
		break;

	case GDCC_INIT:
		DEBUG_LOG(REIOS, "GDROM: CMD INIT");
		gd_hle_state.multi_callback = 0;
		gd_hle_state.multi_read_count = 0;
		cdda.status = cdda_t::NoInfo;
		break;

	case GDCC_PIOREAD:
		GDROM_HLE_ReadPIO();
		SecNumber.Status = GD_STANDBY;
		cdda.status = cdda_t::NoInfo;
		break;

	case GDCC_DMAREAD:
		cdda.status = cdda_t::NoInfo;
		if (gd_hle_state.xfer_end_time == 0)
			GDROM_HLE_ReadDMA();
		if (gd_hle_state.xfer_end_time > 0)
		{
			if (gd_hle_state.xfer_end_time > sh4_sched_now64())
				return;
			gd_hle_state.xfer_end_time = 0;
		}
		gd_hle_state.result[2] = gd_hle_state.params[1] * 2048;
		gd_hle_state.result[3] = 0;
		SecNumber.Status = GD_STANDBY;
		break;


	case GDCC_PLAY_SECTOR:
		{
			u32 start_fad = gd_hle_state.params[0];
			u32 end_fad = gd_hle_state.params[1];
			DEBUG_LOG(REIOS, "GDROM: CMD PLAYSEC from %d to %d repeats %d", start_fad, end_fad, gd_hle_state.params[2]);
			cdda.status = cdda_t::Playing;
			cdda.StartAddr.FAD = start_fad;
			cdda.EndAddr.FAD = end_fad;
			cdda.repeats = gd_hle_state.params[2];
			cdda.CurrAddr.FAD = start_fad;
			SecNumber.Status = GD_PLAY;
		}
		break;

	case GDCC_RELEASE:
		DEBUG_LOG(REIOS, "GDROM: CMD RELEASE");
		if (cdda.status == cdda_t::Paused)
			cdda.status = cdda_t::Playing;
		SecNumber.Status = GD_PLAY;
		break;

	case GDCC_STOP:
		DEBUG_LOG(REIOS, "GDROM: CMD STOP");
		cdda.status = cdda_t::NoInfo;
		SecNumber.Status = GD_STANDBY;
		break;

	case GDCC_SEEK:
		DEBUG_LOG(REIOS, "GDROM: CMD SEEK");
		cdda.status = cdda_t::Paused;
		SecNumber.Status = GD_PAUSE;
		break;

	case GDCC_PLAY:
		{
			u32 first_track = gd_hle_state.params[0];
			u32 last_track = gd_hle_state.params[1];
			u32 repeats = gd_hle_state.params[2];
			u32 start_fad, end_fad, dummy;
			libGDR_GetTrack(first_track, start_fad, dummy);
			libGDR_GetTrack(last_track, dummy, end_fad);
			DEBUG_LOG(REIOS, "GDROM: CMD PLAY first_track %x last_track %x repeats %x start_fad %x end_fad %x param4 %x", first_track, last_track, repeats,
					start_fad, end_fad, gd_hle_state.params[3]);
			cdda.status = cdda_t::Playing;
			cdda.StartAddr.FAD = start_fad;
			cdda.EndAddr.FAD = end_fad;
			cdda.repeats = repeats;
			if (SecNumber.Status != GD_PAUSE || cdda.CurrAddr.FAD < start_fad || cdda.CurrAddr.FAD > end_fad)
				cdda.CurrAddr.FAD = start_fad;
			SecNumber.Status = GD_PLAY;
		}
		break;

	case GDCC_PAUSE:
		DEBUG_LOG(REIOS, "GDROM: CMD PAUSE");
		if (cdda.status == cdda_t::Playing)
			cdda.status = cdda_t::Paused;
		SecNumber.Status = GD_PAUSE;
		break;

	case GDCC_READ:
		{
			u32 sector = gd_hle_state.params[0];
			u32 num = gd_hle_state.params[1];

			DEBUG_LOG(REIOS, "GDROM: CMD READ Sector=%d, Num=%d", sector, num);
			gd_hle_state.status = BIOS_DATA_AVAIL;
			gd_hle_state.multi_read_sector = sector;
			gd_hle_state.multi_read_count = num * 2048;
			gd_hle_state.multi_read_total = gd_hle_state.multi_read_count;
			gd_hle_state.multi_read_offset = 0;
			gd_hle_state.result[2] = 2048;
			gd_hle_state.result[3] = num > 0 ? 1 : 0;
		}
		break;

	case GDCC_GETSCD:
		GDCC_HLE_GETSCD();
		break;

	case GDCC_REQ_MODE:
		{
			u32 dest = gd_hle_state.params[0];
			debugf("GDROM: REQ_MODE dest:%x", dest);
			WriteMem32(dest, GD_HardwareInfo.speed);
			WriteMem32(dest + 4, (GD_HardwareInfo.standby_hi << 8) | GD_HardwareInfo.standby_lo);
			WriteMem32(dest + 8, GD_HardwareInfo.read_flags);
			WriteMem32(dest + 12, GD_HardwareInfo.read_retry);

			// record size of pio transfer to gdrom
			gd_hle_state.result[2] = 0xa;
		}
		break;

	case GDCC_SET_MODE:
		{
			u32 speed = gd_hle_state.params[0];
			u32 standby = gd_hle_state.params[1];
			u32 read_flags = gd_hle_state.params[2];
			u32 read_retry = gd_hle_state.params[3];

			debugf("GDROM: SET_MODE speed %x standby %x read_flags %x read_retry %x", speed, standby, read_flags, read_retry);

			GD_HardwareInfo.speed = speed;
			GD_HardwareInfo.standby_hi = (standby & 0xff00) >> 8;
			GD_HardwareInfo.standby_lo = standby & 0xff;
			GD_HardwareInfo.read_flags = read_flags;
			GD_HardwareInfo.read_retry = read_retry;

			// record size of pio transfer to gdrom
			gd_hle_state.result[2] = 0xa;
		}
		break;

	case GDCC_GET_VER:
		{
			u32 dest = gd_hle_state.params[0];

			debugf("GDROM: GDCC_GET_VER dest %x", dest);

			char ver[] = "GDC Version 1.10 1999-03-31 ";
			u32 len = (u32)strlen(ver);

			// 0x8c0013b8 (offset 0xd0 in the gdrom state struct) is then loaded and
			// overwrites the last byte. no idea what this is, but seems to be hard
			// coded to 0x02 on boot
			ver[len - 1] = 0x02;

			for (u32 i = 0; i < len; i++)
				WriteMem8(dest++, ver[i]);
		}
		break;

	case GDCC_REQ_STAT:
		{
			// odd, but this function seems to get passed 4 unique pointers
			u32 dst0 = gd_hle_state.params[0];
			u32 dst1 = gd_hle_state.params[1];
			u32 dst2 = gd_hle_state.params[2];
			u32 dst3 = gd_hle_state.params[3];

			debugf("GDROM: GDCC_REQ_STAT dst0=%08x dst1=%08x dst2=%08x dst3=%08x", dst0, dst1, dst2, dst3);

			// bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0
			// byte  |     |     |     |     |     |     |     |
			// ------------------------------------------------------
			// 0     |  0  |  0  |  0  |  0  | status
			// ------------------------------------------------------
			// 1     |  0  |  0  |  0  |  0  | repeat count
			// ------------------------------------------------------
			// 2-3   |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0
//			WriteMem32(dst0, (cdda.repeats << 8) | SecNumber.Status);
			WriteMem32(dst0, (cdda.repeats << 8) | (SecNumber.Status == GD_STANDBY ? GD_PAUSE : SecNumber.Status));

			// bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0
			// byte  |     |     |     |     |     |     |     |
			// ------------------------------------------------------
			// 0     | subcode q track number
			// ------------------------------------------------------
			// 1-3   |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0
			u32 elapsed;
			u32 tracknum = libGDR_GetTrackNumber(gd_hle_state.cur_sector, elapsed);
			WriteMem32(dst1, tracknum);

			// bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0
			// byte  |     |     |     |     |     |     |     |
			// ------------------------------------------------------
			// 0-2  | fad (little-endian)
			// ------------------------------------------------------
			// 3    | address                | control
			u32 out = (((SecNumber.DiscFormat == 0 ? 0 : 0x40) | 1) << 24)
					| (gd_hle_state.cur_sector & 0x00ffffff);
			WriteMem32(dst2, out);

			// bit   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0
			// byte  |     |     |     |     |     |     |     |
			// ------------------------------------------------------
			// 0     | subcode q index number
			// ------------------------------------------------------
			// 1-3   |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0
			WriteMem32(dst3, 1);

			// record pio transfer size
			gd_hle_state.result[2] = 0xa;
		}
		break;

	case GDCC_MULTI_DMAREAD:
	case GDCC_MULTI_PIOREAD:
		{
			u32 sector = gd_hle_state.params[0];
			u32 num = gd_hle_state.params[1];
			bool dma = cc == GDCC_MULTI_DMAREAD;

			DEBUG_LOG(REIOS, "GDROM: MULTI_%sREAD Sector=%d, Num=%d", dma ? "DMA" : "PIO", sector, num);

			gd_hle_state.status = BIOS_DATA_AVAIL;
			gd_hle_state.multi_read_sector = sector;
			gd_hle_state.multi_read_count = num * 2048;
			gd_hle_state.multi_read_total = gd_hle_state.multi_read_count;
			gd_hle_state.multi_read_offset = 0;

			// wild guesses here
			gd_hle_state.result[2] = 0;
			gd_hle_state.result[3] = 0;
		}
		break;

	case GDCC_REQ_DMA_TRANS:
	case GDCC_REQ_PIO_TRANS:
		{
			u32 dest = gd_hle_state.params[0];
			u32 size = gd_hle_state.params[1];
			bool dma = cc == GDCC_REQ_DMA_TRANS;
			DEBUG_LOG(REIOS, "GDROM: REQ_%s_TRANS dest %x size %x", dma ? "DMA" : "PIO",
					dest, size);
			if (dma)
				multi_xfer<true>();
			else
				multi_xfer<false>();
		}
		break;

	default:
		WARN_LOG(REIOS, "GDROM: Unknown GDROM CC:%X", cc);
		break;
	}
	if (gd_hle_state.status == BIOS_ACTIVE)
		gd_hle_state.status = BIOS_COMPLETED;
	gd_hle_state.command = -1;
}

#define r Sh4cntx.r

void gdrom_hle_op()
{
	if( SYSCALL_GDROM == r[6] )		// GDROM SYSCALL
	{
		switch(r[7])				// COMMAND CODE
		{
		case GDROM_SEND_COMMAND:
			// Enqueue a command for the GDROM subsystem to execute.
			//
			// Args:
			//	r4 = command code
			//	r5 = pointer to parameter block for the command, can be NULL if the command does not take parameters
			//
			// Returns: a request id (>=0) if successful, negative error code if failed
			debugf("GDROM: HLE SEND COMMAND CC:%X  param ptr: %X bios status %d", r[4], r[5], gd_hle_state.status);
			if (gd_hle_state.status != BIOS_INACTIVE)
			{
				r[0] = 0;
			}
			else
			{
				for (int i = 0; i < 4; i++)
				{
					try {
						gd_hle_state.params[i] = r[5] == 0 ? 0 : ReadMem32(r[5] + i * 4);
					} catch (SH4ThrownException& ex) {
						// Ignore page faults. happens for commands not taking params
						gd_hle_state.params[i] = 0;
					}
				}
				memset(gd_hle_state.result, 0, sizeof(gd_hle_state.result));
				if (gd_hle_state.next_request_id == ~0u || gd_hle_state.next_request_id == 0)
					gd_hle_state.next_request_id = 1;
				gd_hle_state.last_request_id = r[0] = gd_hle_state.next_request_id++;
				gd_hle_state.status = BIOS_ACTIVE;
				gd_hle_state.command = r[4];
				gd_hle_state.multi_read_count = 0;
			}
			break;

		case GDROM_CHECK_COMMAND:
			// Check if an enqueued command has completed.
			//
			// Args:
			//	r4 = request id
			//	r5 = pointer to four 32 bit integers to receive extended status information. The first is a generic error code.
			//
			// Returns:
			//	0 - no such request active
			//	1 - request is still being processed
			//	2 - request has completed (if queried again, you will get a 0)
			//	3 - multi request has data available
			//	-1 - request has failed (examine extended status information for cause of failure)
			try {
				WriteMem32(r[5], gd_hle_state.result[0]);
				WriteMem32(r[5] + 4, gd_hle_state.result[1]);
				WriteMem32(r[5] + 8, gd_hle_state.result[2]);
				WriteMem32(r[5] + 12, gd_hle_state.result[3]);
			} catch (SH4ThrownException& ex) {
			}
			if (gd_hle_state.status == BIOS_INACTIVE || gd_hle_state.status == BIOS_ACTIVE)
			{
				r[0] = gd_hle_state.status;	// no such request active or still being processed
			}
			else if (r[4] != gd_hle_state.last_request_id)
			{
				r[0] = 0; // no such request active
			}
			else
			{
				if (gd_hle_state.status == BIOS_DATA_AVAIL && gd_hle_state.command == GDCC_REQ_PIO_TRANS)
					r[0] = BIOS_ACTIVE;	// Bust-a-move 4 likes this
				else
					r[0] = gd_hle_state.status;	// completed or error
				// Fixes NBA 2K
				if (gd_hle_state.status == BIOS_DATA_AVAIL && gd_hle_state.multi_read_count == 0)
				{
					gd_hle_state.status = BIOS_COMPLETED;
					gd_hle_state.result[3] = 0;
				}
				else if (gd_hle_state.status != BIOS_DATA_AVAIL)
				{
					gd_hle_state.status = BIOS_INACTIVE;
					gd_hle_state.last_request_id = 0xFFFFFFFF;
				}
			}
			debugf("GDROM: HLE CHECK COMMAND REQID:%X  param ptr: %X -> %X : %x %x %x %x", r[4], r[5], r[0],
					gd_hle_state.result[0], gd_hle_state.result[1], gd_hle_state.result[2], gd_hle_state.result[3]);
			break;

		case GDROM_MAIN:
			// In order for enqueued commands to get processed, this function must be called a few times.
			debugf("GDROM: HLE GDROM_MAIN");
			if (gd_hle_state.status == BIOS_ACTIVE || (gd_hle_state.status == BIOS_DATA_AVAIL && gd_hle_state.command == GDCC_REQ_PIO_TRANS))
			{
				GD_HLE_Command(gd_hle_state.command);
			}
			break;

		case GDROM_INIT:
			// Initialize the GDROM subsystem. Should be called before any requests are enqueued.
			DEBUG_LOG(REIOS, "GDROM: HLE GDROM_INIT");
			gd_hle_state = {};
			break;

		case GDROM_RESET:
			// Resets the drive.
			DEBUG_LOG(REIOS, "GDROM: HLE GDROM_RESET");
			gd_hle_state.last_request_id = 0xFFFFFFFF;
			gd_hle_state.status = BIOS_INACTIVE;
			break;

		case GDROM_CHECK_DRIVE:
			{
				// Checks the general condition of the drive.
				//
				// Args:
				//	r4 = pointer to two 32 bit integers, to receive the drive status. The first is the current drive status, the second is the type of disc inserted (if any).
				//	0 	Drive is busy
				//	1 	Drive is paused
				//	2 	Drive is in standby
				//	3 	Drive is playing
				//	4 	Drive is seeking
				//	5 	Drive is scanning
				//	6 	Drive lid is open
				//	7 	Lid is closed, but there is no disc
				//
				// Returns: zero if successful, nonzero if failure
				u32 discType = libGDR_GetDiscType();
				switch (discType)
				{
				case Open:
					WriteMem32(r[4], 6);
					WriteMem32(r[4] + 4, 0);
					break;
				case NoDisk:
					WriteMem32(r[4], 7);
					WriteMem32(r[4] + 4, 0);
					break;
				default:
					WriteMem32(r[4], (gd_hle_state.status == BIOS_DATA_AVAIL || SecNumber.Status == GD_PLAY) ? 3 : 1);
					if (memcmp(ip_meta.disk_type, "GD-ROM", sizeof(ip_meta.disk_type)) == 0)
						WriteMem32(r[4] + 4, GdRom);
					else
						WriteMem32(r[4] + 4, discType);
					break;
				}
				debugf("GDROM: HLE GDROM_CHECK_DRIVE r4:%X -> %x %x", r[4], ReadMem32(r[4]), ReadMem32(r[4] + 4));
				r[0] = 0;
			}
			break;

		case GDROM_ABORT_COMMAND:
			// Tries to abort a previously enqueued command.
			//
			// Args:
			//	r4 = request id
			//
			// Returns: zero if successful, nonzero if failure
			WARN_LOG(REIOS, "GDROM: HLE GDROM_ABORT_COMMAND r4:%X",r[4]);
			if (r[4] == gd_hle_state.last_request_id
					&& (gd_hle_state.status == BIOS_DATA_AVAIL || gd_hle_state.status == BIOS_ACTIVE))
			{
				r[0] = 0;
				gd_hle_state.multi_read_count = 0;
				gd_hle_state.xfer_end_time = 0;
			}
			else
			{
				r[0] = -1;
			}
			break;


		case GDROM_SECTOR_MODE:
			// Sets/gets the sector format for read commands.
			//
			// Args:
			//	r4 = pointer to a struct of four 32 bit integers containing new values, or to receive the old values
			//	Field	Function
			//	0 	Get/Set, if 0 the mode will be set, if 1 it will be queried.
			//	1 	? (always 8192)
			//	2 	1024 = mode 1, 2048 = mode 2, 0 = auto detect
			//	3 	Sector size in bytes (normally 2048)
			//
			// Returns: zero if successful, -1 if failure
			DEBUG_LOG(REIOS, "GDROM: HLE GDROM_SECTOR_MODE PTR_r4:%X",r[4]);
			for(int i=0; i<4; i++) {
				SecMode[i] = ReadMem32(r[4]+(i<<2));
				DEBUG_LOG(REIOS, "%08X", SecMode[i]);
			}
			r[0] = 0;
			break;

		case GDROM_G1_DMA_END:
			DEBUG_LOG(REIOS, "GDROM: G1_DMA_END callback %x arg %x", r[4], r[5]);
			gd_hle_state.multi_callback = r[4];
			gd_hle_state.multi_callback_arg = r[5];
			r[0] = 0;
			if (gd_hle_state.multi_callback != 0 && gd_hle_state.dma_trans_ended)	// FIXME hack for 2K sports games
			{
				r[4] = gd_hle_state.multi_callback_arg;
				Sh4cntx.pc = gd_hle_state.multi_callback;
				gd_hle_state.dma_trans_ended = false;
			}
			break;

		case GDROM_REQ_DMA_TRANS:
			gd_hle_state.params[0] = ReadMem32(r[5]);
			gd_hle_state.params[1] = ReadMem32(r[5] + 4);
			DEBUG_LOG(REIOS, "GDROM: REQ_DMA_TRANS req_id %x dest %x size %x",
					r[4], gd_hle_state.params[0], gd_hle_state.params[1]);

			if (gd_hle_state.status != BIOS_DATA_AVAIL || gd_hle_state.params[1] > gd_hle_state.multi_read_count)
			{
				r[0] = -1;
			}
			else
			{
				multi_xfer<true>();
				r[0] = 0;
			}
			break;

		case GDROM_REQ_PIO_TRANS:
			gd_hle_state.params[0] = ReadMem32(r[5]);
			gd_hle_state.params[1] = ReadMem32(r[5] + 4);
			DEBUG_LOG(REIOS, "GDROM: REQ_PIO_TRANS req_id %x dest %x size %x",
					r[4], gd_hle_state.params[0], gd_hle_state.params[1]);
			if (gd_hle_state.status != BIOS_DATA_AVAIL || gd_hle_state.params[1] > gd_hle_state.multi_read_count)
			{
				r[0] = -1;
			}
			else
			{
				gd_hle_state.command = GDCC_REQ_PIO_TRANS;
				r[0] = 0;
			}
			break;

		case GDROM_CHECK_DMA_TRANS:
			{
				u32 len_addr = r[5];
				DEBUG_LOG(REIOS, "GDROM: CHECK_DMA_TRANS req_id %x len_addr %x -> %x", r[4], len_addr, gd_hle_state.multi_read_count);
				if (gd_hle_state.status == BIOS_DATA_AVAIL)
				{
					WriteMem32(len_addr, gd_hle_state.multi_read_count);
					r[0] = 0;
				}
				else
				{
					r[0] = -1;
				}
			}
			break;

		case GDROM_SET_PIO_CALLBACK:
			DEBUG_LOG(REIOS, "GDROM: SET_PIO_CALLBACK callback %x arg %x", r[4], r[5]);
			gd_hle_state.multi_callback = r[4];
			gd_hle_state.multi_callback_arg = r[5];
			r[0] = 0;
			break;

		case GDROM_CHECK_PIO_TRANS:
			{
				u32 len_addr = r[5];
				DEBUG_LOG(REIOS, "GDROM: CHECK_PIO_TRANS req_id %x len_addr %x -> %x", r[4], len_addr, gd_hle_state.multi_read_count);
				if (gd_hle_state.status == BIOS_DATA_AVAIL)
				{
					WriteMem32(len_addr, gd_hle_state.multi_read_count);
					r[0] = 0;
				}
				else
				{
					r[0] = -1;
				}
			}
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
			// Initializes all the syscall vectors to their default values.
			// Returns: zero
			WARN_LOG(REIOS, "GDROM: MISC_INIT not implemented");
			r[0] = 0;
			break;

		case MISC_SETVECTOR:
			// Sets/clears the handler for one of the eight superfunctions for this vector. Setting a handler is only allowed if it not currently set.
			//
			// Args:
			//	r4 = superfunction number (0-7)
			//	r5 = pointer to handler function, or NULL to clear
			//
			// Returns: zero if successful, -1 if setting/clearing the handler fails
			WARN_LOG(REIOS, "GDROM: MISC_SETVECTOR not implemented");
			break;

		default:
			WARN_LOG(REIOS, "GDROM: Unknown MISC command %x", r[7]);
			break;
		}
	}
}
