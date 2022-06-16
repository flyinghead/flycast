/*
	Copyright 2022 flyinghead

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
#include "scraper.h"
#include "http_client.h"
#include "stdclass.h"

bool Scraper::downloadImage(const std::string& url, const std::string& localName)
{
	DEBUG_LOG(COMMON, "downloading %s", url.c_str());
	std::vector<u8> content;
	std::string contentType;
	if (!http::success(http::get(url, content, contentType)))
	{
		WARN_LOG(COMMON, "download_image http error: %s", url.c_str());
		return false;
	}
	if (contentType.substr(0, 6) != "image/")
	{
		WARN_LOG(COMMON, "download_image bad content type %s", contentType.c_str());
		return false;
	}
	FILE *f = nowide::fopen(localName.c_str(), "wb");
	if (f == nullptr)
	{
		WARN_LOG(COMMON, "can't create local file %s: error %d", localName.c_str(), errno);
		return false;
	}
	fwrite(&content[0], 1, content.size(), f);
	fclose(f);

	return true;
}

std::string Scraper::makeUniqueFilename(const std::string& url)
{
	std::string extension = get_file_extension(url);
	std::string path;
	do {
		path = saveDirectory + "/" + std::to_string(rand()) + "." + extension;
	} while (file_exists(path));
	return path;
}
