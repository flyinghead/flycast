#include "common.h"
#include "hw/gdrom/gdromv3.h"
#include "cfg/option.h"
#include "stdclass.h"
#include "hw/sh4/sh4_sched.h"
#include "serialize.h"

Disc* chd_parse(const char* file, std::vector<u8> *digest);
Disc* gdi_parse(const char* file, std::vector<u8> *digest);
Disc* cdi_parse(const char* file, std::vector<u8> *digest);
Disc* cue_parse(const char* file, std::vector<u8> *digest);
Disc* ioctl_parse(const char* file, std::vector<u8> *digest);

static u32 NullDriveDiscType;
Disc* disc;
static int schedId = -1;

constexpr Disc* (*drivers[])(const char* path, std::vector<u8> *digest)
{
	chd_parse,
	gdi_parse,
	cdi_parse,
	cue_parse,
#if defined(_WIN32) && !defined(TARGET_UWP)
	ioctl_parse,
#endif
};

static u8 q_subchannel[96];

static bool convertSector(u8* in_buff , u8* out_buff , int from , int to,int sector)
{
	//get subchannel data, if any
	if (from == 2448)
	{
		memcpy(q_subchannel, in_buff + 2352, 96);
		from -= 96;
	}
	else
		memset(q_subchannel, 0, sizeof(q_subchannel));

	//if no conversion
	if (to == from)
	{
		memcpy(out_buff, in_buff, to);
		return true;
	}
	switch (to)
	{
	case 2340:
		verify(from == 2352);
		memcpy(out_buff, &in_buff[12], 2340);
		break;
	case 2328:
		verify(from == 2352);
		memcpy(out_buff, &in_buff[24], 2328);
		break;
	case 2336:
		verify(from == 2352);
		memcpy(out_buff, &in_buff[0x10], 2336);
		break;
	case 2048:
		verify(from == 2448 || from == 2352 || from == 2336);
		if (from == 2352 || from == 2448)
		{
			if (in_buff[15] == 1)
				memcpy(out_buff, &in_buff[0x10], 2048); //0x10 -> mode1
			else
				memcpy(out_buff, &in_buff[0x18], 2048); //0x18 -> mode2 (all forms ?)
		}
		else
			memcpy(out_buff, &in_buff[0x8], 2048);	//hmm only possible on mode2.Skip the mode2 header
		break;
	case 2352:
		//if (from >= 2352)
		memcpy(out_buff, &in_buff[0], 2352);
		break;
	default :
		INFO_LOG(GDROM, "Sector conversion from %d to %d not supported \n", from , to);
		break;
	}

	return true;
}

Disc* OpenDisc(const std::string& path, std::vector<u8> *digest)
{
	for (auto driver : drivers)
	{
		Disc *disc = driver(path.c_str(), digest);

		if (disc != nullptr)
			return disc;
	}

	throw FlycastException("Unknown disk format");
}

static bool loadDisk(const std::string& path)
{
	TermDrive();

	//try all drivers
	std::vector<u8> digest;
	disc = OpenDisc(path, config::GGPOEnable ? &digest : nullptr);

	if (disc != NULL)
	{
		if (config::GGPOEnable)
			MD5Sum().add(digest)
					.getDigest(settings.network.md5.game);
		INFO_LOG(GDROM, "gdrom: Opened image \"%s\"", path.c_str());
	}
	else
	{
		INFO_LOG(GDROM, "gdrom: Failed to open image \"%s\"", path.c_str());
		NullDriveDiscType = NoDisk;
	}

	return disc != NULL;
}

static bool doDiscSwap(const std::string& path);

bool InitDrive(const std::string& path)
{
	bool rc = doDiscSwap(path);
	if (rc && disc == nullptr)
	{
		// Drive is busy
		sns_asc = 4;
		sns_ascq = 1;
		sns_key = 2;
		SecNumber.Status = GD_BUSY;
		sh4_sched_request(schedId, SH4_MAIN_CLOCK);
	}
	else {
		gd_setdisc();
	}

	return rc;
}

void DiscOpenLid()
{
	TermDrive();
	NullDriveDiscType = Open;
	gd_setdisc();
}

static bool doDiscSwap(const std::string& path)
{
	if (path.empty())
	{
		TermDrive();
		NullDriveDiscType = NoDisk;
		return true;
	}

	if (loadDisk(path))
		return true;

	NullDriveDiscType = NoDisk;
	return false;
}

void TermDrive()
{
	sh4_sched_request(schedId, -1);
	delete disc;
	disc = nullptr;
}


//
//convert our nice toc struct to dc's native one :)

static u32 createTrackInfo(const Track& track, u32 fad)
{
	const u32 adr = 1; // force sub-q channel
	u8 p[4];
	p[0] = (track.CTRL << 4) | adr;
	p[1] = fad >> 16;
	p[2] = fad >> 8;
	p[3] = fad >> 0;

	return *(u32 *)p;
}

static u32 createTrackInfoFirstLast(const Track& track, u32 tracknum)
{
	return createTrackInfo(track, tracknum << 16);
}

void libGDR_ReadSector(u8 *buff, u32 startSector, u32 sectorCount, u32 sectorSize)
{
	if (disc != nullptr)
		disc->ReadSectors(startSector, sectorCount, buff, sectorSize);
}

