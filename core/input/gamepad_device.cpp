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
#include "stdclass.h"
#include "ui/gui.h"
#include "emulator.h"
#include "hw/maple/maple_devs.h"
#include "mouse.h"

#include <algorithm>
#include <mutex>
#include <vector>

#define MAPLE_PORT_CFG_PREFIX "maple_"

// Gamepads
u32 kcode[4] = { ~0u, ~0u, ~0u, ~0u };
s16 joyx[4];
s16 joyy[4];
s16 joyrx[4];
s16 joyry[4];
s16 joy3x[4];
s16 joy3y[4];
u16 rt[4];
u16 lt[4];
u16 lt2[4];
u16 rt2[4];
// Keyboards
u8 kb_shift[MAPLE_PORTS];	// shift keys pressed (bitmask)
u8 kb_key[MAPLE_PORTS][6];	// normal keys pressed

std::vector<std::shared_ptr<GamepadDevice>> GamepadDevice::_gamepads;
std::mutex GamepadDevice::_gamepads_mutex;

#ifdef TEST_AUTOMATION
#include "hw/sh4/sh4_sched.h"
#include <cstdio>
static FILE *record_input;
#endif

// Constructor is now defined only in the header file
// GamepadDevice::GamepadDevice(int maple_port, const char *api_name, bool remappable)
//  : _api_name(api_name), _maple_port(maple_port), _input_detected(nullptr), _remappable(remappable),
//    digitalToAnalogState{}
// {
//  // Initialize our pressedButtons sets
//  for (int i = 0; i < 4; i++)
//      pressedButtons[i].clear();
// }

bool GamepadDevice::handleButtonInput(int port, DreamcastKey key, bool pressed)
{
	if (key == EMU_BTN_NONE)
		return false;

	if (key <= DC_BTN_BITMAPPED_LAST)
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
				settings.input.fastForwardMode = !settings.input.fastForwardMode && !settings.network.online && !settings.naomi.multiboard;
			break;
		case EMU_BTN_LOADSTATE:
			if (pressed)
				gui_loadState();
			break;
		case EMU_BTN_SAVESTATE:
			if (pressed)
				gui_saveState();
			break;
		case EMU_BTN_SCREENSHOT:
			if (pressed)
				gui_takeScreenshot();
			break;
		case DC_AXIS_LT:
			if (port >= 0)
				lt[port] = pressed ? 0xffff : 0;
			break;
		case DC_AXIS_RT:
			if (port >= 0)
				rt[port] = pressed ? 0xffff : 0;
			break;
		case DC_AXIS_LT2:
			if (port >= 0)
				lt2[port] = pressed ? 0xffff : 0;
			break;
		case DC_AXIS_RT2:
			if (port >= 0)
				rt2[port] = pressed ? 0xffff : 0;
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
		case DC_AXIS3_UP:
		case DC_AXIS3_DOWN:
			buttonToAnalogInput<DC_AXIS3_UP, DIGANA3_UP, DIGANA3_DOWN>(port, key, pressed, joy3y[port]);
			break;
		case DC_AXIS3_LEFT:
		case DC_AXIS3_RIGHT:
			buttonToAnalogInput<DC_AXIS3_LEFT, DIGANA3_LEFT, DIGANA3_RIGHT>(port, key, pressed, joy3x[port]);
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
	// When detecting input for button mapping
	if (_input_detected != nullptr && _detecting_button 
			&& getTimeMs() >= _detection_start_time)
	{
		if (pressed) {
			// Button pressed - add to mapping and tracking
			_input_detected(code, false, false);
			detectionButtons.insert(code);
			
			// If we're not in combo detection mode, stop detecting after first button
			if (!_detecting_combo) {
				_input_detected = nullptr;
				detectionButtons.clear();
			}
		}
		else if (_detecting_combo) {
			// Button released - if this is a button we pressed during detection, end detection
			if (detectionButtons.find(code) != detectionButtons.end()) {
				_input_detected = nullptr;
				detectionButtons.clear();
				DEBUG_LOG(INPUT, "Ending combo detection on button release: %d", code);
			}
		}
		
		return true;
	}
	
	if (!input_mapper || _maple_port > (int)std::size(kcode))
		return false;

	bool rc = false;

	// Update button press tracking
	int targetPort = (_maple_port == 4) ? 0 : _maple_port; // Use port 0 for all-ports mode as a base
	if (pressed)
		pressedButtons[targetPort].insert(code);
	else
		pressedButtons[targetPort].erase(code);
		
	// First handle individual button mapping (standard behavior)
	DreamcastKey key = input_mapper->get_button_id(targetPort, code);
	if (key != EMU_BTN_NONE)
	{
		if (_maple_port == 4)
		{
			for (int port = 0; port < 4; port++)
				rc = handleButtonInput(port, key, pressed) || rc;
		}
		else
		{
			rc = handleButtonInput(_maple_port, key, pressed);
		}
	}
	
	// Only process combinations if a button is pressed - skip for releases (efficiency improvement)
	if (pressed) {
		// Special handling for trigger buttons in SDL-based controllers
		// Map common triggers to standard codes for combo detection
		u32 normalizedCode = code;
		// For common SDL trigger button codes (varies by controller)
		if (code == 2 || code == 5 || code == 4 || code == 6 || code == 7)
		{
			// Check if this is mapped to a trigger
			DreamcastKey triggerKey = input_mapper->get_button_id(targetPort, code);
			if (triggerKey == DC_AXIS_LT || triggerKey == DC_AXIS_RT ||
				triggerKey == DC_AXIS_LT2 || triggerKey == DC_AXIS_RT2)
			{
				// This is a trigger - use the DreamcastKey value as the normalized code
				// This makes it consistent across different controllers
				normalizedCode = triggerKey;
			}
		}
		
		// Then process button combinations
		// We do this separately to ensure both individual buttons AND combinations work
		for (const auto& pair : input_mapper->get_all_button_combinations(targetPort))
		{
			// We only care about combinations with more than one button
			if (pair.second.buttons.size() <= 1)
				continue;
				
			// Check if this button is part of this combination
			// Use either the original code or normalized code for triggers
			bool isInCombo = false;
			for (u32 comboCode : pair.second.buttons) {
				if (comboCode == code || comboCode == normalizedCode) {
					isInCombo = true;
					break;
				}
			}
			if (!isInCombo)
				continue;
				
			// Check if the combination state changed because of this button press/release
			bool comboPressed = isButtonCombinationPressed(targetPort, pair.second);
			
			// Handle the combination state
			if (_maple_port == 4)
			{
				for (int port = 0; port < 4; port++)
					rc = handleButtonInput(port, pair.first, comboPressed) || rc;
			}
			else
			{
				rc = handleButtonInput(_maple_port, pair.first, comboPressed) || rc;
			}
		}
	}

	return rc;
}

