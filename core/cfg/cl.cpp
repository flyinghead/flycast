/*
	Copyright 2025 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "cfg/cfg.h"
#include "stdclass.h"
#include <cstdio>
#include <cstring>

namespace config
{

static void usage(const char *exe)
{
	fprintf(stderr, "Usage: %s [option]... [<rom path>]\n", exe);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-config section:key=value,...  set a transient config value.\n");
	fprintf(stderr, "                               Transient config values won't be saved to emu.cfg.\n");
	fprintf(stderr, "-help                          display this help\n");
}

static void parseConfigOption(const std::string& str)
{
	size_t pos = 0;
	while (pos < str.length())
	{
		size_t comma = str.find(',', pos);
		if (comma == str.npos)
			comma = str.length();
		std::string option = str.substr(pos, comma - pos);
		size_t colpos = option.find(':');
		size_t eqpos = option.npos;
		if (colpos != option.npos)
		{
			std::string section = option.substr(0, colpos);
			eqpos = option.find('=', colpos);
			if (eqpos != option.npos)
			{
				std::string key = option.substr(colpos + 1, eqpos - (colpos + 1));
				std::string value = option.substr(eqpos + 1);
				setTransient(section, key, value);
				DEBUG_LOG(COMMON, "-config [%s] %s = %s", section.c_str(), key.c_str(), value.c_str());
			}
		}
		if (colpos == option.npos || eqpos == option.npos) {
			WARN_LOG(COMMON, "Invalid -config option '%s'. Format is: -config section:key=value,...", str.c_str());
			return;
		}
		pos = comma + 1;
	}
}

void parseCommandLine(int argc, const char * const argv[])
{
	settings.content.path.clear();
	const char *exe = argv[0];
	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "--help")) {
			usage(exe);
			exit(0);
		}
		if (!strcmp(argv[i], "-config") || !strcmp(argv[i], "--config"))
		{
			if (i < argc - 1)
				parseConfigOption(argv[++i]);
			continue;
		}
		// macOS
		if (!strncmp(argv[i], "-NSDocumentRevisions", 20)) {
			i++;
			continue;
		}
		if (argv[i][0] == '-') {
			WARN_LOG(COMMON, "Ignoring unknown command line option '%s'", argv[i]);
			continue;
		}
		std::string extension = get_file_extension(argv[i]);
		if (extension == "cdi" || extension == "chd"
				|| extension == "gdi"|| extension == "cue")
		{
			INFO_LOG(COMMON, "Using '%s' as CD image", argv[i]);
			settings.content.path = argv[i];
		}
		else if (extension == "elf")
		{
			INFO_LOG(COMMON, "Using '%s' as reios elf file", argv[i]);
			setTransient("config", "bios.UseReios", "yes");
			settings.content.path = argv[i];
		}
		else {
			INFO_LOG(COMMON, "Using '%s' as rom", argv[i]);
			settings.content.path = argv[i];
		}
		if (i < argc - 1)
			WARN_LOG(COMMON, "Rest of command line ignored: '%s'...", argv[i + 1]);
		break;
	}
}

}	// namespace config
