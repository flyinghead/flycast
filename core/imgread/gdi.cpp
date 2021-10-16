#include "common.h"
#include "stdclass.h"
#include <algorithm>
#include <sstream>

// On windows, transform / to \\

std::string normalize_path_separator(std::string path)
{
#ifdef _WIN32
	std::replace(path.begin(), path.end(), '/', '\\');
#endif

	return path;
}

// given file/name.ext or file\name.ext returns file/ or file\, depending on the platform
// given name.ext returns ./ or .\, depending on the platform
std::string OS_dirname(std::string file)
{
	file = normalize_path_separator(file);
	#ifdef _WIN32
		const char sep = '\\';
	#else
		const char sep = '/';
	#endif

	size_t last_slash = file.find_last_of(sep);

	if (last_slash == std::string::npos)
	{
		std::string local_dir = ".";
		local_dir += sep;
		return local_dir;
	}

	return file.substr(0, last_slash + 1);
}

#if 0 // TODO: Move this to some tests, make it platform agnostic
namespace {
	struct OS_dirname_Test {
		OS_dirname_Test() {
			verify(OS_dirname("local/path") == "local/");
			verify(OS_dirname("local/path/two") == "local/path/");
			verify(OS_dirname("local/path/three\\a.exe") == "local/path/");
			verify(OS_dirname("local.ext") == "./");
		}
	} test1;

	struct normalize_path_separator_Test {
		normalize_path_separator_Test() {
			verify(normalize_path_separator("local/path") == "local/path");
			verify(normalize_path_separator("local\\path") == "local/path");
			verify(normalize_path_separator("local\\path\\") == "local/path/");
			verify(normalize_path_separator("\\local\\path\\") == "/local/path/");
			verify(normalize_path_separator("loc\\al\\pa\\th") == "loc/al/pa/th");
		}
	} test2;
}
#endif

Disc* load_gdi(const char* file, std::vector<u8> *digest)
{
	FILE *t = nowide::fopen(file, "rb");
	if (t == nullptr)
		throw FlycastException(std::string("Cannot open GDI file ") + file);

	size_t gdi_len = flycast::fsize(t);

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

	std::string basepath = OS_dirname(file);

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
			std::string path = basepath + normalize_path_separator(track_filename);
			FILE *file = nowide::fopen(path.c_str(), "rb");
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
