#pragma once
#include "types.h"
#include <vector>

#include "deps/coreio/coreio.h"
#include "emulator.h"
#include "rend/gui.h"

extern u32 NullDriveDiscType;
struct TocTrackInfo
{
	u32 FAD;    //fad, Intel format
	u8 Control; //control info
	u8 Addr;    //address info
	u8 Session; //Session where the track belongs
};
struct TocInfo
{
	//0-98 ->1-99
	TocTrackInfo tracks[99];

	u8 FistTrack;
	u8 LastTrack;

	TocTrackInfo LeadOut; //session set to 0 on that one
};

struct SessionInfo
{
	u32 SessionsEndFAD;   //end of Disc (?)
	u8 SessionCount;      //must be at least 1
	u32 SessionStart[99]; //start track for session
	u32 SessionFAD[99];   //for sessions 1-99 ;)
};

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

bool ConvertSector(u8* in_buff , u8* out_buff , int from , int to,int sector);

bool InitDrive();
void TermDrive();
bool DiscSwap();
void DiscOpenLid();
extern signed int sns_asc;
extern signed int sns_ascq;
extern signed int sns_key;

void ConvToc(u32* to,TocInfo* from);
void GetDriveToc(u32* to,DiskArea area);
void GetDriveSector(u8 * buff,u32 StartSector,u32 SectorCount,u32 secsz);

void GetDriveSessionInfo(u8* to,u8 session);
int msgboxf(char* text,unsigned int type,...);
void printtoc(TocInfo* toc,SessionInfo* ses);
extern u8 q_subchannel[96];


struct Session
{
	u32 StartFAD;			//session's start fad
	u8 FirstTrack;			//session's first track
};

struct TrackFile
{
	virtual void Read(u32 FAD,u8* dst,SectorFormat* sector_type,u8* subcode,SubcodeFormat* subcode_type)=0;
	virtual ~TrackFile() {};
};

struct Track
{
	TrackFile* file;	//handler for actual IO
	u32 StartFAD;		//Start FAD
	u32 EndFAD;			//End FAD
	u8 CTRL;
	u8 ADDR;

	Track()
	{
		file = 0;
		StartFAD = 0;
		EndFAD = 0;
		CTRL = 0;
		ADDR = 0;
	}
	bool Read(u32 FAD,u8* dst,SectorFormat* sector_type,u8* subcode,SubcodeFormat* subcode_type)
	{
		if (FAD>=StartFAD && (FAD<=EndFAD || EndFAD==0) && file)
		{
			file->Read(FAD,dst,sector_type,subcode,subcode_type);
			return true;
		}
		else
			return false;
	}
	void Destroy() { delete file; file=0; }
};

struct Disc
{
	std::wstring path;
	std::vector<Session> sessions;	//info for sessions
	std::vector<Track> tracks;		//info for tracks
	Track LeadOut;				//info for lead out track (can't read from here)
	u32 EndFAD;					//Last valid disc sector
	DiscType type;

	//functions !
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

	void ReadSectors(u32 FAD,u32 count,u8* dst,u32 fmt)
	{
		u8 temp[2448];
		SectorFormat secfmt;
		SubcodeFormat subfmt;

		u32 progress = ~0;
		for (u32 i = 1; i <= count; i++)
		{
			if (count >= 1000)
			{
				if (loading_canceled)
					break;
				// Progress report when loading naomi gd-rom games
				const u32 new_progress = i * 100 / count;
				if (progress != new_progress)
				{
					progress = new_progress;
					char status_str[16];
					sprintf(status_str, "%d%%", progress);
					gui_display_notification(status_str, 2000);
				}
			}
			if (ReadSector(FAD,temp,&secfmt,q_subchannel,&subfmt))
			{
				//TODO: Proper sector conversions
				if (secfmt==SECFMT_2352)
				{
					ConvertSector(temp,dst,2352,fmt,FAD);
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
					ConvertSector(temp, dst, 2448, fmt, FAD);
				}
				else
				{
					WARN_LOG(GDROM, "ERROR: UNABLE TO CONVERT SECTOR. THIS IS FATAL. Format: %d Sector format: %d", fmt, secfmt);
					//verify(false);
				}
			}
			else
			{
				INFO_LOG(GDROM, "Sector Read miss FAD: %d", FAD);
			}
			dst+=fmt;
			FAD++;
		}
	}
	virtual ~Disc() 
	{
		for (size_t i=0;i<tracks.size();i++)
			tracks[i].Destroy();
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
			
			FILE* fo=fopen(fsto,"wb");

			for (u32 j=tracks[i].StartFAD;j<=tracks[i].EndFAD;j++)
			{
				u8 temp[2352];
				ReadSectors(j,1,temp,fmt);
				fwrite(temp,fmt,1,fo);
			}
			fclose(fo);
		}
	}
};

extern Disc* disc;

Disc* OpenDisc(const char* fn);

struct RawTrackFile : TrackFile
{
	core_file* file;
	s32 offset;
	u32 fmt;
	bool cleanup;

	RawTrackFile(core_file* file,u32 file_offs,u32 first_fad,u32 secfmt)
	{
		verify(file!=0);
		this->file=file;
		this->offset=file_offs-first_fad*secfmt;
		this->fmt=secfmt;
		this->cleanup=true;
	}

	virtual void Read(u32 FAD,u8* dst,SectorFormat* sector_type,u8* subcode,SubcodeFormat* subcode_type)
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

		core_fseek(file,offset+FAD*fmt,SEEK_SET);
		core_fread(file, dst, fmt);
	}
	virtual ~RawTrackFile()
	{
		if (cleanup && file)
			core_fclose(file);
	}
};

DiscType GuessDiscType(bool m1, bool m2, bool da);

extern void gd_setdisc();
