/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "types.h"
#include "gamepad.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

namespace emucfg {
struct ConfigFile;
}

class InputMapping
{
public:
	// Structure to hold a button combination
	struct ButtonCombination
	{
		std::vector<u32> buttons;

		ButtonCombination() = default;
		explicit ButtonCombination(u32 singleButton) { buttons.push_back(singleButton); }
		ButtonCombination(const std::vector<u32>& buttonList) : buttons(buttonList) {}

		bool operator==(const ButtonCombination& other) const {
			return buttons == other.buttons;
		}

		bool operator<(const ButtonCombination& other) const {
			return buttons < other.buttons;
		}
	};

	InputMapping() = default;
	InputMapping(const InputMapping& other) {
		name = other.name;
		dead_zone = other.dead_zone;
		saturation = other.saturation;
		rumblePower = other.rumblePower;
		for (int port = 0; port < 4; port++)
		{
			buttonCombinations[port] = other.buttonCombinations[port];
			reverseButtonMap[port] = other.reverseButtonMap[port];
			axes[port] = other.axes[port];
		}
	}

	std::string name;
	float dead_zone = 0.1f;
	float saturation = 1.0f;
	int rumblePower = 100;
	int version = 3;

	DreamcastKey get_button_id(u32 port, u32 code)
	{
		auto it = reverseButtonMap[port].find(code);
		if (it != reverseButtonMap[port].end())
		{
			DreamcastKey key = it->second;
			// Check if this key is mapped to a combination of buttons
			auto comboIt = buttonCombinations[port].find(key);
			if (comboIt != buttonCombinations[port].end() && comboIt->second.buttons.size() > 1)
			{
				// If it's a combination with multiple buttons, we need to be careful
				// Check if this button is ALSO mapped individually to something else
				for (auto& mapPair : buttonCombinations[port]) {
					if (mapPair.first != key && // Not the same mapping
						mapPair.second.buttons.size() == 1 && // Single button mapping
						mapPair.second.buttons[0] == code) // For this button
					{
						// Found individual mapping - return it instead
						return mapPair.first;
					}
				}
				
				// No individual mapping found - don't return anything for individual presses
				return EMU_BTN_NONE;
			}
			return key;
		}
		else
			return EMU_BTN_NONE;
	}
	
	void clear_button(u32 port, DreamcastKey id);
	void set_button(u32 port, DreamcastKey id, u32 code);
	void set_button_combination(u32 port, DreamcastKey id, const ButtonCombination& combo);
	void set_button(DreamcastKey id, u32 code) { set_button(0, id, code); }
	
	// Get all button codes for a DreamcastKey
	ButtonCombination get_button_combination(u32 port, DreamcastKey key);
	// Get the first button code for a DreamcastKey (for backward compatibility)
	u32 get_button_code(u32 port, DreamcastKey key);
	// Get all button combination mappings for a port
	const std::map<DreamcastKey, ButtonCombination>& get_all_button_combinations(u32 port) const {
		return buttonCombinations[port];
	}

	DreamcastKey get_axis_id(u32 port, u32 code, bool pos)
	{
		auto it = axes[port].find(std::make_pair(code, pos));
		if (it != axes[port].end())
			return it->second;
		else
			return EMU_AXIS_NONE;
	}
	std::pair<u32, bool> get_axis_code(u32 port, DreamcastKey key);

	void clear_axis(u32 port, DreamcastKey id);
	void set_axis(u32 port, DreamcastKey id, u32 code, bool positive);
	void set_axis(DreamcastKey id, u32 code, bool positive) { set_axis(0, id, code, positive); }

	void load(FILE* fp);
	bool save(const std::string& name);

	void set_dirty();
	bool is_dirty() const { return dirty; }

	static std::shared_ptr<InputMapping> LoadMapping(const std::string& name);
	static void SaveMapping(const std::string& name, const std::shared_ptr<InputMapping>& mapping);
	static void DeleteMapping(const std::string& name);

	void ClearMappings();

protected:
	bool dirty = false;

private:
	void loadv1(emucfg::ConfigFile& mf);

	// Maps a DreamcastKey to a ButtonCombination
	std::map<DreamcastKey, ButtonCombination> buttonCombinations[4];
	// Maps a single button code to a DreamcastKey for quick lookups
	std::map<u32, DreamcastKey> reverseButtonMap[4];
	std::map<std::pair<u32, bool>, DreamcastKey> axes[4];

	static std::map<std::string, std::shared_ptr<InputMapping>> loaded_mappings;
};

class IdentityInputMapping : public InputMapping
{
public:
	IdentityInputMapping() {
		name = "Default";
		dead_zone = 0.1f;
		
		for (int i = 0; i < 32; i++)
			set_button(0, (DreamcastKey)(1 << i), 1 << i);
		set_button(0, EMU_BTN_FFORWARD, EMU_BTN_FFORWARD);
		set_button(0, EMU_BTN_MENU, EMU_BTN_MENU);
		set_button(0, EMU_BTN_ESCAPE, EMU_BTN_ESCAPE);
		set_axis(0, DC_AXIS_LEFT, DC_AXIS_LEFT, true);
		set_axis(0, DC_AXIS_RIGHT, DC_AXIS_RIGHT, true);
		set_axis(0, DC_AXIS_UP, DC_AXIS_UP, true);
		set_axis(0, DC_AXIS_DOWN, DC_AXIS_DOWN, true);
		set_axis(0, DC_AXIS_LT, DC_AXIS_LT, true);
		set_axis(0, DC_AXIS_RT, DC_AXIS_RT, true);
		set_axis(0, DC_AXIS_LT2, DC_AXIS_LT2, true);
		set_axis(0, DC_AXIS_RT2, DC_AXIS_RT2, true);
		set_axis(0, DC_AXIS2_LEFT, DC_AXIS2_LEFT, true);
		set_axis(0, DC_AXIS2_RIGHT, DC_AXIS2_RIGHT, true);
		set_axis(0, DC_AXIS2_UP, DC_AXIS2_UP, true);
		set_axis(0, DC_AXIS2_DOWN, DC_AXIS2_DOWN, true);
		set_axis(0, DC_AXIS3_LEFT, DC_AXIS3_LEFT, true);
		set_axis(0, DC_AXIS3_RIGHT, DC_AXIS3_RIGHT, true);
		set_axis(0, DC_AXIS3_UP, DC_AXIS3_UP, true);
		set_axis(0, DC_AXIS3_DOWN, DC_AXIS3_DOWN, true);
	}
};
