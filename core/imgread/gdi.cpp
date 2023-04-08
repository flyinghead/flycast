#include "common.h"
#include "stdclass.h"
#include "oslib/storage.h"
#include <sstream>

Disc* load_gdi(const char* file, std::vector<u8> *digest)
{
	FILE *t = hostfs::storage().openFile(file, "rb");
	if (t == nullptr)
	{
		WARN_LOG(COMMON, "Cannot open file '%s' errno %d", file, errno);
		throw FlycastException(std::string("Cannot open GDI file ") + file);
	}

	hostfs::FileInfo fileInfo = hostfs::storage().getFileInfo(file);
	size_t gdi_len = fileInfo.size;

	char gdi_data[8193] = { 0 };

	if (gdi_len >= sizeof(gdi_data))
	{
		std::fclose(t);
		throw FlycastException("GDI file too big");
	}

	if (std::fread(gdi_data, 1, gdi_len, t) != gdi_len)
		WARN_LOG(GDROM, "Failed or truncated read of gdi file '%s'", file);
	std::fclose(t);

	std::istringstream gdi(gdi_data);

	u32 iso_tc = 0;
	gdi >> iso_tc;
	if (iso_tc == 0)
		throw FlycastException("GDI: empty or invalid GDI file");

	INFO_LOG(GDROM, "GDI : %d tracks", iso_tc);

	std::string basepath = hostfs::storage().getParentPath(file);

	MD5Sum md5;

	Disc* disc = new Disc();
	u32 TRACK=0,FADS=0,CTRL=0,SSIZE=0;
	s32 OFFSET=0;
	for (u32 i=0;i<iso_tc;i++)
	{
		std::string track_filename;

		//TRACK FADS CTRL SSIZE file OFFSET
		gdi >> TRACK;
		gdi >> FADS;
		gdi >> CTRL;
		gdi >> SSIZE;

		char last;

		do {
			gdi >> last;
		} while (isspace(last));
		
		if (last == '"')
		{
			gdi >> std::noskipws;
			for(;;) {
				gdi >> last;
				if (last == '"')
					break;
				track_filename += last;
			}
			gdi >> std::skipws;
		}
		else
		{
			gdi >> track_filename;
			track_filename = last + track_filename;
		}

		gdi >> OFFSET;
		
		DEBUG_LOG(GDROM, "file[%d] \"%s\": FAD:%d, CTRL:%d, SSIZE:%d, OFFSET:%d", TRACK, track_filename.c_str(), FADS, CTRL, SSIZE, OFFSET);

		Track t;
		t.StartFAD = FADS + 150;
		t.CTRL = CTRL;

		if (SSIZE!=0)
		{
			std::string path = hostfs::storage().getSubPath(basepath, track_filename);
			FILE *file = hostfs::storage().openFile(path, "rb");
			if (file == nullptr)
			{
				delete disc;
				throw FlycastException("GDI file: Cannot open track " + path);
			}
			if (digest != nullptr)
				md5.add(file);
			t.file = new RawTrackFile(file, OFFSET, t.StartFAD, SSIZE);
		}
		if (!disc->tracks.empty())
			disc->tracks.back().EndFAD = t.StartFAD - 1;
		disc->tracks.push_back(t);
	}

	if (disc->tracks.size() < 3) {
		delete disc;
		throw FlycastException("GDI parse error: less than 3 tracks");
	}
	disc->FillGDSession();
	if (digest != nullptr)
		*digest = md5.getDigest();

	return disc;
}


Disc* gdi_parse(const char* file, std::vector<u8> *digest)
{
	if (get_file_extension(file) != "gdi")
		return nullptr;

	return load_gdi(file, digest);
}
