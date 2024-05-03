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
#include "imgui.h"
#include "gui_util.h"
#include <mutex>
#include <vector>

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
		Challenge,
		Error
	};
	void notify(Type type, const std::string& image, const std::string& text1,
			const std::string& text2 = {}, const std::string& text3 = {});
	void showChallenge(const std::string& image);
	void hideChallenge(const std::string& image);
	bool draw();

private:
	u64 startTime = 0;
	u64 endTime = 0;
	Type type = Type::None;
	ImguiTexture image;
	std::string text[3];
	std::mutex mutex;
	std::vector<ImguiTexture> challenges;
};

extern Notification notifier;

void achievementList();

}
