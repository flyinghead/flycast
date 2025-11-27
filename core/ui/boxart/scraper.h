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

#include "types.h"
#include "json.hpp"

#include <string>
#include <vector>

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
	std::string fallbackBoxartPath;
	std::string boxartUrl;

	bool custom = false;
	bool arcade = false;
	bool parsed = false;
	bool scraped = false;
	bool busy = false;

	bool forceUpdate = false;

	enum Region { JAPAN = 1, USA = 2, EUROPE = 4 };

	json to_json(const std::string& baseArtPath) const;

	template<typename T>
	static void loadProperty(T& i, const json& j, const std::string& propName)
	{
		try {
			i = j.at(propName).get<T>();
		} catch (const json::exception& e) {
			i = T();
		}
	}

	GameBoxart() = default;

	GameBoxart(const json& j, const std::string& baseArtPath);

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
	bool downloadImage(const std::string& url, const std::string& localName, bool mute = false);
	std::string makeUniqueFilename(const std::string& url);

private:
	std::string saveDirectory;
};

class OfflineScraper : public Scraper
{
public:
	void scrape(GameBoxart& item) override;
};
