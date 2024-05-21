#pragma once
#include "input/keyboard_device.h"
#include "sdl.h"

class SDLKeyboardDevice : public KeyboardDevice
{
public:
	SDLKeyboardDevice(int maple_port) : KeyboardDevice(maple_port, "SDL") {
		_unique_id = "sdl_keyboard";
		if (find_mapping())
		{
			if (input_mapper->version == 1)
			{
				// Convert keycodes to scancode
				SDL_Scancode scancodes[4][26] {};
				for (int i = 0; i < 26; i++)
				{
					DreamcastKey key = (DreamcastKey)(1 << i);
					for (int port = 0; port < 4; port++)
					{
						SDL_Keycode keycode = (SDL_Keycode)input_mapper->get_button_code(port, key);
						if ((int)keycode != -1)
							scancodes[port][i] = SDL_GetScancodeFromKey(keycode);
					}
				}
				for (int i = 0; i < 26; i++)
				{
					DreamcastKey key = (DreamcastKey)(1 << i);
					for (int port = 0; port < 4; port++)
						if (scancodes[port][i] != 0)
							input_mapper->set_button(port, key, (u32)scancodes[port][i]);
				}
				save_mapping();
			}
		}
		else
			input_mapper = getDefaultMapping();
	}

	const char *get_button_name(u32 code) override
	{
		const char *name = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)code));
		if (name[0] == 0)
			return nullptr;
		return name;
	}

	void input(SDL_Scancode scancode, bool pressed)
	{
		u8 keycode;
		if (settings.input.keyboardLangId != KeyboardLayout::US && scancode == SDL_SCANCODE_BACKSLASH)
			keycode = (u8)SDL_SCANCODE_NONUSHASH;
		else
			keycode = (u8)scancode;
		KeyboardDevice::input(keycode, pressed, 0);
	}
};
