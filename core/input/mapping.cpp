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
#include <cmath>
#include "mapping.h"
#include "cfg/ini.h"
#include "stdclass.h"

static struct
{
	DreamcastKey id;
	std::string section;
	std::string option;
}
button_list[] =
{
	{ DC_BTN_A, "dreamcast", "btn_a" },
	{ DC_BTN_B, "dreamcast", "btn_b" },
	{ DC_BTN_C, "dreamcast", "btn_c" },
	{ DC_BTN_D, "dreamcast", "btn_d" },
	{ DC_BTN_X, "dreamcast", "btn_x" },
	{ DC_BTN_Y, "dreamcast", "btn_y" },
	{ DC_BTN_Z, "dreamcast", "btn_z" },
	{ DC_BTN_START, "dreamcast", "btn_start" },
	{ DC_DPAD_LEFT, "dreamcast", "btn_dpad1_left" },
	{ DC_DPAD_RIGHT, "dreamcast", "btn_dpad1_right" },
	{ DC_DPAD_UP, "dreamcast", "btn_dpad1_up" },
	{ DC_DPAD_DOWN, "dreamcast", "btn_dpad1_down" },
	{ DC_DPAD2_LEFT, "dreamcast", "btn_dpad2_left" },
	{ DC_DPAD2_RIGHT, "dreamcast", "btn_dpad2_right" },
	{ DC_DPAD2_UP, "dreamcast", "btn_dpad2_up" },
	{ DC_DPAD2_DOWN, "dreamcast", "btn_dpad2_down" },
	{ EMU_BTN_ESCAPE, "emulator", "btn_escape" },
	{ EMU_BTN_MENU, "emulator", "btn_menu" },
	{ EMU_BTN_FFORWARD, "emulator", "btn_fforward" },
	{ DC_AXIS_LT, "compat", "btn_trigger_left" },
	{ DC_AXIS_RT, "compat", "btn_trigger_right" },
	{ DC_AXIS_LT2, "compat", "btn_trigger2_left" },
	{ DC_AXIS_RT2, "compat", "btn_trigger2_right" },
	{ DC_AXIS_UP, "compat", "btn_analog_up" },
	{ DC_AXIS_DOWN, "compat", "btn_analog_down" },
	{ DC_AXIS_LEFT, "compat", "btn_analog_left" },
	{ DC_AXIS_RIGHT, "compat", "btn_analog_right" },
	{ DC_BTN_RELOAD, "dreamcast", "reload" },
	{ DC_BTN_INSERT_CARD, "emulator", "insert_card" },
	{ EMU_BTN_LOADSTATE, "emulator", "btn_jump_state" },
	{ EMU_BTN_SAVESTATE, "emulator", "btn_quick_save" },
	{ EMU_BTN_BYPASS_KB, "emulator", "btn_bypass_kb" },
	{ EMU_BTN_SCREENSHOT, "emulator", "btn_screenshot" },
};

