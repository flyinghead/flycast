#include "../input/gamepad_device.h"
#include "sdl.h"

class DefaultInputMapping : public InputMapping
{
public:
	DefaultInputMapping()
	{
		name = "Default";
		set_button(DC_BTN_Y, 0);
		set_button(DC_BTN_B, 1);
		set_button(DC_BTN_A, 2);
		set_button(DC_BTN_X, 3);
		set_button(DC_BTN_START, 9);

		set_axis(DC_AXIS_X, 0, false);
		set_axis(DC_AXIS_Y, 1, false);
		dirty = false;
	}
};

class Xbox360InputMapping : public InputMapping
{
public:
	Xbox360InputMapping()
	{
		name = "Xbox 360";
		set_button(DC_BTN_A, 0);
		set_button(DC_BTN_B, 1);
		set_button(DC_BTN_X, 2);
		set_button(DC_BTN_Y, 3);
		set_button(DC_BTN_START, 7);

		set_axis(DC_AXIS_X, 0, false);
		set_axis(DC_AXIS_Y, 1, false);
		set_axis(DC_AXIS_LT, 2, false);
		set_axis(DC_AXIS_RT, 5, false);
		set_axis(DC_DPAD_LEFT, 6, false);
		set_axis(DC_DPAD_UP, 7, false);
		dirty = false;
	}
};

class SDLGamepadDevice : public GamepadDevice
{
public:
	SDLGamepadDevice(int maple_port, SDL_Joystick* sdl_joystick) : GamepadDevice(maple_port), sdl_joystick(sdl_joystick)
	{
		_name = SDL_JoystickName(sdl_joystick);
		if (!find_mapping())
		{
			if (_name == "Microsoft X-Box 360 pad")
			{
				input_mapper = new Xbox360InputMapping();
				printf("Using Xbox 360 mapping\n");
			}
			else
			{
				input_mapper = new DefaultInputMapping();
				printf("Using default mapping\n");
			}
			save_mapping();
		}
	}
	virtual const char* api_name() override { return "SDL"; }
	virtual const char* name() override { return _name.c_str(); }
	virtual ~SDLGamepadDevice() override
	{
		SDL_JoystickClose(sdl_joystick);
	}

protected:
	virtual void load_axis_min_max(u32 axis) override
	{
		axis_min_values[axis] = -32768;
		axis_ranges[axis] = 65535;
	}

private:
	std::string _name;
	SDL_Joystick* sdl_joystick;
};

class KbInputMapping : public InputMapping
{
public:
	KbInputMapping()
	{
		name = "SDL Keyboard";
		set_button(DC_BTN_A, SDLK_x);
		set_button(DC_BTN_B, SDLK_c);
		set_button(DC_BTN_X, SDLK_s);
		set_button(DC_BTN_Y, SDLK_d);
		set_button(DC_DPAD_UP, SDLK_UP);
		set_button(DC_DPAD_DOWN, SDLK_DOWN);
		set_button(DC_DPAD_LEFT, SDLK_LEFT);
		set_button(DC_DPAD_RIGHT, SDLK_RIGHT);
		set_button(DC_BTN_START, SDLK_RETURN);
		set_button(EMU_BTN_TRIGGER_LEFT, SDLK_f);
		set_button(EMU_BTN_TRIGGER_RIGHT, SDLK_v);
		set_button(EMU_BTN_MENU, SDLK_TAB);

		dirty = false;
	}
};

class SDLKbGamepadDevice : public GamepadDevice
{
public:
	SDLKbGamepadDevice(int maple_port) : GamepadDevice(maple_port)
	{
		if (!find_mapping())
			input_mapper = new KbInputMapping();
	}
	virtual const char* api_name() override { return "SDL"; }
	virtual const char* name() override { return "Keyboard"; }
	virtual ~SDLKbGamepadDevice() {}
};

class MouseInputMapping : public InputMapping
{
public:
	MouseInputMapping()
	{
		name = "SDL Mouse";
		set_button(DC_BTN_A, SDL_BUTTON_LEFT);
		set_button(DC_BTN_B, SDL_BUTTON_RIGHT);
		set_button(DC_BTN_START, SDL_BUTTON_MIDDLE);

		dirty = false;
	}
};

class SDLMouseGamepadDevice : public GamepadDevice
{
public:
	SDLMouseGamepadDevice(int maple_port) : GamepadDevice(maple_port)
	{
		if (!find_mapping())
			input_mapper = new MouseInputMapping();
	}
	virtual const char* api_name() override { return "SDL"; }
	virtual const char* name() override { return "Mouse"; }
	virtual ~SDLMouseGamepadDevice() {}
};

