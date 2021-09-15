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

#include "input/gamepad_device.h"
#include <algorithm>

static jobject input_device_manager;
static jmethodID input_device_manager_rumble;

enum {
	AXIS_X = 0,
	AXIS_Y = 1,
	AXIS_Z = 0xb,
	AXIS_RX = 0xc,
	AXIS_RY = 0xd,
	AXIS_RZ = 0xe,
	AXIS_HAT_X = 0xf,
	AXIS_HAT_Y = 0x10,
	AXIS_LTRIGGER = 0x11,
	AXIS_RTRIGGER = 0x12,
	AXIS_THROTTLE = 0x13,
	AXIS_RUDDER = 0x14,
	AXIS_WHEEL = 0x15,
	AXIS_GAS = 0x16,
	AXIS_BRAKE = 0x17,

	KEYCODE_BACK = 4,
	KEYCODE_DPAD_UP = 19,
	KEYCODE_DPAD_DOWN = 20,
	KEYCODE_DPAD_LEFT = 21,
	KEYCODE_DPAD_RIGHT = 22,
	KEYCODE_DPAD_CENTER = 23,
	KEYCODE_BUTTON_A = 96,
	KEYCODE_BUTTON_B = 97,
	KEYCODE_BUTTON_C = 98,
	KEYCODE_BUTTON_X = 99,
	KEYCODE_BUTTON_Y = 100,
	KEYCODE_BUTTON_Z = 101,
	KEYCODE_BUTTON_L1 = 102,
	KEYCODE_BUTTON_R1 = 103,
	KEYCODE_BUTTON_L2 = 104,
	KEYCODE_BUTTON_R2 = 105,
	KEYCODE_BUTTON_THUMBL = 106,
	KEYCODE_BUTTON_THUMBR = 107,
	KEYCODE_BUTTON_START = 108,
	KEYCODE_BUTTON_SELECT = 109,
	KEYCODE_BUTTON_MODE = 110,
};

class DefaultInputMapping : public InputMapping
{
public:
	DefaultInputMapping()
	{
		name = "Default";
		set_button(DC_BTN_Y, KEYCODE_BUTTON_Y);
		set_button(DC_BTN_B, KEYCODE_BUTTON_B);
		set_button(DC_BTN_A, KEYCODE_BUTTON_A);
		set_button(DC_BTN_X, KEYCODE_BUTTON_X);
		set_button(DC_BTN_START, KEYCODE_BUTTON_START);
		set_button(DC_DPAD_UP, KEYCODE_DPAD_UP);
		set_button(DC_DPAD_DOWN, KEYCODE_DPAD_DOWN);
		set_button(DC_DPAD_LEFT, KEYCODE_DPAD_LEFT);
		set_button(DC_DPAD_RIGHT, KEYCODE_DPAD_RIGHT);
		set_button(EMU_BTN_MENU, KEYCODE_BACK);

		set_axis(DC_AXIS_X, AXIS_X, false);
		set_axis(DC_AXIS_Y, AXIS_Y, false);
		set_axis(DC_AXIS_LT, AXIS_LTRIGGER, false);
		set_axis(DC_AXIS_RT, AXIS_RTRIGGER, false);
		set_axis(DC_AXIS_X2, AXIS_Z, false);
		set_axis(DC_AXIS_Y2, AXIS_RZ, false);

		dirty = false;
	}
};

class ShieldRemoteInputMapping : public InputMapping
{
public:
	ShieldRemoteInputMapping()
	{
		name = "Default";
		set_button(DC_BTN_A, KEYCODE_DPAD_CENTER);
		set_button(DC_DPAD_UP, KEYCODE_DPAD_UP);
		set_button(DC_DPAD_DOWN, KEYCODE_DPAD_DOWN);
		set_button(DC_DPAD_LEFT, KEYCODE_DPAD_LEFT);
		set_button(DC_DPAD_RIGHT, KEYCODE_DPAD_RIGHT);
		set_button(EMU_BTN_MENU, KEYCODE_BACK);

		dirty = false;
	}
};

