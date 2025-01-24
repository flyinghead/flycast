#pragma once
#include "input/gamepad_device.h"
#include "input/mouse.h"
#include "stdclass.h"
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
		NOTICE_LOG(INPUT, "SDL: Opened joystick %d on port %d: '%s' unique_id=%s", sdl_joystick_instance, maple_port, _name.c_str(), _unique_id.c_str());

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

		hasAnalogStick = SDL_JoystickNumAxes(sdl_joystick) > 0;
		set_maple_port(maple_port);

#if SDL_VERSION_ATLEAST(2, 0, 18)
		rumbleEnabled = SDL_JoystickHasRumble(sdl_joystick);
#else
		rumbleEnabled = (SDL_JoystickRumble(sdl_joystick, 1, 1, 1) != -1);
#endif

		// Open the haptic interface
		haptic = SDL_HapticOpenFromJoystick(sdl_joystick);
		if (haptic != nullptr)
		{
			// Query supported haptic effects for force-feedback
			u32 hapq = SDL_HapticQuery(haptic);
			INFO_LOG(INPUT, "SDL_HapticQuery: supported: %x", hapq);
			if ((hapq & SDL_HAPTIC_SINE) != 0 && SDL_JoystickGetType(sdl_joystick) == SDL_JOYSTICK_TYPE_WHEEL)
			{
				SDL_HapticEffect effect{};
				effect.type = SDL_HAPTIC_SINE;
				effect.periodic.direction.type = SDL_HAPTIC_CARTESIAN;
				effect.periodic.direction.dir[0] = -1;	// west
				effect.periodic.period = 40; 			// 25 Hz
				effect.periodic.magnitude = 0x7fff;
				effect.periodic.length = SDL_HAPTIC_INFINITY;
				sineEffectId = SDL_HapticNewEffect(haptic, &effect);
				if (sineEffectId != -1)
				{
					rumbleEnabled = true;
					hapticRumble = true;
					NOTICE_LOG(INPUT, "wheel %d: haptic sine supported", sdl_joystick_instance);
				}
			}
			if (hapq & SDL_HAPTIC_AUTOCENTER)
			{
				SDL_HapticSetAutocenter(haptic, 0);
				hasAutocenter = true;
				NOTICE_LOG(INPUT, "wheel %d: haptic autocenter supported", sdl_joystick_instance);
			}
			if (hapq & SDL_HAPTIC_GAIN)
				SDL_HapticSetGain(haptic, 100);
			if (hapq & SDL_HAPTIC_CONSTANT)
			{
				SDL_HapticEffect effect{};
				effect.type = SDL_HAPTIC_CONSTANT;
				effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
				effect.constant.direction.dir[0] = -1;	// west, updated when used
				effect.constant.length = SDL_HAPTIC_INFINITY;
				effect.constant.delay = 0;
				effect.constant.level = 0; // updated when used
				constEffectId = SDL_HapticNewEffect(haptic, &effect);
				if (constEffectId != -1)
					NOTICE_LOG(INPUT, "wheel %d: haptic constant supported", sdl_joystick_instance);
			}
			if (hapq & SDL_HAPTIC_SPRING)
			{
				SDL_HapticEffect effect{};
				effect.type = SDL_HAPTIC_SPRING;
				effect.condition.length = SDL_HAPTIC_INFINITY;
				effect.condition.direction.type = SDL_HAPTIC_CARTESIAN;	// not used but required!
				// effect level at full deflection
				effect.condition.left_sat[0] = effect.condition.right_sat[0] = 0xffff;
				// how fast to increase the force
				effect.condition.left_coeff[0] = effect.condition.right_coeff[0] = 0x7fff;
				springEffectId = SDL_HapticNewEffect(haptic, &effect);
				if (springEffectId != -1)
					NOTICE_LOG(INPUT, "wheel %d: haptic spring supported", sdl_joystick_instance);
			}
			if (hapq & SDL_HAPTIC_DAMPER)
			{
				SDL_HapticEffect effect{};
				effect.type = SDL_HAPTIC_DAMPER;
				effect.condition.length = SDL_HAPTIC_INFINITY;
				effect.condition.direction.type = SDL_HAPTIC_CARTESIAN;	// not used but required!
				// max effect level
				effect.condition.left_sat[0] = effect.condition.right_sat[0] = 0xffff;
				// how fast to increase the force
				effect.condition.left_coeff[0] = effect.condition.right_coeff[0] = 0x7fff;
				damperEffectId = SDL_HapticNewEffect(haptic, &effect);
				if (damperEffectId != -1)
					NOTICE_LOG(INPUT, "wheel %d: haptic damper supported", sdl_joystick_instance);
			}
			if (sineEffectId == -1 && constEffectId == -1 && damperEffectId == -1 && springEffectId == -1 && !hasAutocenter) {
				SDL_HapticClose(haptic);
				haptic = nullptr;
			}
		}
	}

	bool gamepad_axis_input(u32 code, int value) override
	{
		if (code == leftTrigger || code == rightTrigger)
			value = (u16)(value + 32768) / 2;
		return GamepadDevice::gamepad_axis_input(code, value);
	}

	void set_maple_port(int port) override
	{
		GamepadDevice::set_maple_port(port);
		SDL_JoystickSetPlayerIndex(sdl_joystick, port <= 3 ? port : -1);
	}

	u16 getRumbleIntensity(float power)
	{
		if (rumblePower == 0)
			return 0;
		else
			return (u16)std::min(power * 65535.f / std::pow(1.06f, 100.f - rumblePower), 65535.f);
	}
	void doRumble(float power, u32 duration_ms)
	{
		const u16 intensity = getRumbleIntensity(power);
		if (hapticRumble)
		{
			if (intensity != 0 && duration_ms != 0)
			{
				SDL_HapticEffect effect{};
				effect.type = SDL_HAPTIC_SINE;
				effect.periodic.direction.type = SDL_HAPTIC_CARTESIAN;
				effect.periodic.direction.dir[0] = (vib_stop_time & 1) ? -1 : 1;	// west or east randomly
				effect.periodic.period = 40; 				// 25 Hz
				effect.periodic.magnitude = intensity / 4;	// scale by an additional 0.5 to soften it
				effect.periodic.length = duration_ms;
				SDL_HapticUpdateEffect(haptic, sineEffectId, &effect);
				SDL_HapticRunEffect(haptic, sineEffectId, 1);
			}
			else {
				SDL_HapticStopEffect(haptic, sineEffectId);
			}
		}
		else {
			SDL_JoystickRumble(sdl_joystick, intensity, intensity, duration_ms);
		}
	}

	void rumble(float power, float inclination, u32 duration_ms) override
	{
		if (rumbleEnabled)
		{
			vib_inclination = inclination * power;
			vib_stop_time = getTimeMs() + duration_ms;
			doRumble(power, duration_ms);
		}
	}
	void update_rumble() override
	{
		if (!rumbleEnabled)
			return;
		if (vib_inclination > 0)
		{
			int rem_time = vib_stop_time - getTimeMs();
			if (rem_time <= 0)
			{
				vib_inclination = 0;
				if (hapticRumble)
					SDL_HapticStopEffect(haptic, sineEffectId);
				else
					SDL_JoystickRumble(sdl_joystick, 0, 0, 0);
			}
			else {
				doRumble(vib_inclination * rem_time, rem_time);
			}
		}
	}

	void setTorque(float torque)
	{
		if (haptic == nullptr || constEffectId == -1)
			return;
		if (torque != 0.f && rumblePower != 0)
		{
			SDL_HapticEffect effect{};
			effect.type = SDL_HAPTIC_CONSTANT;
			effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
			effect.constant.direction.dir[0] = torque < 0 ? -1 : 1;	// west/cw if torque < 0
			effect.constant.length = SDL_HAPTIC_INFINITY;
			effect.constant.level = std::abs(torque) * 32767.f * rumblePower / 100.f;
			SDL_HapticUpdateEffect(haptic, constEffectId, &effect);
			SDL_HapticRunEffect(haptic, constEffectId, 1);
		}
		else {
			SDL_HapticStopEffect(haptic, constEffectId);
		}
	}

	void stopHaptic()
	{
		if (haptic != nullptr)
		{
			SDL_HapticStopAll(haptic);
			if (hasAutocenter)
				SDL_HapticSetAutocenter(haptic, 0);
			vib_inclination = 0;
		}
		if (!hapticRumble)
			rumble(0, 0, 0);
	}

	void setSpring(float saturation, float speed)
	{
		if (haptic == nullptr)
			return;
		if (springEffectId == -1)
		{
			// Spring not supported so use autocenter if available
			if (hasAutocenter)
			{
				if (speed != 0.f)
					SDL_HapticSetAutocenter(haptic, saturation * rumblePower);
				else
					SDL_HapticSetAutocenter(haptic, 0);
			}
		}
		else
		{
			if (saturation != 0.f && speed != 0.f && rumblePower != 0)
			{
				SDL_HapticEffect effect{};
				effect.type = SDL_HAPTIC_SPRING;
				effect.condition.length = SDL_HAPTIC_INFINITY;
				effect.condition.direction.type = SDL_HAPTIC_CARTESIAN;
				// effect level at full deflection
				effect.condition.left_sat[0] = effect.condition.right_sat[0] = (saturation * rumblePower / 100.f) * 0xffff;
				// how fast to increase the force
				effect.condition.left_coeff[0] = effect.condition.right_coeff[0] = speed * 0x7fff;
				SDL_HapticUpdateEffect(haptic, springEffectId, &effect);
				SDL_HapticRunEffect(haptic, springEffectId, 1);
			}
			else {
				SDL_HapticStopEffect(haptic, springEffectId);
			}
		}
	}

	void setDamper(float param, float speed)
	{
		if (haptic == nullptr || damperEffectId == -1)
			return;
		if (param != 0.f && speed != 0.f && rumblePower != 0)
		{
			SDL_HapticEffect effect{};
			effect.type = SDL_HAPTIC_DAMPER;
			effect.condition.length = SDL_HAPTIC_INFINITY;
			effect.condition.direction.type = SDL_HAPTIC_CARTESIAN;
			// max effect level
			effect.condition.left_sat[0] = effect.condition.right_sat[0] = (param * rumblePower / 100.f) * 0xffff;
			// how fast to increase the force
			effect.condition.left_coeff[0] = effect.condition.right_coeff[0] = speed * 0x7fff;
			SDL_HapticUpdateEffect(haptic, damperEffectId, &effect);
			SDL_HapticRunEffect(haptic, damperEffectId, 1);
		}
		else {
			SDL_HapticStopEffect(haptic, damperEffectId);
		}
	}

	void close()
	{
		NOTICE_LOG(INPUT, "SDL: Joystick '%s' on port %d disconnected", _name.c_str(), maple_port());
		if (haptic != nullptr)
		{
			stopHaptic();
			SDL_HapticSetGain(haptic, 0);
			if (sineEffectId != -1) {
				SDL_HapticDestroyEffect(haptic, sineEffectId);
				sineEffectId = -1;
			}
			if (constEffectId != -1) {
				SDL_HapticDestroyEffect(haptic, constEffectId);
				constEffectId = -1;
			}
			if (springEffectId != -1) {
				SDL_HapticDestroyEffect(haptic, springEffectId);
				springEffectId = -1;
			}
			if (damperEffectId != -1) {
				SDL_HapticDestroyEffect(haptic, damperEffectId);
				damperEffectId = -1;
			}
			SDL_HapticClose(haptic);
			haptic = nullptr;
			rumbleEnabled = false;
			hapticRumble = false;
			hasAutocenter = false;
		}
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
	static void UpdateRumble() {
		for (auto &[k, gamepad] : sdl_gamepads)
			gamepad->update_rumble();
	}

	template<typename Func, typename... Args>
	static void applyToPort(int port, Func func, Args&&... args)
	{
		for (auto &[k, gamepad] : sdl_gamepads)
			if (gamepad->maple_port() == port)
				((*gamepad).*func)(std::forward<Args>(args)...);
	}
	static void SetTorque(int port, float torque) {
		applyToPort(port, &SDLGamepad::setTorque, torque);
	}
	static void SetSpring(int port, float saturation, float speed) {
		applyToPort(port, &SDLGamepad::setSpring, saturation, speed);
	}
	static void SetDamper(int port, float param, float speed) {
		applyToPort(port, &SDLGamepad::setDamper, param, speed);
	}
	static void StopHaptic(int port) {
		applyToPort(port, &SDLGamepad::stopHaptic);
	}

protected:
	u64 vib_stop_time = 0;
	SDL_JoystickID sdl_joystick_instance;

private:
	SDL_Joystick* sdl_joystick;
	float vib_inclination = 0;
	SDL_GameController *sdl_controller = nullptr;
	static std::map<SDL_JoystickID, std::shared_ptr<SDLGamepad>> sdl_gamepads;
	SDL_Haptic *haptic = nullptr;
	bool hapticRumble = false;
	bool hasAutocenter = false;
	int sineEffectId = -1;
	int constEffectId = -1;
	int springEffectId = -1;
	int damperEffectId = -1;
};

class SDLMouse : public Mouse
{
public:
	SDLMouse(u64 mouseId) : Mouse("SDL")
	{
		if (mouseId == 0) {
			this->_name = "Default Mouse";
			this->_unique_id = "sdl_mouse";
		}
		else {
			this->_name = "Mouse " + std::to_string(mouseId);
			this->_unique_id = "sdl_mouse_" + std::to_string(mouseId);
		}
		loadMapping();
	}

	void setAbsPos(int x, int y);
};

