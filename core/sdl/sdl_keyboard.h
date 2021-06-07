#pragma once
#include "input/keyboard_device.h"
#include "sdl.h"

class SDLKeyboardDevice : public KeyboardDeviceTemplate<SDL_Scancode>
{
public:
	SDLKeyboardDevice(int maple_port) : KeyboardDeviceTemplate(maple_port, "SDL") {
		_unique_id = "sdl_keyboard";
		loadMapping();
	}

	const char *get_button_name(u32 code) override
	{
		const char *name = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)code));
		if (name[0] == 0)
			return nullptr;
		return name;
	}

protected:
	u8 convert_keycode(SDL_Scancode scancode) override
	{
		if (settings.input.keyboardLangId != KeyboardLayout::US && scancode == SDL_SCANCODE_BACKSLASH)
			return (u8)SDL_SCANCODE_NONUSHASH;
		else
			return (u8)scancode;
	}
};
