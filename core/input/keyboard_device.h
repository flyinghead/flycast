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
#include "mapping.h"

class KeyboardDevice
{
public:
	virtual const char* name() = 0;
	int maple_port() { return _maple_port; }
	void keyboard_character(char c);
	std::string get_character_input();
	virtual ~KeyboardDevice() {}

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
	KeyboardDeviceTemplate(int maple_port) : KeyboardDevice(maple_port), _kb_used(0), _modifier_keys(0) {}
	virtual u8 convert_keycode(Keycode keycode) = 0;

private:
	int _modifier_keys;
	int _kb_used;
};

extern u8 kb_key[6];		// normal keys pressed
extern u8 kb_shift; 		// shift keys pressed (bitmask)

template <typename Keycode>
void KeyboardDeviceTemplate<Keycode>::keyboard_input(Keycode keycode, bool pressed, int modifier_keys)
{
	u8 dc_keycode = convert_keycode(keycode);
	if (dc_keycode == 0xE1 || dc_keycode == 0xE5)		// SHIFT
	{
		if (pressed)
			_modifier_keys |= 0x02 | 0x20;
		else
			_modifier_keys &= ~(0x02 | 0x20);
	}
	else if (dc_keycode == 0xE0 || dc_keycode == 0xE4)	// CTRL
	{
		if (pressed)
			_modifier_keys |= 0x01 | 0x10;
		else
			_modifier_keys &= ~(0x01 | 0x10);
	}
	else if (dc_keycode != 0)
	{
		if (pressed)
		{
			if (_kb_used < ARRAY_SIZE(kb_key))
			{
				bool found = false;
				for (int i = 0; !found && i < _kb_used; i++)
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
			for (int i = 0; i < _kb_used; i++)
			{
				if (kb_key[i] == dc_keycode)
				{
					_kb_used--;
					for (int j = i; j < ARRAY_SIZE(kb_key) - 1; j++)
						kb_key[j] = kb_key[j + 1];
					kb_key[ARRAY_SIZE(kb_key) - 1] = 0;
					break;
				}
			}
		}
		kb_shift = modifier_keys | _modifier_keys;
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
