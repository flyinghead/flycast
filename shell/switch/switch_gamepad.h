/*
	Copyright 2024 flyinghead

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
#include "sdl/sdl_gamepad.h"
#include "nswitch.h"

//
// SDL 2.0.14-4 rumble doesn't seem to work
// so this class implements rumble using the native API.
//
class SwitchGamepad : public SDLGamepad
{
public:
	SwitchGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
		: SDLGamepad(maple_port, joystick_idx, sdl_joystick)
	{
		// A dual joycon controller has 2 vibration devices (left and right joycons)
		// Joystick 0 is either HidNpadIdType_Handheld or HidNpadIdType_No1 depending on the joycon configuration
		Result rc = hidInitializeVibrationDevices(vibDeviceHandlesNoN, 2,  (HidNpadIdType)(HidNpadIdType_No1 + joystick_idx),
				HidNpadStyleTag_NpadJoyDual);
		if (R_FAILED(rc))
			WARN_LOG(INPUT, "hidInitializeVibrationDevices(No%d) failed %x", joystick_idx + 1, rc);
		if (joystick_idx == 0)
		{
			padInitializeDefault(&pad);
			rc = hidInitializeVibrationDevices(vibDeviceHandlesHandHeld, 2,  HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
			if (R_FAILED(rc))
				WARN_LOG(INPUT, "hidInitializeVibrationDevices(handHeld) failed %x", rc);
		}
	}

	void rumble(float power, float inclination, u32 duration_ms) override
	{
		if (!rumbleEnabled)
			return;

		power = std::min(power / std::pow(1.06f, 100.f - rumblePower), 1.f);
		float freq = 160.f + inclination * 100.f;
		HidVibrationValue vibValues[2]{};
		vibValues[0].amp_low = power;
		vibValues[0].freq_low  = freq;
		vibValues[0].amp_high = power;
		vibValues[0].freq_high  = freq;
		memcpy(&vibValues[1], &vibValues[0], sizeof(HidVibrationValue));
		hidSendVibrationValues(getDeviceHandle(), vibValues, 2);
		if (power != 0.f)
			vib_stop_time = getTimeMs() + duration_ms;
		else
			vib_stop_time = 0;
	}

	void update_rumble() override
	{
		if (!rumbleEnabled || vib_stop_time == 0.0)
			return;
		int rem_time = vib_stop_time - getTimeMs();
		if (rem_time <= 0)
		{
			HidVibrationValue vibValues[2]{};
			vibValues[0].freq_low  = vibValues[1].freq_low  = 160.f;
			vibValues[0].freq_high  = vibValues[1].freq_high  = 320.f;
			hidSendVibrationValues(getDeviceHandle(), vibValues, 2);
			vib_stop_time = 0;
		}
	}

private:
	HidVibrationDeviceHandle *getDeviceHandle()
	{
		if (sdl_joystick_instance != 0)
			return vibDeviceHandlesNoN;
		padUpdate(&pad);
		return padIsHandheld(&pad) ? vibDeviceHandlesHandHeld : vibDeviceHandlesNoN;
	}

	PadState pad;
	HidVibrationDeviceHandle vibDeviceHandlesHandHeld[2];
	HidVibrationDeviceHandle vibDeviceHandlesNoN[2];
};