static struct
{
	DreamcastKey id;
	std::string section;
	std::string option;
	std::string section_inverted;
	std::string option_inverted;
}
axis_list[] =
{
	// v3
	{ DC_AXIS_LEFT, "", "axis_left", "", "" },
	{ DC_AXIS_RIGHT, "", "axis_right", "", "" },
	{ DC_AXIS_UP, "", "axis_up", "", "" },
	{ DC_AXIS_DOWN, "", "axis_down", "", "" },
	{ DC_AXIS2_LEFT, "", "axis2_left", "", "" },
	{ DC_AXIS2_RIGHT, "", "axis2_right", "", "" },
	{ DC_AXIS2_UP, "", "axis2_up", "", "" },
	{ DC_AXIS2_DOWN, "", "axis2_down", "", "" },
	{ DC_AXIS3_LEFT,  "", "axis3_left", "", "" },
	{ DC_AXIS3_RIGHT, "", "axis3_right", "", "" },
	{ DC_AXIS3_UP,    "", "axis3_up", "", "" },
	{ DC_AXIS3_DOWN,  "", "axis3_down", "", "" },
	{ DC_AXIS_LT, "dreamcast", "axis_trigger_left",  "compat", "axis_trigger_left_inverted" },
	{ DC_AXIS_RT, "dreamcast", "axis_trigger_right", "compat", "axis_trigger_right_inverted" },
	{ DC_AXIS_LT2, "dreamcast", "axis_trigger2_left",   "compat", "axis_trigger2_left_inverted" },
	{ DC_AXIS_RT2, "dreamcast", "axis_trigger2_right", "compat", "axis_trigger2_right_inverted" },

	// legacy (v2)
	{ DC_AXIS_RIGHT, "dreamcast", "axis_x", "compat", "axis_x_inverted" },
	{ DC_AXIS_DOWN, "dreamcast", "axis_y", "compat", "axis_y_inverted" },
	{ DC_AXIS2_RIGHT, "dreamcast", "axis_right_x", "compat", "axis_right_x_inverted" },
	{ DC_AXIS2_DOWN, "dreamcast", "axis_right_y", "compat", "axis_right_y_inverted" },
	{ DC_DPAD_LEFT, "compat", "axis_dpad1_x", "compat", "axis_dpad1_x_inverted" },
	{ DC_DPAD_UP, "compat", "axis_dpad1_y", "compat", "axis_dpad1_y_inverted" },
	{ DC_DPAD2_LEFT, "compat", "axis_dpad2_x", "compat", "axis_dpad2_x_inverted" },
	{ DC_DPAD2_UP, "compat", "axis_dpad2_y", "compat", "axis_dpad2_y_inverted" },
	{ DC_BTN_A, "compat", "axis_btn_a", "compat", "axis_btn_a_inverted" },
	{ DC_BTN_B, "compat", "axis_btn_b", "compat", "axis_btn_b_inverted" },
	{ DC_BTN_C, "compat", "axis_btn_c", "compat", "axis_btn_c_inverted" },
	{ DC_BTN_D, "compat", "axis_btn_d", "compat", "axis_btn_d_inverted" },
	{ DC_BTN_X, "compat", "axis_btn_x", "compat", "axis_btn_x_inverted" },
	{ DC_BTN_Y, "compat", "axis_btn_y", "compat", "axis_btn_y_inverted" },
	{ DC_BTN_Z, "compat", "axis_btn_z", "compat", "axis_btn_z_inverted" },
	{ DC_BTN_START, "compat", "axis_btn_start", "compat", "axis_btn_start_inverted" },
	{ DC_DPAD_LEFT, "compat", "axis_dpad1_left", "compat", "axis_dpad1_left_inverted" },
	{ DC_DPAD_RIGHT, "compat", "axis_dpad1_right", "compat", "axis_dpad1_right_inverted" },
	{ DC_DPAD_UP, "compat", "axis_dpad1_up", "compat", "axis_dpad1_up_inverted" },
	{ DC_DPAD_DOWN, "compat", "axis_dpad1_down", "compat", "axis_dpad1_down_inverted" },
	{ DC_DPAD2_LEFT, "compat", "axis_dpad2_left", "compat", "axis_dpad2_left_inverted" },
	{ DC_DPAD2_RIGHT, "compat", "axis_dpad2_right", "compat", "axis_dpad2_right_inverted" },
	{ DC_DPAD2_UP, "compat", "axis_dpad2_up", "compat", "axis_dpad2_up_inverted" },
	{ DC_DPAD2_DOWN, "compat", "axis_dpad2_down", "compat", "axis_dpad2_down_inverted" },

};

std::map<std::string, std::shared_ptr<InputMapping>> InputMapping::loaded_mappings;

void InputMapping::clear_button(u32 port, DreamcastKey id)
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return;
	}

	// Only clear if a single button is at the given port/id
	std::map<DreamcastKey, InputSet>& inputMap = multiEmuButtonMap[port];
	std::map<DreamcastKey, InputSet>::const_iterator iter = inputMap.find(id);
	if (iter != inputMap.end() && iter->second.size() == 1 && iter->second.begin()->is_button())
	{
		clear_combo(port, id);
	}
}

void InputMapping::set_button(u32 port, DreamcastKey id, u32 code)
{
	if (id != EMU_BTN_NONE)
	{
		set_combo(port, id, InputSet{InputDef{code, InputDef::InputType::BUTTON}});
	}
}

