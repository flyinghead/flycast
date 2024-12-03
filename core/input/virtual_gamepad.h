/*
	Copyright 2024 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include "gamepad_device.h"
#include "ui/vgamepad.h"

class VirtualGamepad : public GamepadDevice
{
public:
	VirtualGamepad(const char *api_name, int maple_port = 0)
		: GamepadDevice(maple_port, api_name, false)
	{
		_name = "Virtual Gamepad";
		_unique_id = "virtual_gamepad_uid";
		input_mapper = std::make_shared<IdentityInputMapping>();
		// hasAnalogStick = true; // TODO has an analog stick but input mapping isn't persisted

		leftTrigger = DC_AXIS_LT;
		rightTrigger = DC_AXIS_RT;
	}

	bool is_virtual_gamepad() override {
		return true;
	};

	// normalized coordinates [-1, 1]
	void joystickInput(float x, float y)
	{
		vgamepad::setAnalogStick(x, y);
		int joyx = std::round(x * 32767.f);
		int joyy = std::round(y * 32767.f);
		if (joyx >= 0)
			gamepad_axis_input(DC_AXIS_RIGHT, joyx);
		else
			gamepad_axis_input(DC_AXIS_LEFT, -joyx);
		if (joyy >= 0)
			gamepad_axis_input(DC_AXIS_DOWN, joyy);
		else
			gamepad_axis_input(DC_AXIS_UP, -joyy);
	}

	void releaseAll()
	{
		for (int i = 0; i < 32; i++)
			if (buttonState & (1 << i))
				gamepad_btn_input(1 << i, false);
		buttonState = 0;
		joystickInput(0, 0);
		gamepad_axis_input(DC_AXIS_LT, 0);
		gamepad_axis_input(DC_AXIS_RT, 0);
		if (previousFastForward)
			gamepad_btn_input(EMU_BTN_FFORWARD, false);
		previousFastForward = false;
	}

	virtual bool handleButtonInput(u32& state, u32 key, bool pressed) {
		// can be overridden in derived classes to handle specific key combos
		// (iOS up+down or left+right)
		return false;
	}

	void buttonInput(vgamepad::ControlId controlId, bool pressed)
	{
		u32 kcode = vgamepad::controlToDcKey(controlId);
		if (kcode == 0)
			return;
		if (handleButtonInput(buttonState, kcode, pressed))
			return;
		if (kcode == DC_AXIS_LT) {
			gamepad_axis_input(DC_AXIS_LT, pressed ? 0x7fff : 0);
		}
		else if (kcode == DC_AXIS_RT) {
			gamepad_axis_input(DC_AXIS_RT, pressed ? 0x7fff : 0);
		}
		else if (kcode == EMU_BTN_SRVMODE) {
			if (pressed)
				vgamepad::toggleServiceMode();
		}
		else
		{
			if (pressed)
				buttonState |= kcode;
			else
				buttonState &= ~kcode;
			if ((kcode & (DC_DPAD_LEFT | DC_DPAD_RIGHT)) != 0
				&& (kcode & (DC_DPAD_UP | DC_DPAD_DOWN)) != 0)
			{
				// diagonals
				gamepad_btn_input(kcode & (DC_DPAD_LEFT | DC_DPAD_RIGHT), pressed);
				gamepad_btn_input(kcode & (DC_DPAD_UP | DC_DPAD_DOWN), pressed);
			}
			else {
				gamepad_btn_input(kcode, pressed);
			}
		}
	}

private:
	u32 buttonState = 0;
	bool previousFastForward = false;
};
