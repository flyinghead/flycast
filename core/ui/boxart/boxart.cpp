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
#include "oslib/oslib.h"
#include "oslib/storage.h"
#include "cfg/option.h"
#include "arcade_scraper.h"
#include <algorithm>
#include <array>
#include <chrono>

static bool isContentUri(const std::string& path)
{
#ifdef __ANDROID__
	return path.rfind("content://", 0) == 0;
#else
	return false;
#endif
}

static bool hasTrailingSeparator(const std::string& path)
{
	if (path.empty())
		return false;
	return path.find_last_of("/\\") == path.size() - 1;
}

static std::string getCustomBoxartDirectory()
{
	std::string customPath = config::CustomBoxartPath.get();
	if (customPath.empty())
	{
		customPath = get_writable_data_path("/boxart/custom/");
	}
	else if (!isContentUri(customPath) && !hasTrailingSeparator(customPath))
	{
		customPath += '/';
	}
	if (!isContentUri(customPath) && !file_exists(customPath))
		make_directory(customPath);
	return customPath;
}

static std::string buildCustomBoxartPath(const std::string& dir, const std::string& name, const std::string& ext)
{
	if (isContentUri(dir))
		return hostfs::storage().getSubPath(dir, name + "." + ext);
	return dir + name + "." + ext;
}

static bool customFileExists(const std::string& path)
{
	if (isContentUri(path))
		return hostfs::storage().exists(path);
	return file_exists(path);
}

static bool applyCustomBoxart(GameBoxart& boxart)
{
	if (boxart.fileName.empty())
		return false;
	const std::string customDir = getCustomBoxartDirectory();
	const std::array<std::string, 6> extensions{ "png", "jpg", "jpeg", "PNG", "JPG", "JPEG" };
	const std::array<std::string, 2> names{ get_file_basename(boxart.fileName), boxart.fileName };
	for (const auto& name : names)
	{
		if (name.empty())
			continue;
		for (const auto& ext : extensions)
		{
			std::string path = buildCustomBoxartPath(customDir, name, ext);
			if (customFileExists(path))
			{
				boxart.boxartPath = path;
				boxart.busy = false;
				boxart.scraped = true;
				boxart.parsed = true;
				return true;
			}
		}
	}
	return false;
}

GameBoxart Boxart::getBoxart(const GameMedia& media)
{
	loadDatabase();
	GameBoxart boxart;
	{
		std::lock_guard<std::mutex> guard(mutex);
		auto it = games.find(media.fileName);
		if (it != games.end())
		{
			boxart = it->second;
			if (applyCustomBoxart(boxart))
			{
				games[boxart.fileName] = boxart;
				databaseDirty = true;
			}
		}
	}
	return boxart;
}

GameBoxart Boxart::getBoxartAndLoad(const GameMedia& media)
{
	loadDatabase();
	GameBoxart boxart;
	bool scheduleFetch = false;
	{
		std::lock_guard<std::mutex> guard(mutex);
		auto it = games.find(media.fileName);
		if (it != games.end())
		{
			boxart = it->second;
		}
		else
		{
			boxart.fileName = media.fileName;
			boxart.gamePath = media.path;
			boxart.name = media.name;
			boxart.searchName = media.gameName;	// for arcade games
			boxart.busy = true;
			boxart.arcade = media.arcade;
			it = games.emplace(boxart.fileName, boxart).first;
			scheduleFetch = true;
		}
		if (applyCustomBoxart(boxart))
		{
			boxart.gamePath = media.path;
			boxart.arcade = media.arcade;
			it->second = boxart;
			databaseDirty = true;
			scheduleFetch = false;
			toFetch.erase(std::remove_if(toFetch.begin(), toFetch.end(),
				[&boxart](const GameBoxart& pending) { return pending.fileName == boxart.fileName; }),
				toFetch.end());
		}
		if (config::FetchBoxart && !boxart.busy && !boxart.scraped)
		{
			boxart.busy = true;
			boxart.gamePath = media.path;
			boxart.arcade = media.arcade;
			it->second = boxart;
			scheduleFetch = true;
		}
		if (scheduleFetch)
			toFetch.push_back(boxart);
	}
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
		ThreadName _("BoxArt-scraper");
		if (offlineScraper == nullptr)
		{
			offlineScraper = std::unique_ptr<Scraper>(new OfflineScraper());
			offlineScraper->initialize(getSaveDirectory());
		}
		if (config::FetchBoxart)
		{
			if (scraper == nullptr)
			{
				scraper = std::unique_ptr<Scraper>(new TheGamesDb());
				if (!scraper->initialize(getSaveDirectory()))
				{
					ERROR_LOG(COMMON, "thegamesdb scraper initialization failed");
					scraper.reset();
					return;
				}
			}
			if (arcadeScraper == nullptr) {
				arcadeScraper = std::make_unique<ArcadeScraper>();
				arcadeScraper->initialize(getSaveDirectory());
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
		offlineScraper->scrape(boxart);
		{
			std::lock_guard<std::mutex> guard(mutex);
			for (GameBoxart& b : boxart)
				if (b.scraped || b.parsed)
				{
					if (!config::FetchBoxart || b.scraped)
						b.busy = false;
					games[b.fileName] = b;
					databaseDirty = true;
				}
		}
		if (config::FetchBoxart)
		{
			try {
				arcadeScraper->scrape(boxart);
				scraper->scrape(boxart);
				{
					std::lock_guard<std::mutex> guard(mutex);
					for (GameBoxart& b : boxart)
					{
						b.busy = false;
						games[b.fileName] = b;
					}
				}
				databaseDirty = true;
			} catch (const std::runtime_error& e) {
				if (*e.what() != '\0')
					INFO_LOG(COMMON, "thegamesdb error: %s", e.what());
				{
					// put back failed items into toFetch array
					std::lock_guard<std::mutex> guard(mutex);
					for (GameBoxart& b : boxart)
						if (b.scraped)
						{
							b.busy = false;
							games[b.fileName] = b;
							databaseDirty = true;
						}
						else
						{
							toFetch.push_back(b);
						}
				}
			}
		}
		saveDatabase();
	});
}

void Boxart::saveDatabase()
{
	if (!databaseDirty)
		return;
	std::string basePath = getSaveDirectory();
	std::string db_name = basePath + DB_NAME;
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
			if (game.second.scraped || game.second.parsed)
				array.push_back(game.second.to_json(basePath));
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
			GameBoxart game(o, save_dir);
			games[game.fileName] = game;
		}
	} catch (const json::exception& e) {
		WARN_LOG(COMMON, "Corrupted database file: %s", e.what());
	}
}

void Boxart::term()
{
	if (fetching.valid())
		fetching.get();
}
