#include "../input/gamepad_device.h"
#include "oslib/oslib.h"
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
		set_axis(DC_AXIS_X2, 2, false);
		set_axis(DC_AXIS_Y2, 3, false);
		dirty = false;
	}

	DefaultInputMapping(int joystick_idx) : DefaultInputMapping()
	{
		if (SDL_IsGameController(joystick_idx))
		{
			SDL_GameController *sdl_controller = SDL_GameControllerOpen(joystick_idx);
			name = SDL_GameControllerName(sdl_controller);
			INFO_LOG(INPUT, "SDL: using SDL game controller mappings for '%s'", name.c_str());

			auto map_button = [&](SDL_GameControllerButton sdl_btn, DreamcastKey dc_btn) {
				SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForButton(sdl_controller, sdl_btn);
				if (bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON)
					set_button(dc_btn, bind.value.button);
				else if (bind.bindType == SDL_CONTROLLER_BINDTYPE_HAT)
				{
					int dir;
					switch (bind.value.hat.hat_mask)
					{
					case SDL_HAT_UP:
						dir = 0;
						break;
					case SDL_HAT_DOWN:
						dir = 1;
						break;
					case SDL_HAT_LEFT:
						dir = 2;
						break;
					case SDL_HAT_RIGHT:
						dir = 3;
						break;
					default:
						return;
					}
					set_button(dc_btn, ((bind.value.hat.hat + 1) << 8) | dir);
				}
			};
			map_button(SDL_CONTROLLER_BUTTON_A, DC_BTN_A);
			map_button(SDL_CONTROLLER_BUTTON_B, DC_BTN_B);
			map_button(SDL_CONTROLLER_BUTTON_X, DC_BTN_X);
			map_button(SDL_CONTROLLER_BUTTON_Y, DC_BTN_Y);
			map_button(SDL_CONTROLLER_BUTTON_START, DC_BTN_START);
			map_button(SDL_CONTROLLER_BUTTON_DPAD_UP, DC_DPAD_UP);
			map_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN, DC_DPAD_DOWN);
			map_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT, DC_DPAD_LEFT);
			map_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, DC_DPAD_RIGHT);
			map_button(SDL_CONTROLLER_BUTTON_BACK, EMU_BTN_MENU);
			map_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, DC_BTN_C); // service
			map_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, DC_BTN_Z); // test

			auto map_axis = [&](SDL_GameControllerAxis sdl_axis, DreamcastKey dc_axis) {
				SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForAxis(sdl_controller, sdl_axis);
				if (bind.bindType != SDL_CONTROLLER_BINDTYPE_AXIS)
					return false;

				bool invert_axis = false;
				const char *s = SDL_GameControllerGetStringForAxis(sdl_axis);
				if (s != nullptr && s[strlen(s) - 1] == '~')
					invert_axis = true;
				set_axis(dc_axis, bind.value.axis, invert_axis);
				return true;
			};
			map_axis(SDL_CONTROLLER_AXIS_LEFTX, DC_AXIS_X);
			map_axis(SDL_CONTROLLER_AXIS_LEFTY, DC_AXIS_Y);
			map_axis(SDL_CONTROLLER_AXIS_RIGHTX, DC_AXIS_X2);
			map_axis(SDL_CONTROLLER_AXIS_RIGHTY, DC_AXIS_Y2);
			if (!map_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, DC_AXIS_LT))
				map_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, EMU_BTN_TRIGGER_LEFT);
			if (!map_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, DC_AXIS_RT))
				map_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, EMU_BTN_TRIGGER_RIGHT);

			SDL_GameControllerClose(sdl_controller);
			dirty = false;
		}
		else
			INFO_LOG(INPUT, "using default mapping");
	}
};

class SDLGamepad : public GamepadDevice
{
public:
	SDLGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
		: GamepadDevice(maple_port, "SDL"), sdl_joystick(sdl_joystick)
	{
		_name = SDL_JoystickName(sdl_joystick);
		sdl_joystick_instance = SDL_JoystickInstanceID(sdl_joystick);
		_unique_id = "sdl_joystick_" + std::to_string(sdl_joystick_instance);
		INFO_LOG(INPUT, "SDL: Opened joystick %d on port %d: '%s' unique_id=%s", sdl_joystick_instance, maple_port, _name.c_str(), _unique_id.c_str());

		if (!find_mapping())
			input_mapper = std::make_shared<DefaultInputMapping>(joystick_idx);
		else
			INFO_LOG(INPUT, "using custom mapping '%s'", input_mapper->name.c_str());
		sdl_haptic = SDL_HapticOpenFromJoystick(sdl_joystick);
		if (SDL_HapticRumbleInit(sdl_haptic) != 0)
		{
			SDL_HapticClose(sdl_haptic);
			sdl_haptic = NULL;
		}
	}

