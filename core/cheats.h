/*
    Created on: Sep 23, 2019

	Copyright 2019 flyinghead

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

struct WidescreenCheat
{
	const char *game_id;
	const char *area_or_version;
	u32 addresses[16];
	u32 values[16];
};

struct Cheat
{
	enum class Type {
		disabled,
		setValue,
		increase,
		decrease,
		runNextIfEq,
		runNextIfNeq,
		runNextIfGt,
		runNextIfLt,
		copy
	};
	Type type = Type::disabled;
	std::string description;
	bool enabled = false;
	u32 size = 0;
	u32 address = 0;
	u32 value = 0;
	u8 valueMask = 0;
	u32 repeatCount = 1;
	u32 repeatValueIncrement = 0;
	u32 repeatAddressIncrement = 0;
	u32 destAddress = 0;
};

class CheatManager
{
public:
	void reset(const std::string& gameId);
	void apply();
	size_t cheatCount() const { return cheats.size(); }
	const std::string& cheatDescription(size_t index) const { return cheats[index].description; }
	bool cheatEnabled(size_t index) const { return cheats[index].enabled; }
	void enableCheat(size_t index, bool enabled) { cheats[index].enabled = enabled; }
	void loadCheatFile(const std::string& filename);
	void saveCheatFile(const std::string& filename);
	// Returns true if using 16:9 anamorphic screen ratio
	bool isWidescreen() const { return widescreen_cheat != nullptr; }
	void addGameSharkCheat(const std::string& name, const std::string& s);

private:
	u32 readRam(u32 addr, u32 bits);
	void writeRam(u32 addr, u32 value, u32 bits);

	static const WidescreenCheat widescreen_cheats[];
	static const WidescreenCheat naomi_widescreen_cheats[];
	const WidescreenCheat *widescreen_cheat = nullptr;
	bool active = false;
	std::vector<Cheat> cheats;
	std::string gameId;

	friend class CheatManagerTest_TestLoad_Test;
	friend class CheatManagerTest_TestGameShark_Test;
	friend class CheatManagerTest_TestSave_Test;
};

extern CheatManager cheatManager;
