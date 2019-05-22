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
#include <algorithm>
#include <limits.h>
#include "gamepad_device.h"
#include "rend/gui.h"
#include "oslib/oslib.h"
#include "cfg/cfg.h"

#define MAPLE_PORT_CFG_PREFIX "maple_"

extern void dc_exit();

extern u16 kcode[4];
extern u8 rt[4], lt[4];
extern s8 joyx[4], joyy[4];

std::vector<std::shared_ptr<GamepadDevice>> GamepadDevice::_gamepads;
std::mutex GamepadDevice::_gamepads_mutex;

bool GamepadDevice::gamepad_btn_input(u32 code, bool pressed)
{
	if (_input_detected != NULL && _detecting_button 
			&& os_GetSeconds() >= _detection_start_time && pressed)
	{
		_input_detected(code);
		_input_detected = NULL;
	}
	if (input_mapper == NULL || _maple_port < 0 || _maple_port >= ARRAY_SIZE(kcode))
		return false;
	DreamcastKey key = input_mapper->get_button_id(code);
	if (key == EMU_BTN_NONE)
		return false;

	if (key < 0x10000)
	{
		if (pressed)
		{
			kcode[_maple_port] &= ~(u16)key;
			// Avoid two opposite dpad keys being pressed simultaneously
			switch (key)
			{
			case DC_DPAD_UP:
				kcode[_maple_port] |= (u16)DC_DPAD_DOWN;
				break;
			case DC_DPAD_DOWN:
				kcode[_maple_port] |= (u16)DC_DPAD_UP;
				break;
			case DC_DPAD_LEFT:
				kcode[_maple_port] |= (u16)DC_DPAD_RIGHT;
				break;
			case DC_DPAD_RIGHT:
				kcode[_maple_port] |= (u16)DC_DPAD_LEFT;
				break;
			case DC_DPAD2_UP:
				kcode[_maple_port] |= (u16)DC_DPAD2_DOWN;
				break;
			case DC_DPAD2_DOWN:
				kcode[_maple_port] |= (u16)DC_DPAD2_UP;
				break;
			case DC_DPAD2_LEFT:
				kcode[_maple_port] |= (u16)DC_DPAD2_RIGHT;
				break;
			case DC_DPAD2_RIGHT:
				kcode[_maple_port] |= (u16)DC_DPAD2_LEFT;
				break;
			default:
				break;
			}
		}
		else
			kcode[_maple_port] |= (u16)key;
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
		case EMU_BTN_TRIGGER_LEFT:
			lt[_maple_port] = pressed ? 255 : 0;
			break;
		case EMU_BTN_TRIGGER_RIGHT:
			rt[_maple_port] = pressed ? 255 : 0;
			break;
		default:
			return false;
		}
	}

	//printf("%d: BUTTON %s %x -> %d. kcode=%x\n", _maple_port, pressed ? "down" : "up", code, key, kcode[_maple_port]);
	return true;
}

bool GamepadDevice::gamepad_axis_input(u32 code, int value)
{
	s32 v;
	if (input_mapper->get_axis_inverted(code))
		v = (get_axis_min_value(code) + get_axis_range(code) - value) * 255 / get_axis_range(code) - 128;
	else
		v = (value - get_axis_min_value(code)) * 255 / get_axis_range(code) - 128; //-128 ... + 127 range
	if (_input_detected != NULL && !_detecting_button 
			&& os_GetSeconds() >= _detection_start_time && (v >= 64 || v <= -64))
	{
		_input_detected(code);
		_input_detected = NULL;
	}
	if (input_mapper == NULL || _maple_port < 0 || _maple_port >= ARRAY_SIZE(kcode))
		return false;
	DreamcastKey key = input_mapper->get_axis_id(code);

	if ((int)key < 0x10000)
	{
		kcode[_maple_port] |= key | (key << 1);
		if (v <= -64)
			kcode[_maple_port] &= ~key;
		else if (v >= 64)
			kcode[_maple_port] &= ~(key << 1);

		// printf("Mapped to %d %d %d\n",mo,kcode[port]&mo,kcode[port]&(mo*2));
	}
	else if (((int)key >> 16) == 1)	// Triggers
	{
		//printf("T-AXIS %d Mapped to %d -> %d\n",key, value, v + 128);

		if (key == DC_AXIS_LT)
			lt[_maple_port] = (u8)(v + 128);
		else if (key == DC_AXIS_RT)
			rt[_maple_port] = (u8)(v + 128);
		else
			return false;
	}
	else if (((int)key >> 16) == 2) // Analog axes
	{
		//printf("AXIS %d Mapped to %d -> %d\n", key, value, v);
		s8 *this_axis;
		s8 *other_axis;
		if (key == DC_AXIS_X)
		{
			this_axis = &joyx[_maple_port];
			other_axis = &joyy[_maple_port];
		}
		else if (key == DC_AXIS_Y)
		{
			this_axis = &joyy[_maple_port];
			other_axis = &joyx[_maple_port];
		}
		else
			return false;
		// Radial dead zone
		// FIXME compute both axes at the same time
		if ((float)(v * v + *other_axis * *other_axis) < _dead_zone * _dead_zone * 128.f * 128.f * 2.f)
		{
			*this_axis = 0;
			*other_axis = 0;
		}
		else
			*this_axis = (s8)v;
	}
	else
		return false;

	return true;
}

