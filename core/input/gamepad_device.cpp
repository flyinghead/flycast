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

#include "gamepad_device.h"
#include "cfg/cfg.h"
#include "oslib/oslib.h"
#include "rend/gui.h"
#include "emulator.h"
#include "hw/maple/maple_devs.h"
#include "stdclass.h"

#include <algorithm>
#include <climits>
#include <fstream>

#define MAPLE_PORT_CFG_PREFIX "maple_"

// Gamepads
u32 kcode[4] = { ~0u, ~0u, ~0u, ~0u };
s8 joyx[4];
s8 joyy[4];
s8 joyrx[4];
s8 joyry[4];
u8 rt[4];
u8 lt[4];
// Keyboards
u8 kb_shift[MAPLE_PORTS];	// shift keys pressed (bitmask)
u8 kb_key[MAPLE_PORTS][6];	// normal keys pressed

std::vector<std::shared_ptr<GamepadDevice>> GamepadDevice::_gamepads;
std::mutex GamepadDevice::_gamepads_mutex;

#ifdef TEST_AUTOMATION
#include "hw/sh4/sh4_sched.h"
static FILE *record_input;
#endif

bool GamepadDevice::handleButtonInput(int port, DreamcastKey key, bool pressed)
{
	if (key == EMU_BTN_NONE)
		return false;

	if (key <= DC_BTN_RELOAD)
	{
		if (port >= 0)
		{
			if (pressed)
				kcode[port] &= ~key;
			else
				kcode[port] |= key;
		}
#ifdef TEST_AUTOMATION
		if (record_input != NULL)
			fprintf(record_input, "%ld button %x %04x\n", sh4_sched_now64(), port, kcode[port]);
#endif
	}
	else
	{
		switch (key)
		{
		case EMU_BTN_ESCAPE:
			if (pressed)
				dc_exit();
			break;
		case EMU_BTN_MENU:
			if (pressed)
				gui_open_settings();
			break;
		case EMU_BTN_FFORWARD:
			if (pressed && !gui_is_open())
				settings.input.fastForwardMode = !settings.input.fastForwardMode && !settings.network.online;
			break;
		case DC_AXIS_LT:
			if (port >= 0)
				lt[port] = pressed ? 255 : 0;
			break;
		case DC_AXIS_RT:
			if (port >= 0)
				rt[port] = pressed ? 255 : 0;
			break;

		case DC_AXIS_UP:
		case DC_AXIS_DOWN:
			buttonToAnalogInput<DC_AXIS_UP, DIGANA_UP, DIGANA_DOWN>(port, key, pressed, joyy[port]);
			break;
		case DC_AXIS_LEFT:
		case DC_AXIS_RIGHT:
			buttonToAnalogInput<DC_AXIS_LEFT, DIGANA_LEFT, DIGANA_RIGHT>(port, key, pressed, joyx[port]);
			break;
		case DC_AXIS2_UP:
		case DC_AXIS2_DOWN:
			buttonToAnalogInput<DC_AXIS2_UP, DIGANA2_UP, DIGANA2_DOWN>(port, key, pressed, joyry[port]);
			break;
		case DC_AXIS2_LEFT:
		case DC_AXIS2_RIGHT:
			buttonToAnalogInput<DC_AXIS2_LEFT, DIGANA2_LEFT, DIGANA2_RIGHT>(port, key, pressed, joyrx[port]);
			break;

		default:
			return false;
		}
	}
	DEBUG_LOG(INPUT, "%d: BUTTON %s %d. kcode=%x", port, pressed ? "down" : "up", key, port >= 0 ? kcode[port] : 0);

	return true;
}

bool GamepadDevice::gamepad_btn_input(u32 code, bool pressed)
{
	if (_input_detected != nullptr && _detecting_button
			&& os_GetSeconds() >= _detection_start_time && pressed)
	{
		_input_detected(code, false, false);
		_input_detected = nullptr;
		return true;
	}
	if (!input_mapper || _maple_port > (int)ARRAY_SIZE(kcode))
		return false;

	bool rc = false;
	if (_maple_port == 4)
	{
		for (int port = 0; port < 4; port++)
		{
			DreamcastKey key = input_mapper->get_button_id(port, code);
			rc = handleButtonInput(port, key, pressed) || rc;
		}
	}
	else
	{
		DreamcastKey key = input_mapper->get_button_id(0, code);
		rc = handleButtonInput(_maple_port, key, pressed);
	}

	return rc;
}