void InputMapping::clear_axis(u32 port, DreamcastKey id)
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return;
	}

	// Only clear if a single axis is at the given port/id
	std::map<DreamcastKey, InputSet>& inputMap = multiEmuButtonMap[port];
	std::map<DreamcastKey, InputSet>::const_iterator iter = inputMap.find(id);
	if (iter != inputMap.end() && iter->second.size() == 1 && iter->second.begin()->is_axis())
	{
		clear_combo(port, id);
	}
}

void InputMapping::set_axis(u32 port, DreamcastKey id, u32 code, bool positive)
{
	if (id != EMU_AXIS_NONE)
	{
		InputDef::InputType type = (positive ? InputDef::InputType::AXIS_POS : InputDef::InputType::AXIS_NEG);
		set_combo(port, id, InputSet{InputDef{code, type}});
	}
}

using namespace emucfg;

static DreamcastKey getKeyId(const std::string& name)
{
	for (u32 i = 0; i < std::size(button_list); i++)
		if (name == button_list[i].option)
			return button_list[i].id;
	for (u32 i = 0; i < std::size(axis_list); i++)
		if (name == axis_list[i].option)
			return axis_list[i].id;

	WARN_LOG(INPUT, "Unknown key/axis: %s", name.c_str());
	return EMU_BTN_NONE;
}

void InputMapping::load(FILE* fp)
{
	ConfigFile mf;
	mf.parse(fp);

	this->name = mf.get("emulator", "mapping_name", "<Unknown>");

	int dz = mf.get_int("emulator", "dead_zone", 10);
	dz = std::min(dz, 100);
	dz = std::max(dz, 0);
	this->dead_zone = (float)dz / 100.f;
	int sat = std::clamp(mf.get_int("emulator", "saturation", 100), 50, 200);
	this->saturation = (float)sat / 100.f;
	this->rumblePower = mf.get_int("emulator", "rumble_power", this->rumblePower);

	version = mf.get_int("emulator", "version", 1);
	if (version < 3)
	{
		loadv1(mf);
		return;
	}
	int bindIndex = 0;
	while (true)
	{
		std::string s = mf.get("digital", "bind" + std::to_string(bindIndex++), "");
		if (s.empty())
			break;
		size_t colon = s.find(':');
		if (colon == std::string::npos || colon == 0)
		{
			WARN_LOG(INPUT, "Invalid bind entry: %s", s.c_str());
			break;
		}
		u32 code = atoi(s.substr(0, colon).c_str());
		std::string key = s.substr(colon + 1);
		if (key.empty())
		{
			WARN_LOG(INPUT, "Invalid bind entry: %s", s.c_str());
			break;
		}
		int port = 0;
		if (key[key.size() - 1] >= '1' && key[key.size() - 1] <= '3')
		{
			port = key[key.size() - 1] - '0';
			key = key.substr(0, key.size() - 1);
		}
		DreamcastKey id = getKeyId(key);
		set_button(port, id, code);
	}
	bindIndex = 0;
	while (true)
	{
		std::string s = mf.get("analog", "bind" + std::to_string(bindIndex++), "");
		if (s.empty())
			break;
		size_t colon = s.find(':');
		if (colon == std::string::npos || colon < 2)
		{
			WARN_LOG(INPUT, "Invalid bind entry: %s", s.c_str());
			break;
		}
		bool positive = s[colon - 1] == '+';
		u32 code = atoi(s.substr(0, colon - 1).c_str());
		std::string key = s.substr(colon + 1);
		if (key.empty())
		{
			WARN_LOG(INPUT, "Invalid bind entry: %s", s.c_str());
			break;
		}
		int port = 0;
		if (key[key.size() - 1] >= '1' && key[key.size() - 1] <= '3')
		{
			port = key[key.size() - 1] - '0';
			key = key.substr(0, key.size() - 1);
		}
		DreamcastKey id = getKeyId(key);
		set_axis(port, id, code, positive);
	}
	bindIndex = 0;
	if (version >= 4)
	{
		while (true)
		{
			std::string s = mf.get("combo", "bind" + std::to_string(bindIndex++), "");
			size_t colon = s.find(':');
			if (colon == std::string::npos || colon < 2)
			{
				WARN_LOG(INPUT, "Invalid bind entry: %s", s.c_str());
				break;
			}
			std::string codeStr = s.substr(0, colon);
			InputSet combo;
			size_t start = 0, end = 0;
			while ((end = codeStr.find(',', start)) != std::string::npos)
			{
				combo.insert_back(InputDef::from_str(codeStr.substr(start, end - start)));
				start = end + 1;
			}
			combo.insert_back(InputDef::from_str(codeStr.substr(start)));
			bool allValid = true;
			for (const InputDef& inputDef : combo)
			{
				if (!inputDef.is_valid())
				{
					allValid = false;
					break;
				}
			}
			if (combo.empty() || !allValid)
			{
				WARN_LOG(INPUT, "Invalid bind entry: %s", s.c_str());
				break;
			}
			std::string key = s.substr(colon + 1);
			if (key.empty())
			{
				WARN_LOG(INPUT, "Invalid bind entry: %s", s.c_str());
				break;
			}
			int port = 0;
			if (key[key.size() - 1] >= '1' && key[key.size() - 1] <= '3')
			{
				port = key[key.size() - 1] - '0';
				key = key.substr(0, key.size() - 1);
			}
			DreamcastKey id = getKeyId(key);
			set_combo(port, id, combo);
		}
	}
	dirty = false;
}

