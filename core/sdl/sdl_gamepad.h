#include "input/gamepad_device.h"
#include "input/mouse.h"
#include "oslib/oslib.h"
#include "sdl.h"

template<bool Arcade = false, bool Gamepad = false>
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

		set_axis(0, DC_AXIS_LEFT, 0, false);
		set_axis(0, DC_AXIS_RIGHT, 0, true);
		set_axis(0, DC_AXIS_UP, 1, false);
		set_axis(0, DC_AXIS_DOWN, 1, true);
		set_axis(0, DC_AXIS2_LEFT, 2, false);
		set_axis(0, DC_AXIS2_RIGHT, 2, true);
		set_axis(0, DC_AXIS2_UP, 3, false);
		set_axis(0, DC_AXIS2_DOWN, 3, true);
		dirty = false;
	}

	DefaultInputMapping(SDL_GameController *sdlController) : DefaultInputMapping()
	{
		if (sdlController != nullptr)
		{
			name = SDL_GameControllerName(sdlController);
			INFO_LOG(INPUT, "SDL: using SDL game controller mappings for '%s'", name.c_str());

			auto map_button = [&](SDL_GameControllerButton sdl_btn, DreamcastKey dc_btn) {
				SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForButton(sdlController, sdl_btn);
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
			auto map_axis = [&](SDL_GameControllerAxis sdl_axis, DreamcastKey dc_axis, bool positive) {
				SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForAxis(sdlController, sdl_axis);
				if (bind.bindType != SDL_CONTROLLER_BINDTYPE_AXIS)
					return false;

				bool invert_axis = false;
				char *gcdb = SDL_GameControllerMapping(sdlController);
				const char *axisName = SDL_GameControllerGetStringForAxis(sdl_axis);
				if (gcdb != nullptr && axisName != nullptr)
				{
					const char *p = strstr(gcdb, axisName);
					if (p != nullptr)
					{
						const char *pend = strchr(p, ',');
						if (pend == nullptr)
							pend = p + strlen(p);
						invert_axis = pend[-1] == '~';
					}
				}
				set_axis(dc_axis, bind.value.axis, invert_axis ^ positive);
				SDL_free(gcdb);

				return true;
			};

			if constexpr (Arcade)
			{
				if constexpr (Gamepad)
				{
					// 1  2  3  4  5  6
					// A  B  X  Y  R  L
					map_button(SDL_CONTROLLER_BUTTON_A, DC_BTN_A);
					map_button(SDL_CONTROLLER_BUTTON_B, DC_BTN_B);
					map_button(SDL_CONTROLLER_BUTTON_X, DC_BTN_C);
					map_button(SDL_CONTROLLER_BUTTON_Y, DC_BTN_X);

					if (!map_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, DC_AXIS_LT, true))
						map_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, DC_AXIS_LT);
					else
						map_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, DC_BTN_Z);
					if (!map_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, DC_AXIS_RT, true))
						map_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, DC_AXIS_RT);
					else
						map_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, DC_BTN_Y);
				}
				else
				{
					// Hitbox
					// 1  2  3  4  5  6  7  8
					// X  Y  R1 A  B  R2 L1 L2
					map_button(SDL_CONTROLLER_BUTTON_X, DC_BTN_A);
					map_button(SDL_CONTROLLER_BUTTON_Y, DC_BTN_B);
					map_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, DC_BTN_C);			// R1
					map_button(SDL_CONTROLLER_BUTTON_A, DC_BTN_X);
					map_button(SDL_CONTROLLER_BUTTON_B, DC_BTN_Y);
					map_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, DC_BTN_Z, true);			// R2
					map_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, DC_DPAD2_LEFT);		// L1 (Naomi button 7)
					map_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, DC_DPAD2_RIGHT, true);	// L2 (Naomi button 8)
				}
			}
			else
			{
				map_button(SDL_CONTROLLER_BUTTON_A, DC_BTN_A);
				map_button(SDL_CONTROLLER_BUTTON_B, DC_BTN_B);
				map_button(SDL_CONTROLLER_BUTTON_X, DC_BTN_X);
				map_button(SDL_CONTROLLER_BUTTON_Y, DC_BTN_Y);
				if (!map_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, DC_AXIS_LT, true))
					map_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, DC_AXIS_LT);
				else
					map_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, DC_BTN_Z);
				if (!map_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, DC_AXIS_RT, true))
					map_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, DC_AXIS_RT);
				else
					map_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, DC_BTN_C);
			}
			map_button(SDL_CONTROLLER_BUTTON_START, DC_BTN_START);
			map_button(SDL_CONTROLLER_BUTTON_DPAD_UP, DC_DPAD_UP);
			map_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN, DC_DPAD_DOWN);
			map_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT, DC_DPAD_LEFT);
			map_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, DC_DPAD_RIGHT);
			map_button(SDL_CONTROLLER_BUTTON_GUIDE, DC_DPAD2_UP); // service
			map_button(SDL_CONTROLLER_BUTTON_BACK, EMU_BTN_MENU);

			map_axis(SDL_CONTROLLER_AXIS_LEFTX, DC_AXIS_LEFT, false);
			map_axis(SDL_CONTROLLER_AXIS_LEFTX, DC_AXIS_RIGHT, true);
			map_axis(SDL_CONTROLLER_AXIS_LEFTY, DC_AXIS_UP, false);
			map_axis(SDL_CONTROLLER_AXIS_LEFTY, DC_AXIS_DOWN, true);
			map_axis(SDL_CONTROLLER_AXIS_RIGHTX, DC_AXIS2_LEFT, false);
			map_axis(SDL_CONTROLLER_AXIS_RIGHTX, DC_AXIS2_RIGHT, true);
			map_axis(SDL_CONTROLLER_AXIS_RIGHTY, DC_AXIS2_UP, false);
			map_axis(SDL_CONTROLLER_AXIS_RIGHTY, DC_AXIS2_DOWN, true);

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
		const char *joyName = SDL_JoystickName(sdl_joystick);
		if (joyName == nullptr)
		{
			WARN_LOG(INPUT, "Can't get joystick %d name: %s", joystick_idx, SDL_GetError());
			throw FlycastException("joystick failure");
		}
		_name = joyName;
		sdl_joystick_instance = SDL_JoystickInstanceID(sdl_joystick);
		_unique_id = "sdl_joystick_" + std::to_string(sdl_joystick_instance);
		INFO_LOG(INPUT, "SDL: Opened joystick %d on port %d: '%s' unique_id=%s", sdl_joystick_instance, maple_port, _name.c_str(), _unique_id.c_str());

		if (SDL_IsGameController(joystick_idx))
		{
			sdl_controller = SDL_GameControllerOpen(joystick_idx);
			if (sdl_controller == nullptr)
			{
				WARN_LOG(INPUT, "Can't open game controller %d: %s", joystick_idx, SDL_GetError());
			}
			else
			{
				SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForAxis(sdl_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
				if (bind.bindType == SDL_CONTROLLER_BINDTYPE_AXIS)
					leftTrigger = bind.value.axis;
				bind = SDL_GameControllerGetBindForAxis(sdl_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
				if (bind.bindType == SDL_CONTROLLER_BINDTYPE_AXIS)
					rightTrigger = bind.value.axis;
			}
		}

		if (!find_mapping())
			input_mapper = std::make_shared<DefaultInputMapping<>>(sdl_controller);
		else
			INFO_LOG(INPUT, "using custom mapping '%s'", input_mapper->name.c_str());

#if SDL_VERSION_ATLEAST(2, 0, 18)
		rumbleEnabled = SDL_JoystickHasRumble(sdl_joystick);
#else
		rumbleEnabled = (SDL_JoystickRumble(sdl_joystick, 1, 1, 1) != -1);
#endif
	}

	bool gamepad_axis_input(u32 code, int value) override
	{
		if (code == leftTrigger || code == rightTrigger)
			value = (u16)(value + 32768) / 2;
		return GamepadDevice::gamepad_axis_input(code, value);
	}

	u16 getRumbleIntensity(float power)	{
		return (u16)std::min(power * 65535.f / std::pow(1.06f, 100.f - rumblePower), 65535.f);
	}

	void rumble(float power, float inclination, u32 duration_ms) override
	{
		if (rumbleEnabled)
		{
			vib_inclination = inclination * power;
			vib_stop_time = os_GetSeconds() + duration_ms / 1000.0;

			u16 intensity = getRumbleIntensity(power);
			SDL_JoystickRumble(sdl_joystick, intensity, intensity, duration_ms);
		}
	}
	void update_rumble() override
	{
		if (!rumbleEnabled)
			return;
		if (vib_inclination > 0)
		{
			int rem_time = (vib_stop_time - os_GetSeconds()) * 1000;
			if (rem_time <= 0)
				vib_inclination = 0;
			else
			{
				u16 intensity = getRumbleIntensity(vib_inclination * rem_time);
				SDL_JoystickRumble(sdl_joystick, intensity, intensity, rem_time);
			}
		}
	}

	void close()
	{
		INFO_LOG(INPUT, "SDL: Joystick '%s' on port %d disconnected", _name.c_str(), maple_port());
		if (sdl_controller != nullptr)
			SDL_GameControllerClose(sdl_controller);
		SDL_JoystickClose(sdl_joystick);
		GamepadDevice::Unregister(sdl_gamepads[sdl_joystick_instance]);
		sdl_gamepads.erase(sdl_joystick_instance);
	}

	const char *get_button_name(u32 code) override
	{
		static struct
		{
			const char *sdlButton;
			const char *label;
		} buttonsTable[] =
		{
				{ "a", "A" },
				{ "b", "B" },
				{ "x", "X" },
				{ "y", "Y" },
				{ "back", "Back" },
				{ "guide", "Guide" },
				{ "start", "Start" },
				{ "leftstick", "L3" },
				{ "rightstick", "R3" },
				{ "leftshoulder", "L1" },
				{ "rightshoulder", "R1" },
				{ "dpup", "DPad Up" },
				{ "dpdown", "DPad Down" },
				{ "dpleft", "DPad Left" },
				{ "dpright", "DPad Right" },
				{ "misc1", "Misc" },
				{ "paddle1", "Paddle 1" },
				{ "paddle2", "Paddle 2" },
				{ "paddle3", "Paddle 3" },
				{ "paddle4", "Paddle 4" },
				{ "touchpad", "Touchpad" },
		};
		if (sdl_controller != nullptr)
			for (SDL_GameControllerButton button = SDL_CONTROLLER_BUTTON_A; button < SDL_CONTROLLER_BUTTON_MAX; button = (SDL_GameControllerButton)(button + 1))
			{
				SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForButton(sdl_controller, button);
				if (bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON && bind.value.button == (int)code)
				{
					const char *sdlButton = SDL_GameControllerGetStringForButton(button);
					if (sdlButton == nullptr)
						return nullptr;
					for (const auto& button : buttonsTable)
						if (!strcmp(button.sdlButton, sdlButton))
							return button.label;
					return sdlButton;
				}
				if (bind.bindType == SDL_CONTROLLER_BINDTYPE_HAT && (code >> 8) - 1 == (u32)bind.value.hat.hat)
				{
					int hat;
					const char *name;
					switch (code & 0xff)
					{
					case 0:
						hat = SDL_HAT_UP;
						name =  "DPad Up";
						break;
					case 1:
						hat = SDL_HAT_DOWN;
						name =  "DPad Down";
						break;
					case 2:
						hat = SDL_HAT_LEFT;
						name =  "DPad Left";
						break;
					case 3:
						hat = SDL_HAT_RIGHT;
						name =  "DPad Right";
						break;
					default:
						hat = 0;
						name = nullptr;
						break;
					}
					if (hat == bind.value.hat.hat_mask)
						return name;
				}
			}
		return nullptr;
	}

	const char *get_axis_name(u32 code) override
	{
		static struct
		{
			const char *sdlAxis;
			const char *label;
		} axesTable[] =
		{
				{ "leftx", "Left Stick X" },
				{ "lefty", "Left Stick Y" },
				{ "rightx", "Right Stick X" },
				{ "righty", "Right Stick Y" },
				{ "lefttrigger", "L2" },
				{ "righttrigger", "R2" },
		};

		if (sdl_controller != nullptr)
			for (SDL_GameControllerAxis axis = SDL_CONTROLLER_AXIS_LEFTX; axis < SDL_CONTROLLER_AXIS_MAX; axis = (SDL_GameControllerAxis)(axis + 1))
			{
				SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForAxis(sdl_controller, axis);
				if (bind.bindType == SDL_CONTROLLER_BINDTYPE_AXIS && bind.value.axis == (int)code)
				{
					const char *sdlAxis = SDL_GameControllerGetStringForAxis(axis);
					if (sdlAxis == nullptr)
						return nullptr;
					for (const auto& axis : axesTable)
						if (!strcmp(axis.sdlAxis, sdlAxis))
							return axis.label;
					return sdlAxis;
				}
			}
		return nullptr;
	}

	void resetMappingToDefault(bool arcade, bool gamepad) override
	{
		NOTICE_LOG(INPUT, "Resetting SDL gamepad to default: %d %d", arcade, gamepad);
		if (arcade)
		{
			if (gamepad)
				input_mapper = std::make_shared<DefaultInputMapping<true, true>>(sdl_controller);
			else
				input_mapper = std::make_shared<DefaultInputMapping<true, false>>(sdl_controller);
		}
		else
			input_mapper = std::make_shared<DefaultInputMapping<false, false>>(sdl_controller);
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
	static void closeAllGamepads()
	{
		while (!sdl_gamepads.empty())
			sdl_gamepads.begin()->second->close();
	}
	static void UpdateRumble()
	{
		for (auto& pair : sdl_gamepads)
			pair.second->update_rumble();
	}

private:
	SDL_Joystick* sdl_joystick;
	SDL_JoystickID sdl_joystick_instance;
	float vib_inclination = 0;
	double vib_stop_time = 0;
	SDL_GameController *sdl_controller = nullptr;
	static std::map<SDL_JoystickID, std::shared_ptr<SDLGamepad>> sdl_gamepads;
};

std::map<SDL_JoystickID, std::shared_ptr<SDLGamepad>> SDLGamepad::sdl_gamepads;

class SDLMouse : public Mouse
{
public:
	SDLMouse() : Mouse("SDL")
	{
		this->_name = "Default Mouse";
		this->_unique_id = "sdl_mouse";
		loadMapping();
	}

	void setAbsPos(int x, int y);
};

