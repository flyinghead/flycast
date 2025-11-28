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
#include "cfg/option.h"

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
	bool applyCustomBoxart(GameBoxart& boxart);
	void loadDatabase();
	void saveDatabase();
	std::string getSaveDirectory() const {
		// *must* end with a path separator
		if (!config::BoxartPath.get().empty()) {
			std::string path = config::BoxartPath.get();
			if (!path.empty() && path.back() != '/' && path.back() != '\\')
				path += '/';
			return path;
		}
		return get_writable_data_path("/boxart/");
	}
	void fetchBoxart();

	std::unordered_map<std::string, GameBoxart> games;
	std::mutex mutex;
	std::unique_ptr<Scraper> scraper;
	std::unique_ptr<Scraper> offlineScraper;
	std::unique_ptr<Scraper> arcadeScraper;
	bool databaseLoaded = false;
	bool databaseDirty = false;

	std::vector<GameBoxart> toFetch;
	std::future<void> fetching;

	static constexpr char const *DB_NAME = "flycast-gamedb.json";
};