void InputMapping::loadv1(ConfigFile& mf)
{
	for (int port = 0; port < 4; port++)
	{
		for (u32 i = 0; i < std::size(button_list); i++)
		{
			std::string option;
			if (port == 0)
				option = button_list[i].option;
			else
				option = button_list[i].option + std::to_string(port);
			int button_code = mf.get_int(button_list[i].section, option, -1);
			if (button_code >= 0)
			{
				DreamcastKey id = button_list[i].id;
				// remap service and test buttons to their new aliases
				if (id == DC_BTN_C)
					id = DC_DPAD2_UP;
				else if (id == DC_BTN_Z)
					id = DC_DPAD2_DOWN;
				this->set_button(port, id, button_code);
			}
		}

		for (u32 i = 0; i < std::size(axis_list); i++)
		{
			std::string option;
			if (port == 0)
				option = axis_list[i].option;
			else
				option = axis_list[i].option + std::to_string(port);
			int axis_code = mf.get_int(axis_list[i].section, option, -1);
			if (axis_code >= 0)
			{
				if (port == 0)
					option = axis_list[i].option_inverted;
				else
					option = axis_list[i].option_inverted + std::to_string(port);
				bool inverted = mf.get_bool(axis_list[i].section_inverted, option, false);

				this->set_axis(port, axis_list[i].id, axis_code, !inverted);

				if (axis_list[i].id == DC_AXIS_RIGHT)
					this->set_axis(port, DC_AXIS_LEFT, axis_code, inverted);
				else if (axis_list[i].id == DC_AXIS_DOWN)
					this->set_axis(port, DC_AXIS_UP, axis_code, inverted);
				else if (axis_list[i].id == DC_AXIS2_RIGHT)
					this->set_axis(port, DC_AXIS2_LEFT, axis_code, inverted);
				else if (axis_list[i].id == DC_AXIS2_DOWN)
					this->set_axis(port, DC_AXIS2_UP, axis_code, inverted);
				else if (axis_list[i].id == DC_AXIS3_RIGHT)
					this->set_axis(port, DC_AXIS3_LEFT, axis_code, inverted);
				else if (axis_list[i].id == DC_AXIS3_DOWN)
					this->set_axis(port, DC_AXIS3_UP, axis_code, inverted);
			}
		}
	}
	dirty = true;
}

u32 InputMapping::get_button_code(u32 port, DreamcastKey key)
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return InputDef::INVALID_CODE;
	}

	const std::map<DreamcastKey, InputSet>& inputMap = multiEmuButtonMap[port];
	std::map<DreamcastKey, InputSet>::const_iterator iter = inputMap.find(key);
	if (iter != inputMap.end() && iter->second.size() == 1 && iter->second.begin()->is_button())
	{
		return iter->second.begin()->code;
	}
	return InputDef::INVALID_CODE;
}

std::pair<u32, bool> InputMapping::get_axis_code(u32 port, DreamcastKey key)
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return std::make_pair(InputDef::INVALID_CODE, false);
	}

	const std::map<DreamcastKey, InputSet>& inputMap = multiEmuButtonMap[port];
	std::map<DreamcastKey, InputSet>::const_iterator iter = inputMap.find(key);
	if (iter != inputMap.end() && iter->second.size() == 1 && iter->second.begin()->is_axis())
	{
		return std::make_pair(iter->second.begin()->code, (iter->second.begin()->type == InputDef::InputType::AXIS_POS));
	}
	return std::make_pair(InputDef::INVALID_CODE, false);
}

