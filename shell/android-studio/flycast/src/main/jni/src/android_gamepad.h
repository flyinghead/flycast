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
#include "input/virtual_gamepad.h"
#include "jni_util.h"
#include "oslib/i18n.h"
#include <algorithm>
#include <android/input.h>

extern jobject inputDeviceManager;
extern jmethodID inputDeviceManager_rumble;

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
		set_button(DC_BTN_A, AKEYCODE_DPAD_CENTER);
		set_button(DC_DPAD_UP, AKEYCODE_DPAD_UP);
		set_button(DC_DPAD_DOWN, AKEYCODE_DPAD_DOWN);
		set_button(DC_DPAD_LEFT, AKEYCODE_DPAD_LEFT);
		set_button(DC_DPAD_RIGHT, AKEYCODE_DPAD_RIGHT);
		set_button(EMU_BTN_MENU, AKEYCODE_BACK);

		dirty = false;
	}
};

class AndroidGamepadDevice : public GamepadDevice
{
public:
	AndroidGamepadDevice(int maple_port, int id, const char *name, const char *unique_id,
			const std::vector<int>& fullAxes, const std::vector<int>& halfAxes)
		: GamepadDevice(maple_port, "Android"), android_id(id),
		  fullAxes(fullAxes), halfAxes(halfAxes)
	{
		_name = name;
		_unique_id = unique_id;
		INFO_LOG(INPUT, "Android: Opened joystick %d on port %d: '%s' descriptor '%s'", id, maple_port, _name.c_str(), _unique_id.c_str());

		loadMapping();
		for (int axis : halfAxes)
			input_mapper->addTrigger(axis, false);
		for (int axis : fullAxes)
			input_mapper->deleteTrigger(axis);
		save_mapping();
		hasAnalogStick = !fullAxes.empty();
	}
	~AndroidGamepadDevice() override {
		INFO_LOG(INPUT, "Android: Joystick '%s' on port %d disconnected", _name.c_str(), maple_port());
	}

	std::shared_ptr<InputMapping> getDefaultMapping() override {
		if (_name == "SHIELD Remote")
			return std::make_shared<ShieldRemoteInputMapping>();
		else
			return std::make_shared<DefaultInputMapping<>>(*this);
	}

	const char *get_button_name(u32 code) override
	{
		using namespace i18n;
		switch(code)
		{
		case AKEYCODE_BACK:
			return T("Back");
		case AKEYCODE_DPAD_UP:
			return T("DPad Up");
		case AKEYCODE_DPAD_DOWN:
			return T("DPad Down");
		case AKEYCODE_DPAD_LEFT:
			return T("DPad Left");
		case AKEYCODE_DPAD_RIGHT:
			return T("DPad Right");
		case AKEYCODE_DPAD_CENTER:
			return T("DPad Center");
		case AKEYCODE_BUTTON_A:
			return "A";
		case AKEYCODE_BUTTON_B:
			return "B";
		case AKEYCODE_BUTTON_C:
			return "C";
		case AKEYCODE_BUTTON_X:
			return "X";
		case AKEYCODE_BUTTON_Y:
			return "Y";
		case AKEYCODE_BUTTON_Z:
			return "Z";
		case AKEYCODE_BUTTON_L1:
			return "L1";
		case AKEYCODE_BUTTON_R1:
			return "R1";
		case AKEYCODE_BUTTON_L2:
			return "L2";
		case AKEYCODE_BUTTON_R2:
			return "R2";
		case AKEYCODE_BUTTON_THUMBL:
			return "L3";
		case AKEYCODE_BUTTON_THUMBR:
			return "R3";
		case AKEYCODE_BUTTON_START:
			return T("Start");
		case AKEYCODE_BUTTON_SELECT:
			return T("Select");
		case AKEYCODE_BUTTON_MODE:
			return T("Mode");
		default:
			return nullptr;
		}
	}

