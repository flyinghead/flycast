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

#include "input/gamepad_device.h"

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

};

class DefaultInputMapping : public InputMapping
{
public:
	DefaultInputMapping()
	{
		name = "Default";
		set_button(DC_BTN_Y, 100);
		set_button(DC_BTN_B, 97);
		set_button(DC_BTN_A, 96);
		set_button(DC_BTN_X, 99);
		set_button(DC_BTN_START, 108);
		set_button(DC_DPAD_UP, 19);
		set_button(DC_DPAD_DOWN, 20);
		set_button(DC_DPAD_LEFT, 21);
		set_button(DC_DPAD_RIGHT, 22);
		set_button(EMU_BTN_MENU, 4);

		set_axis(DC_AXIS_X, AXIS_X, false);
		set_axis(DC_AXIS_Y, AXIS_Y, false);
		set_axis(DC_AXIS_LT, AXIS_LTRIGGER, false);
		set_axis(DC_AXIS_RT, AXIS_RTRIGGER, false);

		dirty = false;
	}
};

class ShieldRemoteInputMapping : public InputMapping
{
public:
	ShieldRemoteInputMapping()
	{
		name = "Default";
		set_button(DC_BTN_A, 23);
		set_button(DC_DPAD_UP, 19);
		set_button(DC_DPAD_DOWN, 20);
		set_button(DC_DPAD_LEFT, 21);
		set_button(DC_DPAD_RIGHT, 22);
		set_button(EMU_BTN_MENU, 4);

		dirty = false;
	}
};

class AndroidGamepadDevice : public GamepadDevice
{
public:
	AndroidGamepadDevice(int maple_port, int id, const char *name, const char *unique_id)
		: GamepadDevice(maple_port, "Android", id != VIRTUAL_GAMEPAD_ID), android_id(id)
	{
		_name = name;
		_unique_id = unique_id;
		printf("Android: Opened joystick %d on port %d: '%s' descriptor '%s'", id, maple_port, _name.c_str(), _unique_id.c_str());
		if (id == VIRTUAL_GAMEPAD_ID)
		{
			input_mapper = new IdentityInputMapping();
			axis_min_values[DC_AXIS_X] = -128;
			axis_ranges[DC_AXIS_X] = 255;
			axis_min_values[DC_AXIS_Y] = -128;
			axis_ranges[DC_AXIS_Y] = 255;
			axis_min_values[DC_AXIS_LT] = 0;
			axis_ranges[DC_AXIS_LT] = 255;
			axis_min_values[DC_AXIS_RT] = 0;
			axis_ranges[DC_AXIS_RT] = 255;
			printf("\n");
		}
		else if (!find_mapping())
		{
			if (_name == "SHIELD Remote")
				input_mapper = new ShieldRemoteInputMapping();
			else
				input_mapper = new DefaultInputMapping();
			save_mapping();
			printf("using default mapping\n");
		}
		else
			printf("using custom mapping '%s'\n", input_mapper->name.c_str());
	}
	virtual ~AndroidGamepadDevice() override
	{
		printf("Android: Joystick '%s' on port %d disconnected\n", _name.c_str(), maple_port());
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
			kcode = 0xffff;
			joyx = joyy = rt = lt = 0;
		}
		u16 changes = kcode ^ previous_kcode;
		for (int i = 0; i < 16; i++)
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
		if (axis == AXIS_LTRIGGER || axis == AXIS_RTRIGGER)
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
	u16 previous_kcode = 0xffff;
};

std::map<int, std::shared_ptr<AndroidGamepadDevice>> AndroidGamepadDevice::android_gamepads;

class MouseInputMapping : public InputMapping
{
public:
	MouseInputMapping()
	{
		name = "Android Mouse";
		set_button(DC_BTN_A, 1);
		set_button(DC_BTN_B, 2);
		set_button(DC_BTN_START, 4);

		dirty = false;
	}
};

class AndroidMouseGamepadDevice : public GamepadDevice
{
public:
	AndroidMouseGamepadDevice(int maple_port) : GamepadDevice(maple_port, "Android")
	{
		_name = "Mouse";
		_unique_id = "android_mouse";
		if (!find_mapping())
			input_mapper = new MouseInputMapping();
	}

	bool gamepad_btn_input(u32 code, bool pressed) override
	{
		if (gui_is_open())
			// Don't register mouse clicks as gamepad presses when gui is open
			// This makes the gamepad presses to be handled first and the mouse position to be ignored
			// TODO Make this generic
			return false;
		else
			return GamepadDevice::gamepad_btn_input(code, pressed);
	}
};
// FIXME Don't connect it by default or any screen touch will register as button A press
AndroidMouseGamepadDevice mouse_gamepad(-1);

