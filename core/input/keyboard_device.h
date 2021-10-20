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
#pragma once
#include "types.h"
#include "gamepad_device.h"
#include "rend/gui.h"
#include <memory>

class KeyboardInputMapping : public InputMapping
{
public:
	KeyboardInputMapping()
	{
		name = "Keyboard";
		set_button(DC_BTN_A, 27);				// X
		set_button(DC_BTN_B, 6);				// C
		set_button(DC_BTN_X, 22);				// S
		set_button(DC_BTN_Y, 7);				// D
		set_button(DC_DPAD_UP, 82);
		set_button(DC_DPAD_DOWN, 81);
		set_button(DC_DPAD_LEFT, 80);
		set_button(DC_DPAD_RIGHT, 79);
		set_button(DC_BTN_START, 40);			// Return
		set_button(DC_AXIS_LT, 9);				// F
		set_button(DC_AXIS_RT, 25);				// V
		set_button(EMU_BTN_MENU, 43);			// TAB
		set_button(EMU_BTN_FFORWARD, 44);		// Space
		set_button(DC_AXIS_UP, 12);				// I
		set_button(DC_AXIS_DOWN, 14);			// K
		set_button(DC_AXIS_LEFT, 13);			// J
		set_button(DC_AXIS_RIGHT, 15);			// L
		set_button(DC_BTN_D, 4);				// Q (Coin)

		dirty = false;
	}
};

class KeyboardDevice : public GamepadDevice
{
protected:
	KeyboardDevice(int maple_port, const char* apiName, bool remappable = true)
		: GamepadDevice(maple_port, apiName, remappable) {
		_name = "Keyboard";
	}

