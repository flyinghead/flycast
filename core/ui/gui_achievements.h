/*
	Copyright 2024 flyinghead

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
#include "types.h"
#include "gui_util.h"
#include <mutex>
#include <vector>
#include <map>

namespace achievements
{

class Notification
{
public:
	enum Type {
		None,
		Login,
		GameLoaded,
		Unlocked,
		Progress,
		Mastery,
		Challenge,   // internal use
		Leaderboard, // internal use
		Error
	};
	void notify(Type type, const std::string& image, const std::string& text1,
			const std::string& text2 = {}, const std::string& text3 = {});
	void showChallenge(const std::string& image);
	void hideChallenge(const std::string& image);
	void showLeaderboard(u32 id, const std::string& text);
	void hideLeaderboard(u32 id);
	bool draw();

private:
	u64 startTime = 0;
	u64 endTime = 0;
	Type type = Type::None;
	ImguiFileTexture image;
	std::string text[3];
	std::mutex mutex;
	std::vector<ImguiFileTexture> challenges;
	std::map<u32, std::string> leaderboards;
};

extern Notification notifier;

void achievementList();

}
