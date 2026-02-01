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
#include "cfg/option.h"
#include "oslib/storage.h"

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
	{ EMU_BTN_NEXTSLOT, "emulator", "btn_next_slot" },
	{ EMU_BTN_PREVSLOT, "emulator", "btn_prev_slot" },
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

//
// InputDef
//

u64 InputMapping::InputDef::get_hash() const
{
	return (static_cast<u64>(type) << 32) | code;
}

bool InputMapping::InputDef::operator<(const InputDef& rhs) const
{
	return (get_hash() < rhs.get_hash());
}

bool InputMapping::InputDef::operator==(const InputDef& rhs) const
{
	return (get_hash() == rhs.get_hash());
}

bool InputMapping::InputDef::operator!=(const InputDef& rhs) const
{
	return !(*this == rhs);
}

bool InputMapping::InputDef::is_button() const
{
	return (type == InputType::BUTTON);
}

bool InputMapping::InputDef::is_axis() const
{
	return (type == InputType::AXIS_POS || type == InputType::AXIS_NEG);
}

bool InputMapping::InputDef::is_valid() const
{
	return (code != INVALID_CODE && (is_button() || is_axis()));
}

const char* InputMapping::InputDef::get_suffix() const
{
	if (type == InputType::AXIS_POS)
	{
		return "+";
	}
	else if (type == InputType::AXIS_NEG)
	{
		return "-";
	}

	// Assume button otherwise
	return "";
}

std::string InputMapping::InputDef::to_str() const
{
	return std::to_string(code) + std::string(get_suffix());
}

InputMapping::InputDef InputMapping::InputDef::from_str(const std::string& str)
{
	InputDef inputDef;
	if (!str.empty())
	{
		std::string tmp = str;
		if (tmp.back() == '+')
		{
			inputDef.type = InputType::AXIS_POS;
			tmp.erase(tmp.size() - 1);
		}
		else if (tmp.back() == '-')
		{
			inputDef.type = InputType::AXIS_NEG;
			tmp.erase(tmp.size() - 1);
		}
		else
		{
			inputDef.type = InputType::BUTTON;
		}

		try
		{
			inputDef.code = std::stoul(tmp);
		}
		catch (const std::exception&)
		{
			inputDef.code = INVALID_CODE;
		}
	}
	return inputDef;
}

InputMapping::InputDef InputMapping::InputDef::from_button(u32 code)
{
	return InputDef{code, InputType::BUTTON};
}

InputMapping::InputDef InputMapping::InputDef::from_axis(u32 code, bool positive)
{
	return InputDef{code, positive ? InputType::AXIS_POS : InputType::AXIS_NEG};
}

//
// InputSet
//

InputMapping::InputSet::InputSet() : std::list<InputDef>() {}

InputMapping::InputSet::InputSet(std::initializer_list<value_type> l) : std::list<InputDef>()
{
	// Do insert_back of each to ensure uniqueness
	for (const auto& x : l)
	{
		insert_back(x);
	}
}

bool InputMapping::InputSet::insert_back(const InputMapping::InputDef& val)
{
	remove_inverse_axis(val);
	const_iterator iter = std::find(cbegin(), cend(), val);
	if (iter == cend())
	{
		push_back(val);
		return true;
	}
	return false;
}

bool InputMapping::InputSet::insert_back(InputMapping::InputDef&& val)
{
	remove_inverse_axis(val);
	const_iterator iter = std::find(cbegin(), cend(), val);
	if (iter == cend())
	{
		push_back(std::move(val));
		return true;
	}
	return false;
}

std::size_t InputMapping::InputSet::remove(const InputMapping::InputDef& val)
{
	std::size_t removedCount = 0;
	const_iterator iter;
	while ((iter = std::find(cbegin(), cend(), val)) != cend())
	{
		erase(iter);
		++removedCount;
	}
	return removedCount;
}

bool InputMapping::InputSet::contains(const InputMapping::InputDef& val) const
{
	return (std::find(cbegin(), cend(), val) != cend());
}

