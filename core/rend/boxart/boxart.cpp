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
#include "boxart.h"
#include "gamesdb.h"
#include "../game_scanner.h"
#include <chrono>

static std::string getGameFileName(const std::string& path)
{
	size_t slash = get_last_slash_pos(path);
	std::string fileName;
	if (slash != std::string::npos)
		return path.substr(slash + 1);
	else
		return path;
}

const GameBoxart *Boxart::getBoxart(const GameMedia& media)
{
	loadDatabase();
	std::string fileName = getGameFileName(media.path);
	const GameBoxart *boxart = nullptr;
	{
		std::lock_guard<std::mutex> guard(mutex);
		auto it = games.find(fileName);
		if (it != games.end())
			boxart = &it->second;
		else if (config::FetchBoxart)
		{
			GameBoxart box;
			box.fileName = fileName;
			box.gamePath = media.path;
			box.name = media.name;
			box.searchName = media.gameName;	// for arcade games
			games[box.fileName] = box;
			toFetch.push_back(box);
		}
	}
	if (config::FetchBoxart)
		fetchBoxart();
	return boxart;
}

void Boxart::fetchBoxart()
{
	if (fetching.valid() && fetching.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		fetching.get();
	if (fetching.valid())
		return;
	if (toFetch.empty())
		return;
	fetching = std::async(std::launch::async, [this]() {
		if (scraper == nullptr)
		{
			scraper = std::unique_ptr<Scraper>(new TheGamesDb());
			if (!scraper->initialize(getSaveDirectory()))
			{
				WARN_LOG(COMMON, "thegamesdb scraper initialization failed");
				scraper.reset();
				return;
			}
		}
		std::vector<GameBoxart> boxart;
		{
			std::lock_guard<std::mutex> guard(mutex);
			size_t size = std::min(toFetch.size(), (size_t)10);
			boxart = std::vector<GameBoxart>(toFetch.begin(), toFetch.begin() + size);
			toFetch.erase(toFetch.begin(), toFetch.begin() + size);
		}
		DEBUG_LOG(COMMON, "Scraping %d games", (int)boxart.size());
		try {
			scraper->scrape(boxart);
			{
				std::lock_guard<std::mutex> guard(mutex);
				for (const GameBoxart& b : boxart)
					if (b.scraped)
						games[b.fileName] = b;
			}
			databaseDirty = true;
		} catch (const std::exception& e) {
			if (*e.what() != '\0')
				INFO_LOG(COMMON, "thegamesdb error: %s", e.what());
			{
				// put back items into toFetch array
				std::lock_guard<std::mutex> guard(mutex);
				toFetch.insert(toFetch.begin(), boxart.begin(), boxart.end());
			}
		}
	});
}

void Boxart::saveDatabase()
{
	if (fetching.valid())
		fetching.get();
	if (!databaseDirty)
		return;
	std::string db_name = getSaveDirectory() + DB_NAME;
	FILE *file = nowide::fopen(db_name.c_str(), "wt");
	if (file == nullptr)
	{
		WARN_LOG(COMMON, "Can't save boxart database to %s: error %d", db_name.c_str(), errno);
		return;
	}
	DEBUG_LOG(COMMON, "Saving boxart database to %s", db_name.c_str());

	json array;
	{
		std::lock_guard<std::mutex> guard(mutex);
		for (const auto& game : games)
			if (game.second.scraped)
				array.push_back(game.second.to_json());
	}
	std::string serialized = array.dump(4);
	fwrite(serialized.c_str(), 1, serialized.size(), file);
	fclose(file);
	databaseDirty = false;
}

void Boxart::loadDatabase()
{
	if (databaseLoaded)
		return;
	databaseLoaded = true;
	databaseDirty = false;
	std::string save_dir = getSaveDirectory();
	if (!file_exists(save_dir))
		make_directory(save_dir);
	std::string db_name = save_dir + DB_NAME;
	FILE *f = nowide::fopen(db_name.c_str(), "rt");
	if (f == nullptr)
		return;

	DEBUG_LOG(COMMON, "Loading boxart database from %s", db_name.c_str());
	std::string all_data;
	char buf[4096];
	while (true)
	{
		int s = fread(buf, 1, sizeof(buf), f);
		if (s <= 0)
			break;
		all_data.append(buf, s);
	}
	fclose(f);
	try {
		std::lock_guard<std::mutex> guard(mutex);

		json v = json::parse(all_data);
		for (const auto& o : v)
		{
			GameBoxart game(o);
			games[game.fileName] = game;
		}
	} catch (const json::exception& e) {
		WARN_LOG(COMMON, "Corrupted database file: %s", e.what());
	}
}
