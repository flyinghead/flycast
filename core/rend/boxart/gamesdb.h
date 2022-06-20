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
#include <map>
#include "scraper.h"
#include "json.hpp"

using namespace nlohmann;

class TheGamesDb : public Scraper
{
public:
	bool initialize(const std::string& saveDirectory) override;
	void scrape(GameBoxart& item) override;
	~TheGamesDb();

private:
	void scrape(GameBoxart& item, const std::string& diskId);
	void fetchPlatforms();
	bool fetchGameInfo(GameBoxart& item, const std::string& url, const std::string& diskId = "");
	std::string makeUrl(const std::string& endpoint);
	void copyFile(const std::string& from, const std::string& to);
	bool httpGet(const std::string& url, std::vector<u8>& receivedData);
	void parseBoxart(GameBoxart& item, const json& j, int gameId);

	int dreamcastPlatformId;
	int arcadePlatformId;
	double blackoutPeriod = 0.0;

	std::map<std::string, std::string> boxartCache;	// key: url, value: local file path
};