bool InputMapping::InputSet::ends_with(const InputSet& rhs, bool sequential) const
{
	InputSet::const_reverse_iterator iter = crbegin();

	if (sequential)
	{
		InputSet::const_reverse_iterator riter = rhs.crbegin();

		for (; iter != crend() && riter != rhs.crend(); ++iter, ++riter)
		{
			if (*iter != *riter)
			{
				break;
			}
		}

		return (riter == rhs.crend());
	}
	else
	{
		std::size_t foundCount = 0;
		for (; iter != crend(); ++iter)
		{
			if (std::find(rhs.begin(), rhs.end(), *iter) != rhs.end())
			{
				++foundCount;
			}
			else
			{
				break;
			}
		}

		return (foundCount == rhs.size());
	}
}

void InputMapping::InputSet::remove_inverse_axis(const InputMapping::InputDef& val)
{
	if (val.is_axis())
	{
		InputMapping::InputDef::InputType inversetype(
			val.type == InputMapping::InputDef::InputType::AXIS_POS
			? InputMapping::InputDef::InputType::AXIS_NEG
			: InputMapping::InputDef::InputType::AXIS_POS);
		remove(InputMapping::InputDef{val.code, inversetype});
	}
}

//
// ButtonCombo
//

bool InputMapping::ButtonCombo::operator<(const ButtonCombo& rhs) const
{

	return inputs < rhs.inputs;
}

bool InputMapping::ButtonCombo::operator==(const ButtonCombo& rhs) const
{
	return (inputs == rhs.inputs && sequential == rhs.sequential);
}

bool InputMapping::ButtonCombo::operator!=(const ButtonCombo& rhs) const
{
	return !(*this == rhs);
}

bool InputMapping::ButtonCombo::intersects(const ButtonCombo& rhs) const
{
	if (!sequential || !rhs.sequential)
	{
		// Return true iff both sets contain the same elements
		return (inputs.size() == rhs.inputs.size() && inputs.ends_with(rhs.inputs, false));
	}
	else
	{
		// Return true iff exactly matching
		return *this == rhs;
	}
}

//
// InputMapping
//

std::map<std::string, std::shared_ptr<InputMapping>> InputMapping::loaded_mappings;

void InputMapping::clear_axis(u32 port, DreamcastKey id)
{
	if (port >= NUM_PORTS || id == EMU_AXIS_NONE)
	{
		// Invalid port or ID not set
		return;
	}

	while (true)
	{
		std::pair<u32, bool> code = get_axis_code(port, id);
		if (code.first == (u32)-1)
			break;
		axes[port][code] = EMU_AXIS_NONE;
		dirty = true;
	}
}

void InputMapping::set_axis(u32 port, DreamcastKey id, u32 code, bool positive)
{
	if (id != EMU_AXIS_NONE)
	{
		clear_axis(port, id);
		axes[port][std::make_pair(code, positive)] = id;
		dirty = true;
	}
}

bool InputMapping::isTrigger(u32 code) const {
	return triggers.find(code) != triggers.end();
}
bool InputMapping::isReverseTrigger(u32 code) const
{
	auto it = triggers.find(code);
	if (it == triggers.end())
		return false;
	return it->second;
}
void InputMapping::addTrigger(u32 code, bool reverse)
{
	if (!isTrigger(code) || isReverseTrigger(code) != reverse) {
		triggers[code] = reverse;
		dirty = true;
	}
}
void InputMapping::deleteTrigger(u32 code)
{
	if (isTrigger(code)) {
		triggers.erase(code);
		dirty = true;
	}
}

