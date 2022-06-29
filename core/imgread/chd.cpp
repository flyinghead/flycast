#include "common.h"
#include "stdclass.h"

#include <libchdr/chd.h>

struct CHDDisc : Disc
{
	// tracks are padded to a multiple of this many frames
	static constexpr u32 CD_TRACK_PADDING = 4;
	// lead out, lead in and pregap between 2 sessions of MIL-CDs
	static constexpr u32 SESSION_GAP = 11400;

	chd_file *chd = nullptr;
	FILE *fp = nullptr;
	u8* hunk_mem = nullptr;
	u32 old_hunk = 0;

	u32 hunkbytes = 0;
	u32 sph = 0;

	void tryOpen(const char* file);

	~CHDDisc()
	{
		delete[] hunk_mem;

		if (chd)
			chd_close(chd);
		if (fp)
			std::fclose(fp);
	}
};

struct CHDTrack : TrackFile
{
	CHDDisc* disc;
	s32 Offset;
	u32 fmt;
	bool swap_bytes;

	CHDTrack(CHDDisc* disc, s32 Offset, u32 fmt, bool swap_bytes)
	{
		this->disc = disc;
		this->Offset = Offset;
		this->fmt = fmt;
		this->swap_bytes = swap_bytes;
	}

	bool Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type) override
	{
		u32 fad_offs = FAD + Offset;
		u32 hunk=(fad_offs)/disc->sph;
		if (disc->old_hunk!=hunk)
		{
			if (chd_read(disc->chd,hunk,disc->hunk_mem) != CHDERR_NONE)
				return false;
			disc->old_hunk = hunk;
		}

		u32 hunk_ofs = fad_offs%disc->sph;

		memcpy(dst, disc->hunk_mem + hunk_ofs * (2352+96), fmt);

		if (swap_bytes)
		{
			for (u32 i = 0; i < fmt; i += 2)
			{
				u8 b = dst[i];
				dst[i] = dst[i + 1];
				dst[i + 1] = b;
			}
		}
		switch (fmt)
		{
		case 2048:
			*sector_type = SECFMT_2048_MODE1;
			break;
		case 2336:
			*sector_type = SECFMT_2336_MODE2;
			break;
		case 2352:
		default:
			*sector_type = SECFMT_2352;
			break;
		}

		//While space is reserved for it, the images contain no actual subcodes
		//memcpy(subcode,disc->hunk_mem+hunk_ofs*(2352+96)+2352,96);
		*subcode_type = SUBFMT_NONE;

		return true;
	}
};

static u32 getSectorSize(const std::string& type)
{
	if (type == "AUDIO")
		return 2352;	// PCM Audio
	else if (type == "MODE1" || type == "MODE1/2048")
		return 2048;	// CDROM Mode1 Data (cooked)
	else if (type == "MODE1_RAW" || type == "MODE1/2352")
		return 2352;	// CDROM Mode1 Data (raw)
	else if (type == "MODE2" || type == "MODE2/2336")
		return 2336;	// CDROM XA Mode2 Data
	else if (type == "MODE2_RAW" || type == "MODE2/2352" || type == "CDI/2352")
		return 2352;	// CDROM XA Mode2 Data

	throw FlycastException("chd: track type " + type + " is not supported");
}

