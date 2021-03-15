#pragma once
#include "input/keyboard_device.h"
#include "sdl.h"

class SDLKeyboardDevice : public KeyboardDeviceTemplate<SDL_Scancode>
{
public:
	SDLKeyboardDevice(int maple_port) : KeyboardDeviceTemplate(maple_port) {}
	virtual ~SDLKeyboardDevice() = default;
	virtual const char* name() override { return "SDL Keyboard"; }

protected:
	virtual u8 convert_keycode(SDL_Scancode scancode) override
	{
		if (settings.input.keyboardLangId != KeyboardLayout::US && scancode == SDL_SCANCODE_BACKSLASH)
			return (u8)SDL_SCANCODE_NONUSHASH;
		else
			return (u8)scancode;
	}
};