using namespace config;

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
	IniFile mf;
	mf.load(fp);

	this->name = mf.get("emulator", "mapping_name", "<Unknown>");

	int dz = mf.getInt("emulator", "dead_zone", 10);
	dz = std::min(dz, 100);
	dz = std::max(dz, 0);
	this->dead_zone = (float)dz / 100.f;
	int sat = std::clamp(mf.getInt("emulator", "saturation", 100), 50, 200);
	this->saturation = (float)sat / 100.f;
	this->rumblePower = mf.getInt("emulator", "rumble_power", this->rumblePower);

	version = mf.getInt("emulator", "version", 1);
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
			if (s.empty())
				break;
			// Parse for 1 more colon than what is needed for forward compatibility
			// (third colon and everything after it is ignored)
			std::vector<std::string> parts = strSplit(s, ':', 3);
			if (parts.size() < 2)
			{
				WARN_LOG(INPUT, "Invalid bind entry: %s", s.c_str());
				break;
			}
			const std::string& codeStr = parts[0];
			InputSet inputs;
			bool allValid = true;
			for (const std::string& inputStr : strSplit(codeStr, ','))
			{
				InputDef inputDef = InputDef::from_str(inputStr);
				if (!inputDef.is_valid())
				{
					allValid = false;
					break;
				}
				inputs.insert_back(inputDef);
			}
			std::string key = parts[1];
			if (inputs.empty() || !allValid || key.empty())
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
			bool sequential = true;
			if (parts.size() > 2)
			{
				const std::string& seqStr = parts[2];
				try
				{
					sequential = (std::stoul(seqStr) != 0);
				}
				catch (const std::exception&)
				{
					sequential = true;
				}
			}
			set_button(port, id, ButtonCombo{inputs, sequential});
		}
	}
	std::string triggers = mf.get("emulator", "triggers", "");
	const char *p = strtok(triggers.data(), ",");
	while (p != nullptr) {
		addTrigger(atoi(p), strchr(p, '~') != nullptr);
		p = strtok(nullptr, ",");
	}
	dirty = false;
}

void InputMapping::loadv1(IniFile& mf)
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
			int button_code = mf.getInt(button_list[i].section, option, -1);
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
			int axis_code = mf.getInt(axis_list[i].section, option, -1);
			if (axis_code >= 0)
			{
				if (port == 0)
					option = axis_list[i].option_inverted;
				else
					option = axis_list[i].option_inverted + std::to_string(port);
				bool inverted = mf.getBool(axis_list[i].section_inverted, option, false);

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

std::pair<u32, bool> InputMapping::get_axis_code(u32 port, DreamcastKey key)
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return std::make_pair(InputDef::INVALID_CODE, false);
	}

	for (auto& it : axes[port])
	{
		if (it.second == key)
			return it.first;
	}

	return std::make_pair(InputDef::INVALID_CODE, false);
}

DreamcastKey InputMapping::get_button_id(
	u32 port,
	const InputMapping::InputSet& inputSet) const
{
	if (port >= NUM_PORTS || inputSet.empty())
	{
		// Invalid port or no input
		return EMU_BTN_NONE;
	}

	DreamcastKey matchedKey = EMU_BTN_NONE;
	size_t matchedInputSize = 0;

	const std::map<ButtonCombo, DreamcastKey>& inputMap = buttonMap[port];
	for (const auto& it : inputMap)
	{
		const DreamcastKey& key = it.second;
		if (key != EMU_BTN_NONE)
		{
			const InputMapping::ButtonCombo& combo = it.first;
			if (inputSet.ends_with(combo.inputs, combo.sequential))
			{
				if (combo.inputs.size() > matchedInputSize)
				{
					matchedInputSize = combo.inputs.size();
					matchedKey = key;
				}
			}
		}
	}

	return matchedKey;
}

std::list<DreamcastKey> InputMapping::get_button_released_ids(
	u32 port,
	const std::list<DreamcastKey>& activeKeys,
	const InputDef& releasedInput) const
{
	if (port >= NUM_PORTS || activeKeys.empty())
	{
		// Invalid port or nothing active
		return std::list<DreamcastKey>();
	}

	std::list<DreamcastKey> releasedKeys;
	const std::multimap<DreamcastKey, ButtonCombo>& inputMap = reverseButtonMap[port];
	for (const DreamcastKey& key : activeKeys)
	{
		const auto& range = inputMap.equal_range(key);
		for(auto iter = range.first; iter != range.second; ++iter)
		{
			if (iter->second.inputs.contains(releasedInput))
			{
				releasedKeys.push_back(key);
			}
		}
	}

	return releasedKeys;
}

