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
#include "http_client.h"
#include "reios/reios.h"
#include "imgread/common.h"
#include "stdclass.h"
#include "oslib/oslib.h"
#include <cctype>

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
	FILE *ffrom = nowide::fopen(from.c_str(), "rb");
	if (ffrom == nullptr)
	{
		WARN_LOG(COMMON, "Can't open %s: error %d", from.c_str(), errno);
		return;
	}
	FILE *fto = nowide::fopen(to.c_str(), "wb");
	if (fto == nullptr)
	{
		WARN_LOG(COMMON, "Can't open %s: error %d", to.c_str(), errno);
		fclose(ffrom);
		return;
	}
	u8 buffer[4096];
	while (true)
	{
		int l = fread(buffer, 1, sizeof(buffer), ffrom);
		if (l == 0)
			break;
		fwrite(buffer, 1, l, fto);
	}
	fclose(ffrom);
	fclose(fto);
}

bool TheGamesDb::httpGet(const std::string& url, std::vector<u8>& receivedData)
{
	int status = http::get(url, receivedData);
	bool success = http::success(status);
	if (status == 403)
		// hit rate-limit cap
		blackoutPeriod = os_GetSeconds() + 60.0;
	else if (!success)
		blackoutPeriod = os_GetSeconds() + 1.0;

	return success;
}

void TheGamesDb::fetchPlatforms()
{
	if (dreamcastPlatformId != 0 && arcadePlatformId != 0)
		return;

	auto getPlatformId = [this](const std::string& platform)
	{
		std::string url = makeUrl("Platforms/ByPlatformName") + "&name=" + platform;
		DEBUG_LOG(COMMON, "TheGameDb: GET %s", url.c_str());

		std::vector<u8> receivedData;
		if (!httpGet(url, receivedData))
			throw std::runtime_error("http error");

		std::string content((const char *)&receivedData[0], receivedData.size());
		json v = json::parse(content);

		const json& array = v["data"]["platforms"];

		for (const auto& o : array)
		{
			std::string name = o["name"];
			if (name == "Sega Dreamcast")
				dreamcastPlatformId = o["id"];
			else if (name == "Arcade")
				arcadePlatformId = o["id"];
		}
	};
	getPlatformId("Dreamcast");
	getPlatformId("Arcade");
	if (dreamcastPlatformId == 0 || arcadePlatformId == 0)
		throw std::runtime_error("can't find dreamcast or arcade platform id");
}

bool TheGamesDb::fetchGameInfo(GameBoxart& item, const std::string& url)
{
	fetchPlatforms();

	DEBUG_LOG(COMMON, "TheGameDb: GET %s", url.c_str());
	std::vector<u8> receivedData;
	if (!httpGet(url, receivedData))
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
			status = v["status"];
		} catch (const json::exception& e) {
		}
		throw std::runtime_error(std::string("TheGamesDB error ") + std::to_string(code) + ": " + status);
	}
	json array = v["data"]["games"];
	if (array.empty())
		return false;

	// Name
	item.name = array[0]["game_title"];

	// Release date
	try {
		item.releaseDate = array[0]["release_date"];
	} catch (const json::exception& e) {
	}

	// Overview
	try {
		item.overview = array[0]["overview"];
	} catch (const json::exception& e) {
	}

	// GameDB id
	int id;
	try {
		id = array[0]["id"];
	} catch (const json::exception& e) {
		return true;
	}

	// Boxart
	json boxart = v["include"]["boxart"];
	std::string baseUrl;
	try {
		baseUrl = boxart["base_url"]["thumb"];
	} catch (const json::exception& e) {
		try {
			baseUrl = boxart["base_url"]["small"];
		} catch (const json::exception& e) {
			return true;
		}
	}

	json dataArray = boxart["data"][std::to_string(id)];
	for (const auto& o : dataArray)
	{
		try {
			// Look for boxart
			if (o["type"] != "boxart")
				continue;
		} catch (const json::exception& e) {
			continue;
		}
		try {
			// We want the front side if specified
			if (o["side"] == "back")
				continue;
		} catch (const json::exception& e) {
			// ignore if not found
		}
		// Build the full URL and get from cache or download
		std::string url = baseUrl + o["filename"].get<std::string>();
		std::string filename = makeUniqueFilename("dummy.jpg");	// thegamesdb returns some images as png, but they are really jpeg
		auto cached = boxartCache.find(url);
		if (cached != boxartCache.end())
		{
			copyFile(cached->second, filename);
			item.boxartPath = filename;
		}
		else
		{
			if (downloadImage(url, filename))
			{
				item.boxartPath = filename;
				boxartCache[url] = filename;
			}
		}
		break;
	}
	return true;
}

void TheGamesDb::scrape(GameBoxart& item)
{
	if (os_GetSeconds() < blackoutPeriod)
		throw std::runtime_error("");
	blackoutPeriod = 0.0;

	item.found = false;
	int platform = getGamePlatform(item.gamePath.c_str());
	std::string gameName;
	std::string uniqueId;
	if (platform == DC_PLATFORM_DREAMCAST)
	{
		Disc *disc;
		try {
			disc = OpenDisc(item.gamePath.c_str());
		} catch (const std::exception& e) {
			WARN_LOG(COMMON, "Can't open disk %s: %s", item.gamePath.c_str(), e.what());
			// No need to retry if the disk is invalid/corrupted
			item.scraped = true;
			return;
		}

		u32 base_fad;
		if (disc->type == GdRom) {
			base_fad = 45150;
		} else {
			u8 ses[6];
			disc->GetSessionInfo(ses, 0);
			disc->GetSessionInfo(ses, ses[2]);
			base_fad = (ses[3] << 16) | (ses[4] << 8) | (ses[5] << 0);
		}
		u8 sector[2048];
		disc->ReadSectors(base_fad, 1, sector, sizeof(sector));
		ip_meta_t diskId;
		memcpy(&diskId, sector, sizeof(diskId));
		delete disc;

		uniqueId = trim_trailing_ws(std::string(diskId.product_number, sizeof(diskId.product_number)));

		gameName = trim_trailing_ws(std::string(diskId.software_name, sizeof(diskId.software_name)));
		if (gameName.empty())
			gameName = item.name;

		if (diskId.area_symbols[0] != '\0')
		{
			if (diskId.area_symbols[0] == 'J')
				item.region |= GameBoxart::JAPAN;
			if (diskId.area_symbols[1] == 'U')
				item.region |= GameBoxart::USA;
			if (diskId.area_symbols[2] == 'E')
				item.region |= GameBoxart::EUROPE;
		}
	}
	else
	{
		gameName = item.name;
	}

	if (!uniqueId.empty())
	{
		std::string url = makeUrl("Games/ByGameUniqueID") + "&fields=overview&include=boxart&filter%5Bplatform%5D=";
		if (platform == DC_PLATFORM_DREAMCAST)
			url += std::to_string(dreamcastPlatformId);
		else
			url += std::to_string(arcadePlatformId);
		url += "&uid=" + http::urlEncode(uniqueId);

		item.found = fetchGameInfo(item, url);
	}
	if (!item.found)
	{
		std::string url = makeUrl("Games/ByGameName") + "&fields=overview&include=boxart&filter%5Bplatform%5D=";
		if (platform == DC_PLATFORM_DREAMCAST)
			url += std::to_string(dreamcastPlatformId);
		else
			url += std::to_string(arcadePlatformId);
		url += "&name=" + http::urlEncode(gameName);
		item.found = fetchGameInfo(item, url);
	}
	item.scraped = true;
}