std::list<DreamcastKey> InputMapping::get_combo_ids(
	u32 port,
	const InputMapping::InputSet& inputSet) const
{
	if (port >= NUM_PORTS || inputSet.empty())
	{
		// Invalid port or no input
		return std::list<DreamcastKey>();
	}

	std::list<DreamcastKey> matchedKeys;

	const std::map<DreamcastKey, InputSet>& inputMap = multiEmuButtonMap[port];
	const std::multimap<InputDef, DreamcastKey>& revInputMap = reverseMultiEmuButtonMap[port];
	auto range = revInputMap.equal_range(inputSet.back());
	for (auto it = range.first; it != range.second; ++it)
	{
		const DreamcastKey& key = it->second;
		const InputMapping::InputSet& thisItemSet = inputMap.find(key)->second;
		if (inputSet.ends_with(thisItemSet))
		{
			if (thisItemSet.size() == 1)
			{
				// Single key takes precidence
				matchedKeys.push_front(key);
			}
			else
			{
				matchedKeys.push_back(key);
			}
		}
	}

	return matchedKeys;
}

void InputMapping::clear_combo(u32 port, DreamcastKey id)
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return;
	}

	std::map<DreamcastKey, InputSet>& inputMap = multiEmuButtonMap[port];
	std::multimap<InputDef, DreamcastKey>& revInputMap = reverseMultiEmuButtonMap[port];

	// Remove the existing
	std::map<DreamcastKey, InputSet>::const_iterator existing = inputMap.find(id);
	if (existing != inputMap.end())
	{
		const DreamcastKey& existingKey = existing->first;
		for (const InputDef& existingInput : existing->second)
		{
			auto range = revInputMap.equal_range(existingInput);
			for (auto it = range.first; it != range.second; ++it)
			{
				if (it->second == existingKey)
				{
					revInputMap.erase(it);
					break;
				}
			}
		}
	}

	inputMap.erase(id);
}

bool InputMapping::set_combo(u32 port, DreamcastKey id, const InputMapping::InputSet& combo)
{
	if (port >= NUM_PORTS || combo.size() <= 0)
	{
		// Invalid port or combo
		return false;
	}

	std::map<DreamcastKey, InputSet>& inputMap = multiEmuButtonMap[port];
	std::multimap<InputDef, DreamcastKey>& revInputMap = reverseMultiEmuButtonMap[port];

	// Remove the existing
	clear_combo(port, id);

	inputMap[id] = combo;
	for (const InputDef& input : combo)
	{
		revInputMap.insert(std::make_pair(input, id));
	}

	dirty = true;

	return true;
}

InputMapping::InputSet InputMapping::get_combo_codes(u32 port, DreamcastKey key)
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return InputSet();
	}

	const std::map<DreamcastKey, InputSet>& inputMap = multiEmuButtonMap[port];
	std::map<DreamcastKey, InputSet>::const_iterator iter = inputMap.find(key);

	if (iter != inputMap.end())
	{
		return iter->second;
	}

	// Not found
	return InputSet();
}

void InputMapping::ClearMappings()
{
	loaded_mappings.clear();
}

std::shared_ptr<InputMapping> InputMapping::LoadMapping(const std::string& name)
{
	auto it = loaded_mappings.find(name);
	if (it != loaded_mappings.end())
		return it->second;

	std::string path = get_readonly_config_path(std::string("mappings/") + name);
	FILE *fp = nowide::fopen(path.c_str(), "r");
	if (fp == NULL)
		return NULL;
	std::shared_ptr<InputMapping> mapping = std::make_shared<InputMapping>();
	mapping->load(fp);
	std::fclose(fp);
	loaded_mappings[name] = mapping;

	if (mapping->is_dirty())
	{
		// Make a backup of the current mapping file
		FILE *out = nowide::fopen((path + ".save").c_str(), "w");
		if (out == nullptr)
			WARN_LOG(INPUT, "Can't backup controller mapping file %s", path.c_str());
		else
		{
			fp = nowide::fopen(path.c_str(), "r");
			if (fp != nullptr)
			{
				u8 buf[4096];
				while (true)
				{
					size_t n = fread(buf, 1, sizeof(buf), fp);
					if (n <= 0)
						break;
					fwrite(buf, 1, n, out);
				}
				std::fclose(fp);
			}
			std::fclose(out);
		}
	}

	return mapping;
}