void InputMapping::clear_button(u32 port, DreamcastKey id)
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return;
	}

	std::map<ButtonCombo, DreamcastKey>& inputMap = buttonMap[port];
	std::multimap<DreamcastKey, ButtonCombo>& revInputMap = reverseButtonMap[port];

	// Remove the existing
	const auto& range = revInputMap.equal_range(id);
	for(auto existing = range.first; existing != range.second;)
	{
		inputMap.erase(existing->second);
		existing = revInputMap.erase(existing);
	}
}

bool InputMapping::set_button(u32 port, DreamcastKey id, const InputMapping::ButtonCombo& combo)
{
	if (port >= NUM_PORTS || combo.inputs.size() <= 0)
	{
		// Invalid port or combo
		return false;
	}

	std::map<ButtonCombo, DreamcastKey>& inputMap = buttonMap[port];
	std::multimap<DreamcastKey, ButtonCombo>& revInputMap = reverseButtonMap[port];

	// Remove the existing by ID
	clear_button(port, id);

	// Remove all combos that intersect this
	for (std::map<ButtonCombo, DreamcastKey>::const_iterator iter = inputMap.begin(); iter != inputMap.end();)
	{
		if (combo.intersects(iter->first))
		{
			revInputMap.erase(iter->second);
			iter = inputMap.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	inputMap[combo] = id;
	revInputMap.insert(std::make_pair(id, combo));

	dirty = true;

	return true;
}

InputMapping::ButtonCombo InputMapping::get_button_combo(u32 port, DreamcastKey key) const
{
	if (port >= NUM_PORTS)
	{
		// Invalid port
		return ButtonCombo();
	}

	const std::multimap<DreamcastKey, ButtonCombo>& inputMap = reverseButtonMap[port];
	std::map<DreamcastKey, ButtonCombo>::const_iterator iter = inputMap.find(key);

	if (iter != inputMap.end())
	{
		return iter->second;
	}

	// Not found
	return ButtonCombo();
}

u32 InputMapping::get_button_code(u32 port, DreamcastKey key) const
{
	if (port >= NUM_PORTS || key == DreamcastKey::EMU_BTN_NONE)
	{
		// Invalid port or key
		return InputDef::INVALID_CODE;
	}

	const std::multimap<DreamcastKey, ButtonCombo>& inputMap = reverseButtonMap[port];
	const auto& range = inputMap.equal_range(key);
	for(auto iter = range.first; iter != range.second; ++iter)
	{
		if (iter->second.inputs.size() == 1 && iter->second.inputs.begin()->is_button())
		{
			return iter->second.inputs.begin()->code;
		}
	}
	return InputDef::INVALID_CODE;
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

	std::string path;
	FILE *fp = nullptr;
	// Try user-defined mapping folders first
	for (const auto& base : config::MappingsPath.get())
	{
		if (base.empty())
			continue;
		try
		{
			std::string candidate = hostfs::storage().getSubPath(base, name);
			fp = hostfs::storage().openFile(candidate, "r");
			if (fp != nullptr)
			{
				path = candidate;
				break;
			}
		}
		catch (const hostfs::StorageException&)
		{
		}
	}
	// Fall back to default config path
	if (fp == nullptr)
	{
		path = get_readonly_config_path(std::string("mappings/") + name);
		fp = nowide::fopen(path.c_str(), "r");
		if (fp == NULL)
			return NULL;
	}
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

	std::string path;
	// Prefer first user-defined mapping path for writes
	if (!config::MappingsPath.get().empty() && !config::MappingsPath.get()[0].empty())
	{
		std::string base = config::MappingsPath.get()[0];
		make_directory(base);
		path = hostfs::storage().getSubPath(base, name);
	}
	else
	{
		std::string base = get_writable_config_path("mappings/");
		make_directory(base);
		path = get_writable_config_path(std::string("mappings/") + name);
	}
	FILE *fp = nowide::fopen(path.c_str(), "w");
	if (fp == NULL)
	{
		WARN_LOG(INPUT, "Cannot save controller mappings into %s", path.c_str());
		return false;
	}
	IniFile mf;

	mf.set("emulator", "mapping_name", this->name);
	mf.set("emulator", "dead_zone", (int)std::round(this->dead_zone * 100.f));
	mf.set("emulator", "saturation", (int)std::round(this->saturation * 100.f));
	mf.set("emulator", "rumble_power", this->rumblePower);
	mf.set("emulator", "version", CURRENT_FILE_VERSION);

	int bindIndex = 0;
	for (int port = 0; port < 4; port++)
	{
		for (const auto& pair : buttonMap[port])
		{
			if (pair.first.inputs.size() != 1 || pair.second == EMU_BTN_NONE)
				continue;
			const char *keyName = getKeyName(pair.second);
			if (keyName == nullptr)
				continue;
			std::string option;
			if (port == 0)
				option = keyName;
			else
				option = keyName + std::to_string(port);
			mf.set("digital", "bind" + std::to_string(bindIndex), pair.first.inputs.begin()->to_str() + ":" + option);
			bindIndex++;
		}
	}
	bindIndex = 0;
	for (int port = 0; port < 4; port++)
	{
		for (const auto& pair : axes[port])
		{
			if (pair.second == EMU_BTN_NONE)
				continue;
			const char *keyName = getKeyName(pair.second);
			if (keyName == nullptr)
				continue;
			std::string option;
			if (port == 0)
				option = keyName;
			else
				option = keyName + std::to_string(port);
			mf.set("analog", "bind" + std::to_string(bindIndex),
				std::to_string(pair.first.first) + (pair.first.second ? "+" : "-") + ":" + option);
			bindIndex++;
		}
	}
	bindIndex = 0;
	for (int port = 0; port < 4; port++)
	{
		for (const auto& pair : buttonMap[port])
		{
			if (pair.first.inputs.size() == 1)
				continue; // Already covered by digital bind
			if (pair.second == EMU_BTN_NONE)
				continue;
			const char *keyName = getKeyName(pair.second);
			if (keyName == nullptr)
				continue;
			std::string option;
			if (port == 0)
				option = keyName;
			else
				option = keyName + std::to_string(port);
			std::string codes;
			for (const InputDef& inputDef : pair.first.inputs)
			{
				if (!codes.empty())
				{
					codes += ",";
				}
				codes += inputDef.to_str();
			}
			mf.set(
				"combo",
				"bind" + std::to_string(bindIndex),
				codes + ":" + option + ":" + std::to_string(pair.first.sequential ? 1 : 0));
			bindIndex++;
		}
	}
	std::string triggerString;
	for (const auto& pair : triggers)
	{
		if (!triggerString.empty())
			triggerString += ',';
		triggerString += std::to_string(pair.first);
		if (pair.second)
			triggerString += '~';
	}
	mf.set("emulator", "triggers", triggerString);
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
	// Try delete from user-defined mapping path first
	if (!config::MappingsPath.get().empty() && !config::MappingsPath.get()[0].empty())
	{
		try
		{
			std::string candidate = hostfs::storage().getSubPath(config::MappingsPath.get()[0], name);
			nowide::remove(candidate.c_str());
		}
		catch (const hostfs::StorageException&)
		{
		}
	}
	nowide::remove(get_writable_config_path(std::string("mappings/") + name).c_str());
}

std::vector<std::string> InputMapping::strSplit(const std::string str, char c, size_t maxsplit)
{
	std::vector<std::string> out;
	if (maxsplit > 0)
	{
		out.reserve(maxsplit + 1);
	}

	size_t pos = 0;
	size_t count = 0;
	while (maxsplit == 0 || count < maxsplit)
	{
		size_t nextPos = str.find(c, pos);
		if (nextPos == std::string::npos)
		{
			out.push_back(str.substr(pos));
			return out;
		}

		out.push_back(str.substr(pos, nextPos - pos));
		pos = nextPos + 1;
		++count;
	}

	out.push_back(str.substr(pos));

	return out;
}
