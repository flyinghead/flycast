#pragma once
#include "input/keyboard_device.h"
#include "sdl.h"

#ifdef __APPLE__
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
#endif

class SDLKeyboardDevice : public KeyboardDeviceTemplate<SDL_Scancode>
{
public:
	SDLKeyboardDevice(int maple_port) : KeyboardDeviceTemplate(maple_port, "SDL") {
		_unique_id = "sdl_keyboard";
		if (find_mapping())
		{
			if (input_mapper->version == 1)
			{
				// Convert keycodes to scancode
				SDL_Scancode scancodes[4][26] {};
				for (int i = 0; i < 26; i++)
				{
					DreamcastKey key = (DreamcastKey)(1 << i);
					for (int port = 0; port < 4; port++)
					{
						SDL_Keycode keycode = (SDL_Keycode)input_mapper->get_button_code(port, key);
						if ((int)keycode != -1)
							scancodes[port][i] = SDL_GetScancodeFromKey(keycode);
					}
				}
				for (int i = 0; i < 26; i++)
				{
					DreamcastKey key = (DreamcastKey)(1 << i);
					for (int port = 0; port < 4; port++)
						if (scancodes[port][i] != 0)
							input_mapper->set_button(port, key, (u32)scancodes[port][i]);
				}
				save_mapping();
			}
		}
		else
			input_mapper = getDefaultMapping();

#ifdef __APPLE__
		uint64_t deviceID;
		if ( MTDeviceIsAvailable() && MTDeviceGetDeviceID(MTDeviceCreateDefault(), &deviceID) == 0 && (vib_device = MTActuatorCreateFromDeviceID(deviceID)) != NULL && MTActuatorOpen(vib_device) == kIOReturnSuccess)
			rumbleEnabled = true;
#endif
	}

	const char *get_button_name(u32 code) override
	{
		const char *name = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)code));
		if (name[0] == 0)
			return nullptr;
		return name;
	}

#ifdef __APPLE__
	void rumble(float power, float inclination, u32 duration_ms) override
	{
		if (rumbleEnabled)
		{
			vib_stop_time = os_GetSeconds() + duration_ms / 1000.0;
			
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
				if ( vib_stop_time - os_GetSeconds() < 0 )
				{
					dispatch_source_cancel(_timer);
					return;
				}
				MTActuatorActuate(vib_device, pattern, 0, 0.0, 0.0);
			});
			
			dispatch_resume(_timer);
		}
	}
	
	~SDLKeyboardDevice() {
		if (rumbleEnabled)
		{
			MTActuatorClose(vib_device);
			CFRelease(vib_device);
		}
	}
#endif

protected:
	u8 convert_keycode(SDL_Scancode scancode) override
	{
		if (settings.input.keyboardLangId != KeyboardLayout::US && scancode == SDL_SCANCODE_BACKSLASH)
			return (u8)SDL_SCANCODE_NONUSHASH;
		else
			return (u8)scancode;
	}

#ifdef __APPLE__
private:
	std::stack<dispatch_source_t> vib_timer_stack;
	CFTypeRef vib_device = NULL;
	double vib_stop_time = 0;
#endif
};