	std::shared_ptr<InputMapping> getDefaultMapping() override {
		return std::make_shared<KeyboardInputMapping>();
	}

public:
	const char *get_button_name(u32 code) override
	{
		switch (code)
		{
		case 0x04:
			return "A";
		case 0x05:
			return "B";
		case 0x06:
			return "C";
		case 0x07:
			return "D";
		case 0x08:
			return "E";
		case 0x09:
			return "F";
		case 0x0A:
			return "G";
		case 0x0B:
			return "H";
		case 0x0C:
			return "I";
		case 0x0D:
			return "J";
		case 0x0E:
			return "K";
		case 0x0F:
			return "L";
		case 0x10:
			return "M";
		case 0x11:
			return "N";
		case 0x12:
			return "O";
		case 0x13:
			return "P";
		case 0x14:
			return "Q";
		case 0x15:
			return "R";
		case 0x16:
			return "S";
		case 0x17:
			return "T";
		case 0x18:
			return "U";
		case 0x19:
			return "V";
		case 0x1A:
			return "W";
		case 0x1B:
			return "X";
		case 0x1C:
			return "Y";
		case 0x1D:
			return "Z";

		case 0x1E:
			return "1";
		case 0x1F:
			return "2";
		case 0x20:
			return "3";
		case 0x21:
			return "4";
		case 0x22:
			return "5";
		case 0x23:
			return "6";
		case 0x24:
			return "7";
		case 0x25:
			return "8";
		case 0x26:
			return "9";
		case 0x27:
			return "0";

		case 0x28:
			return "Return";
		case 0x29:
			return "Escape";
		case 0x2A:
			return "Backspace";
		case 0x2B:
			return "Tab";
		case 0x2C:
			return "Space";

		case 0x2D:
			return "-";
		case 0x2E:
			return "=";
		case 0x2F:
			return "[";
		case 0x30:
			return "]";
		case 0x31:
			return "\\";
		case 0x32:
			return "#";		// non-US
		case 0x33:
			return ";";
		case 0x34:
			return "'";
		case 0x35:
			return "`";
		case 0x36:
			return ",";
		case 0x37:
			return ".";
		case 0x38:
			return "/";
		case 0x39:
			return "CapsLock";

		case 0x3A:
			return "F1";
		case 0x3B:
			return "F2";
		case 0x3C:
			return "F3";
		case 0x3D:
			return "F4";
		case 0x3E:
			return "F5";
		case 0x3F:
			return "F6";
		case 0x40:
			return "F7";
		case 0x41:
			return "F8";
		case 0x42:
			return "F9";
		case 0x43:
			return "F10";
		case 0x44:
			return "F11";
		case 0x45:
			return "F12";

		case 0x46:
			return "PrintScreen";
		case 0x47:
			return "ScrollLock";
		case 0x48:
			return "Pause";
		case 0x49:
			return "Insert";
		case 0x4A:
			return "Home";
		case 0x4B:
			return "Page Up";
		case 0x4C:
			return "Delete";
		case 0x4D:
			return "End";
		case 0x4E:
			return "Page Down";
		case 0x4F:
			return "Right";
		case 0x50:
			return "Left";
		case 0x51:
			return "Down";
		case 0x52:
			return "Up";

		case 0x53:
			return "NumLock";
		case 0x54:
			return "Num /";
		case 0x55:
			return "Num *";
		case 0x56:
			return "Num -";
		case 0x57:
			return "Num +";
		case 0x58:
			return "Num Enter";
		case 0x59:
			return "Num 1";
		case 0x5A:
			return "Num 2";
		case 0x5B:
			return "Num 3";
		case 0x5C:
			return "Num 4";
		case 0x5D:
			return "Num 5";
		case 0x5E:
			return "Num 6";
		case 0x5F:
			return "Num 7";
		case 0x60:
			return "Num 8";
		case 0x61:
			return "Num 9";
		case 0x62:
			return "Num 0";
		case 0x63:
			return "Num .";

		case 0x64:
			return "\\";	// non-US
		case 0x65:
			return "Application";
		case 0x66:
			return "Power";
		case 0x67:
			return "Num =";

		case 0x68:
			return "F13";
		case 0x69:
			return "F14";
		case 0x6A:
			return "F15";
		case 0x6B:
			return "F16";
		case 0x6C:
			return "F17";
		case 0x6D:
			return "F18";
		case 0x6E:
			return "F19";
		case 0x6F:
			return "F20";
		case 0x70:
			return "F21";
		case 0x71:
			return "F22";
		case 0x72:
			return "F23";
		case 0x73:
			return "F24";

		case 0x87:
			return "Int1";
		case 0x88:
			return "Int2";
		case 0x89:
			return "Yen";
		case 0x8A:
			return "Int4";
		case 0x8B:
			return "Int5";
		case 0x8C:
			return "Int6";
		case 0x8D:
			return "Int7";
		case 0x8E:
			return "Int8";
		case 0x8F:
			return "Int9";

		case 0x90:
			return "Hangul";
		case 0x91:
			return "Hanja";
		case 0x92:
			return "Katakana";
		case 0x93:
			return "Hiragana";
		case 0x94:
			return "Zenkaku/Hankaku";
		case 0x95:
			return "Lang6";
		case 0x96:
			return "Lang7";
		case 0x97:
			return "Lang8";
		case 0x98:
			return "Lang9";

		case 0xE0:
			return "Left Ctrl";
		case 0xE1:
			return "Left Shift";
		case 0xE2:
			return "Left Alt";
		case 0xE3:
			return "Left Meta";
		case 0xE4:
			return "Right Ctrl";
		case 0xE5:
			return "Right Shift";
		case 0xE6:
			return "Right Alt";
		case 0xE7:
			return "Right Meta";

		default:
			return nullptr;
		}
	}
};

template <typename Keycode>
class KeyboardDeviceTemplate : public KeyboardDevice
{
public:
	virtual void keyboard_input(Keycode keycode, bool pressed, int modifier_keys = 0);
	virtual ~KeyboardDeviceTemplate() = default;

protected:
	KeyboardDeviceTemplate(int maple_port, const char *apiName, bool remappable = true)
		: KeyboardDevice(maple_port, apiName, remappable), _modifier_keys(0), _kb_used(0) {}
	virtual u8 convert_keycode(Keycode keycode) = 0;

private:
	int _modifier_keys;
	u32 _kb_used;
};

