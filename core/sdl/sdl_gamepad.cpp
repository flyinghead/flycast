/*
	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "sdl_gamepad.h"
#include "stdclass.h"
#include "sdl_mappingparser.h"
#include "oslib/i18n.h"
#include <cmath>

std::map<SDL_JoystickID, std::shared_ptr<SDLGamepad>> SDLGamepad::sdl_gamepads;

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
		if (sdlController == nullptr) {
			INFO_LOG(INPUT, "using default mapping");
			return;
		}

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
		auto map_axis = [&](SDL_GameControllerAxis sdl_axis, DreamcastKey dc_axis, bool positive)
		{
			const bool isTrigger = sdl_axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT
					|| sdl_axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
			SDLControllerMappingParser parser(sdlController);
			SDL_GameControllerButtonBind2 bind = parser.getBindForAxis(sdl_axis, isTrigger ? 0 : positive ? 1 : -1);
			if (bind.bindType == SDL_CONTROLLER_BINDTYPE_NONE)
				return false;
			if (bind.bindType == SDL_CONTROLLER_BINDTYPE_AXIS)
			{
				if (isTrigger)
				{
					if (bind.value.axis.direction == 0)
						addTrigger(bind.value.axis.axis, false);
					else if (bind.value.axis.direction == 2)
						addTrigger(bind.value.axis.axis, true);
					else
						deleteTrigger(bind.value.axis.axis);
				}
				else {
					deleteTrigger(bind.value.axis.axis);
				}
				set_axis(dc_axis, bind.value.axis.axis, bind.value.axis.direction == -1 ? false : true);
			}
			else if (bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON) {
				set_axis(dc_axis, bind.value.button, positive);
			}
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
					return false;
				}
				set_axis(dc_axis, ((bind.value.hat.hat + 1) << 8) | dir, positive);
			}
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
};

SDLGamepad::SDLGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
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

	const int axes = SDL_JoystickNumAxes(sdl_joystick);

	if (SDL_IsGameController(joystick_idx))
	{
		sdl_controller = SDL_GameControllerOpen(joystick_idx);
		if (sdl_controller == nullptr)
			WARN_LOG(INPUT, "Can't open game controller %d: %s", joystick_idx, SDL_GetError());
	}

	loadMapping();

	hasAnalogStick = axes > 0;
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
		isWheel = SDL_JoystickGetType(sdl_joystick) == SDL_JOYSTICK_TYPE_WHEEL;
		if ((hapq & SDL_HAPTIC_SINE) != 0 && isWheel)
		{
			SDL_HapticEffect effect{};
			effect.type = SDL_HAPTIC_SINE;
			effect.periodic.direction.type = SDL_HAPTIC_STEERING_AXIS;
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
			effect.constant.direction.type = isWheel ? SDL_HAPTIC_STEERING_AXIS : SDL_HAPTIC_CARTESIAN;
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
			effect.condition.direction.type = isWheel ? SDL_HAPTIC_STEERING_AXIS : SDL_HAPTIC_CARTESIAN;	// not used but required!
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
			effect.condition.direction.type = isWheel ? SDL_HAPTIC_STEERING_AXIS : SDL_HAPTIC_CARTESIAN;	// not used but required!
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

void SDLGamepad::set_maple_port(int port) {
	GamepadDevice::set_maple_port(port);
	SDL_JoystickSetPlayerIndex(sdl_joystick, port <= 3 ? port : -1);
}

u16 SDLGamepad::getRumbleIntensity(float power) const
{
	if (rumblePower == 0)
		return 0;
	else
		return (u16)std::min(power * 65535.f / std::pow(1.06f, 100.f - rumblePower), 65535.f);
}

void SDLGamepad::doRumble(float power, u32 duration_ms) {
	setSine(power, 25.f, duration_ms);
}

void SDLGamepad::setSine(float power, float freq, u32 duration_ms)
{
	if (!rumbleEnabled)
		return;
	if (hapticRumble)
	{
		if (power != 0.f && freq != 0.f && duration_ms != 0)
		{
			SDL_HapticEffect effect{};
			effect.type = SDL_HAPTIC_SINE;
			effect.periodic.direction.type = SDL_HAPTIC_STEERING_AXIS;
			effect.periodic.direction.dir[0] = 1;
			effect.periodic.period = 1000 / std::min(freq, 100.f); // period in ms
			// pick random direction
			effect.periodic.magnitude = power * rumblePower / 100.f * 32767.f * ((rand() & 1) * 2 - 1);
			effect.periodic.length = duration_ms;
			SDL_HapticUpdateEffect(haptic, sineEffectId, &effect);
			SDL_HapticRunEffect(haptic, sineEffectId, 1);
		}
		else {
			SDL_HapticStopEffect(haptic, sineEffectId);
		}
	}
	else {
		const u16 intensity = getRumbleIntensity(power);
		SDL_JoystickRumble(sdl_joystick, intensity, intensity, duration_ms);
	}
}

void SDLGamepad::rumble(float power, float inclination, u32 duration_ms)
{
	if (rumbleEnabled)
	{
		vib_inclination = inclination * power;
		vib_stop_time = getTimeMs() + duration_ms;
		doRumble(power, duration_ms);
	}
}

void SDLGamepad::update_rumble()
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

void SDLGamepad::setTorque(float torque)
{
	if (haptic == nullptr || constEffectId == -1)
		return;
	if (torque != 0.f && rumblePower != 0)
	{
		SDL_HapticEffect effect{};
		effect.type = SDL_HAPTIC_CONSTANT;
		effect.constant.length = SDL_HAPTIC_INFINITY;
		if (isWheel)
		{
			effect.constant.direction.type = SDL_HAPTIC_STEERING_AXIS;
			effect.constant.direction.dir[0] = 1;
			effect.constant.level = torque * 32767.f * rumblePower / 100.f;
		}
		else
		{
			effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
			effect.constant.direction.dir[0] = torque < 0 ? -1 : 1;	// west/cw if torque < 0
			effect.constant.level = std::abs(torque) * 32767.f * rumblePower / 100.f;
		}
		SDL_HapticUpdateEffect(haptic, constEffectId, &effect);
		SDL_HapticRunEffect(haptic, constEffectId, 1);
	}
	else {
		SDL_HapticStopEffect(haptic, constEffectId);
	}
}

void SDLGamepad::stopHaptic()
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

void SDLGamepad::setSpring(float saturation, float speed)
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
			effect.condition.direction.type = isWheel ? SDL_HAPTIC_STEERING_AXIS : SDL_HAPTIC_CARTESIAN;
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

void SDLGamepad::setDamper(float param, float speed)
{
	if (haptic == nullptr || damperEffectId == -1)
		return;
	if (param != 0.f && speed != 0.f && rumblePower != 0)
	{
		SDL_HapticEffect effect{};
		effect.type = SDL_HAPTIC_DAMPER;
		effect.condition.length = SDL_HAPTIC_INFINITY;
		effect.condition.direction.type = isWheel ? SDL_HAPTIC_STEERING_AXIS : SDL_HAPTIC_CARTESIAN;
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

void SDLGamepad::close()
{
	NOTICE_LOG(INPUT, "SDL: Joystick '%s' on port %d disconnected", _name.c_str(), maple_port());
	if (haptic != nullptr)
	{
		stopHaptic();
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

const char *SDLGamepad::get_button_name(u32 code)
{
	using namespace i18n;
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
			{ "back", Tnop("Back") },
			{ "guide", Tnop("Guide") },
			{ "start", Tnop("Start") },
			{ "leftstick", "L3" },
			{ "rightstick", "R3" },
			{ "leftshoulder", "L1" },
			{ "rightshoulder", "R1" },
			{ "dpup", Tnop("DPad Up") },
			{ "dpdown", Tnop("DPad Down") },
			{ "dpleft", Tnop("DPad Left") },
			{ "dpright", Tnop("DPad Right") },
			{ "misc1", Tnop("Misc") },
			{ "paddle1", Tnop("Paddle 1") },
			{ "paddle2", Tnop("Paddle 2") },
			{ "paddle3", Tnop("Paddle 3") },
			{ "paddle4", Tnop("Paddle 4") },
			{ "touchpad", Tnop("Touchpad") },
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
						return T(button.label);
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
					name =  T("DPad Up");
					break;
				case 1:
					hat = SDL_HAT_DOWN;
					name =  T("DPad Down");
					break;
				case 2:
					hat = SDL_HAT_LEFT;
					name =  T("DPad Left");
					break;
				case 3:
					hat = SDL_HAT_RIGHT;
					name =  T("DPad Right");
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

const char *SDLGamepad::get_axis_name(u32 code)
{
	using namespace i18n;
	static struct
	{
		const char *sdlAxis;
		const char *label;
	} axesTable[] =
	{
			{ "leftx", Tnop("Left Stick X") },
			{ "lefty", Tnop("Left Stick Y") },
			{ "rightx", Tnop("Right Stick X") },
			{ "righty", Tnop("Right Stick Y") },
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
						return T(axis.label);
				return sdlAxis;
			}
		}
	return nullptr;
}

std::shared_ptr<InputMapping> SDLGamepad::getDefaultMapping() {
	return std::make_shared<DefaultInputMapping<>>(sdl_controller);
}

void SDLGamepad::resetMappingToDefault(bool arcade, bool gamepad)
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

bool SDLGamepad::find_mapping(int system)
{
	bool ret =  GamepadDevice::find_mapping(system);
	if (ret && input_mapper != nullptr && input_mapper->getTriggers().empty()
			&& sdl_controller != nullptr)
	{
		// Set the triggers from the SDL gamepad bindings in case they haven't been saved
		SDLControllerMappingParser parser(sdl_controller);
		auto setTrigger = [&](SDL_GameControllerAxis sdl_axis) {
			SDL_GameControllerButtonBind2 bind = parser.getBindForAxis(sdl_axis);
			if (bind.bindType != SDL_CONTROLLER_BINDTYPE_AXIS)
				return;
			if (bind.value.axis.direction == 0)
				input_mapper->addTrigger(bind.value.axis.axis, false);
			else if (bind.value.axis.direction == 2)
				input_mapper->addTrigger(bind.value.axis.axis, true);
		};
		setTrigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
		setTrigger(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
		save_mapping();
	}

	return ret;
}

SDLMouse::SDLMouse(u32 mouseId) : Mouse("SDL")
{
	if (mouseId == 0) {
		this->_name = i18n::Ts("Default Mouse");
		this->_unique_id = "sdl_mouse";
	}
	else {
		this->_name = strprintf(i18n::T("Mouse %d"), mouseId);
		this->_unique_id = "sdl_mouse_" + std::to_string(mouseId);
	}
	loadMapping();
}
