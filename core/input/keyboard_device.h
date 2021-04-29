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
		set_button(EMU_BTN_TRIGGER_LEFT, 9);	// F
		set_button(EMU_BTN_TRIGGER_RIGHT, 25);	// V
		set_button(EMU_BTN_MENU, 43);			// TAB
		set_button(EMU_BTN_FFORWARD, 44);		// Space

		dirty = false;
	}
};

class KeyboardDevice : public GamepadDevice
{
protected:
	KeyboardDevice(int maple_port, const char* apiName, bool remappable = true)
		: GamepadDevice(maple_port, apiName, remappable)
	{
		_name = "Keyboard";
		if (!find_mapping())
			input_mapper = std::make_shared<KeyboardInputMapping>();
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
	const int port = maple_port();
	if (port < 0 || port > (int)ARRAY_SIZE(kb_key))
		return;

	u8 dc_keycode = convert_keycode(keycode);
	if (port < (int)ARRAY_SIZE(kb_key))
	{
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
		kb_shift[port] = _modifier_keys;

		if (dc_keycode != 0 && dc_keycode < 0xE0)
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
	// Do not map keyboard keys to gamepad buttons unless the GUI is open
	// or the corresponding maple device (if any) isn't a keyboard
	if (gui_is_open()
			|| port == (int)ARRAY_SIZE(kb_key)
			|| config::MapleMainDevices[port] != MDT_Keyboard)
		gamepad_btn_input(dc_keycode, pressed);
}
