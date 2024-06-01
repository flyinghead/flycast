/*
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
#include <future>
#include <vector>

namespace achievements
{
#ifdef USE_RACHIEVEMENTS

struct Game
{
	std::string image;
	std::string title;
	u32 unlockedAchievements;
	u32 totalAchievements;
	u32 points;
	u32 totalPoints;
};

struct Achievement
{
	Achievement() = default;
	Achievement(const std::string& image, const std::string& title, const std::string& description, const std::string& category, const std::string& status)
		: image(image), title(title), description(description), category(category), status(status) {}
	std::string image;
	std::string title;
	std::string description;
	std::string category;
	std::string status;
};

bool init();
void term();
std::future<void> login(const char *username, const char *password);
void logout();
bool isLoggedOn();
bool isActive();
Game getCurrentGame();
std::vector<Achievement> getAchievementList();
bool canPause();

#else

static inline bool isActive() {
	return false;
}
static inline bool canPause() {
	return true;
}

#endif

void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

}
