/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "stdclass.h"
#include "oslib/storage.h"
#include <sstream>

static u32 getSectorSize(const std::string& type) {
		if (type == "AUDIO")
			return 2352;	// PCM Audio
		else if (type == "CDG")
			return 2352;	// Karaoke cd+g
		else if (type == "MODE1/2048")
			return 2048;	// CDROM Mode1 Data (cooked)
		else if (type == "MODE1/2352")
			return 2352;	// CDROM Mode1 Data (raw)
		else if (type == "MODE2/2336")
			return 2336;	// CDROM XA Mode2 Data
		else if (type == "MODE2/2352")
			return 2352;	// CDROM XA Mode2 Data
		else if (type == "CDI/2336")
			return 2336;	// CDI Mode2 Data
		else if (type == "CDI/2352")
			return 2352;	// CDI Mode2 Data
		else
			return 0;
}

Disc* cue_parse(const char* file, std::vector<u8> *digest)
{
	if (get_file_extension(file) != "cue")
		return nullptr;

	FILE *fsource = hostfs::storage().openFile(file, "rb");

	if (fsource == nullptr)
	{
		WARN_LOG(COMMON, "Cannot open file '%s' errno %d", file, errno);
		throw FlycastException(std::string("Cannot open CUE file ") + file);
	}

	hostfs::FileInfo fileInfo = hostfs::storage().getFileInfo(file);
	size_t cue_len = fileInfo.size;

	char cue_data[64 * 1024] = { 0 };

	if (cue_len >= sizeof(cue_data))
	{
		std::fclose(fsource);
		throw FlycastException("CUE parse error: CUE file too big");
	}

	if (std::fread(cue_data, 1, cue_len, fsource) != cue_len)
		WARN_LOG(GDROM, "Failed or truncated read of cue file '%s'", file);
	std::fclose(fsource);

	std::istringstream istream(cue_data);

	std::string basepath = hostfs::storage().getParentPath(file);

	MD5Sum md5;

	Disc* disc = new Disc();
	u32 current_fad = 150;
	std::string track_filename;
	u32 track_number = -1;
	std::string track_type;
	u32 session_number = 0;
	std::string line;
	std::string track_isrc;

	while (std::getline(istream, line))
	{
		std::stringstream cuesheet(line);
		std::string token;
		cuesheet >> token;

		if (token == "REM")
		{
			cuesheet >> token;
			if (token == "SESSION")
			{
				// Multi-session
				u32 cur_session;
				cuesheet >> cur_session;
				if (cur_session == 0)
					WARN_LOG(GDROM, "CUE parse error: invalid session number %s", token.c_str());
				else if (cur_session != session_number)
				{
					session_number = cur_session;
					if (session_number == 2)
						// session 1 lead-out: 01:30:00, session 2 lead-in: 01:00:00, pregap: 00:02:00
						current_fad += 11400;

					Session ses;
					ses.FirstTrack = (u8)disc->tracks.size() + 1;
					ses.StartFAD = current_fad;
					disc->sessions.push_back(ses);
					DEBUG_LOG(GDROM, "session[%zd]: 1st track: %d FAD:%d", disc->sessions.size(), ses.FirstTrack, ses.StartFAD);
				}
			}
			else
			{
				// GD-Rom
				if (token == "HIGH-DENSITY")
					current_fad = 45000 + 150;
				else if (token != "SINGLE-DENSITY")
				{
					INFO_LOG(GDROM, "CUE parse: unrecognized REM token %s. Expected SINGLE-DENSITY, HIGH-DENSITY or SESSION", token.c_str());
					continue;
				}
				cuesheet >> token;
				if (token != "AREA")
					WARN_LOG(GDROM, "CUE parse error: unrecognized REM token %s. Expected AREA", token.c_str());
			}
		}
		else if (token == "FILE")
		{
			char last;

			do {
				cuesheet >> last;
			} while (isspace(last));

			if (last == '"')
			{
				cuesheet >> std::noskipws;
				for (;;) {
					cuesheet >> last;
					if (last == '"')
						break;
					track_filename += last;
				}
				cuesheet >> std::skipws;
			}
			else
			{
				cuesheet >> track_filename;
				track_filename = last + track_filename;
			}
			cuesheet >> token;	// BINARY
			if (token != "BINARY")
				WARN_LOG(GDROM, "CUE parse error: unrecognized FILE token %s. Expected BINARY", token.c_str());
		}
		else if (token == "TRACK")
		{
			cuesheet >> track_number;
			cuesheet >> track_type;
		}
		else if (token == "INDEX")
		{
			u32 index_num;
			cuesheet >> index_num;
			if (index_num == 1)
			{
				Track t;
				t.StartFAD = current_fad;
				t.CTRL = (track_type == "AUDIO" || track_type == "CDG") ? 0 : 4;
				std::string path = hostfs::storage().getSubPath(basepath, track_filename);
				FILE *track_file = hostfs::storage().openFile(path, "rb");
				if (track_file == nullptr)
				{
					delete disc;
					throw FlycastException("CUE file: cannot open track " + path);
				}
				u32 sector_size = getSectorSize(track_type);
				if (sector_size == 0)
				{
					std::fclose(track_file);
					delete disc;
					throw FlycastException("CUE file: track has unknown sector type: " + track_type);
				}
				fileInfo = hostfs::storage().getFileInfo(path);
				if (fileInfo.size % sector_size != 0)
					WARN_LOG(GDROM, "Warning: Size of track %s is not multiple of sector size %d", track_filename.c_str(), sector_size);
				current_fad = t.StartFAD + (u32)fileInfo.size / sector_size;
				t.EndFAD = current_fad - 1;
				t.isrc = track_isrc;
				DEBUG_LOG(GDROM, "file[%zd] \"%s\": session %d type %s FAD:%d -> %d %s", disc->tracks.size() + 1, track_filename.c_str(),
						session_number, track_type.c_str(), t.StartFAD, t.EndFAD, t.isrc.empty() ? "" : ("ISRC " + t.isrc).c_str());
				if (digest != nullptr)
					md5.add(track_file);
				t.file = new RawTrackFile(track_file, 0, t.StartFAD, sector_size);
				disc->tracks.push_back(t);
				
				track_number = -1;
				track_type.clear();
				track_filename.clear();
				track_isrc.clear();
			}
		}
		else if (token == "CATALOG")
		{
			cuesheet >> disc->catalog;
		}
		else if (token == "ISRC")
		{
			cuesheet >> track_isrc;
		}
	}
	if (disc->tracks.empty())
	{
		delete disc;
		throw FlycastException("CUE parse error: failed to parse or invalid file with 0 tracks");
	}

	if (session_number == 0)
	{
		if (disc->tracks.size() < 3) {
			delete disc;
			throw FlycastException("CUE parse error: less than 3 tracks");
		}
		disc->FillGDSession();
	}
	else
	{
		disc->type = CdRom_XA;
		disc->LeadOut.ADR = 1;
		disc->LeadOut.CTRL = 4;
		disc->EndFAD = disc->LeadOut.StartFAD = current_fad;
	}

	// Get rid of the pregap for audio tracks
	for (Track& t : disc->tracks)
		if (!t.isDataTrack())
			t.StartFAD += 150;
	if (digest != nullptr)
		*digest = md5.getDigest();

	return disc;
}
