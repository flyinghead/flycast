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
		set_button(EMU_BTN_FFORWARD, kVK_Space);
		
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
			input_mapper = std::make_shared<KbInputMapping>();
	}

	virtual const char *get_button_name(u32 code) override
	{
		switch(code)
		{
		case kVK_ANSI_A:
			return "A";
		case kVK_ANSI_B:
			return "B";
		case kVK_ANSI_C:
			return "C";
		case kVK_ANSI_D:
			return "D";
		case kVK_ANSI_E:
			return "E";
		case kVK_ANSI_F:
			return "F";
		case kVK_ANSI_G:
			return "G";
		case kVK_ANSI_H:
			return "H";
		case kVK_ANSI_I:
			return "I";
		case kVK_ANSI_J:
			return "J";
		case kVK_ANSI_K:
			return "K";
		case kVK_ANSI_L:
			return "L";
		case kVK_ANSI_M:
			return "M";
		case kVK_ANSI_N:
			return "N";
		case kVK_ANSI_O:
			return "O";
		case kVK_ANSI_P:
			return "P";
		case kVK_ANSI_Q:
			return "Q";
		case kVK_ANSI_R:
			return "R";
		case kVK_ANSI_S:
			return "S";
		case kVK_ANSI_T:
			return "T";
		case kVK_ANSI_U:
			return "U";
		case kVK_ANSI_V:
			return "V";
		case kVK_ANSI_W:
			return "W";
		case kVK_ANSI_X:
			return "X";
		case kVK_ANSI_Y:
			return "Y";
		case kVK_ANSI_Z:
			return "Z";

		case kVK_UpArrow:
			return "Up";
		case kVK_DownArrow:
			return "Down";
		case kVK_LeftArrow:
			return "Left";
		case kVK_RightArrow:
			return "Right";
		case kVK_Return:
			return "Return";
		case kVK_Tab:
			return "Tab";
		case kVK_Space:
			return "Space";
		case kVK_Delete:
			return "Delete";
		case kVK_Escape:
			return "Escape";
		case kVK_Help:
			return "Help";
		case kVK_Home:
			return "Home";
		case kVK_PageUp:
			return "Page Up";
		case kVK_PageDown:
			return "Page Down";
		case kVK_ForwardDelete:
			return "Fwd Delete";
		case kVK_End:
			return "End";

		case kVK_ANSI_1:
			return "1";
		case kVK_ANSI_2:
			return "2";
		case kVK_ANSI_3:
			return "3";
		case kVK_ANSI_4:
			return "4";
		case kVK_ANSI_5:
			return "5";
		case kVK_ANSI_6:
			return "6";
		case kVK_ANSI_7:
			return "7";
		case kVK_ANSI_8:
			return "8";
		case kVK_ANSI_9:
			return "9";
		case kVK_ANSI_0:
			return "0";

		case kVK_ANSI_Equal:
			return "=";
		case kVK_ANSI_Minus:
			return "-";
		case kVK_ANSI_RightBracket:
			return "]";
		case kVK_ANSI_LeftBracket:
			return "[";
		case kVK_ANSI_Quote:
			return "'";
		case kVK_ANSI_Semicolon:
			return ";";
		case kVK_ANSI_Backslash:
			return "\\";
		case kVK_ANSI_Comma:
			return ",";
		case kVK_ANSI_Slash:
			return "/";
		case kVK_ANSI_Period:
			return ".";
		case kVK_ANSI_Grave:
			return "`";

		case kVK_ANSI_KeypadDecimal:
			return "Keypad .";
		case kVK_ANSI_KeypadMultiply:
			return "Keypad *";
		case kVK_ANSI_KeypadPlus:
			return "Keypad +";
		case kVK_ANSI_KeypadClear:
			return "Keypad Clear";
		case kVK_ANSI_KeypadDivide:
			return "Keypad /";
		case kVK_ANSI_KeypadEnter:
			return "Keypad Enter";
		case kVK_ANSI_KeypadMinus:
			return "Keypad -";
		case kVK_ANSI_KeypadEquals:
			return "Keypad =";
		case kVK_ANSI_Keypad0:
			return "Keypad 0";
		case kVK_ANSI_Keypad1:
			return "Keypad 1";
		case kVK_ANSI_Keypad2:
			return "Keypad 2";
		case kVK_ANSI_Keypad3:
			return "Keypad 3";
		case kVK_ANSI_Keypad4:
			return "Keypad 4";
		case kVK_ANSI_Keypad5:
			return "Keypad 5";
		case kVK_ANSI_Keypad6:
			return "Keypad 6";
		case kVK_ANSI_Keypad7:
			return "Keypad 7";
		case kVK_ANSI_Keypad8:
			return "Keypad 8";
		case kVK_ANSI_Keypad9:
			return "Keypad 9";

		case kVK_F1:
			return "F1";
		case kVK_F2:
			return "F2";
		case kVK_F3:
			return "F3";
		case kVK_F4:
			return "F4";
		case kVK_F5:
			return "F5";
		case kVK_F6:
			return "F6";
		case kVK_F7:
			return "F7";
		case kVK_F8:
			return "F8";
		case kVK_F9:
			return "F9";
		case kVK_F10:
			return "F10";
		case kVK_F11:
			return "F11";
		case kVK_F12:
			return "F12";

		default:
			return nullptr;
		}
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


