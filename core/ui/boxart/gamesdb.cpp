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
#include "gamesdb.h"
#include "oslib/http_client.h"
#include "stdclass.h"
#include "emulator.h"

#include <filesystem>
#include <fstream>

#define APIKEY "3fcc5e726a129924972be97abfd577ac5311f8f12398a9d9bcb5a377d4656fa8"

std::string TheGamesDb::makeUrl(const std::string& endpoint)
{
	return "https://api.thegamesdb.net/v1/" + endpoint + "?apikey=" APIKEY;
}

bool TheGamesDb::initialize(const std::string& saveDirectory)
{
	if (!Scraper::initialize(saveDirectory))
		return false;

	http::init();
	boxartCache.clear();

	return true;
}

TheGamesDb::~TheGamesDb()
{
	http::term();
}

void TheGamesDb::copyFile(const std::string& from, const std::string& to)
{
    std::ifstream ffrom(from, std::ios::in | std::ios::binary);
	if (!ffrom.is_open())
	{
		WARN_LOG(COMMON, "Can't open %s:", from.c_str());
		return;
	}
    std::ofstream fto(to, std::ios::out | std::ios::binary);
	if (!fto.is_open())
	{
		WARN_LOG(COMMON, "Can't open %s:", to.c_str());
        ffrom.close();
		return;
	}

    u8 buffer[4096];
    while (ffrom.read(reinterpret_cast<char*>(buffer), sizeof(buffer)) || ffrom.gcount() > 0)
    {
        fto.write(reinterpret_cast<char*>(buffer), ffrom.gcount());
    }
    ffrom.close();
    fto.close();
}

json TheGamesDb::httpGet(const std::string& url)
{
	if (getTimeMs() < blackoutPeriod)
		throw std::runtime_error("");
	blackoutPeriod = 0;

	DEBUG_LOG(COMMON, "TheGameDb: GET %s", url.c_str());
	std::vector<u8> receivedData;
	int status = http::get(url, receivedData);
	bool success = http::success(status);
	if (status == 403)
		// hit rate-limit cap
		blackoutPeriod = getTimeMs() + 60 * 1000;
	else if (!success)
		blackoutPeriod = getTimeMs() + 1000;
	if (!success || receivedData.empty())
		throw std::runtime_error("http error");

	std::string content((const char *)&receivedData[0], receivedData.size());
	DEBUG_LOG(COMMON, "TheGameDb: received [%s]", content.c_str());

	json v = json::parse(content);
	int code = v["code"];
	if (!http::success(code))
	{
		// TODO can this happen? http status should be the same
		std::string status;
		try {
			status = v.at("status");
		} catch (const json::exception& e) {
		}
		throw std::runtime_error(std::string("TheGamesDB error ") + std::to_string(code) + ": " + status);
	}

	return v;
}

void TheGamesDb::fetchPlatforms()
{
	if (dreamcastPlatformId != 0 && arcadePlatformId != 0)
		return;

	auto getPlatformId = [this](const std::string& platform)
	{
		std::string url = makeUrl("Platforms/ByPlatformName") + "&name=" + platform;
		json v = httpGet(url);

		try {
			const json& array = v.at("data").at("platforms");

			for (const auto& o : array)
			{
				try {
					std::string name = o.at("name");
					if (name == "Sega Dreamcast")
						dreamcastPlatformId = o.at("id");
					else if (name == "Arcade")
						arcadePlatformId = o.at("id");
				} catch (const json::exception& e) {
				}
			}
		} catch (const json::exception& e) {
		}
	};
	getPlatformId("Dreamcast");
	getPlatformId("Arcade");
	if (dreamcastPlatformId == 0 || arcadePlatformId == 0)
		throw std::runtime_error("can't find dreamcast or arcade platform id");
}

void TheGamesDb::parseBoxart(GameBoxart& item, const json& j, int gameId)
{
	std::string baseUrl;
	try {
		baseUrl = j.at("base_url").at("thumb");
	} catch (const json::exception& e) {
		try {
			baseUrl = j.at("base_url").at("small");
		} catch (const json::exception& e) {
			return;
		}
	}

	if (!j.contains("data") && !j.contains("images"))
		// No boxart
		return;
	const json& images = j.contains("data") ? j.at("data") : j.at("images");

	try {
		const json& dataArray = images.at(std::to_string(gameId));
		std::string imagePath;
		for (const auto& o : dataArray)
		{
			try {
				// Look for boxart
				if (o.at("type") != "boxart")
					continue;
			} catch (const json::exception& e) {
				continue;
			}
			try {
				// We want the front side if specified
				if (o.at("side") == "back")
					continue;
			} catch (const json::exception& e) {
				// ignore if not found
			}
			try {
				imagePath = o.at("filename").get<std::string>();
				break;
			} catch (const json::exception& e) {
				// continue if not found
			}
		}
		if (imagePath.empty())
		{
			// Use titlescreen
			for (const auto& o : dataArray)
			{
				try {
					if (o.at("type") != "titlescreen")
						continue;
					imagePath = o.at("filename").get<std::string>();
					break;
				} catch (const json::exception& e) {
					// continue if not found
				}
			}
		}
		if (imagePath.empty())
		{
			// Use screenshot
			for (const auto& o : dataArray)
			{
				try {
					if (o.at("type") != "screenshot")
						continue;
					imagePath = o.at("filename").get<std::string>();
					break;
				} catch (const json::exception& e) {
					// continue if not found
				}
			}
		}
		if (!imagePath.empty())
		{
			// Build the full URL and get from cache or download
			std::string url = baseUrl + imagePath;
			std::string filename = makeUniqueFilename("dummy.jpg");	// thegamesdb returns some images as png, but they are really jpeg
			auto cached = boxartCache.find(url);
			if (cached != boxartCache.end())
			{
				copyFile(cached->second, filename);
				item.setBoxartPath(filename);
				item.boxartUrl = url;
			}
			else
			{
				if (downloadImage(url, filename))
				{
					item.setBoxartPath(filename);
					item.boxartUrl = url;
					boxartCache[url] = filename;
				}
			}
		}
	} catch (const json::exception& e) {
		// No boxart for this game
	}
}

