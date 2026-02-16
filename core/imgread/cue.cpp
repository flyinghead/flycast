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
#include "oslib/i18n.h"
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
		throw FlycastException(strprintf(i18n::T("Cannot open CUE file %s"), file));
	}

	hostfs::FileInfo fileInfo = hostfs::storage().getFileInfo(file);
	size_t cue_len = fileInfo.size;

	char cue_data[64_KB] = { 0 };

	if (cue_len >= sizeof(cue_data))
	{
		std::fclose(fsource);
		throw FlycastException(i18n::Ts("CUE parse error: CUE file too big"));
	}

	if (std::fread(cue_data, 1, cue_len, fsource) != cue_len)
		WARN_LOG(GDROM, "Failed or truncated read of cue file '%s'", file);
	std::fclose(fsource);

	std::istringstream istream(cue_data);
	istream.imbue(std::locale::classic());

	std::string basepath = hostfs::storage().getParentPath(file);

	MD5Sum md5;

	std::unique_ptr<Disc> disc = std::make_unique<Disc>();
	u32 currentFAD = 150;
	// SESSION context
	u32 session_number = 0;
	// FILE context
	std::string track_filename;
	u32 fileStartFAD = 0;
	// TRACK context
	u32 track_number = -1;
	std::string track_type;
	u32 track_secsize = 0;
	std::string track_isrc;

	std::string line;
	while (std::getline(istream, line))
	{
		std::istringstream cuesheet(line);
		cuesheet.imbue(std::locale::classic());
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
						currentFAD += 11400;

					Session ses;
					ses.FirstTrack = (u8)disc->tracks.size() + 1;
					ses.StartFAD = currentFAD;
					disc->sessions.push_back(ses);
					DEBUG_LOG(GDROM, "session[%zd]: 1st track: %d FAD:%d", disc->sessions.size(), ses.FirstTrack, ses.StartFAD);
				}
			}
			else
			{
				// GD-Rom
				if (token == "HIGH-DENSITY") {
					currentFAD = 45000 + 150;
					disc->type = GdRom;
				}
				else if (token != "SINGLE-DENSITY")
				{
					INFO_LOG(GDROM, "CUE parse: unrecognized REM token %s. Expected SINGLE-DENSITY, HIGH-DENSITY or SESSION", token.c_str());
					continue;
				}
				cuesheet >> token;
				if (token != "AREA")
					WARN_LOG(GDROM, "CUE parse error: unrecognized REM token %s. Assuming AREA", token.c_str());
			}
			// Clear file context
			track_filename.clear();
			fileStartFAD = 0;
			// Clear track context
			track_number = -1;
			track_type.clear();
			track_isrc.clear();
			track_secsize = 0;
		}
		else if (token == "FILE")
		{
			track_filename.clear();
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
			if (token != "BINARY") {
				WARN_LOG(GDROM, "CUE parse error: unsupported FILE format %s. Expected BINARY", token.c_str());
				throw FlycastException(i18n::T("Invalid CUE file"));
			}
			track_filename = hostfs::storage().getSubPath(basepath, track_filename);
			FILE *track_file = hostfs::storage().openFile(track_filename, "rb");
			if (track_file == nullptr)
				throw FlycastException(strprintf(i18n::T("CUE file: cannot open track %s"), track_filename.c_str()));
			if (digest != nullptr)
				md5.add(track_file);
			std::fclose(track_file);
			fileInfo = hostfs::storage().getFileInfo(track_filename);
			fileStartFAD = currentFAD;
			// Clear track context
			track_number = -1;
			track_type.clear();
			track_isrc.clear();
			track_secsize = 0;
		}
		else if (token == "TRACK")
		{
			cuesheet >> track_number;
			cuesheet >> track_type;
			if (track_filename.empty()) {
				WARN_LOG(GDROM, "CUE parse error: TRACK %02d not in a FILE context", track_number);
				throw FlycastException(i18n::T("Invalid CUE file"));
			}
			if (track_number <= 0 || track_number > 99) {
				WARN_LOG(GDROM, "CUE parse error: Invalid track number %d", track_number);
				throw FlycastException(i18n::T("Invalid CUE file"));
			}
			bool firstTrackOfFile = track_secsize == 0;
			track_secsize = getSectorSize(track_type);
			if (track_secsize == 0)
				throw FlycastException(strprintf(i18n::T("CUE file: track has unknown sector type: %s"), track_type.c_str()));
			// Can happen if multiple tracks with different sector sizes share the same file
			if (fileInfo.size % track_secsize != 0)
				WARN_LOG(GDROM, "Warning: Size of track %s is not multiple of sector size %d", track_filename.c_str(), track_secsize);
			// FIXME This is wrong if tracks of different sector size share the same file
			if (firstTrackOfFile)
				currentFAD += fileInfo.size / track_secsize;
		}
		else if (token == "INDEX")
		{
			u32 index_num;
			cuesheet >> index_num;
			if (index_num == 1)
			{
				cuesheet >> token;
				int indexFAD = 0;
				int min = 0, sec = 0, frame = 0;
				if (sscanf(token.c_str(), "%d:%d:%d", &min, &sec, &frame) == 3)
					indexFAD = frame + 75 * (sec + 60 * min);
				Track t;
				t.StartFAD = fileStartFAD + indexFAD;
				t.CTRL = (track_type == "AUDIO" || track_type == "CDG") ? 0 : 4;
				t.EndFAD = currentFAD - 1;
				t.isrc = track_isrc;
				DEBUG_LOG(GDROM, "file[%zd] \"%s\": session %d type %s FAD:%d -> %d %s", disc->tracks.size() + 1, track_filename.c_str(),
						session_number, track_type.c_str(), t.StartFAD, t.EndFAD, t.isrc.empty() ? "" : ("ISRC " + t.isrc).c_str());
				FILE *track_file = hostfs::storage().openFile(track_filename, "rb");
				t.file = new RawTrackFile(track_file, indexFAD * track_secsize, t.StartFAD, track_secsize);
				disc->tracks.push_back(t);
				if (disc->tracks.size() >= 2) {
					Track& prevTrack = disc->tracks[disc->tracks.size() - 2];
					prevTrack.EndFAD = std::min(prevTrack.EndFAD, t.StartFAD - 1);
				}
			}
			else if (index_num != 0) {
				WARN_LOG(GDROM, "INDEX %02d not supported", index_num);
				throw FlycastException(i18n::T("Invalid CUE file"));
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
		throw FlycastException(i18n::Ts("CUE parse error: failed to parse or invalid file with 0 tracks"));
	if (disc->tracks.size() > 99) {
		WARN_LOG(GDROM, "CUE: more than 99 tracks");
		throw FlycastException(i18n::Ts("Invalid CUE file"));
	}

	if (session_number == 0)
	{
		if (disc->type == GdRom)
		{
			// GD-Rom
			if (disc->tracks.size() < 3)
				throw FlycastException(i18n::Ts("CUE parse error: less than 3 tracks"));
			if (!disc->tracks[0].isDataTrack()) {
				WARN_LOG(GDROM, "CUE: track 1 must be a data track");
				throw FlycastException(i18n::Ts("Invalid CUE file"));
			}
			if (disc->tracks[1].isDataTrack()) {
				WARN_LOG(GDROM, "CUE: track 2 must be an audio track");
				throw FlycastException(i18n::Ts("Invalid CUE file"));
			}
			if (!disc->tracks[2].isDataTrack()) {
				WARN_LOG(GDROM, "CUE: track 3 must be a data track");
				throw FlycastException(i18n::Ts("Invalid CUE file"));
			}
			if (disc->tracks[2].StartFAD != 45150) {
				WARN_LOG(GDROM, "CUE: track 3 must start at FAD 45000+150, not %d", disc->tracks[2].StartFAD);
				throw FlycastException(i18n::Ts("Invalid CUE file"));
			}
			disc->FillGDSession();
		}
		else
		{
			// CD-Rom
			Session ses;
			ses.FirstTrack = 1;
			ses.StartFAD = disc->tracks[0].StartFAD;
			disc->sessions.push_back(ses);
		}
	}
	else {
		// CD-Rom XA
		disc->type = CdRom_XA;
	}
	if (disc->type != GdRom)
	{
		disc->LeadOut.ADR = 1;
		disc->LeadOut.CTRL = 4;
		disc->EndFAD = disc->LeadOut.StartFAD = currentFAD;
	}

	if (digest != nullptr)
		*digest = md5.getDigest();

	return disc.release();
}