	const char *get_axis_name(u32 code) override
	{
		using namespace i18n;
		switch(code)
		{
		case AMOTION_EVENT_AXIS_X:
			return "X";
		case AMOTION_EVENT_AXIS_Y:
			return "Y";
		case AMOTION_EVENT_AXIS_Z:
			return "Z";
		case AMOTION_EVENT_AXIS_RX:
			return "RX";
		case AMOTION_EVENT_AXIS_RY:
			return "RY";
		case AMOTION_EVENT_AXIS_RZ:
			return "RZ";
		case AMOTION_EVENT_AXIS_LTRIGGER:
			return T("Left Trigger");
		case AMOTION_EVENT_AXIS_RTRIGGER:
			return T("Right Trigger");
		case AMOTION_EVENT_AXIS_HAT_X:
			return T("Hat X");
		case AMOTION_EVENT_AXIS_HAT_Y:
			return T("Hat Y");
		case AMOTION_EVENT_AXIS_GAS:
			return T("Gas");
		case AMOTION_EVENT_AXIS_BRAKE:
			return T("Brake");
		case AMOTION_EVENT_AXIS_RUDDER:
			return T("Rudder");
		case AMOTION_EVENT_AXIS_WHEEL:
			return T("Wheel");
		case AMOTION_EVENT_AXIS_THROTTLE:
			return T("Throttle");
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

	static void RemoveAll()
	{
		for (auto [id, gamepad] : android_gamepads)
			GamepadDevice::Unregister(gamepad);
		android_gamepads.clear();
	}

	void rumble(float power, float inclination, u32 duration_ms) override
    {
		power *= rumblePower / 100.f;
        jboolean has_vibrator = jni::env()->CallBooleanMethod(inputDeviceManager, inputDeviceManager_rumble, android_id, power, inclination, duration_ms);
        rumbleEnabled = has_vibrator;
    }
	void setRumbleEnabled(bool rumbleEnabled) {
		this->rumbleEnabled = rumbleEnabled;
	}

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

private:
	int android_id;
	static std::map<int, std::shared_ptr<AndroidGamepadDevice>> android_gamepads;
	std::vector<int> fullAxes;
	std::vector<int> halfAxes;
};

std::map<int, std::shared_ptr<AndroidGamepadDevice>> AndroidGamepadDevice::android_gamepads;

template<bool Arcade, bool Gamepad>
inline DefaultInputMapping<Arcade, Gamepad>::DefaultInputMapping(const AndroidGamepadDevice& gamepad)
{
	name = Arcade ? Gamepad ? "Arcade Gamepad" : "Arcade Hitbox" : "Default";
	int ltAxis = AMOTION_EVENT_AXIS_LTRIGGER;
	int rtAxis = AMOTION_EVENT_AXIS_RTRIGGER;
	int rightStickX = AMOTION_EVENT_AXIS_Z;
	int rightStickY = AMOTION_EVENT_AXIS_RZ;
	if (!gamepad.hasHalfAxis(AMOTION_EVENT_AXIS_LTRIGGER) || !gamepad.hasHalfAxis(AMOTION_EVENT_AXIS_RTRIGGER))
	{
		if (gamepad.hasHalfAxis(AMOTION_EVENT_AXIS_BRAKE) && gamepad.hasHalfAxis(AMOTION_EVENT_AXIS_GAS))
		{
			ltAxis = AMOTION_EVENT_AXIS_BRAKE;
			rtAxis = AMOTION_EVENT_AXIS_GAS;
		}
		else if (gamepad.hasHalfAxis(AMOTION_EVENT_AXIS_Z) && gamepad.hasHalfAxis(AMOTION_EVENT_AXIS_RZ))
		{
			ltAxis = AMOTION_EVENT_AXIS_Z;
			rtAxis = AMOTION_EVENT_AXIS_RZ;
			rightStickX = AMOTION_EVENT_AXIS_RX;
			rightStickY = AMOTION_EVENT_AXIS_RY;
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
			set_button(DC_BTN_A, AKEYCODE_BUTTON_A);
			set_button(DC_BTN_B, AKEYCODE_BUTTON_B);
			set_button(DC_BTN_C, AKEYCODE_BUTTON_X);
			set_button(DC_BTN_X, AKEYCODE_BUTTON_Y);
			if (ltAxis != -1)
			{
				set_axis(DC_AXIS_LT, ltAxis, true);
				set_button(DC_BTN_Y, AKEYCODE_BUTTON_L1);
			}
			else
				set_button(DC_AXIS_LT, AKEYCODE_BUTTON_L1);
			if (rtAxis != -1)
			{
				set_axis(DC_AXIS_RT, rtAxis, true);
				set_button(DC_BTN_Z, AKEYCODE_BUTTON_R1);
			}
			else
				set_button(DC_AXIS_RT, AKEYCODE_BUTTON_R1);
		}
		else
		{
			// Hitbox
			// 1  2  3  4  5  6  7  8
			// X  Y  R1 A  B  R2 L1 L2
			set_button(DC_BTN_A, AKEYCODE_BUTTON_X);
			set_button(DC_BTN_B, AKEYCODE_BUTTON_Y);
			set_button(DC_BTN_C, AKEYCODE_BUTTON_R1);
			set_button(DC_BTN_X, AKEYCODE_BUTTON_A);
			set_button(DC_BTN_Y, AKEYCODE_BUTTON_B);
			if (rtAxis != -1)
				set_axis(DC_BTN_Z, rtAxis, true);
			set_button(DC_DPAD2_LEFT, AKEYCODE_BUTTON_L1);	// L1 (Naomi button 7)
			if (ltAxis != -1)
				set_axis(DC_DPAD2_RIGHT, ltAxis, true);		// L2 (Naomi button 8)
		}
	}
	else
	{
		set_button(DC_BTN_A, AKEYCODE_BUTTON_A);
		set_button(DC_BTN_B, AKEYCODE_BUTTON_B);
		set_button(DC_BTN_X, AKEYCODE_BUTTON_X);
		set_button(DC_BTN_Y, AKEYCODE_BUTTON_Y);
		if (rtAxis != -1)
		{
			set_axis(DC_AXIS_RT, rtAxis, true);
			set_button(DC_BTN_C, AKEYCODE_BUTTON_R1);
		}
		else
			set_button(DC_AXIS_RT, AKEYCODE_BUTTON_R1);
		if (ltAxis != -1)
		{
			set_axis(DC_AXIS_LT, ltAxis, true);
			set_button(DC_BTN_Z, AKEYCODE_BUTTON_L1);
		}
		else
			set_button(DC_AXIS_LT, AKEYCODE_BUTTON_L1);

	}
	set_button(DC_BTN_START, AKEYCODE_BUTTON_START);
	set_button(DC_DPAD_UP, AKEYCODE_DPAD_UP);
	set_button(DC_DPAD_DOWN, AKEYCODE_DPAD_DOWN);
	set_button(DC_DPAD_LEFT, AKEYCODE_DPAD_LEFT);
	set_button(DC_DPAD_RIGHT, AKEYCODE_DPAD_RIGHT);
	set_button(EMU_BTN_MENU, AKEYCODE_BUTTON_SELECT);

	set_axis(DC_AXIS_LEFT, AMOTION_EVENT_AXIS_X, false);
	set_axis(DC_AXIS_RIGHT, AMOTION_EVENT_AXIS_X, true);
	set_axis(DC_AXIS_UP, AMOTION_EVENT_AXIS_Y, false);
	set_axis(DC_AXIS_DOWN, AMOTION_EVENT_AXIS_Y, true);

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

class AndroidVirtualGamepad : public VirtualGamepad
{
public:
	AndroidVirtualGamepad(bool rumbleEnabled) : VirtualGamepad("Flycast") {
		this->rumbleEnabled = rumbleEnabled;
	}

	void rumble(float power, float inclination, u32 duration_ms) override
    {
		power *= rumblePower / 100.f;
        jboolean has_vibrator = jni::env()->CallBooleanMethod(inputDeviceManager, inputDeviceManager_rumble, GAMEPAD_ID, power, inclination, duration_ms);
        rumbleEnabled = has_vibrator;
    }

	static constexpr int GAMEPAD_ID = 0x12345678;	// must match the Java definition
};
