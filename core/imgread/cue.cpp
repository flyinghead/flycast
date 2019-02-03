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

#include <sstream>
#include "types.h"
#include "common.h"

extern string OS_dirname(string file);
extern string normalize_path_separator(string path);

static u32 getSectorSize(const string& type) {
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

Disc* cue_parse(const wchar* file)
{
	core_file* fsource = core_fopen(file);

	if (fsource == NULL)
		return NULL;

	Disc* disc = new Disc();

	size_t cue_len = core_fsize(fsource);

	char cue_data[64 * 1024] = { 0 };

	if (cue_len >= sizeof(cue_data))
	{
		printf("CUE parse error: CUE file too big\n");
		core_fclose(fsource);
		return NULL;
	}

	core_fread(fsource, cue_data, cue_len);
	core_fclose(fsource);

	istringstream cuesheet(cue_data);

	string basepath = OS_dirname(file);

	u32 current_fad = 150;
	string track_filename;
	u32 track_number = -1;
	string track_type;

	while (!cuesheet.eof())
	{
		string token;
		cuesheet >> token;

		if (token == "REM")
		{
			cuesheet >> token;
			if (token == "HIGH-DENSITY")
			{
				current_fad = 45000 + 150;
			}
			else if (token != "SINGLE-DENSITY")
				printf("CUE parse error: unrecognized REM token %s. Expected SINGLE-DENSITY or HIGH-DENSITY\n", token.c_str());
			cuesheet >> token;
			if (token != "AREA")
				printf("CUE parse error: unrecognized REM token %s. Expected AREA\n", token.c_str());
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
				printf("CUE parse error: unrecognized FILE token %s. Expected BINARY\n", token.c_str());
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
				t.ADDR = 0;
				t.StartFAD = current_fad;
				t.EndFAD = 0;
				string path = basepath + normalize_path_separator(track_filename);
				core_file* track_file = core_fopen(path.c_str());
				if (track_file == NULL)
				{
					printf("CUE file: cannot open track %d: %s\n", track_number, path.c_str());
					return NULL;
				}
				u32 sector_size = getSectorSize(track_type);
				if (sector_size == 0)
				{
					printf("CUE file: track %d has unknown sector type: %s\n", track_number, track_type.c_str());
					return NULL;
				}
				if (core_fsize(track_file) % sector_size != 0)
					printf("Warning: Size of track %s is not multiple of sector size %d\n", track_filename.c_str(), sector_size);
				current_fad = t.StartFAD + (u32)core_fsize(track_file) / sector_size;
				
				//printf("file[%lu] \"%s\": StartFAD:%d, sector_size:%d file_size:%d\n", disc->tracks.size(),
				//		track_filename.c_str(), t.StartFAD, sector_size, (u32)core_fsize(track_file));

				t.file = new RawTrackFile(track_file, 0, t.StartFAD, sector_size);
				disc->tracks.push_back(t);
				
				track_number = -1;
				track_type.clear();
				track_filename.clear();
			}
		}

	}

	disc->FillGDSession();

	return disc;
}