static DreamcastKey getOppositeAxis(DreamcastKey key)
{
	switch (key)
	{
	case DC_AXIS_RIGHT: return DC_AXIS_LEFT;
	case DC_AXIS_LEFT: return DC_AXIS_RIGHT;
	case DC_AXIS_UP: return DC_AXIS_DOWN;
	case DC_AXIS_DOWN: return DC_AXIS_UP;
	case DC_AXIS2_RIGHT: return DC_AXIS2_LEFT;
	case DC_AXIS2_LEFT: return DC_AXIS2_RIGHT;
	case DC_AXIS2_UP: return DC_AXIS2_DOWN;
	case DC_AXIS2_DOWN: return DC_AXIS2_UP;
	case DC_AXIS3_RIGHT: return DC_AXIS3_LEFT;
	case DC_AXIS3_LEFT: return DC_AXIS3_RIGHT;
	case DC_AXIS3_UP: return DC_AXIS3_DOWN;
	case DC_AXIS3_DOWN: return DC_AXIS3_UP;
	default: return key;
	}
}

//
// value must be >= -32768 and <= 32767 for full axes
// and 0 to 32767 for half axes/triggers
//
bool GamepadDevice::gamepad_axis_input(u32 code, int value)
{
	bool positive = value >= 0;
	if (_input_detected != NULL && _detecting_axis
			&& getTimeMs() >= _detection_start_time && std::abs(value) >= 16384)
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
				lt[port] = std::min(std::abs(v) << 1, 0xffff);
			else if (key == DC_AXIS_RT)
				rt[port] = std::min(std::abs(v) << 1, 0xffff);
			else if (key == DC_AXIS_LT2)
				lt2[port] = std::min(std::abs(v) << 1, 0xffff);
			else if (key == DC_AXIS_RT2)
				rt2[port] = std::min(std::abs(v) << 1, 0xffff);
			else
				return false;
		}
		else if ((key & DC_BTN_GROUP_MASK) == DC_AXIS_STICKS) // Analog axes
		{
			//printf("AXIS %d Mapped to %d -> %d\n", key, value, v);
			s16 *this_axis;
			int otherAxisValue;
			int axisDirection = -1;
			switch (key)
			{
			case DC_AXIS_RIGHT:
				axisDirection = 1;
				[[fallthrough]];
			case DC_AXIS_LEFT:
				this_axis = &joyx[port];
				otherAxisValue = lastAxisValue[port][DC_AXIS_UP];
				break;

			case DC_AXIS_DOWN:
				axisDirection = 1;
				[[fallthrough]];
			case DC_AXIS_UP:
				this_axis = &joyy[port];
				otherAxisValue = lastAxisValue[port][DC_AXIS_LEFT];
				break;

			case DC_AXIS2_RIGHT:
				axisDirection = 1;
				[[fallthrough]];
			case DC_AXIS2_LEFT:
				this_axis = &joyrx[port];
				otherAxisValue = lastAxisValue[port][DC_AXIS2_UP];
				break;

			case DC_AXIS2_DOWN:
				axisDirection = 1;
				[[fallthrough]];
			case DC_AXIS2_UP:
				this_axis = &joyry[port];
				otherAxisValue = lastAxisValue[port][DC_AXIS2_LEFT];
				break;

			case DC_AXIS3_RIGHT:
				axisDirection = 1;
				[[fallthrough]];
			case DC_AXIS3_LEFT:
				this_axis = &joy3x[port];
				otherAxisValue = lastAxisValue[port][DC_AXIS3_UP];
				break;

			case DC_AXIS3_DOWN:
				axisDirection = 1;
				[[fallthrough]];
			case DC_AXIS3_UP:
				this_axis = &joy3y[port];
				otherAxisValue = lastAxisValue[port][DC_AXIS3_LEFT];
				break;

			default:
				return false;
			}
			int& lastValue = lastAxisValue[port][key];
			int& lastOpValue = lastAxisValue[port][getOppositeAxis(key)];
			if (lastValue != v || lastOpValue != v)
			{
				lastValue = lastOpValue = v;
				// Lightgun with left analog stick
				if (key == DC_AXIS_RIGHT || key == DC_AXIS_LEFT)
					mo_x_abs[port] = (std::abs(v) * axisDirection + 32768) * 639 / 65535;
				else if (key == DC_AXIS_UP || key == DC_AXIS_DOWN)
					mo_y_abs[port] = (std::abs(v) * axisDirection + 32768) * 479 / 65535;
			}
			// Radial dead zone
			// FIXME compute both axes at the same time
			const float nv = std::abs(v) / 32768.f;
			const float r2 = nv * nv + otherAxisValue * otherAxisValue / 32768.f / 32768.f;
			if (r2 < input_mapper->dead_zone * input_mapper->dead_zone || r2 == 0.f)
			{
				*this_axis = 0;
			}
			else
			{
				float pdz = nv * input_mapper->dead_zone / std::sqrt(r2);
				// there's a dead angular zone at 45° with saturation > 1 (both axes are saturated)
				v = std::round((nv - pdz) / (1 - pdz) * 32768.f * input_mapper->saturation);
				*this_axis = std::clamp(v * axisDirection, -32768, 32767);
			}
		}
		else if (key != EMU_BTN_NONE && key <= DC_BTN_BITMAPPED_LAST) // Map triggers to digital buttons
		{
			//printf("B-AXIS %d Mapped to %d -> %d\n", key, value, v);
			// TODO hysteresis?
			int threshold = 16384;
			if (code == leftTrigger || code == rightTrigger )
				threshold = 100;

			if (std::abs(v) < threshold)
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
			gamepad->resetMappingToDefault(settings.platform.isArcade(), true);
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
				rumblePower = input_mapper->rumblePower;
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

int GamepadDevice::GetGamepadCount() {
	Lock _(_gamepads_mutex);
	return _gamepads.size();
}

std::shared_ptr<GamepadDevice> GamepadDevice::GetGamepad(int index)
{
	Lock _(_gamepads_mutex);
	if (index >= 0 && index < (int)_gamepads.size())
		return _gamepads[index];
	else
		return nullptr;
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
	_detection_start_time = getTimeMs() + 200;
	detectionButtons.clear();
}

void GamepadDevice::detect_axis_input(input_detected_cb axis_moved)
{
	_input_detected = axis_moved;
	_detecting_button = false;
	_detecting_axis = true;
	_detection_start_time = getTimeMs() + 200;
	detectionButtons.clear();
}

void GamepadDevice::detectButtonOrAxisInput(input_detected_cb input_changed)
{
	_input_detected = input_changed;
	_detecting_button = true;
	_detecting_axis = true;
	_detection_start_time = getTimeMs() + 200;
	_detecting_combo = true;
	detectionButtons.clear();
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
	Lock _(_gamepads_mutex);
	_gamepads.push_back(gamepad);
	MapleConfigMap::UpdateVibration = updateVibration;

	gamepad->_is_registered = true;
	gamepad->registered();
}

void GamepadDevice::Unregister(const std::shared_ptr<GamepadDevice>& gamepad)
{
	Lock _(_gamepads_mutex);
	for (auto it = _gamepads.begin(); it != _gamepads.end(); it++)
		if (*it == gamepad) {
			_gamepads.erase(it);
			break;
		}
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

s16 (&GamepadDevice::getTargetArray(DigAnalog axis))[4]
{
	switch (axis)
	{
	case DIGANA_LEFT:
	case DIGANA_RIGHT:
		return joyx;
	case DIGANA_UP:;
	case DIGANA_DOWN:
		return joyy;
	case DIGANA2_LEFT:
	case DIGANA2_RIGHT:
		return joyrx;
	case DIGANA2_UP:
	case DIGANA2_DOWN:
		return joyry;
	case DIGANA3_LEFT:
	case DIGANA3_RIGHT:
		return joy3x;
	case DIGANA3_UP:
	case DIGANA3_DOWN:
		return joy3y;
	default:
		die("unknown axis");
	}
}

void GamepadDevice::rampAnalog()
{
	Lock _(rampMutex);
	if (lastAnalogUpdate == 0)
		// also used as a flag that no analog ramping is needed on this device (yet)
		return;

	const u64 now = getTimeMs();
	const int delta = std::round(static_cast<float>(now - lastAnalogUpdate) * AnalogRamp);
	lastAnalogUpdate = now;
	for (unsigned port = 0; port < std::size(digitalToAnalogState); port++)
	{
		for (int axis = 0; axis < 12; axis += 2)	// 3 sticks with 2 axes each
		{
			DigAnalog negDir = static_cast<DigAnalog>(1 << axis);
			if ((rampAnalogState[port] & negDir) == 0)
				// axis not active
				continue;
			DigAnalog posDir = static_cast<DigAnalog>(1 << (axis + 1));
			const int socd = digitalToAnalogState[port] & (negDir | posDir);
			s16& axisValue = getTargetArray(negDir)[port];
			if (socd != 0 && socd != (negDir | posDir))
			{
				// One axis is pressed => ramp up
				if (socd == posDir)
					axisValue = std::min(32767, axisValue + delta);
				else
					axisValue = std::max(-32768, axisValue - delta);
			}
			else
			{
				// No axis is pressed (or both) => ramp down
				if (axisValue > 0)
					axisValue = std::max(0, axisValue - delta);
				else if (axisValue < 0)
					axisValue = std::min(0, axisValue + delta);
				else
					rampAnalogState[port] &= ~negDir;
			}
		}
	}
}

void GamepadDevice::RampAnalog()
{
	Lock _(_gamepads_mutex);
	for (auto& gamepad : _gamepads)
		gamepad->rampAnalog();
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

bool GamepadDevice::isButtonCombinationPressed(int port, const InputMapping::ButtonCombination& combo)
{
	if (combo.buttons.empty())
		return false;
		
	// For a single button, just check if it's pressed
	if (combo.buttons.size() == 1)
		return pressedButtons[port].find(combo.buttons[0]) != pressedButtons[port].end();
		
	// For combinations, all buttons must be pressed
	for (u32 code : combo.buttons)
	{
		// First check with the actual code
		if (pressedButtons[port].find(code) != pressedButtons[port].end())
			continue;
			
		// If this is a trigger button, check with the normalized DreamcastKey value
		if ((code == DC_AXIS_LT || code == DC_AXIS_RT || code == DC_AXIS_LT2 || code == DC_AXIS_RT2))
		{
			// Look for possible SDL trigger button codes for this trigger
			bool found = false;
			for (u32 possibleCode : {2, 5, 4, 6, 7})
			{
				// Check if this button is mapped to our trigger
				DreamcastKey triggerKey = input_mapper->get_button_id(port, possibleCode);
				if (triggerKey == code && pressedButtons[port].find(possibleCode) != pressedButtons[port].end())
				{
					found = true;
					break;
				}
			}
			if (found)
				continue;
		}
		
		// Button not pressed in any form
		return false;
	}
	
	return true; // All buttons in the combination are pressed
}
