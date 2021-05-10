//
//  osx_gamepad.h
//  reicast-osx
//
//  Created by flyinghead on 26/02/2019.
//  Copyright © 2019 reicast. All rights reserved.
//
#include "input/gamepad_device.h"

class MouseInputMapping : public InputMapping
{
public:
	MouseInputMapping()
	{
		name = "OSX Mouse";
		set_button(DC_BTN_A, 1);		// Left button
		set_button(DC_BTN_B, 2);		// Right button
		set_button(DC_BTN_START, 3);	// Other button

		dirty = false;
	}
};

class OSXMouseGamepadDevice : public GamepadDevice
{
public:
	OSXMouseGamepadDevice(int maple_port) : GamepadDevice(maple_port, "OSX")
	{
		_name = "Mouse";
		_unique_id = "osx_mouse";
		if (!find_mapping())
			input_mapper = std::make_shared<MouseInputMapping>();
	}

	bool gamepad_btn_input(u32 code, bool pressed) override
	{
		if (gui_is_open() && !is_detecting_input())
			// Don't register mouse clicks as gamepad presses when gui is open
			// This makes the gamepad presses to be handled first and the mouse position to be ignored
			// TODO Make this generic
			return false;
		else
			return GamepadDevice::gamepad_btn_input(code, pressed);
	}

	virtual const char *get_button_name(u32 code) override
	{
		switch (code)
		{
		case 1:
			return "Left Button";
		case 2:
			return "Right Button";
		case 3:
			return "Other Button";
		default:
			return nullptr;
		}
	}
};