int GamepadDevice::get_axis_min_value(u32 axis) {
	auto it = axis_min_values.find(axis);
	if (it == axis_min_values.end()) {
		load_axis_min_max(axis);
		it = axis_min_values.find(axis);
		if (it == axis_min_values.end())
			return INT_MIN;
	}
	return it->second;
}

unsigned int GamepadDevice::get_axis_range(u32 axis) {
	auto it = axis_ranges.find(axis);
	if (it == axis_ranges.end()) {
		load_axis_min_max(axis);
		it = axis_ranges.find(axis);
		if (it == axis_ranges.end())
			return UINT_MAX;
	}
	return it->second;
}

std::string GamepadDevice::make_mapping_filename()
{
	std::string mapping_file = api_name() + "_" + name();
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

bool GamepadDevice::find_mapping(const char *custom_mapping /* = NULL */)
{
	std::string mapping_file;
	if (custom_mapping != NULL)
		mapping_file = custom_mapping;
	else
		mapping_file = make_mapping_filename();

	input_mapper = InputMapping::LoadMapping(mapping_file.c_str());
	return input_mapper != NULL;
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
	if (index >= 0 && index < _gamepads.size())
		dev = _gamepads[index];
	else
		dev = NULL;
	_gamepads_mutex.unlock();
	return dev;
}

void GamepadDevice::save_mapping()
{
	if (input_mapper == NULL)
		return;
	input_mapper->save(make_mapping_filename().c_str());
}

void UpdateVibration(u32 port, float power, float inclination, u32 duration_ms)
{
	int i = GamepadDevice::GetGamepadCount() - 1;
	for ( ; i >= 0; i--)
	{
		std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
		if (gamepad != NULL && gamepad->maple_port() == port && gamepad->is_rumble_enabled())
			gamepad->rumble(power, inclination, duration_ms);
	}
}

void GamepadDevice::detect_btn_input(input_detected_cb button_pressed)
{
	_input_detected = button_pressed;
	_detecting_button = true;
	_detection_start_time = os_GetSeconds() + 0.2;
}

void GamepadDevice::detect_axis_input(input_detected_cb axis_moved)
{
	_input_detected = axis_moved;
	_detecting_button = false;
	_detection_start_time = os_GetSeconds() + 0.2;
}

void GamepadDevice::Register(std::shared_ptr<GamepadDevice> gamepad)
{
	int maple_port = cfgLoadInt("input",
			(MAPLE_PORT_CFG_PREFIX + gamepad->unique_id()).c_str(), 12345);
	if (maple_port != 12345)
		gamepad->set_maple_port(maple_port);

	_gamepads_mutex.lock();
	_gamepads.push_back(gamepad);
	_gamepads_mutex.unlock();
}

void GamepadDevice::Unregister(std::shared_ptr<GamepadDevice> gamepad)
{
	gamepad->save_mapping();
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
			cfgSaveInt("input", (MAPLE_PORT_CFG_PREFIX + gamepad->unique_id()).c_str(), gamepad->maple_port());
	}
}
