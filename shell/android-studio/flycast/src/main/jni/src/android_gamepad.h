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
#include "input/mouse.h"
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

class AndroidGamepadDevice;

template<bool Arcade = false, bool Gamepad = false>
class DefaultInputMapping : public InputMapping
{
public:
	DefaultInputMapping(const AndroidGamepadDevice& gamepad);
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
		else
			return std::make_shared<DefaultInputMapping<>>(*this);
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

	void virtual_gamepad_event(int kcode, int joyx, int joyy, int lt, int rt, bool fastForward)
	{
		// No virtual gamepad when the GUI is open: touch events only
		if (gui_is_open())
		{
			kcode = 0xffffffff;
			joyx = joyy = rt = lt = 0;
		}
		if (settings.platform.system != DC_PLATFORM_DREAMCAST)
		{
			if (rt > 0)
			{
				if ((kcode & DC_BTN_A) == 0)
					// RT + A -> D (coin)
					kcode &= ~DC_BTN_D;
				if ((kcode & DC_BTN_B) == 0)
					// RT + B -> Service
					kcode &= ~DC_DPAD2_UP;
				if ((kcode & DC_BTN_X) == 0)
					// RT + X -> Test
					kcode &= ~DC_DPAD2_DOWN;
			}
			// arcade mapping: X -> btn2, Y -> btn3
			if ((kcode & DC_BTN_X) == 0)
			{
				kcode &= ~DC_BTN_C;
				kcode |= DC_BTN_X;
			}
			if ((kcode & DC_BTN_Y) == 0)
			{
				kcode &= ~DC_BTN_X;
				kcode |= DC_BTN_Y;
			}
			if (rt > 0)
				// naomi btn4
				kcode &= ~DC_BTN_Y;
			if (lt > 0)
				// naomi btn5
				kcode &= ~DC_BTN_Z;
		}
		u32 changes = kcode ^ previous_kcode;
		for (int i = 0; i < 32; i++)
			if (changes & (1 << i))
				gamepad_btn_input(1 << i, (kcode & (1 << i)) == 0);
		if (joyx >= 0)
			gamepad_axis_input(DC_AXIS_RIGHT, joyx | (joyx << 8));
		else
			gamepad_axis_input(DC_AXIS_LEFT, -joyx | (-joyx << 8));
		if (joyy >= 0)
			gamepad_axis_input(DC_AXIS_DOWN, joyy | (joyy << 8));
		else
			gamepad_axis_input(DC_AXIS_UP, -joyy | (-joyy << 8));
		gamepad_axis_input(DC_AXIS_LT, lt == 0 ? 0 : 0x7fff);
		gamepad_axis_input(DC_AXIS_RT, rt == 0 ? 0 : 0x7fff);
		previous_kcode = kcode;
		if (fastForward != previousFastForward)
			gamepad_btn_input(EMU_BTN_FFORWARD, fastForward);
		previousFastForward = fastForward;
	}

	void rumble(float power, float inclination, u32 duration_ms) override
    {
        jboolean has_vibrator = jvm_attacher.getEnv()->CallBooleanMethod(input_device_manager, input_device_manager_rumble, android_id, power, inclination, duration_ms);
        _rumble_enabled = has_vibrator;
    }
	bool is_virtual_gamepad() override { return android_id == VIRTUAL_GAMEPAD_ID; }

	bool hasHalfAxis(int axis) const { return std::find(halfAxes.begin(), halfAxes.end(), axis) != halfAxes.end(); }
	bool hasFullAxis(int axis) const { return std::find(fullAxes.begin(), fullAxes.end(), axis) != fullAxes.end(); }

	void resetMappingToDefault(bool arcade, bool gamepad) override
	{
		NOTICE_LOG(INPUT, "Resetting Android gamepad to default: %d %d", arcade, gamepad);
		if (arcade)
		{
			if (gamepad)
				input_mapper = std::make_shared<DefaultInputMapping<true, true>>(*this);
			else
				input_mapper = std::make_shared<DefaultInputMapping<true, false>>(*this);
		}
		else
			input_mapper = std::make_shared<DefaultInputMapping<false, false>>(*this);
	}

	static const int VIRTUAL_GAMEPAD_ID = 0x12345678;	// must match the Java definition

private:
	int android_id;
	static std::map<int, std::shared_ptr<AndroidGamepadDevice>> android_gamepads;
	u32 previous_kcode = 0xffffffff;
	bool previousFastForward = false;
	std::vector<int> fullAxes;
	std::vector<int> halfAxes;
};

std::map<int, std::shared_ptr<AndroidGamepadDevice>> AndroidGamepadDevice::android_gamepads;