void libGDR_GetToc(u32* to, DiskArea area)
{
	memset(to, 0xFF, 102 * 4);
	if (!disc)
		return;

	//can't get toc on the second area on discs that don't have it
	if (area == DoubleDensity && disc->type != GdRom)
		return;

	//normal CDs: 1 .. tc
	//GDROM: area0 is 1 .. 2, area1 is 3 ... tc

	u32 first_track = 1;
	u32 last_track = disc->tracks.size();
	if (area == DoubleDensity)
		first_track = 3;
	else if (disc->type == GdRom)
		last_track = 2;

	//Generate the TOC info

	//-1 for 1..99 0 ..98
	to[99] = createTrackInfoFirstLast(disc->tracks[first_track - 1], first_track);
	to[100] = createTrackInfoFirstLast(disc->tracks[last_track - 1], last_track);

	if (disc->type == GdRom && area == SingleDensity)
		to[101] = createTrackInfo(disc->LeadOut, disc->tracks[1].EndFAD + 1);
	else
		to[101] = createTrackInfo(disc->LeadOut, disc->LeadOut.StartFAD);

	for (u32 i = first_track - 1; i < last_track; i++)
		to[i] = createTrackInfo(disc->tracks[i], disc->tracks[i].StartFAD);
}

void libGDR_GetSessionInfo(u8* to, u8 session)
{
	if (disc != nullptr)
		disc->GetSessionInfo(to, session);
}

DiscType GuessDiscType(bool m1, bool m2, bool da)
{
	if ((m1==true) && (da==false) && (m2==false))
		return  CdRom;
	else if (m2)
		return  CdRom_XA;
	else if (da && m1)
		return CdRom_Extra;
	else
		return CdRom;
}

bool Disc::readSector(u32 FAD, u8 *dst, SectorFormat *sector_type, u8 *subcode, SubcodeFormat *subcode_type)
{
	for (size_t i = tracks.size(); i-- > 0; )
	{
		*subcode_type = SUBFMT_NONE;
		if (tracks[i].Read(FAD, dst, sector_type, subcode, subcode_type))
			return true;
	}

	return false;
}

void Disc::ReadSectors(u32 FAD, u32 count, u8* dst, u32 fmt, LoadProgress *progress)
{
	u8 temp[2448];
	SectorFormat secfmt;
	SubcodeFormat subfmt;

	for (u32 i = 1; i <= count; i++)
	{
		if (progress != nullptr)
		{
			if (progress->cancelled)
				throw LoadCancelledException();
			progress->label = "Loading...";
			progress->progress = (float)i / count;
		}
		if (readSector(FAD, temp, &secfmt, q_subchannel, &subfmt))
		{
			//TODO: Proper sector conversions
			if (secfmt==SECFMT_2352)
			{
				convertSector(temp,dst,2352,fmt,FAD);
			}
			else if (fmt == 2048 && secfmt==SECFMT_2336_MODE2)
				memcpy(dst,temp+8,2048);
			else if (fmt==2048 && (secfmt==SECFMT_2048_MODE1 || secfmt==SECFMT_2048_MODE2_FORM1 ))
			{
				memcpy(dst,temp,2048);
			}
			else if (fmt==2352 && (secfmt==SECFMT_2048_MODE1 || secfmt==SECFMT_2048_MODE2_FORM1 ))
			{
				INFO_LOG(GDROM, "GDR:fmt=2352;secfmt=2048");
				memcpy(dst,temp,2048);
			}
			else if (fmt==2048 && secfmt==SECFMT_2448_MODE2)
			{
				// Pier Solar and the Great Architects
				convertSector(temp, dst, 2448, fmt, FAD);
			}
			else
			{
				WARN_LOG(GDROM, "ERROR: UNABLE TO CONVERT SECTOR. THIS IS FATAL. Format: %d Sector format: %d", fmt, secfmt);
				//verify(false);
			}
		}
		else
		{
			WARN_LOG(GDROM, "Sector Read miss FAD: %d", FAD);
			memset(dst, 0, fmt);
		}
		dst+=fmt;
		FAD++;
	}
}

void libGDR_ReadSubChannel(u8 * buff, u32 len)
{
	memcpy(buff, q_subchannel, len);
}

u32 libGDR_GetDiscType()
{
	// Pretend no disk is inserted if a disk swapping is in progress
	if (!sh4_sched_is_scheduled(schedId) && disc != nullptr)
		return disc->type;
	else
		return NullDriveDiscType;
}

static int discSwapCallback(int tag, int sch_cycl, int jitter, void *arg)
{
	if (disc != nullptr)
		// The lid was closed
		sns_asc = 0x28;
	else
		// No disc inserted at the time of power-on, reset or hard reset, or TOC cannot be read.
		sns_asc = 0x29;
	sns_ascq = 0;
	sns_key = 6;
	gd_setdisc();

	return 0;
}

bool DiscSwap(const std::string& path)
{
	if (!doDiscSwap(path))
		throw FlycastException("This media cannot be loaded");
	EventManager::event(Event::DiskChange);
	// Drive is busy after the lid was closed
	sns_asc = 4;
	sns_ascq = 1;
	sns_key = 2;
	SecNumber.Status = GD_BUSY;
	sh4_sched_request(schedId, SH4_MAIN_CLOCK); // 1 s

	return true;
}

void libGDR_init()
{
	verify(schedId == -1);
	schedId = sh4_sched_register(0, discSwapCallback);
}
void libGDR_term()
{
	TermDrive();
	sh4_sched_unregister(schedId);
	schedId = -1;
}

void libGDR_serialize(Serializer& ser)
{
	ser << NullDriveDiscType;
	ser << q_subchannel;
	sh4_sched_serialize(ser, schedId);
}
void libGDR_deserialize(Deserializer& deser)
{
	deser >> NullDriveDiscType;
	deser >> q_subchannel;
	if (deser.version() >= Deserializer::V46)
		sh4_sched_deserialize(deser, schedId);
	else
		sh4_sched_request(schedId, -1);
}
