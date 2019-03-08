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
#include <memory>
#include <mutex>
#include "types.h"
#include "mapping.h"

class GamepadDevice
{
	typedef void (*input_detected_cb)(u32 code);
public:
	const std::string& api_name() { return _api_name; }
	const std::string& name() { return _name; }
	int maple_port() { return _maple_port; }
	void set_maple_port(int port) { _maple_port = port; }
	virtual bool gamepad_btn_input(u32 code, bool pressed);
	bool gamepad_axis_input(u32 code, int value);
	virtual ~GamepadDevice() {}
	
	void detect_btn_input(input_detected_cb button_pressed)
	{
		_input_detected = button_pressed;
		_detecting_button = true;
	}
	void detect_axis_input(input_detected_cb axis_moved)
	{
		_input_detected = axis_moved;
		_detecting_button = false;
	}
	void cancel_detect_input()
	{
		_input_detected = NULL;
	}
	InputMapping *get_input_mapping() { return input_mapper; }
	void save_mapping();
	bool remappable() { return _remappable && input_mapper != NULL; }
	virtual bool is_virtual_gamepad() { return false; }

	virtual void rumble(float power, float inclination, u32 duration_ms) {}
	virtual void update_rumble() {}
	bool is_rumble_enabled() { return _rumble_enabled; }

	static void Register(std::shared_ptr<GamepadDevice> gamepad)
	{
		_gamepads_mutex.lock();
		_gamepads.push_back(gamepad);
		_gamepads_mutex.unlock();
	}

	static void Unregister(std::shared_ptr<GamepadDevice> gamepad)
	{
		gamepad->save_mapping();
		_gamepads_mutex.lock();
		for (auto it = _gamepads.begin(); it != _gamepads.end(); it++)
			if (*it == gamepad)
			{
				_gamepads.erase(it);
				break;
			}
		_gamepads_mutex.unlock();
	}

	static int GetGamepadCount();
	static std::shared_ptr<GamepadDevice> GetGamepad(int index);

protected:
	GamepadDevice(int maple_port, const char *api_name, bool remappable = true)
		: _api_name(api_name), _maple_port(maple_port), input_mapper(NULL), _input_detected(NULL), _remappable(remappable)
	{
	}
	bool find_mapping(const char *custom_mapping = NULL);

	virtual void load_axis_min_max(u32 axis) {}

	std::string _name;
	InputMapping *input_mapper;
	std::map<u32, int> axis_min_values;
	std::map<u32, unsigned int> axis_ranges;
	bool _rumble_enabled = true;

private:
	int get_axis_min_value(u32 axis);
	unsigned int get_axis_range(u32 axis);
	std::string make_mapping_filename();

	std::string _api_name;
	int _maple_port;
	bool _detecting_button = false;
	input_detected_cb _input_detected;
	bool _remappable;
	float _dead_zone = 0.1f;

	static std::vector<std::shared_ptr<GamepadDevice>> _gamepads;
	static std::mutex _gamepads_mutex;
};
