// nullGDR.cpp : Defines the entry point for the DLL application.
//

//Get a copy of the operators for structs ... ugly , but works :)
#include "common.h"

void GetSessionInfo(u8* out,u8 ses);

void libGDR_ReadSubChannel(u8 * buff, u32 format, u32 len)
{
	if (format==0)
	{
		memcpy(buff,q_subchannel,len);
	}
}

void libGDR_ReadSector(u8 * buff,u32 StartSector,u32 SectorCount,u32 secsz)
{
	GetDriveSector(buff,StartSector,SectorCount,secsz);
	//if (CurrDrive)
	//	CurrDrive->ReadSector(buff,StartSector,SectorCount,secsz);
}

void libGDR_GetToc(u32* toc,u32 area)
{
	GetDriveToc(toc,(DiskArea)area);
}

u32 libGDR_GetTrackNumber(u32 sector, u32& elapsed)
{
	if (disc != NULL)
	{
		for (size_t i = 0; i < disc->tracks.size(); i++)
			if (disc->tracks[i].StartFAD <= sector && (sector <= disc->tracks[i].EndFAD || disc->tracks[i].EndFAD == 0))
			{
				elapsed = sector - disc->tracks[i].StartFAD;
				return i + 1;
			}
	}
	elapsed = 0;
	return 0xAA;
}

bool libGDR_GetTrack(u32 track_num, u32& start_fad, u32& end_fad)
{
	if (track_num == 0 || track_num > disc->tracks.size())
		return false;
	start_fad = disc->tracks[track_num - 1].StartFAD;
	end_fad = disc->tracks[track_num - 1].EndFAD;
	if (end_fad == 0)
	{
		if (track_num == disc->tracks.size())
			end_fad = disc->LeadOut.StartFAD - 1;
		else
			end_fad = disc->tracks[track_num].StartFAD - 1;
	}

	return true;
}

//TODO : fix up
u32 libGDR_GetDiscType()
{
	if (disc)
		return disc->type;
	else
		return NullDriveDiscType;
}

void libGDR_GetSessionInfo(u8* out,u8 ses)
{
	GetDriveSessionInfo(out,ses);
}

//It's supposed to reset everything (if not a soft reset)
void libGDR_Reset(bool hard)
{
	libCore_gdrom_disc_change();
}

//called when entering sh4 thread , from the new thread context (for any thread specific init)
s32 libGDR_Init()
{
	libCore_gdrom_disc_change();
	return 0;
}

//called when exiting from sh4 thread , from the new thread context (for any thread specific init) :P
void libGDR_Term()
{
	TermDrive();
}
