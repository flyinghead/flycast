#include "common.h"

Disc* chd_parse(const char* file);
Disc* gdi_parse(const char* file);
Disc* cdi_parse(const char* file);
Disc* cue_parse(const char* file);
#ifdef _WIN32
Disc* ioctl_parse(const char* file);
#endif

u32 NullDriveDiscType;
Disc* disc;

Disc*(*drivers[])(const char* path)=
{
	chd_parse,
	gdi_parse,
	cdi_parse,
	cue_parse,
#ifdef _WIN32
	ioctl_parse,
#endif
	0
};

u8 q_subchannel[96];

bool ConvertSector(u8* in_buff , u8* out_buff , int from , int to,int sector)
{
	//get subchannel data, if any
	if (from == 2448)
	{
		memcpy(q_subchannel, in_buff + 2352, 96);
		from -= 96;
	}
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

Disc* OpenDisc(const char* fn)
{
	Disc* rv = NULL;

	for (unat i=0; drivers[i] && !rv; i++) {  // ;drivers[i] && !(rv=drivers[i](fn));
		rv = drivers[i](fn);

		if (rv && cdi_parse == drivers[i]) {
			const char warn_str[] = "Warning: CDI Image Loaded! Many CDI images are known to be defective, GDI, CUE or CHD format is preferred. "
					"Please only file bug reports when using images known to be good (GDI, CUE or CHD).";
			WARN_LOG(GDROM, "%s", warn_str);

			break;
		}
	}

	return rv;
}

bool InitDrive_(char* fn)
{
	TermDrive();

	//try all drivers
	disc = OpenDisc(fn);

	if (disc != NULL)
	{
		INFO_LOG(GDROM, "gdrom: Opened image \"%s\"", fn);
		NullDriveDiscType = Busy;
	}
	else
	{
		INFO_LOG(GDROM, "gdrom: Failed to open image \"%s\"", fn);
		NullDriveDiscType = NoDisk; //no disc :)
	}
	libCore_gdrom_disc_change();

	return disc != NULL;
}

bool InitDrive()
{
	bool rc = DiscSwap();
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

bool DiscSwap()
{
	// These Additional Sense Codes mean "The lid was closed"
	sns_asc = 0x28;
	sns_ascq = 0x00;
	sns_key = 0x6;

	if (settings.imgread.ImagePath[0] == '\0')
	{
		NullDriveDiscType = NoDisk;
		gd_setdisc();
		return true;
	}

	if (InitDrive_(settings.imgread.ImagePath))
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

void GetDriveSector(u8 * buff,u32 StartSector,u32 SectorCount,u32 secsz)
{
	if (disc != nullptr)
		disc->ReadSectors(StartSector, SectorCount, buff, secsz);
}

void GetDriveToc(u32* to,DiskArea area)
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

void GetDriveSessionInfo(u8* to,u8 session)
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
