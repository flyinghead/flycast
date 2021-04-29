/*
	Copyright 2021 flyinghead

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
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "rend/gui.h"
#include <windows.h>

extern int screen_width, screen_height;

namespace rawinput {

class RawMouseInputMapping : public InputMapping
{
public:
	RawMouseInputMapping()
	{
		name = "Mouse";
		set_button(DC_BTN_A, 0);	// Left
		set_button(DC_BTN_B, 2);	// Right
		set_button(DC_BTN_START, 1);// Middle

		dirty = false;
	}
};

class RawMouse : public GamepadDevice
{
public:
	RawMouse(int maple_port, const std::string& name, const std::string& uniqueId, HANDLE handle);

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

	const char *get_button_name(u32 code) override
	{
		switch (code)
		{
		case 0:
			return "Left Button";
		case 2:
			return "Right Button";
		case 1:
			return "Middle Button";
		case 3:
			return "Button 4";
		case 4:
			return "Button 5";
		default:
			return nullptr;
		}
	}

	void updateState(RAWMOUSE* state);

private:
	void buttonInput(u32 buttonId, u16 flags, u16 downFlag, u16 upFlag);

	HANDLE handle = NULL;
};

class RawKeyboard : public KeyboardDeviceTemplate<u8>
{
public:
	RawKeyboard(int maple_port, const std::string& name, const std::string& uniqueId, HANDLE handle)
		: KeyboardDeviceTemplate(maple_port, "RAW"), handle(handle)
	{
		this->_name = name;
		this->_unique_id = uniqueId;
		std::replace(this->_unique_id.begin(), this->_unique_id.end(), '=', '_');
		std::replace(this->_unique_id.begin(), this->_unique_id.end(), '[', '_');
		std::replace(this->_unique_id.begin(), this->_unique_id.end(), ']', '_');
		if (!find_mapping())
			input_mapper = std::make_shared<KeyboardInputMapping>();
	}

protected:
	virtual u8 convert_keycode(u8 scancode) override
	{
		if (settings.input.keyboardLangId != KeyboardLayout::US && scancode == 0x31)	// US: backslash and pipe
			return (u8)0x32;	// non-US: hash and tilde
		else
			return (u8)scancode;
	}
private:
	HANDLE handle = NULL;
};

void init();
void term();

}