//
// value must be >= -32768 and <= 32767 for full axes
// and 0 to 32767 for half axes/triggers
//
bool GamepadDevice::gamepad_axis_input(u32 code, int value)
{
	bool positive = value >= 0;
	if (_input_detected != NULL && _detecting_axis
			&& os_GetSeconds() >= _detection_start_time && std::abs(value) >= 16384)
	{
		_input_detected(code, true, positive);
		_input_detected = nullptr;
		return true;
	}
	if (!input_mapper || _maple_port < 0 || _maple_port > 4)
		return false;

	auto handle_axis = [&](u32 port, DreamcastKey key, int v)
	{
		if ((key & DC_BTN_GROUP_MASK) == DC_AXIS_TRIGGERS)	// Triggers
		{
			//printf("T-AXIS %d Mapped to %d -> %d\n", key, value, std::min(std::abs(v) >> 7, 255));
			if (key == DC_AXIS_LT)
				lt[port] = std::min(std::abs(v) >> 7, 255);
			else if (key == DC_AXIS_RT)
				rt[port] = std::min(std::abs(v) >> 7, 255);
			else
				return false;
		}
		else if ((key & DC_BTN_GROUP_MASK) == DC_AXIS_STICKS) // Analog axes
		{
			//printf("AXIS %d Mapped to %d -> %d\n", key, value, v);
			s8 *this_axis;
			s8 *other_axis;
			int axisDirection = -1;
			switch (key)
			{
			case DC_AXIS_RIGHT:
				axisDirection = 1;
				//no break
			case DC_AXIS_LEFT:
				this_axis = &joyx[port];
				other_axis = &joyy[port];
				break;

			case DC_AXIS_DOWN:
				axisDirection = 1;
				//no break
			case DC_AXIS_UP:
				this_axis = &joyy[port];
				other_axis = &joyx[port];
				break;

			case DC_AXIS2_RIGHT:
				axisDirection = 1;
				//no break
			case DC_AXIS2_LEFT:
				this_axis = &joyrx[port];
				other_axis = &joyry[port];
				break;

			case DC_AXIS2_DOWN:
				axisDirection = 1;
				//no break
			case DC_AXIS2_UP:
				this_axis = &joyry[port];
				other_axis = &joyrx[port];
				break;

			default:
				return false;
			}
			// Radial dead zone
			// FIXME compute both axes at the same time
			v = std::min(127, std::abs(v >> 8));
			if ((float)(v * v + *other_axis * *other_axis) < input_mapper->dead_zone * input_mapper->dead_zone * 128.f * 128.f)
			{
				*this_axis = 0;
				*other_axis = 0;
			}
			else
				*this_axis = v * axisDirection;
		}
		else if (key != EMU_BTN_NONE && key <= DC_BTN_RELOAD) // Map triggers to digital buttons
		{
			//printf("B-AXIS %d Mapped to %d -> %d\n", key, value, v);
			// TODO hysteresis?
			if (std::abs(v) < 16384)
				kcode[port] |=  key; // button released
			else
				kcode[port] &= ~key; // button pressed
		}
		else if ((key & DC_BTN_GROUP_MASK) == EMU_BUTTONS) // Map triggers to emu buttons
		{
			int lastValue = lastAxisValue[port][key];
			int newValue = std::abs(v);
			if ((lastValue < 16384 && newValue >= 16384) || (lastValue >= 16384 && newValue < 16384))
				handleButtonInput(port, key, newValue >= 16384);
			lastAxisValue[port][key] = newValue;
		}
		else
			return false;

		return true;
	};

	bool rc = false;
	if (_maple_port == 4)
	{
		for (u32 port = 0; port < 4; port++)
		{
			DreamcastKey key = input_mapper->get_axis_id(port, code, !positive);
			handle_axis(port, key, 0);
			key = input_mapper->get_axis_id(port, code, positive);
			rc = handle_axis(port, key, value) || rc;
		}
	}
	else
	{
		DreamcastKey key = input_mapper->get_axis_id(0, code, !positive);
		// Reset opposite axis to 0
		handle_axis(_maple_port, key, 0);
		key = input_mapper->get_axis_id(0, code, positive);
		rc = handle_axis(_maple_port, key, value);
	}

	return rc;
}