template<bool Arcade, bool Gamepad>
inline DefaultInputMapping<Arcade, Gamepad>::DefaultInputMapping(const AndroidGamepadDevice& gamepad)
{
	name = Arcade ? Gamepad ? "Arcade Gamepad" : "Arcade Hitbox" : "Default";
	int ltAxis = AXIS_LTRIGGER;
	int rtAxis = AXIS_RTRIGGER;
	int rightStickX = AXIS_Z;
	int rightStickY = AXIS_RZ;
	if (!gamepad.hasHalfAxis(AXIS_LTRIGGER) || !gamepad.hasHalfAxis(AXIS_RTRIGGER))
	{
		if (gamepad.hasHalfAxis(AXIS_BRAKE) && gamepad.hasHalfAxis(AXIS_GAS))
		{
			ltAxis = AXIS_BRAKE;
			rtAxis = AXIS_GAS;
		}
		else if (gamepad.hasHalfAxis(AXIS_Z) && gamepad.hasHalfAxis(AXIS_RZ))
		{
			ltAxis = AXIS_Z;
			rtAxis = AXIS_RZ;
			rightStickX = AXIS_RX;
			rightStickY = AXIS_RY;
		}
		else
		{
			ltAxis = -1;
			rtAxis = -1;
		}
	}
	if (!gamepad.hasFullAxis(rightStickX) || !gamepad.hasFullAxis(rightStickY))
	{
		rightStickX = -1;
		rightStickY = -1;
	}
	else
	{
		set_axis(DC_AXIS2_LEFT, rightStickX, false);
		set_axis(DC_AXIS2_RIGHT, rightStickX, true);
		set_axis(DC_AXIS2_UP, rightStickY, false);
		set_axis(DC_AXIS2_DOWN, rightStickY, true);
	}

	if (Arcade)
	{
		if (Gamepad)
		{
			// 1  2  3  4  5  6
			// A  B  X  Y  L  R
			set_button(DC_BTN_A, KEYCODE_BUTTON_A);
			set_button(DC_BTN_B, KEYCODE_BUTTON_B);
			set_button(DC_BTN_C, KEYCODE_BUTTON_X);
			set_button(DC_BTN_X, KEYCODE_BUTTON_Y);
			if (ltAxis != -1)
			{
				set_axis(DC_AXIS_LT, ltAxis, true);
				set_button(DC_BTN_Y, KEYCODE_BUTTON_L1);
			}
			else
				set_button(DC_AXIS_LT, KEYCODE_BUTTON_L1);
			if (rtAxis != -1)
			{
				set_axis(DC_AXIS_RT, rtAxis, true);
				set_button(DC_BTN_Z, KEYCODE_BUTTON_R1);
			}
			else
				set_button(DC_AXIS_RT, KEYCODE_BUTTON_R1);
		}
		else
		{
			// Hitbox
			// 1  2  3  4  5  6  7  8
			// X  Y  R1 A  B  R2 L1 L2
			set_button(DC_BTN_A, KEYCODE_BUTTON_X);
			set_button(DC_BTN_B, KEYCODE_BUTTON_Y);
			set_button(DC_BTN_C, KEYCODE_BUTTON_R1);
			set_button(DC_BTN_X, KEYCODE_BUTTON_A);
			set_button(DC_BTN_Y, KEYCODE_BUTTON_B);
			if (rtAxis != -1)
				set_axis(DC_BTN_Z, rtAxis, true);
			set_button(DC_DPAD2_LEFT, KEYCODE_BUTTON_L1);	// L1 (Naomi button 7)
			if (ltAxis != -1)
				set_axis(DC_DPAD2_RIGHT, ltAxis, true);		// L2 (Naomi button 8)
		}
	}
	else
	{
		set_button(DC_BTN_A, KEYCODE_BUTTON_A);
		set_button(DC_BTN_B, KEYCODE_BUTTON_B);
		set_button(DC_BTN_X, KEYCODE_BUTTON_X);
		set_button(DC_BTN_Y, KEYCODE_BUTTON_Y);
		if (rtAxis != -1)
		{
			set_axis(DC_AXIS_RT, rtAxis, true);
			set_button(DC_BTN_C, KEYCODE_BUTTON_R1);
		}
		else
			set_button(DC_AXIS_RT, KEYCODE_BUTTON_R1);
		if (ltAxis != -1)
		{
			set_axis(DC_AXIS_LT, ltAxis, true);
			set_button(DC_BTN_Z, KEYCODE_BUTTON_L1);
		}
		else
			set_button(DC_AXIS_LT, KEYCODE_BUTTON_L1);

	}
	set_button(DC_BTN_START, KEYCODE_BUTTON_START);
	set_button(DC_DPAD_UP, KEYCODE_DPAD_UP);
	set_button(DC_DPAD_DOWN, KEYCODE_DPAD_DOWN);
	set_button(DC_DPAD_LEFT, KEYCODE_DPAD_LEFT);
	set_button(DC_DPAD_RIGHT, KEYCODE_DPAD_RIGHT);
	set_button(EMU_BTN_MENU, KEYCODE_BUTTON_SELECT);

	set_axis(DC_AXIS_LEFT, AXIS_X, false);
	set_axis(DC_AXIS_RIGHT, AXIS_X, true);
	set_axis(DC_AXIS_UP, AXIS_Y, false);
	set_axis(DC_AXIS_DOWN, AXIS_Y, true);

	dirty = false;
}

class AndroidMouse : public SystemMouse
{
public:
	AndroidMouse(int maple_port) : SystemMouse("Android", maple_port)
	{
		_unique_id = "android_mouse";
		loadMapping();
	}
};

