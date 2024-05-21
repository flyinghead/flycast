/*
	Copyright 2024 flyinghead

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
#include "resources.h"
#include "archive/ZipArchive.h"
#include <cstring>
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(flycast);

namespace resource
{

std::unique_ptr<u8[]> load(const std::string& path, size_t& size)
{
	try {
		cmrc::embedded_filesystem fs = cmrc::flycast::get_filesystem();
		std::string zipName = path + ".zip";
		if (fs.exists(zipName))
		{
			cmrc::file zipFile = fs.open(zipName);
			ZipArchive zip;
			if (zip.Open(zipFile.cbegin(), zipFile.size()))
			{
				std::unique_ptr<ArchiveFile> file;
				file.reset(zip.OpenFirstFile());
				if (file != nullptr)
				{
					size = file->length();
					std::unique_ptr<u8[]> buffer = std::make_unique<u8[]>(size);
					size = file->Read(buffer.get(), size);

					return buffer;
				}
			}
		}
		else
		{
			cmrc::file file = fs.open(path);
			size = file.size();
			std::unique_ptr<u8[]> buffer = std::make_unique<u8[]>(size);
			memcpy(buffer.get(), file.begin(), size);

			return buffer;
		}
	} catch (const std::system_error& e) {
	}
	INFO_LOG(COMMON, "Resource not found: %s", path.c_str());
	size = 0;
	return nullptr;
}

}