bool TheGamesDb::parseGameInfo(const json& gameArray, const json& boxartArray, GameBoxart& item, const std::string& diskId)
{
	for (const auto& game : gameArray)
	{
		if (!diskId.empty())
		{
			bool found = false;
			try {
				for (const auto& uid : game.at("uids"))
					if (uid.at("uid") == diskId) {
						found = true;
						break;
					}
			} catch (const json::exception& e) {
			}
			if (!found)
				continue;
		}
		// Name
		try {
			item.name = game.at("game_title");
		} catch (const json::exception& e) {
		}

		// Release date
		try {
			item.releaseDate = game.at("release_date");
		} catch (const json::exception& e) {
		}

		// Overview
		try {
			item.overview = game.at("overview");
		} catch (const json::exception& e) {
		}

		// GameDB id
		int id;
		try {
			id = game.at("id");
		} catch (const json::exception& e) {
			return true;
		}

		// Boxart
		parseBoxart(item, boxartArray, id);

		if (item.boxartPath.empty())
		{
			std::string imgUrl = makeUrl("Games/Images") + "&games_id=" + std::to_string(id);
			json images = httpGet(imgUrl);
			try {
				parseBoxart(item, images.at("data"), id);
			} catch (const json::exception& e) {
			}
		}
		return true;
	}
	return false;
}

bool TheGamesDb::fetchGameInfo(GameBoxart& item, const std::string& url, const std::string& diskId)
{
	json v = httpGet(url);
	try {
		const json& array = v.at("data").at("games");
		const json& boxart = v.at("include").at("boxart");
		return parseGameInfo(array, boxart, item, diskId);
	} catch (const json::exception& e) {
		return false;
	}
}

void TheGamesDb::scrape(GameBoxart& item)
{
	if (item.searchName.empty())
		// invalid rom or disk
		return;
	fetchPlatforms();

	// Ignore default disk ids used by kos and katana
	if (!item.uniqueId.empty() && item.uniqueId != "T0000" && item.uniqueId != "T0000M")
	{
		std::string url = makeUrl("Games/ByGameUniqueID") + "&fields=overview,uids&include=boxart&filter%5Bplatform%5D="
			+ std::to_string(dreamcastPlatformId) + "&uid=" + http::urlEncode(item.uniqueId);
		if (fetchGameInfo(item, url, item.uniqueId))
			item.scraped = true;
	}
	if (!item.scraped)
		fetchByName(item);

	item.scraped = true;
}

void TheGamesDb::fetchByName(GameBoxart& item)
{
	if (item.searchName.empty())
		return;
	int platform = getGamePlatform(item.fileName);
	std::string url = makeUrl("Games/ByGameName") + "&fields=overview&include=boxart&filter%5Bplatform%5D=";
	if (platform == DC_PLATFORM_DREAMCAST)
		url += std::to_string(dreamcastPlatformId);
	else
		url += std::to_string(arcadePlatformId);
	url += "&name=" + http::urlEncode(item.searchName);
	if (fetchGameInfo(item, url))
		item.scraped = true;
}

void TheGamesDb::fetchByUids(std::vector<GameBoxart>& items)
{
	std::string uidCriteria;
	for (const GameBoxart& item : items)
	{
		if (item.scraped || item.uniqueId.empty())
			continue;
		if (!uidCriteria.empty())
			uidCriteria += ',';
		uidCriteria += item.uniqueId;
	}
	if (uidCriteria.empty())
		return;
	std::string url = makeUrl("Games/ByGameUniqueID") + "&fields=overview,uids&include=boxart&filter%5Bplatform%5D="
			+ std::to_string(dreamcastPlatformId) + "&uid=" + http::urlEncode(uidCriteria);
	json v = httpGet(url);
	try {
		const json& array = v.at("data").at("games");
		const json& boxartArray = v.at("include").at("boxart");
		for (GameBoxart& item : items)
		{
			if (!item.scraped && !item.uniqueId.empty() && parseGameInfo(array, boxartArray, item, item.uniqueId))
				item.scraped = true;
		}
	} catch (const json::exception& e) {
	}
}

void TheGamesDb::scrape(std::vector<GameBoxart>& items)
{
	if (getTimeMs() < blackoutPeriod)
		throw std::runtime_error("");
	blackoutPeriod = 0.0;

	fetchPlatforms();
	fetchByUids(items);
	for (GameBoxart& item : items)
	{
		if (!item.scraped)
		{
			if (!item.searchName.empty())
				fetchByName(item);
			else if (item.gamePath.empty())
			{
				std::string localPath = makeUniqueFilename("dreamcast_logo_grey.png");
				std::string biosArtUrl{ "https://flyinghead.github.io/flycast-builds/dreamcast_logo_grey.png" };
				if (downloadImage(biosArtUrl, localPath)) {
					item.setBoxartPath(localPath);
					item.boxartUrl = biosArtUrl;
				}
			}
			item.scraped = true;
		}
	}
}
