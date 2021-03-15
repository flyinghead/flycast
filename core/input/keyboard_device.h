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
#include "types.h"

#include <memory>

class KeyboardDevice
{
public:
	virtual const char* name() = 0;
	int maple_port() { return _maple_port; }
	void keyboard_character(char c);
	std::string get_character_input();
	virtual ~KeyboardDevice() = default;

	static KeyboardDevice *GetInstance() { return _instance; }

protected:
	KeyboardDevice(int maple_port) : _maple_port(maple_port) { _instance = this; }

private:
	int _maple_port;
	std::string char_input;
	static KeyboardDevice *_instance;
};

template <typename Keycode>
class KeyboardDeviceTemplate : public KeyboardDevice
{
public:
	virtual void keyboard_input(Keycode keycode, bool pressed, int modifier_keys = 0);

protected:
	KeyboardDeviceTemplate(int maple_port) : KeyboardDevice(maple_port), _modifier_keys(0), _kb_used(0) {}
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

extern u8 kb_key[6];		// normal keys pressed
extern u8 kb_shift; 		// modifier keys pressed (bitmask)

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
	kb_shift = _modifier_keys;

	if (dc_keycode != 0 && dc_keycode < 0xE0)
	{
		if (pressed)
		{
			if (_kb_used < ARRAY_SIZE(kb_key))
			{
				bool found = false;
				for (u32 i = 0; !found && i < _kb_used; i++)
				{
					if (kb_key[i] == dc_keycode)
						found = true;
				}
				if (!found)
					kb_key[_kb_used++] = dc_keycode;
			}
		}
		else
		{
			for (u32 i = 0; i < _kb_used; i++)
			{
				if (kb_key[i] == dc_keycode)
				{
					_kb_used--;
					for (u32 j = i; j < ARRAY_SIZE(kb_key) - 1; j++)
						kb_key[j] = kb_key[j + 1];
					kb_key[ARRAY_SIZE(kb_key) - 1] = 0;
					break;
				}
			}
		}
		kb_shift |= modifier_keys;
	}
}

inline void KeyboardDevice::keyboard_character(char c) {
	char_input.push_back(c);
}

inline std::string KeyboardDevice::get_character_input() {
	std::string input = char_input;
	char_input.clear();
	return input;
}
