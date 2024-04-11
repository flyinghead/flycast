/*
	Copyright 2022 edw

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
#pragma once
#ifdef __APPLE__
#include "sdl_keyboard.h"
#include "stdclass.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOTypes.h>
#include <stack>

// Rumbling Taptic Engine by Private MultitouchSupport.framework
extern "C" {
typedef void *MTDeviceRef;
bool MTDeviceIsAvailable(void);
MTDeviceRef MTDeviceCreateDefault(void);
OSStatus MTDeviceGetDeviceID(MTDeviceRef, uint64_t*) __attribute__ ((weak_import));
CFTypeRef MTActuatorCreateFromDeviceID(UInt64 deviceID);
IOReturn MTActuatorOpen(CFTypeRef actuatorRef);
IOReturn MTActuatorClose(CFTypeRef actuatorRef);
IOReturn MTActuatorActuate(CFTypeRef actuatorRef, SInt32 actuationID, UInt32 unknown1, Float32 unknown2, Float32 unknown3);
bool MTActuatorIsOpen(CFTypeRef actuatorRef);
enum ActuatePattern { minimal = 3, weak = 5, medium = 4, strong = 6 };
}

class SDLMacKeyboard : public SDLKeyboardDevice
{
public:
	SDLMacKeyboard(int maple_port) : SDLKeyboardDevice(maple_port)
	{
		uint64_t deviceID;
		if (MTDeviceIsAvailable()
				&& MTDeviceGetDeviceID(MTDeviceCreateDefault(), &deviceID) == 0
				&& (vib_device = MTActuatorCreateFromDeviceID(deviceID)) != NULL
				&& MTActuatorOpen(vib_device) == kIOReturnSuccess)
			rumbleEnabled = true;
	}

	void rumble(float power, float inclination, u32 duration_ms) override
	{
		if (!rumbleEnabled)
			return;

		vib_stop_time = getTimeMs() + duration_ms;

		__block int pattern;
		if (power >= 0.75)
			pattern = ActuatePattern::strong;
		else if (power >= 0.5)
			pattern = ActuatePattern::medium;
		else if (power >= 0.25)
			pattern = ActuatePattern::weak;
		else if (power > 0)
			pattern = ActuatePattern::minimal;
		else
		{
			while(!vib_timer_stack.empty())
			{
				dispatch_source_cancel(vib_timer_stack.top());
				vib_timer_stack.pop();
			}
			return;
		}
		// Since the Actuator API does not support duration
		// using a interval timer with `10ms * rumblePower percentage` to fake it
		__block dispatch_source_t _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
		vib_timer_stack.push(_timer);
		dispatch_source_set_timer(_timer, DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC * rumblePower / 100.f, 0);

		dispatch_source_set_event_handler(_timer, ^{
			if (vib_stop_time < getTimeMs())
			{
				dispatch_source_cancel(_timer);
				return;
			}
			MTActuatorActuate(vib_device, pattern, 0, 0.0, 0.0);
		});

		dispatch_resume(_timer);
	}

	~SDLMacKeyboard()
	{
		if (rumbleEnabled)
		{
			MTActuatorClose(vib_device);
			CFRelease(vib_device);
		}
	}

private:
	std::stack<dispatch_source_t> vib_timer_stack;
	CFTypeRef vib_device = NULL;
	u64 vib_stop_time = 0;
};

#endif // _APPLE_
