#include "common.h"
#include "stdclass.h"
#include "oslib/storage.h"
#include "oslib/i18n.h"
#include <sstream>

static Disc* load_gdi(const char* file, std::vector<u8> *digest)
{
	FILE *t = hostfs::storage().openFile(file, "rb");
	if (t == nullptr)
	{
		WARN_LOG(COMMON, "Cannot open file '%s' errno %d", file, errno);
		throw FlycastException(strprintf(i18n::T("Cannot open GDI file %s"), file));
	}

	hostfs::FileInfo fileInfo = hostfs::storage().getFileInfo(file);
	size_t gdi_len = fileInfo.size;

	char gdi_data[16384] {};

	if (gdi_len >= sizeof(gdi_data))
	{
		std::fclose(t);
		throw FlycastException(i18n::Ts("GDI file too big"));
	}

	if (std::fread(gdi_data, 1, gdi_len, t) != gdi_len)
		WARN_LOG(GDROM, "Failed or truncated read of gdi file '%s'", file);
	std::fclose(t);

	std::istringstream gdi(gdi_data);
	gdi.imbue(std::locale::classic());

	u32 trackCount = 0;
	gdi >> trackCount;
	if (trackCount < 3 || trackCount > 99) {
		WARN_LOG(GDROM, "Invalid track count: %d", trackCount);
		throw FlycastException(i18n::Ts("Invalid GDI file"));
	}

	INFO_LOG(GDROM, "GDI: %d tracks", trackCount);

	std::string basepath = hostfs::storage().getParentPath(file);

	MD5Sum md5;

	std::unique_ptr<Disc> disc = std::make_unique<Disc>();
	for (u32 i = 0; i < trackCount; i++)
	{
		std::string track_filename;

		u32 TRACK = 0, FADS = 0, CTRL = 0, SSIZE = 0;
		s32 OFFSET = 0;
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

		if (TRACK < 1 || TRACK > trackCount) {
			WARN_LOG(GDROM, "Track number out of bounds: %d", TRACK);
			throw FlycastException(i18n::Ts("Invalid GDI file"));
		}
		if (CTRL != 0 && CTRL != 4) {
			WARN_LOG(GDROM, "Unsupported track type: %d", CTRL);
			throw FlycastException(i18n::Ts("Invalid GDI file"));
		}
		switch (TRACK)
		{
		case 1:
			if (CTRL != 4) {
				WARN_LOG(GDROM, "Track 1 must be a data track");
				throw FlycastException(i18n::Ts("Invalid GDI file"));
			}
			break;
		case 2:
			if (CTRL != 0) {
				WARN_LOG(GDROM, "Track 2 must be an audio track");
				throw FlycastException(i18n::Ts("Invalid GDI file"));
			}
			break;
		case 3:
			if (CTRL != 4) {
				WARN_LOG(GDROM, "Track 3 must be a data track");
				throw FlycastException(i18n::Ts("Invalid GDI file"));
			}
			if (FADS != 45000) {
				WARN_LOG(GDROM, "Track 3 should start at 45000, not %d", FADS);
				throw FlycastException(i18n::Ts("Invalid GDI file"));
			}
			break;
		default:
			break;
		}
		if (SSIZE != 2352 && SSIZE != 2048) {
			WARN_LOG(GDROM, "Unsupported sector size: %d", SSIZE);
			throw FlycastException(i18n::Ts("Invalid GDI file"));
		}

		DEBUG_LOG(GDROM, "file[%d] \"%s\": FAD:%d, CTRL:%d, SSIZE:%d, OFFSET:%d", TRACK, track_filename.c_str(), FADS, CTRL, SSIZE, OFFSET);

		Track t;
		t.StartFAD = FADS + 150;
		t.CTRL = CTRL;

		std::string path = hostfs::storage().getSubPath(basepath, track_filename);
		FILE *file = hostfs::storage().openFile(path, "rb");
		if (file == nullptr)
			throw FlycastException(strprintf(i18n::T("GDI file: Cannot open track %s"), path.c_str()));
		if (digest != nullptr)
			md5.add(file);
		t.file = new RawTrackFile(file, OFFSET, t.StartFAD, SSIZE);
		hostfs::FileInfo fileInfo = hostfs::storage().getFileInfo(path);
		if ((fileInfo.size - OFFSET) % SSIZE != 0)
			WARN_LOG(GDROM, "Warning: Size of track %s is not multiple of sector size %d", track_filename.c_str(), SSIZE);
		t.EndFAD = t.StartFAD + (u32)(fileInfo.size - OFFSET) / SSIZE - 1;
		disc->tracks.push_back(t);
	}

	if (disc->tracks.size() != trackCount) {
		WARN_LOG(GDROM, "Track count %d but found %d tracks", trackCount, (int)disc->tracks.size());
		throw FlycastException(i18n::Ts("Invalid GDI file"));
	}
	disc->FillGDSession();
	if (digest != nullptr)
		*digest = md5.getDigest();

	return disc.release();
}


Disc* gdi_parse(const char* file, std::vector<u8> *digest)
{
	if (get_file_extension(file) != "gdi")
		return nullptr;

	return load_gdi(file, digest);
}