void GamepadDevice::load_system_mappings()
{
	for (int i = 0; i < GetGamepadCount(); i++)
	{
		std::shared_ptr<GamepadDevice> gamepad = GetGamepad(i);
		if (!gamepad->find_mapping())
			gamepad->resetMappingToDefault(settings.platform.system != DC_PLATFORM_DREAMCAST, true);
	}
}

std::string GamepadDevice::make_mapping_filename(bool instance, int system, bool perGame /* = false */)
{
	std::string mapping_file = api_name() + "_" + name();
	if (instance)
		mapping_file += "-" + _unique_id;
	if (perGame && !settings.content.gameId.empty())
		mapping_file += "_" + settings.content.gameId;
	if (system != DC_PLATFORM_DREAMCAST)
		mapping_file += "_arcade";
	std::replace(mapping_file.begin(), mapping_file.end(), '/', '-');
	std::replace(mapping_file.begin(), mapping_file.end(), '\\', '-');
	std::replace(mapping_file.begin(), mapping_file.end(), ':', '-');
	std::replace(mapping_file.begin(), mapping_file.end(), '?', '-');
	std::replace(mapping_file.begin(), mapping_file.end(), '*', '-');
	std::replace(mapping_file.begin(), mapping_file.end(), '|', '-');
	std::replace(mapping_file.begin(), mapping_file.end(), '"', '-');
	std::replace(mapping_file.begin(), mapping_file.end(), '<', '-');
	std::replace(mapping_file.begin(), mapping_file.end(), '>', '-');
	mapping_file += ".cfg";

	return mapping_file;
}

bool GamepadDevice::find_mapping(int system /* = settings.platform.system */)
{
	if (!_remappable)
		return true;
	instanceMapping = false;
	bool cloneMapping = false;
	while (true)
	{
		bool perGame = !settings.content.gameId.empty();
		while (true)
		{
			std::string mapping_file = make_mapping_filename(true, system, perGame);
			input_mapper = InputMapping::LoadMapping(mapping_file);
			if (!input_mapper)
			{
				mapping_file = make_mapping_filename(false, system, perGame);
				input_mapper = InputMapping::LoadMapping(mapping_file);
			}
			else
			{
				instanceMapping = true;
			}
			if (!!input_mapper)
			{
				if (cloneMapping)
					input_mapper = std::make_shared<InputMapping>(*input_mapper);
				perGameMapping = perGame;
				return true;
			}
			if (!perGame)
				break;
			perGame = false;
		}
		if (system == DC_PLATFORM_DREAMCAST)
			break;
		system = DC_PLATFORM_DREAMCAST;
		cloneMapping = true;
	}
	return false;
}

int GamepadDevice::GetGamepadCount()
{
	_gamepads_mutex.lock();
	int count = _gamepads.size();
	_gamepads_mutex.unlock();
	return count;
}

std::shared_ptr<GamepadDevice> GamepadDevice::GetGamepad(int index)
{
	_gamepads_mutex.lock();
	std::shared_ptr<GamepadDevice> dev;
	if (index >= 0 && index < (int)_gamepads.size())
		dev = _gamepads[index];
	else
		dev = NULL;
	_gamepads_mutex.unlock();
	return dev;
}

void GamepadDevice::save_mapping(int system /* = settings.platform.system */)
{
	if (!input_mapper)
		return;
	std::string filename = make_mapping_filename(instanceMapping, system, perGameMapping);
	InputMapping::SaveMapping(filename, input_mapper);
}

void GamepadDevice::setPerGameMapping(bool enabled)
{
	perGameMapping = enabled;
	if (enabled)
		input_mapper = std::make_shared<InputMapping>(*input_mapper);
	else
	{
		auto deleteMapping = [this](bool instance, int system) {
			std::string filename = make_mapping_filename(instance, system, true);
			InputMapping::DeleteMapping(filename);
		};
		deleteMapping(false, DC_PLATFORM_DREAMCAST);
		deleteMapping(false, DC_PLATFORM_NAOMI);
		deleteMapping(true, DC_PLATFORM_DREAMCAST);
		deleteMapping(true, DC_PLATFORM_NAOMI);
	}
}