enum DCKeyboardModifiers {
	DC_KBMOD_LEFTCTRL   = 0x01,
	DC_KBMOD_LEFTSHIFT  = 0x02,
	DC_KBMOD_LEFTALT    = 0x04,
	DC_KBMOD_LEFTGUI    = 0x08,
	DC_KBMOD_RIGHTCTRL  = 0x10,
	DC_KBMOD_RIGHTSHIFT = 0x20,
	DC_KBMOD_RIGHTALT   = 0x40,
	DC_KBMOD_S2         = 0x80,
};

extern u8 kb_key[4][6];	// normal keys pressed
extern u8 kb_shift[4];	// modifier keys pressed (bitmask)

static inline void setFlag(int& v, u32 bitmask, bool set)
{
	if (set)
		v |= bitmask;
	else
		v &= ~bitmask;
}

template <typename Keycode>
void KeyboardDeviceTemplate<Keycode>::keyboard_input(Keycode keycode, bool pressed, int modifier_keys)
{
	u8 dc_keycode = convert_keycode(keycode);
	// Some OSes (Mac OS) don't distinguish left and right modifier keys to we set them both.
	// But not for Alt since Right Alt is used as a special modifier keys on some international
	// keyboards.
	switch (dc_keycode)
	{
		case 0xE1: // Left Shift
		case 0xE5: // Right Shift
			setFlag(_modifier_keys, DC_KBMOD_LEFTSHIFT | DC_KBMOD_RIGHTSHIFT, pressed);
			break;
		case 0xE0: // Left Ctrl
		case 0xE4: // Right Ctrl
			setFlag(_modifier_keys, DC_KBMOD_LEFTCTRL | DC_KBMOD_RIGHTCTRL, pressed);
			break;
		case 0xE2: // Left Alt
			setFlag(_modifier_keys, DC_KBMOD_LEFTALT, pressed);
			break;
		case 0xE6: // Right Alt
			setFlag(_modifier_keys, DC_KBMOD_RIGHTALT, pressed);
			break;
		case 0xE7: // S2 special key
			setFlag(_modifier_keys, DC_KBMOD_S2, pressed);
			break;
		default:
			break;
	}
	const int port = maple_port();
	if (port >= 0 && port < (int)ARRAY_SIZE(kb_shift))
		kb_shift[port] = _modifier_keys;

	if (dc_keycode != 0 && dc_keycode < 0xE0)
	{
		gui_keyboard_key(dc_keycode, pressed, _modifier_keys);
		if (port >= 0 && port < (int)ARRAY_SIZE(kb_key))
		{
			if (pressed)
			{
				if (_kb_used < ARRAY_SIZE(kb_key[port]))
				{
					bool found = false;
					for (u32 i = 0; !found && i < _kb_used; i++)
					{
						if (kb_key[port][i] == dc_keycode)
							found = true;
					}
					if (!found)
						kb_key[port][_kb_used++] = dc_keycode;
				}
			}
			else
			{
				for (u32 i = 0; i < _kb_used; i++)
				{
					if (kb_key[port][i] == dc_keycode)
					{
						_kb_used--;
						for (u32 j = i; j < ARRAY_SIZE(kb_key[port]) - 1; j++)
							kb_key[port][j] = kb_key[port][j + 1];
						kb_key[port][ARRAY_SIZE(kb_key[port]) - 1] = 0;
						break;
					}
				}
			}
			kb_shift[port] |= modifier_keys;
		}
	}
	if (gui_keyboard_captured())
	{
		// chat: disable the keyboard controller. Only accept emu keys (menu, escape...)
		set_maple_port(-1);
		gamepad_btn_input(dc_keycode, pressed);
		set_maple_port(port);
	}
	// Do not map keyboard keys to gamepad buttons unless the GUI is open
	// or the corresponding maple device (if any) isn't a keyboard
	else if (gui_is_open()
			|| port == (int)ARRAY_SIZE(kb_key)
			|| (settings.platform.system == DC_PLATFORM_DREAMCAST && config::MapleMainDevices[port] != MDT_Keyboard)
			|| (settings.platform.system == DC_PLATFORM_NAOMI && settings.input.JammaSetup != JVS::Keyboard)
			|| settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		gamepad_btn_input(dc_keycode, pressed);
}
