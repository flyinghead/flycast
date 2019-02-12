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

extern void dc_stop();

extern u16 kcode[4];
extern u8 rt[4], lt[4];
extern s8 joyx[4], joyy[4];

std::vector<GamepadDevice *> GamepadDevice::_gamepads;
std::mutex GamepadDevice::_gamepads_mutex;

void GamepadDevice::gamepad_btn_input(u32 code, bool pressed)
{
	if (_input_detected != NULL && _detecting_button && pressed)
	{
		_input_detected(code);
		_input_detected = NULL;
	}
	if (input_mapper == NULL || _maple_port < 0 || _maple_port >= ARRAY_SIZE(kcode))
		return;
	DreamcastKey key = input_mapper->get_button_id(code);

	printf("%d: BUTTON %x -> %d\n", _maple_port, code, key);

	if (key < 0x10000)
	{
		if (pressed)
			kcode[_maple_port] &= ~(u16)key;
		else
			kcode[_maple_port] |= (u16)key;
	}
	else
	{
		switch (key)
		{
		case EMU_BTN_ESCAPE:
			dc_stop();
			break;
		case EMU_BTN_MENU:
			gui_open_settings();
			break;
		case EMU_BTN_TRIGGER_LEFT:
			lt[_maple_port] = pressed ? 255 : 0;
			break;
		case EMU_BTN_TRIGGER_RIGHT:
			rt[_maple_port] = pressed ? 255 : 0;
			break;
		default:
			break;
		}
	}
}

void GamepadDevice::gamepad_axis_input(u32 code, int value)
{
	s32 v = (value - get_axis_min_value(code)) * 255 / get_axis_range(code) - 128; //-128 ... + 127 range
	if (_input_detected != NULL && !_detecting_button && (v >= 64 || v <= -64))
	{
		_input_detected(code);
		_input_detected = NULL;
	}
	if (input_mapper == NULL || _maple_port < 0 || _maple_port >= ARRAY_SIZE(kcode))
		return;
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
	}
	else if (((int)key >> 16) == 2) // Analog axes
	{
		//printf("AXIS %d Mapped to %d -> %d\n", key, value, v);
		if (key == DC_AXIS_X)
			joyx[_maple_port] = (s8)v;
		else if (key == DC_AXIS_Y)
			joyy[_maple_port] = (s8)v;
	}
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
	std::string mapping_file = std::string(api_name()) + "_" + name();
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

GamepadDevice *GamepadDevice::GetGamepad(int index)
{
	_gamepads_mutex.lock();
	GamepadDevice *dev;
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
	std::string filename = make_mapping_filename();
	if (!input_mapper->is_dirty())
		return;
	input_mapper->save(make_mapping_filename().c_str());
}