static void updateVibration(u32 port, float power, float inclination, u32 duration_ms)
{
	int i = GamepadDevice::GetGamepadCount() - 1;
	for ( ; i >= 0; i--)
	{
		std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
		if (gamepad != NULL && gamepad->maple_port() == (int)port && gamepad->is_rumble_enabled())
			gamepad->rumble(power, inclination, duration_ms);
	}
}

void GamepadDevice::detect_btn_input(input_detected_cb button_pressed)
{
	_input_detected = button_pressed;
	_detecting_button = true;
	_detecting_axis = false;
	_detection_start_time = os_GetSeconds() + 0.2;
}

void GamepadDevice::detect_axis_input(input_detected_cb axis_moved)
{
	_input_detected = axis_moved;
	_detecting_button = false;
	_detecting_axis = true;
	_detection_start_time = os_GetSeconds() + 0.2;
}

void GamepadDevice::detectButtonOrAxisInput(input_detected_cb input_changed)
{
	_input_detected = input_changed;
	_detecting_button = true;
	_detecting_axis = true;
	_detection_start_time = os_GetSeconds() + 0.2;
}

#ifdef TEST_AUTOMATION
static FILE *get_record_input(bool write)
{
	if (write && !cfgLoadBool("record", "record_input", false))
		return NULL;
	if (!write && !cfgLoadBool("record", "replay_input", false))
		return NULL;
	std::string game_dir = settings.content.path;
	size_t slash = game_dir.find_last_of("/");
	size_t dot = game_dir.find_last_of(".");
	std::string input_file = "scripts/" + game_dir.substr(slash + 1, dot - slash) + "input";
	return nowide::fopen(input_file.c_str(), write ? "w" : "r");
}
#endif

void GamepadDevice::Register(const std::shared_ptr<GamepadDevice>& gamepad)
{
	int maple_port = cfgLoadInt("input",
			MAPLE_PORT_CFG_PREFIX + gamepad->unique_id(), 12345);
	if (maple_port != 12345)
		gamepad->set_maple_port(maple_port);
#ifdef TEST_AUTOMATION
	if (record_input == NULL)
	{
		record_input = get_record_input(true);
		if (record_input != NULL)
			setbuf(record_input, NULL);
	}
#endif
	_gamepads_mutex.lock();
	_gamepads.push_back(gamepad);
	_gamepads_mutex.unlock();
	MapleConfigMap::UpdateVibration = updateVibration;
}

void GamepadDevice::Unregister(const std::shared_ptr<GamepadDevice>& gamepad)
{
	_gamepads_mutex.lock();
	for (auto it = _gamepads.begin(); it != _gamepads.end(); it++)
		if (*it == gamepad) {
			_gamepads.erase(it);
			break;
		}
	_gamepads_mutex.unlock();
}

void GamepadDevice::SaveMaplePorts()
{
	for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
	{
		std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
		if (gamepad != NULL && !gamepad->unique_id().empty())
			cfgSaveInt("input", MAPLE_PORT_CFG_PREFIX + gamepad->unique_id(), gamepad->maple_port());
	}
}

#ifdef TEST_AUTOMATION
#include "cfg/option.h"
static bool replay_inited;
FILE *replay_file;
u64 next_event;
u32 next_port;
u32 next_kcode;
bool do_screenshot;

void replay_input()
{
	if (!replay_inited)
	{
		replay_file = get_record_input(false);
		replay_inited = true;
	}
	u64 now = sh4_sched_now64();
	if (config::UseReios)
	{
		// Account for the swirl time
		if (config::Broadcast == 0)
			now = std::max((int64_t)now - 2152626532L, 0L);
		else
			now = std::max((int64_t)now - 2191059108L, 0L);
	}
	if (replay_file == NULL)
	{
		if (next_event > 0 && now - next_event > SH4_MAIN_CLOCK * 5)
			die("Automation time-out after 5 s\n");
		return;
	}
	while (next_event <= now)
	{
		if (next_event > 0)
			kcode[next_port] = next_kcode;

		char action[32];
		if (fscanf(replay_file, "%ld %s %x %x\n", &next_event, action, &next_port, &next_kcode) != 4)
		{
			fclose(replay_file);
			replay_file = NULL;
			NOTICE_LOG(INPUT, "Input replay terminated");
			do_screenshot = true;
			break;
		}
	}
}
#endif
