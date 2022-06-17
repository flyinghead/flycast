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
#pragma once
#include <string>
#include <memory>
#include <vector>

#include "types.h"
#include "json.hpp"

using namespace nlohmann;

struct GameBoxart
{
	std::string fileName;
	std::string name;
	std::string uniqueId;
	u32 region = 0;
	std::string releaseDate;
	std::string overview;

	std::string gamePath;
	std::string screenshotPath;
	std::string fanartPath;
	std::string boxartPath;

	bool scraped = false;
	bool found = false;

	enum Region { JAPAN = 1, USA = 2, EUROPE = 4 };

	json to_json() const
	{
		json j = {
			{ "file_name", fileName },
			{ "name", name },
			{ "region", region },
			{ "release_date", releaseDate },
			{ "overview", overview },
			{ "game_path", gamePath },
			{ "screenshot_path", screenshotPath },
			{ "fanart_path", fanartPath },
			{ "boxart_path", boxartPath },
			{ "scraped", scraped },
			{ "found", found },
		};
		return j;
	}

	template<typename T>
	static void loadProperty(T& i, const json& j, const std::string& propName)
	{
		try {
			// asan error if missing contains(). json bug?
			if (j.contains(propName)) {
				i = j[propName].get<T>();
				return;
			}
		} catch (const json::exception& e) {
		}
		i = T();
	}

	GameBoxart() = default;

	GameBoxart(const json& j)
	{
		loadProperty(fileName, j, "file_name");
		loadProperty(name, j, "name");
		loadProperty(region, j, "region");
		loadProperty(releaseDate, j, "release_date");
		loadProperty(overview, j, "overview");
		loadProperty(gamePath, j, "game_path");
		loadProperty(screenshotPath, j, "screenshot_path");
		loadProperty(fanartPath, j, "fanart_path");
		loadProperty(boxartPath, j, "boxart_path");
		loadProperty(scraped, j, "scraped");
		loadProperty(found, j, "found");
	}
};

class Scraper
{
public:
	virtual bool initialize(const std::string& saveDirectory) {
		this->saveDirectory = saveDirectory;
		return true;
	}
	virtual void scrape(GameBoxart& item) = 0;
	virtual ~Scraper() = default;

protected:
	bool downloadImage(const std::string& url, const std::string& localName);
	std::string makeUniqueFilename(const std::string& url);

private:
	std::string saveDirectory;
};