void CHDDisc::tryOpen(const char* file)
{
	fp = nowide::fopen(file, "rb");
	if (fp == nullptr)
	{
		WARN_LOG(COMMON, "Cannot open file '%s' errno %d", file, errno);
		throw FlycastException(std::string("Cannot open CHD file ") + file);
	}

	chd_error err = chd_open_file(fp, CHD_OPEN_READ, 0, &chd);

	if (err != CHDERR_NONE)
		throw FlycastException(std::string("Invalid CHD file ") + file);

	INFO_LOG(GDROM, "chd: parsing file %s", file);

	const chd_header* head = chd_get_header(chd);

	hunkbytes = head->hunkbytes;
	hunk_mem = new u8[hunkbytes];
	old_hunk=0xFFFFFFF;

	sph = hunkbytes/(2352+96);

	if (hunkbytes % (2352 + 96) != 0)
		throw FlycastException(std::string("Invalid hunkbytes for CHD file ") + file);

	u32 tag;
	u8 flags;
	char temp[512];
	u32 temp_len;
	u32 total_frames = 150;

	u32 Offset = 0;
	bool isGdrom = head->version < 5;	// MIL-CDs only supported starting with CHD v5
	bool needAudioSwap = false;

	for(;;)
	{
		char type[16], subtype[16], pgtype[16], pgsub[16];
		int tkid=-1, frames=0, pregap=0, postgap=0, padframes=0;

		err = chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, (u32)tracks.size(), temp, sizeof(temp), &temp_len, &tag, &flags);
		if (err == CHDERR_NONE)
		{
			//"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d PREGAP:%d PGTYPE:%s PGSUB:%s POSTGAP:%d"
			sscanf(temp, CDROM_TRACK_METADATA2_FORMAT, &tkid, type, subtype, &frames, &pregap, pgtype, pgsub, &postgap);
		}
		else if (CHDERR_NONE== (err = chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, (u32)tracks.size(), temp, sizeof(temp), &temp_len, &tag, &flags)) )
		{
			//CDROM_TRACK_METADATA_FORMAT	"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d"
			sscanf(temp, CDROM_TRACK_METADATA_FORMAT, &tkid, type, subtype, &frames);
		}
		else
		{
			err = chd_get_metadata(chd, GDROM_OLD_METADATA_TAG, (u32)tracks.size(), temp, sizeof(temp), &temp_len, &tag, &flags);
			if (err != CHDERR_NONE)
			{
				err = chd_get_metadata(chd, GDROM_TRACK_METADATA_TAG, (u32)tracks.size(), temp, sizeof(temp), &temp_len, &tag, &flags);
				if (err == CHDERR_NONE)
					needAudioSwap = true;
			}

			if (err != CHDERR_NONE)
				break;
			//GDROM_TRACK_METADATA_FORMAT	"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d PAD:%d PREGAP:%d PGTYPE:%s PGSUB:%s POSTGAP:%d"
			sscanf(temp, GDROM_TRACK_METADATA_FORMAT, &tkid, type, subtype, &frames, &padframes, &pregap, pgtype, pgsub, &postgap);
			isGdrom = true;
		}

		if (tkid != (int)tracks.size() + 1)
			throw FlycastException("Unexpected track number");

		if (strcmp(subtype, "NONE") != 0 || pregap != 0 || postgap != 0)
			throw FlycastException("Unsupported subtype or pre/postgap");

		DEBUG_LOG(GDROM, "%s", temp);
		Track t;
		t.StartFAD = total_frames;
		total_frames += frames;
		t.EndFAD = total_frames - 1;
		t.CTRL = strcmp(type,"AUDIO") == 0 ? 0 : 4;

		u32 sectorSize = getSectorSize(type);
		t.file = new CHDTrack(this, Offset - t.StartFAD, sectorSize,
							  // audio tracks are byteswapped in recent CHDv5+
							  t.CTRL == 0 && needAudioSwap);

		// CHD files are padded, so we have to respect the offset
		int padded = (frames + CD_TRACK_PADDING - 1) / CD_TRACK_PADDING;
		Offset += padded * CD_TRACK_PADDING;

		tracks.push_back(t);
	}

	if (isGdrom && (total_frames != 549300 || tracks.size() < 3))
		WARN_LOG(GDROM, "WARNING: chd: Total GD-Rom frames is wrong: %u frames (549300 expected) in %zu tracks", total_frames, tracks.size());

	if (isGdrom)
		FillGDSession();
	else
	{
		type = CdRom_XA;

		Session ses;
		ses.FirstTrack = 1;
		ses.StartFAD = tracks[0].StartFAD;
		sessions.push_back(ses);
		DEBUG_LOG(GDROM, "session 1: FAD %d", ses.StartFAD);

		ses.FirstTrack = tracks.size();
		// session 1 lead-out: 01:30:00, session 2 lead-in: 01:00:00, pregap: 00:02:00
		tracks.back().StartFAD += SESSION_GAP;
		tracks.back().EndFAD += SESSION_GAP;
		((CHDTrack *)tracks.back().file)->Offset -= SESSION_GAP;
		ses.StartFAD = tracks.back().StartFAD;
		sessions.push_back(ses);
		DEBUG_LOG(GDROM, "session 2: track %d FAD %d", ses.FirstTrack, ses.StartFAD);

		EndFAD = LeadOut.StartFAD = total_frames + SESSION_GAP - 1;
}
}


Disc* chd_parse(const char* file, std::vector<u8> *digest)
{
	if (get_file_extension(file) != "chd")
		return nullptr;

	CHDDisc* rv = new CHDDisc();

	try {
		rv->tryOpen(file);
		if (digest != nullptr)
		{
			digest->resize(sizeof(chd_get_header(rv->chd)->sha1));
			memcpy(digest->data(), chd_get_header(rv->chd)->sha1, digest->size());
		}
		return rv;
	} catch (...) {
		delete rv;
		throw;
	}
}
