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
	std::string searchName;
	u32 region = 0;
	std::string releaseDate;
	std::string overview;

	std::string gamePath;
	std::string boxartPath;

	bool parsed = false;
	bool scraped = false;
	bool busy = false;

	enum Region { JAPAN = 1, USA = 2, EUROPE = 4 };

	json to_json() const
	{
		json j = {
			{ "file_name", fileName },
			{ "name", name },
			{ "unique_id", uniqueId },
			{ "search_name", searchName },
			{ "region", region },
			{ "release_date", releaseDate },
			{ "overview", overview },
			{ "boxart_path", boxartPath },
			{ "parsed", parsed },
			{ "scraped", scraped },
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
		loadProperty(uniqueId, j, "unique_id");
		loadProperty(searchName, j, "search_name");
		loadProperty(region, j, "region");
		loadProperty(releaseDate, j, "release_date");
		loadProperty(overview, j, "overview");
		loadProperty(boxartPath, j, "boxart_path");
		loadProperty(parsed, j, "parsed");
		loadProperty(scraped, j, "scraped");
	}

	void setBoxartPath(const std::string& path) {
		if (!boxartPath.empty())
			nowide::remove(boxartPath.c_str());
		boxartPath = path;
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

	virtual void scrape(std::vector<GameBoxart>& items) {
		for (GameBoxart& item : items)
			scrape(item);
	}

	virtual ~Scraper() = default;

protected:
	bool downloadImage(const std::string& url, const std::string& localName);
	std::string makeUniqueFilename(const std::string& url);

private:
	std::string saveDirectory;
};

class OfflineScraper : public Scraper
{
public:
	void scrape(GameBoxart& item) override;
};
