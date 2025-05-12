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
#include "scraper.h"
#include "stdclass.h"
#include "oslib/oslib.h"

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct GameMedia;

class Boxart
{
public:
	GameBoxart getBoxartAndLoad(const GameMedia& media);
	GameBoxart getBoxart(const GameMedia& media);
	void term();

private:
	void loadDatabase();
	void saveDatabase();
	std::string getSaveDirectory() const {
		return get_writable_data_path("/boxart/");
	}
	std::string getCustomBoxartDirectory() const {
		return hostfs::getCustomBoxartPath();
	}
	GameBoxart checkCustomBoxart(const GameMedia& media);
	void fetchBoxart();

	std::unordered_map<std::string, GameBoxart> games;
	std::mutex mutex;
	std::unique_ptr<Scraper> scraper;
	std::unique_ptr<Scraper> offlineScraper;
	bool databaseLoaded = false;
	bool databaseDirty = false;

	std::vector<GameBoxart> toFetch;
	std::future<void> fetching;

	static constexpr char const *DB_NAME = "flycast-gamedb.json";
};
