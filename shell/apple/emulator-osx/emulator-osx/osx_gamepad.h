//
//  osx_gamepad.h
//  reicast-osx
//
//  Created by flyinghead on 26/02/2019.
//  Copyright © 2019 reicast. All rights reserved.
//
#include "input/gamepad_device.h"

class KbInputMapping : public InputMapping
{
public:
	KbInputMapping()
	{
		name = "OSX Keyboard";
		set_button(DC_BTN_A, kVK_ANSI_X);
		set_button(DC_BTN_B, kVK_ANSI_C);
		set_button(DC_BTN_X, kVK_ANSI_S);
		set_button(DC_BTN_Y, kVK_ANSI_D);
		set_button(DC_DPAD_UP, kVK_UpArrow);
		set_button(DC_DPAD_DOWN, kVK_DownArrow);
		set_button(DC_DPAD_LEFT, kVK_LeftArrow);
		set_button(DC_DPAD_RIGHT, kVK_RightArrow);
		set_button(DC_BTN_START, kVK_Return);
		set_button(EMU_BTN_TRIGGER_LEFT, kVK_ANSI_F);
		set_button(EMU_BTN_TRIGGER_RIGHT, kVK_ANSI_V);
		set_button(EMU_BTN_MENU, kVK_Tab);
		
		dirty = false;
	}
};

class OSXKbGamepadDevice : public GamepadDevice
{
public:
	OSXKbGamepadDevice(int maple_port) : GamepadDevice(maple_port, "OSX")
	{
		_name = "Keyboard";
		_unique_id = "osx_keyboard";
		if (!find_mapping())
			input_mapper = new KbInputMapping();
	}
};

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