class AndroidGamepadDevice : public GamepadDevice
{
public:
	AndroidGamepadDevice(int maple_port, int id, const char *name, const char *unique_id,
			const std::vector<int>& fullAxes, const std::vector<int>& halfAxes)
		: GamepadDevice(maple_port, "Android", id != VIRTUAL_GAMEPAD_ID), android_id(id),
		  fullAxes(fullAxes), halfAxes(halfAxes)
	{
		_name = name;
		_unique_id = unique_id;
		INFO_LOG(INPUT, "Android: Opened joystick %d on port %d: '%s' descriptor '%s'", id, maple_port, _name.c_str(), _unique_id.c_str());
		if (id == VIRTUAL_GAMEPAD_ID)
		{
			input_mapper = std::make_shared<IdentityInputMapping>();
			axis_min_values[DC_AXIS_X] = -128;
			axis_ranges[DC_AXIS_X] = 255;
			axis_min_values[DC_AXIS_Y] = -128;
			axis_ranges[DC_AXIS_Y] = 255;
			axis_min_values[DC_AXIS_LT] = 0;
			axis_ranges[DC_AXIS_LT] = 255;
			axis_min_values[DC_AXIS_RT] = 0;
			axis_ranges[DC_AXIS_RT] = 255;
		}
		else
		{
			loadMapping();
			save_mapping();
		}
	}
	virtual ~AndroidGamepadDevice() override
	{
		INFO_LOG(INPUT, "Android: Joystick '%s' on port %d disconnected", _name.c_str(), maple_port());
	}

	virtual std::shared_ptr<InputMapping> getDefaultMapping() override {
		if (_name == "SHIELD Remote")
			return std::make_shared<ShieldRemoteInputMapping>();
		std::shared_ptr<InputMapping> mapping = std::make_shared<DefaultInputMapping>();
		auto ltAxis = std::find(halfAxes.begin(), halfAxes.end(), AXIS_LTRIGGER);
		auto rtAxis = std::find(halfAxes.begin(), halfAxes.end(), AXIS_RTRIGGER);
		if (ltAxis != halfAxes.end() && rtAxis != halfAxes.end())
		{
			mapping->set_axis(DC_AXIS_LT, *ltAxis, false);
			mapping->set_axis(DC_AXIS_RT, *rtAxis, false);
		}
		else
		{
			ltAxis = std::find(halfAxes.begin(), halfAxes.end(), AXIS_BRAKE);
			rtAxis = std::find(halfAxes.begin(), halfAxes.end(), AXIS_GAS);
			if (ltAxis != halfAxes.end() && rtAxis != halfAxes.end())
			{
				mapping->set_axis(DC_AXIS_LT, *ltAxis, false);
				mapping->set_axis(DC_AXIS_RT, *rtAxis, false);
			}
			else
			{
				// Xbox controller?
				mapping->set_axis(DC_AXIS_LT, AXIS_Z, false);
				mapping->set_axis(DC_AXIS_RT, AXIS_RZ, false);
				mapping->set_axis(DC_AXIS_X2, AXIS_RX, false);
				mapping->set_axis(DC_AXIS_Y2, AXIS_RY, false);
				
			}
		}
		return mapping;
	}

	virtual const char *get_button_name(u32 code) override
	{
		switch(code)
		{
		case KEYCODE_BACK:
			return "Back";
		case KEYCODE_DPAD_UP:
			return "DPad Up";
		case KEYCODE_DPAD_DOWN:
			return "DPad Down";
		case KEYCODE_DPAD_LEFT:
			return "DPad Left";
		case KEYCODE_DPAD_RIGHT:
			return "DPad Right";
		case KEYCODE_DPAD_CENTER:
			return "DPad Center";
		case KEYCODE_BUTTON_A:
			return "A";
		case KEYCODE_BUTTON_B:
			return "B";
		case KEYCODE_BUTTON_C:
			return "C";
		case KEYCODE_BUTTON_X:
			return "X";
		case KEYCODE_BUTTON_Y:
			return "Y";
		case KEYCODE_BUTTON_Z:
			return "Z";
		case KEYCODE_BUTTON_L1:
			return "L1";
		case KEYCODE_BUTTON_R1:
			return "R1";
		case KEYCODE_BUTTON_L2:
			return "L2";
		case KEYCODE_BUTTON_R2:
			return "R2";
		case KEYCODE_BUTTON_THUMBL:
			return "Thumb L";
		case KEYCODE_BUTTON_THUMBR:
			return "Thumb R";
		case KEYCODE_BUTTON_START:
			return "Start";
		case KEYCODE_BUTTON_SELECT:
			return "Select";
		case KEYCODE_BUTTON_MODE:
			return "Mode";
		default:
			return nullptr;
		}
	}

