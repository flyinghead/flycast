#include "../input/gamepad_device.h"
#include "sdl.h"
#include "rend/gui.h"

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
	SDLGamepadDevice(int maple_port, SDL_Joystick* sdl_joystick) : GamepadDevice(maple_port, "SDL"), sdl_joystick(sdl_joystick)
	{
		_name = SDL_JoystickName(sdl_joystick);
		sdl_joystick_instance = SDL_JoystickInstanceID(sdl_joystick);
		printf("SDL: Opened joystick on port %d: '%s' ", maple_port, _name.c_str());
		if (!find_mapping())
		{
			if (_name == "Microsoft X-Box 360 pad")
			{
				input_mapper = new Xbox360InputMapping();
				printf("using Xbox 360 mapping\n");
			}
			else
			{
				input_mapper = new DefaultInputMapping();
				printf("using default mapping\n");
			}
			save_mapping();
		}
		else
			printf("using custom mapping '%s'\n", input_mapper->name.c_str());
		auto it = sdl_gamepads.find(sdl_joystick_instance);
		if (it != sdl_gamepads.end())
			delete it->second;
		sdl_gamepads[sdl_joystick_instance] = this;
	}
	virtual ~SDLGamepadDevice() override
	{
		printf("SDL: Joystick '%s' on port %d disconnected\n", _name.c_str(), maple_port());
		SDL_JoystickClose(sdl_joystick);
		sdl_gamepads.erase(sdl_joystick_instance);
	}
	SDL_JoystickID sdl_instance() { return sdl_joystick_instance; }

	static SDLGamepadDevice *GetSDLGamepad(SDL_JoystickID id)
	{
		auto it = sdl_gamepads.find(id);
		if (it != sdl_gamepads.end())
			return it->second;
		else
			return NULL;
	}

protected:
	virtual void load_axis_min_max(u32 axis) override
	{
		axis_min_values[axis] = -32768;
		axis_ranges[axis] = 65535;
	}

private:
	SDL_Joystick* sdl_joystick;
	SDL_JoystickID sdl_joystick_instance;
	static std::map<SDL_JoystickID, SDLGamepadDevice*> sdl_gamepads;
};

std::map<SDL_JoystickID, SDLGamepadDevice*> SDLGamepadDevice::sdl_gamepads;

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
	SDLKbGamepadDevice(int maple_port) : GamepadDevice(maple_port, "SDL")
	{
		_name = "Keyboard";
		if (!find_mapping())
			input_mapper = new KbInputMapping();
	}
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
	SDLMouseGamepadDevice(int maple_port) : GamepadDevice(maple_port, "SDL")
	{
		_name = "Mouse";
		if (!find_mapping())
			input_mapper = new MouseInputMapping();
	}
	virtual ~SDLMouseGamepadDevice() {}
	bool gamepad_btn_input(u32 code, bool pressed) override
	{
		if (gui_is_open())
			// Don't register mouse clicks as gamepad presses when gui is open
			// This makes the gamepad presses to be handled first and the mouse position to be ignored
			// TODO Make this generic
			return false;
		else
			return GamepadDevice::gamepad_btn_input(code, pressed);
	}
};