void InputMapping::set_dirty()
{
	dirty = true;
}

static const char *getKeyName(DreamcastKey key)
{
	for (u32 i = 0; i < std::size(button_list); i++)
		if (key == button_list[i].id)
			return button_list[i].option.c_str();
	for (u32 i = 0; i < std::size(axis_list); i++)
		if (key == axis_list[i].id)
			return axis_list[i].option.c_str();
	ERROR_LOG(INPUT, "Invalid key %x", key);

	return nullptr;
}

bool InputMapping::save(const std::string& name)
{
	if (!dirty)
		return true;

	std::string path = get_writable_config_path("mappings/");
	make_directory(path);
	path = get_writable_config_path(std::string("mappings/") + name);
	FILE *fp = nowide::fopen(path.c_str(), "w");
	if (fp == NULL)
	{
		WARN_LOG(INPUT, "Cannot save controller mappings into %s", path.c_str());
		return false;
	}
	ConfigFile mf;

	mf.set("emulator", "mapping_name", this->name);
	mf.set_int("emulator", "dead_zone", (int)std::round(this->dead_zone * 100.f));
	mf.set_int("emulator", "saturation", (int)std::round(this->saturation * 100.f));
	mf.set_int("emulator", "rumble_power", this->rumblePower);
	mf.set_int("emulator", "version", CURRENT_FILE_VERSION);

	int bindIndex = 0;
	for (int port = 0; port < 4; port++)
	{
		for (const auto& pair : multiEmuButtonMap[port])
		{
			if (pair.second.size() != 1 || !pair.second.begin()->is_button())
				continue;
			if (pair.first == EMU_BTN_NONE)
				continue;
			const char *keyName = getKeyName(pair.first);
			if (keyName == nullptr)
				continue;
			std::string option;
			if (port == 0)
				option = keyName;
			else
				option = keyName + std::to_string(port);
			mf.set("digital", "bind" + std::to_string(bindIndex), pair.second.begin()->to_str() + ":" + option);
			bindIndex++;
		}
	}
	bindIndex = 0;
	for (int port = 0; port < 4; port++)
	{
		for (const auto& pair : multiEmuButtonMap[port])
		{
			if (pair.second.size() != 1 || !pair.second.begin()->is_axis())
				continue;
			if (pair.first == EMU_BTN_NONE)
				continue;
			const char *keyName = getKeyName(pair.first);
			if (keyName == nullptr)
				continue;
			std::string option;
			if (port == 0)
				option = keyName;
			else
				option = keyName + std::to_string(port);
			mf.set("analog", "bind" + std::to_string(bindIndex), pair.second.begin()->to_str() + ":" + option);
			bindIndex++;
		}
	}
	bindIndex = 0;
	for (int port = 0; port < 4; port++)
	{
		for (const auto& pair : multiEmuButtonMap[port])
		{
			if (pair.second.size() == 1)
				continue; // Already covered above
			if (pair.first == EMU_BTN_NONE)
				continue;
			const char *keyName = getKeyName(pair.first);
			if (keyName == nullptr)
				continue;
			std::string option;
			if (port == 0)
				option = keyName;
			else
				option = keyName + std::to_string(port);
			std::string codes;
			for (const InputDef& inputDef : pair.second)
			{
				if (!codes.empty())
				{
					codes += ",";
				}
				codes += inputDef.to_str();
			}
			mf.set("combo", "bind" + std::to_string(bindIndex), codes + ":" + option);
			bindIndex++;
		}
	}
	mf.save(fp);
	dirty = false;
	std::fclose(fp);

	return true;
}

void InputMapping::SaveMapping(const std::string& name, const std::shared_ptr<InputMapping>& mapping)
{
	mapping->save(name);
	InputMapping::loaded_mappings[name] = mapping;
}

void InputMapping::DeleteMapping(const std::string& name)
{
	loaded_mappings.erase(name);
	std::remove(get_writable_config_path(std::string("mappings/") + name).c_str());
}
