#pragma once
#include "types.h"
#include <vector>

#include "emulator.h"
#include "hw/gdrom/gdrom_if.h"

/*
Mode2 Subheader:

"1" file number for identification of nested files (0 = not interleaver.)

"2" channel number, the infantry of the various channels are selectable for playback

"3" SUBMODE byte:

7: last sector of file (EOF)
6: Real-time sector (f.Echtzeitwiedergabe without error correction)
5: Form 2 (bit = 1), form 1 (bit = 0)
4: Trigger on (depending on OS)
3: data sector (Submodebyte 3 or 2 or 1)
2: ADPCM audio sector "
1: Video-sector "
0: last sector of a record (EOR)
"4" Encoding type of audio (eg mono / stereo) and video data (in this byte data sectors is set to 0)

"5" to "8" is the repetition of "1" through "4"


RAW: 2352
MODE1:
SYNC (12) | HEAD (4) | data (2048) | edc (4) | space (8) | ecc (276)
MODE2:
SYNC (12) | HEAD (4) | sub-head (8) | sector_data (2328)
  -form1 sector_data: 
   data (2048) | edc (4) | ecc (276)

  -form2 sector_data: 
   data (2324) |edc(4)
*/

enum SectorFormat
{
	SECFMT_2352,				//full sector
	SECFMT_2048_MODE1,			//2048 user byte, form1 sector
	SECFMT_2048_MODE2_FORM1,	//2048 user bytes, form2m1 sector
	SECFMT_2336_MODE2,			//2336 user bytes, 
	SECFMT_2448_MODE2,			//2048 user bytes, ? SYNC (12) | HEAD (4) | sub-head (8) | data (2048) | edc (4) | ecc (276) + subcodes (96) ?
};

enum SubcodeFormat
{
	SUBFMT_NONE,				//No subcode info
	SUBFMT_96					//raw 96-byte subcode info
};

enum DiskArea
{
	SingleDensity,
	DoubleDensity
};

bool InitDrive(const std::string& path);
void TermDrive();
bool DiscSwap(const std::string& path);
void DiscOpenLid();

struct Session
{
	u32 StartFAD;			//session's start fad
	u8 FirstTrack;			//session's first track
};

struct TrackFile
{
	virtual bool Read(u32 FAD, u8 *dst, SectorFormat *sector_type, u8 *subcode, SubcodeFormat *subcode_type) = 0;
	virtual ~TrackFile() = default;
};

struct Track
{
	TrackFile* file = nullptr;	// handler for actual IO
	u32 StartFAD = 0;			// Start FAD
	u32 EndFAD = 0;				// End FAD
	u8 CTRL = 0;
	u8 ADDR = 0;

	bool Read(u32 FAD, u8 *dst, SectorFormat *sector_type, u8 *subcode, SubcodeFormat *subcode_type)
	{
		if (FAD >= StartFAD && (FAD <= EndFAD || EndFAD == 0) && file != nullptr)
			return file->Read(FAD, dst, sector_type, subcode, subcode_type);
		else
			return false;
	}
	void Destroy() {
		delete file;
		file = nullptr;
	}
};

struct Disc
{
	std::vector<Session> sessions;	//info for sessions
	std::vector<Track> tracks;		//info for tracks
	Track LeadOut;				//info for lead out track (can't read from here)
	u32 EndFAD;					//Last valid disc sector
	DiscType type;

	bool ReadSector(u32 FAD,u8* dst,SectorFormat* sector_type,u8* subcode,SubcodeFormat* subcode_type)
	{
		for (size_t i=tracks.size();i-->0;)
		{
			*subcode_type=SUBFMT_NONE;
			if (tracks[i].Read(FAD,dst,sector_type,subcode,subcode_type))
				return true;
		}

		return false;
	}

	void ReadSectors(u32 FAD, u32 count, u8 *dst, u32 fmt, LoadProgress *progress = nullptr);

	virtual ~Disc() 
	{
		for (auto& track : tracks)
			track.Destroy();
	};

	void FillGDSession()
	{
		Session ses;

		//session 1 : start @ track 1, and its fad
		ses.FirstTrack=1;
		ses.StartFAD=tracks[0].StartFAD;
		sessions.push_back(ses);

		//session 2 : start @ track 3, and its fad
		ses.FirstTrack=3;
		ses.StartFAD=tracks[2].StartFAD;
		sessions.push_back(ses);

		//this isn't always true for gdroms, depends on area look @ the get-toc code
		type=GdRom;
		LeadOut.ADDR=0;
		LeadOut.CTRL=0;
		LeadOut.StartFAD=549300;

		EndFAD=549300;
	}

	void Dump(const std::string& path)
	{
		for (u32 i=0;i<tracks.size();i++)
		{
			u32 fmt=tracks[i].CTRL==4?2048:2352;
			char fsto[1024];
			sprintf(fsto,"%s%s%d.img",path.c_str(),".track",i);
			
			FILE *fo = nowide::fopen(fsto, "wb");

			for (u32 j=tracks[i].StartFAD;j<=tracks[i].EndFAD;j++)
			{
				u8 temp[2352];
				ReadSectors(j,1,temp,fmt);
				std::fwrite(temp, fmt, 1, fo);
			}
			std::fclose(fo);
		}
	}
};

Disc* OpenDisc(const std::string& path, std::vector<u8> *digest = nullptr);

struct RawTrackFile : TrackFile
{
	FILE *file;
	s32 offset;
	u32 fmt;

	RawTrackFile(FILE *file, u32 file_offs, u32 first_fad, u32 secfmt)
	{
		verify(file != nullptr);
		this->file = file;
		this->offset = file_offs - first_fad * secfmt;
		this->fmt = secfmt;
	}

	bool Read(u32 FAD,u8* dst,SectorFormat* sector_type,u8* subcode,SubcodeFormat* subcode_type) override
	{
		//for now hackish
		if (fmt==2352)
			*sector_type=SECFMT_2352;
		else if (fmt==2048)
			*sector_type=SECFMT_2048_MODE2_FORM1;
		else if (fmt==2336)
			*sector_type=SECFMT_2336_MODE2;
		else if (fmt==2448)
			*sector_type=SECFMT_2448_MODE2;
		else
		{
			verify(false);
		}

		std::fseek(file, offset + FAD * fmt, SEEK_SET);
		if (std::fread(dst, 1, fmt, file) != fmt)
		{
			WARN_LOG(GDROM, "Failed or truncated GD-Rom read");
			return false;
		}
		return true;
	}

	~RawTrackFile() override
	{
		std::fclose(file);
	}
};

DiscType GuessDiscType(bool m1, bool m2, bool da);

//IO
void libGDR_ReadSector(u8 * buff,u32 StartSector,u32 SectorCount,u32 secsz);
void libGDR_ReadSubChannel(u8 * buff, u32 len);
void libGDR_GetToc(u32 *toc, DiskArea area);
u32 libGDR_GetDiscType();
void libGDR_GetSessionInfo(u8* pout,u8 session);
u32 libGDR_GetTrackNumber(u32 sector, u32& elapsed);
bool libGDR_GetTrack(u32 track_num, u32& start_fad, u32& end_fad);

namespace flycast
{

inline static size_t fsize(FILE *f)
{
	size_t p = std::ftell(f);
    std::fseek(f, 0, SEEK_END);
    size_t size = std::ftell(f);
    std::fseek(f, p, SEEK_SET);
    return size;
}

}
