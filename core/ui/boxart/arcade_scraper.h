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
#pragma once
#include "scraper.h"
#include "oslib/http_client.h"

class ArcadeScraper : public Scraper
{
public:
	bool initialize(const std::string& saveDirectory) override;
	void scrape(GameBoxart& item) override;
};

bool ArcadeScraper::initialize(const std::string& saveDirectory)
{
	if (!Scraper::initialize(saveDirectory))
		return false;

	http::init();

	return true;
}

void ArcadeScraper::scrape(GameBoxart& item)
{
	if (item.fileName.empty() || !item.arcade)
		// invalid rom or not an arcade game
		return;
	std::string filename = makeUniqueFilename("dummy.jpg");
	std::string extension = get_file_extension(item.fileName);
	if (extension == "zip" || extension == "7z")
	{
		std::string url = "https://flyinghead.github.io/flycast-content/arcade/jpg/" + get_file_basename(item.fileName) + ".jpg";
		if (downloadImage(url, filename, true))
		{
			item.setBoxartPath(filename);
			item.boxartUrl = url;
			item.scraped = true;
			return;
		}
	}
	if (!item.uniqueId.empty())
	{
		bool valid = true;
		for (char c : item.uniqueId)
			if (c < ' ' || c > '~') {
				valid = false;
				break;
			}
		if (valid)
		{
			std::string url = "https://flyinghead.github.io/flycast-content/arcade/jpg/" + http::urlEncode(item.uniqueId) + ".jpg";
			if (downloadImage(url, filename, true))
			{
				item.setBoxartPath(filename);
				item.boxartUrl = url;
				item.scraped = true;
			}
		}
	}
}