	virtual void rumble(float power, float inclination, u32 duration_ms) override
	{
		if (sdl_haptic != NULL)
		{
			vib_inclination = inclination * power;
			vib_stop_time = os_GetSeconds() + duration_ms / 1000.0;

			SDL_HapticRumblePlay(sdl_haptic, power, duration_ms);
		}
	}
	virtual void update_rumble() override
	{
		if (sdl_haptic == NULL)
			return;
		if (vib_inclination > 0)
		{
			int rem_time = (vib_stop_time - os_GetSeconds()) * 1000;
			if (rem_time <= 0)
				vib_inclination = 0;
			else
				SDL_HapticRumblePlay(sdl_haptic, vib_inclination * rem_time, rem_time);
		}
	}

	void close()
	{
		INFO_LOG(INPUT, "SDL: Joystick '%s' on port %d disconnected", _name.c_str(), maple_port());
		if (sdl_haptic != NULL)
			SDL_HapticClose(sdl_haptic);
		SDL_JoystickClose(sdl_joystick);
		GamepadDevice::Unregister(sdl_gamepads[sdl_joystick_instance]);
		sdl_gamepads.erase(sdl_joystick_instance);
	}

	static void AddSDLGamepad(std::shared_ptr<SDLGamepad> gamepad)
	{
		sdl_gamepads[gamepad->sdl_joystick_instance] = gamepad;
		GamepadDevice::Register(gamepad);
	}
	static std::shared_ptr<SDLGamepad> GetSDLGamepad(SDL_JoystickID id)
	{
		auto it = sdl_gamepads.find(id);
		if (it != sdl_gamepads.end())
			return it->second;
		else
			return NULL;
	}
	static void UpdateRumble()
	{
		for (auto& pair : sdl_gamepads)
			pair.second->update_rumble();
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
	SDL_Haptic *sdl_haptic;
	float vib_inclination = 0;
	double vib_stop_time = 0;
	static std::map<SDL_JoystickID, std::shared_ptr<SDLGamepad>> sdl_gamepads;
};

std::map<SDL_JoystickID, std::shared_ptr<SDLGamepad>> SDLGamepad::sdl_gamepads;

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
		set_button(EMU_BTN_FFORWARD, SDLK_SPACE);

		dirty = false;
	}
};

class SDLKbGamepadDevice : public GamepadDevice
{
public:
	SDLKbGamepadDevice(int maple_port) : GamepadDevice(maple_port, "SDL")
	{
		_name = "Keyboard";
		_unique_id = "sdl_keyboard";
		if (!find_mapping())
			input_mapper = std::make_shared<KbInputMapping>();
	}

	virtual const char *get_button_name(u32 code) override
	{
		const char *name = SDL_GetKeyName((SDL_Keycode)code);
		if (name[0] == 0)
			return nullptr;
		return name;
	}
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

class SDLMouse : public GamepadDevice
{
public:
	SDLMouse() : GamepadDevice(0, "SDL")
	{
		this->_name = "Default Mouse";
		this->_unique_id = "sdl_mouse";
		if (!find_mapping())
			input_mapper = std::make_shared<MouseInputMapping>();
	}
	SDLMouse(int maple_port, const std::string& name, const std::string& uniqueId, u32 handle)
		: GamepadDevice(maple_port, "RAW")
	{
		this->_name = name;
		this->_unique_id = uniqueId;
		std::replace(this->_unique_id.begin(), this->_unique_id.end(), '=', '_');
		std::replace(this->_unique_id.begin(), this->_unique_id.end(), '[', '_');
		std::replace(this->_unique_id.begin(), this->_unique_id.end(), ']', '_');

		this->rawHandle = handle;
		if (!find_mapping())
			input_mapper = std::make_shared<MouseInputMapping>();
	}

	bool gamepad_btn_input(u32 code, bool pressed) override
	{
		if (!is_detecting_input() && detectedRawMouse != nullptr)
		{
			bool handled = detectedRawMouse->gamepad_btn_input(code, pressed);
			if (!detectedRawMouse->is_detecting_input())
				detectedRawMouse = nullptr;
			return handled;
		}
		if (gui_is_open() && !is_detecting_input())
			// Don't register mouse clicks as gamepad presses when gui is open
			// This makes the gamepad presses to be handled first and the mouse position to be ignored
			// TODO Make this generic
			return false;
		else
			return GamepadDevice::gamepad_btn_input(code, pressed);
	}

	virtual const char *get_button_name(u32 code) override
	{
		switch(code)
		{
		case SDL_BUTTON_LEFT:
			return "Left Button";
		case SDL_BUTTON_RIGHT:
			return "Right Button";
		case SDL_BUTTON_MIDDLE:
			return "Middle Button";
		case SDL_BUTTON_X1:
			return "X1 Button";
		case SDL_BUTTON_X2:
			return "X2 Button";
		default:
			return nullptr;
		}
	}

	void setMouseAbsPos(int x, int y);
	void setMouseRelPos(int deltax, int deltay);
	void setMouseButton(u32 button, bool pressed);
	virtual void detect_btn_input(input_detected_cb button_pressed) override;
	virtual void cancel_detect_input() override;

private:
	u32 rawHandle = 0;
	std::shared_ptr<SDLMouse> detectedRawMouse;
};
