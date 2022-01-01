#include "common.h"
#include "hw/gdrom/gdromv3.h"
#include "cfg/option.h"
#include "stdclass.h"

Disc* chd_parse(const char* file, std::vector<u8> *digest);
Disc* gdi_parse(const char* file, std::vector<u8> *digest);
Disc* cdi_parse(const char* file, std::vector<u8> *digest);
Disc* cue_parse(const char* file, std::vector<u8> *digest);
Disc* ioctl_parse(const char* file, std::vector<u8> *digest);

u32 NullDriveDiscType;
Disc* disc;

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

u8 q_subchannel[96];

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
		{
			if (cdi_parse == driver) {
				const char warn_str[] = "Warning: CDI Image Loaded! Many CDI images are known to be defective, GDI, CUE or CHD format is preferred. "
						"Please only file bug reports when using images known to be good (GDI, CUE or CHD).";
				WARN_LOG(GDROM, "%s", warn_str);
			}
			return disc;
		}
	}

	return nullptr;
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
	libCore_gdrom_disc_change();

	return disc != NULL;
}

bool InitDrive(const std::string& path)
{
	bool rc = DiscSwap(path);
	// not needed at startup and confuses some games
	sns_asc = 0;
	sns_ascq = 0;
	sns_key = 0;

	return rc;
}

void DiscOpenLid()
{
	TermDrive();
	NullDriveDiscType = Open;
	gd_setdisc();
	sns_asc = 0x29;
	sns_ascq = 0x00;
	sns_key = 0x6;
}

bool DiscSwap(const std::string& path)
{
	// These Additional Sense Codes mean "The lid was closed"
	sns_asc = 0x28;
	sns_ascq = 0x00;
	sns_key = 0x6;

	if (path.empty())
	{
		TermDrive();
		NullDriveDiscType = NoDisk;
		gd_setdisc();
		return true;
	}

	if (loadDisk(path))
		return true;

	NullDriveDiscType = NoDisk;
	gd_setdisc();

	return false;
}

void TermDrive()
{
	delete disc;
	disc = NULL;
}


//
//convert our nice toc struct to dc's native one :)

static u32 CreateTrackInfo(u32 ctrl, u32 addr, u32 fad)
{
	u8 p[4];
	p[0]=(ctrl<<4)|(addr<<0);
	p[1]=fad>>16;
	p[2]=fad>>8;
	p[3]=fad>>0;

	return *(u32*)p;
}

static u32 CreateTrackInfo_se(u32 ctrl, u32 addr, u32 tracknum)
{
	u8 p[4];
	p[0]=(ctrl<<4)|(addr<<0);
	p[1]=tracknum;
	p[2]=0;
	p[3]=0;
	return *(u32*)p;
}

void libGDR_ReadSector(u8 *buff, u32 startSector, u32 sectorCount, u32 sectorSize)
{
	if (disc != nullptr)
		disc->ReadSectors(startSector, sectorCount, buff, sectorSize);
}

void libGDR_GetToc(u32* to, DiskArea area)
{
	if (!disc)
		return;
	memset(to, 0xFF, 102 * 4);

	//can't get toc on the second area on discs that don't have it
	verify(area != DoubleDensity || disc->type == GdRom);

	//normal CDs: 1 .. tc
	//GDROM: area0 is 1 .. 2, area1 is 3 ... tc

	u32 first_track=1;
	u32 last_track=disc->tracks.size();
	if (area==DoubleDensity)
		first_track=3;
	else if (disc->type==GdRom)
		last_track=2;

	//Generate the TOC info

	//-1 for 1..99 0 ..98
	to[99]=CreateTrackInfo_se(disc->tracks[first_track-1].CTRL,disc->tracks[first_track-1].ADDR,first_track);
	to[100]=CreateTrackInfo_se(disc->tracks[last_track-1].CTRL,disc->tracks[last_track-1].ADDR,last_track);

	if (disc->type==GdRom)
	{
		//use smaller LEADOUT
		if (area==SingleDensity)
			to[101]=CreateTrackInfo(disc->LeadOut.CTRL,disc->LeadOut.ADDR,13085);
	}
	else
		to[101] = CreateTrackInfo(disc->LeadOut.CTRL, disc->LeadOut.ADDR, disc->LeadOut.StartFAD);

	for (u32 i=first_track-1;i<last_track;i++)
		to[i]=CreateTrackInfo(disc->tracks[i].CTRL,disc->tracks[i].ADDR,disc->tracks[i].StartFAD);
}

void libGDR_GetSessionInfo(u8* to, u8 session)
{
	if (!disc)
		return;
	to[0]=2;//status, will get overwritten anyway
	to[1]=0;//0's

	if (session==0)
	{
		to[2]=disc->sessions.size();//count of sessions
		to[3]=disc->EndFAD>>16;//fad is sessions end
		to[4]=disc->EndFAD>>8;
		to[5]=disc->EndFAD>>0;
	}
	else
	{
		to[2]=disc->sessions[session-1].FirstTrack;//start track of this session
		to[3]=disc->sessions[session-1].StartFAD>>16;//fad is session start
		to[4]=disc->sessions[session-1].StartFAD>>8;
		to[5]=disc->sessions[session-1].StartFAD>>0;
	}
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
		if (ReadSector(FAD,temp,&secfmt,q_subchannel,&subfmt))
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
	if (disc)
		return disc->type;
	else
		return NullDriveDiscType;
}
