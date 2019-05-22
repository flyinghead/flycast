#include "common.h"

#include "deps/chdr/chd.h"

/* tracks are padded to a multiple of this many frames */
const uint32_t CD_TRACK_PADDING = 4;

struct CHDDisc : Disc
{
	chd_file* chd;
	u8* hunk_mem;
	u32 old_hunk;

	u32 hunkbytes;
	u32 sph;

	CHDDisc()
	{
		chd=0;
		hunk_mem=0;
	}

	bool TryOpen(const wchar* file);

	~CHDDisc()
	{
		if (hunk_mem)
			delete [] hunk_mem;
		if (chd)
			chd_close(chd);
	}
};

struct CHDTrack : TrackFile
{
	CHDDisc* disc;
	u32 StartFAD;
	s32 Offset;
	u32 fmt;
	bool swap_bytes;

	CHDTrack(CHDDisc* disc, u32 StartFAD, s32 Offset, u32 fmt, bool swap_bytes)
	{
		this->disc=disc;
		this->StartFAD=StartFAD;
		this->Offset = Offset;
		this->fmt=fmt;
		this->swap_bytes = swap_bytes;
	}

	virtual void Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type)
	{
		u32 fad_offs = FAD + Offset;
		u32 hunk=(fad_offs)/disc->sph;
		if (disc->old_hunk!=hunk)
		{
			chd_read(disc->chd,hunk,disc->hunk_mem); //CHDERR_NONE
			disc->old_hunk = hunk;
		}

		u32 hunk_ofs = fad_offs%disc->sph;

		memcpy(dst, disc->hunk_mem + hunk_ofs * (2352+96), fmt);

		if (swap_bytes)
		{
			for (int i = 0; i < fmt; i += 2)
			{
				u8 b = dst[i];
				dst[i] = dst[i + 1];
				dst[i + 1] = b;
			}
		}
		*sector_type=fmt==2352?SECFMT_2352:SECFMT_2048_MODE1;

		//While space is reserved for it, the images contain no actual subcodes
		//memcpy(subcode,disc->hunk_mem+hunk_ofs*(2352+96)+2352,96);
		*subcode_type=SUBFMT_NONE;
	}
};

bool CHDDisc::TryOpen(const wchar* file)
{
	chd_error err=chd_open(file,CHD_OPEN_READ,0,&chd);

	if (err!=CHDERR_NONE)
	{
		printf("chd: chd_open failed for file %s: %d\n", file, err);
		return false;
	}

	printf("chd: parsing file %s\n",file);

	const chd_header* head = chd_get_header(chd);

	hunkbytes = head->hunkbytes;
	hunk_mem = new u8[hunkbytes];
	old_hunk=0xFFFFFFF;

	sph = hunkbytes/(2352+96);

	if (hunkbytes%(2352+96)!=0)
	{
		printf("chd: hunkbytes is invalid, %d\n",hunkbytes);
		return false;
	}

	u32 tag;
	u8 flags;
	char temp[512];
	u32 temp_len;
	u32 total_frames = 150;

	u32 Offset = 0;

	for(;;)
	{
		char type[16], subtype[16], pgtype[16], pgsub[16];
		int tkid=-1, frames=0, pregap=0, postgap=0, padframes=0;

		err = chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, tracks.size(), temp, sizeof(temp), &temp_len, &tag, &flags);
		if (err == CHDERR_NONE)
		{
			//"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d PREGAP:%d PGTYPE:%s PGSUB:%s POSTGAP:%d"
			sscanf(temp, CDROM_TRACK_METADATA2_FORMAT, &tkid, type, subtype, &frames, &pregap, pgtype, pgsub, &postgap);
		}
		else if (CHDERR_NONE== (err = chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, tracks.size(), temp, sizeof(temp), &temp_len, &tag, &flags)) )
		{
			//CDROM_TRACK_METADATA_FORMAT	"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d"
			sscanf(temp, CDROM_TRACK_METADATA_FORMAT, &tkid, type, subtype, &frames);
		}
		else
		{
			err = chd_get_metadata(chd, GDROM_OLD_METADATA_TAG, tracks.size(), temp, sizeof(temp), &temp_len, &tag, &flags);
			if (err != CHDERR_NONE)
			{
				err = chd_get_metadata(chd, GDROM_TRACK_METADATA_TAG, tracks.size(), temp, sizeof(temp), &temp_len, &tag, &flags);
			}
			if (err == CHDERR_NONE)
			{
				//GDROM_TRACK_METADATA_FORMAT	"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d PAD:%d PREGAP:%d PGTYPE:%s PGSUB:%s POSTGAP:%d"
				sscanf(temp, GDROM_TRACK_METADATA_FORMAT, &tkid, type, subtype, &frames, &padframes, &pregap, pgtype, pgsub, &postgap);
			}
			else
			{
				break;
			}
		}

		if (tkid!=(tracks.size()+1) || (strcmp(type,"MODE1_RAW")!=0 && strcmp(type,"AUDIO")!=0 && strcmp(type,"MODE1")!=0) || strcmp(subtype,"NONE")!=0 || pregap!=0 || postgap!=0)
		{
			printf("chd: track type %s is not supported\n",type);
			return false;
		}
		printf("%s\n",temp);
		Track t;
		t.StartFAD = total_frames;
		total_frames += frames;
		t.EndFAD = total_frames - 1;
		t.ADDR = 0;
		t.CTRL = strcmp(type,"AUDIO") == 0 ? 0 : 4;

		t.file = new CHDTrack(this, t.StartFAD, Offset - t.StartFAD, strcmp(type, "MODE1") ? 2352 : 2048,
							  // audio tracks are byteswapped in CHDv5+
							  t.CTRL == 0 && head->version >= 5);

		// CHD files are padded, so we have to respect the offset
		int padded = (frames + CD_TRACK_PADDING - 1) / CD_TRACK_PADDING;
		Offset += padded * CD_TRACK_PADDING;

		tracks.push_back(t);
	}

	if (total_frames!=549300 || tracks.size()<3)
	{
		printf("WARNING: chd: Total frames is wrong: %u frames in %u tracks\n",total_frames,tracks.size());
#ifndef NOT_REICAST
		msgboxf("This is an improper dump!",MBX_ICONEXCLAMATION);
#endif
		return false;
	}

	FillGDSession();

	return true;
}


Disc* chd_parse(const wchar* file)
{
	CHDDisc* rv = new CHDDisc();

	if (rv->TryOpen(file))
		return rv;
	else
	{
		delete rv;
		return 0;
	}
}
