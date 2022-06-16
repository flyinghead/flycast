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
	auto it = games.find(fileName);
	if (it != games.end())
		return &it->second;
	else
		return nullptr;
}

std::future<const GameBoxart *> Boxart::fetchBoxart(const GameMedia& media)
{
	return std::async(std::launch::async, [this, media]() {
		std::string fileName = getGameFileName(media.path);
		if (scraper == nullptr)
		{
			scraper = std::unique_ptr<Scraper>(new TheGamesDb());
			if (!scraper->initialize(getSaveDirectory()))
			{
				WARN_LOG(COMMON, "thegamesdb scraper initialization failed");
				scraper.reset();
			}
		}
		const GameBoxart *rv = nullptr;
		if (scraper != nullptr)
		{
			GameBoxart boxart;
			boxart.fileName = fileName;
			boxart.gamePath = media.path;
			boxart.name = trim_trailing_ws(get_file_basename(media.gameName));
			while (!boxart.name.empty())
			{
				size_t pos{ std::string::npos };
				if (boxart.name.back() == ')')
					pos = boxart.name.find_last_of('(');
				else if (boxart.name.back() == ']')
					pos = boxart.name.find_last_of('[');
				if (pos == std::string::npos)
					break;
				boxart.name = trim_trailing_ws(boxart.name.substr(0, pos));
			}
			DEBUG_LOG(COMMON, "Scraping %s -> %s", media.name.c_str(), boxart.name.c_str());
			try {
				scraper->scrape(boxart);
				games[fileName] = boxart;
				rv = &games[fileName];
				databaseDirty = true;
			} catch (const std::exception& e) {
				if (*e.what() != '\0')
					INFO_LOG(COMMON, "thegamesdb error: %s", e.what());
			}
		}
		return rv;
	});
}

void Boxart::saveDatabase()
{
	if (!databaseDirty || games.empty())
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
	for (const auto& game : games)
		if (game.second.scraped)
			array.push_back(game.second.to_json());
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
