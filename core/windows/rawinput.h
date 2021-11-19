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
#include "build.h"
#ifndef TARGET_UWP
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "input/mouse.h"
#include "rend/gui.h"
#include <windows.h>

#include <algorithm>

namespace rawinput {

class RawMouse : public Mouse
{
public:
	RawMouse(int maple_port, const std::string& name, const std::string& uniqueId, HANDLE handle);

	void updateState(RAWMOUSE* state);

private:
	void buttonInput(Button button, u16 flags, u16 downFlag, u16 upFlag);

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
		loadMapping();
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
#endif