	virtual const char *get_axis_name(u32 code) override
	{
		switch(code)
		{
		case AXIS_X:
			return "X";
		case AXIS_Y:
			return "Y";
		case AXIS_Z:
			return "Z";
		case AXIS_RX:
			return "RX";
		case AXIS_RY:
			return "RY";
		case AXIS_RZ:
			return "RZ";
		case AXIS_LTRIGGER:
			return "Left Trigger";
		case AXIS_RTRIGGER:
			return "Right Trigger";
		case AXIS_HAT_X:
			return "Hat X";
		case AXIS_HAT_Y:
			return "Hat Y";
		case AXIS_GAS:
			return "Gas";
		case AXIS_BRAKE:
			return "Brake";
		case AXIS_RUDDER:
			return "Rudder";
		case AXIS_WHEEL:
			return "Wheel";
		case AXIS_THROTTLE:
			return "Throttle";
		default:
			return nullptr;
		}
	}

	static std::shared_ptr<AndroidGamepadDevice> GetAndroidGamepad(int id)
	{
		auto it = android_gamepads.find(id);
		if (it != android_gamepads.end())
			return it->second;
		else
			return NULL;
	}

	static void AddAndroidGamepad(std::shared_ptr<AndroidGamepadDevice> gamepad)
	{
		auto it = android_gamepads.find(gamepad->android_id);
		if (it != android_gamepads.end())
			GamepadDevice::Unregister(it->second);
		android_gamepads[gamepad->android_id] = gamepad;
		GamepadDevice::Register(gamepad);
	};

	static void RemoveAndroidGamepad(std::shared_ptr<AndroidGamepadDevice> gamepad)
	{
		android_gamepads.erase(gamepad->android_id);
		GamepadDevice::Unregister(gamepad);
	};

	void virtual_gamepad_event(int kcode, int joyx, int joyy, int lt, int rt)
	{
		// No virtual gamepad when the GUI is open: touch events only
		if (gui_is_open())
		{
			kcode = 0xffffffff;
			joyx = joyy = rt = lt = 0;
		}
		if (rt > 0)
		{
			if ((kcode & DC_BTN_A) == 0)
				// RT + A -> D (coin)
				kcode &= ~DC_BTN_D;
			if ((kcode & DC_BTN_B) == 0)
				// RT + B -> C (service)
				kcode &= ~DC_BTN_C;
			if ((kcode & DC_BTN_X) == 0)
				// RT + X -> Z (test)
				kcode &= ~DC_BTN_Z;
		}
		u32 changes = kcode ^ previous_kcode;
		for (int i = 0; i < 32; i++)
			if (changes & (1 << i))
				gamepad_btn_input(1 << i, (kcode & (1 << i)) == 0);
		gamepad_axis_input(DC_AXIS_X, joyx);
		gamepad_axis_input(DC_AXIS_Y, joyy);
		gamepad_axis_input(DC_AXIS_LT, lt);
		gamepad_axis_input(DC_AXIS_RT, rt);
		previous_kcode = kcode;
	}

	void rumble(float power, float inclination, u32 duration_ms) override
    {
        jboolean has_vibrator = jvm_attacher.getEnv()->CallBooleanMethod(input_device_manager, input_device_manager_rumble, android_id, power, inclination, duration_ms);
        _rumble_enabled = has_vibrator;
    }
	bool is_virtual_gamepad() override { return android_id == VIRTUAL_GAMEPAD_ID; }

	static const int VIRTUAL_GAMEPAD_ID = 0x12345678;	// must match the Java definition

protected:
	virtual void load_axis_min_max(u32 axis) override
	{
		auto axisIt = std::find(halfAxes.begin(), halfAxes.end(), axis);
		if (axisIt != halfAxes.end())
		{
			axis_min_values[axis] = 0;
			axis_ranges[axis] = 32767;
		}
		else
		{
			axis_min_values[axis] = -32768;
			axis_ranges[axis] = 65535;
		}
	}

private:
	int android_id;
	static std::map<int, std::shared_ptr<AndroidGamepadDevice>> android_gamepads;
	u32 previous_kcode = 0xffffffff;
	std::vector<int> fullAxes;
	std::vector<int> halfAxes;
};

std::map<int, std::shared_ptr<AndroidGamepadDevice>> AndroidGamepadDevice::android_gamepads;

class AndroidMouse : public SystemMouse
{
public:
	AndroidMouse(int maple_port) : SystemMouse("Android", maple_port)
	{
		_unique_id = "android_mouse";
		loadMapping();
	}
};

