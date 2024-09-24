int jogWheelsDelta[2];
bool jogWheelsTouched[2];
short crossFader;

#if defined(USE_SDL)
#include "types.h"
#include "hid.h"
#include "input/gamepad_device.h"
#include <SDL.h>

#if SDL_VERSION_ATLEAST(2, 0, 18)
static SDL_hid_device *device;
static u8 lastJogTick[2];
static u16 lastJogTime[2];

// need rw access to the hid device (sudo chmod o+rw /dev/hidraw4)
void hidInit()
{
	SDL_hid_init();

	/*
	SDL_hid_device_info *devInfo = SDL_hid_enumerate(0, 0);	// vendor id, product id
	while (devInfo != nullptr)
	{
		printf("path %s vendor/prod %04x/%04x manufact %S product %S\n", devInfo->path, devInfo->vendor_id, devInfo->product_id,
				devInfo->manufacturer_string, devInfo->product_string);
		devInfo = devInfo->next;
	}
	SDL_hid_free_enumeration(devInfo);
	*/

	// 17cc/1310 is traktor s4
	device = SDL_hid_open(0x17cc, 0x1310, nullptr);
	if (device == nullptr)
		WARN_LOG(INPUT, "SDL_hid_open failed");
	else
		SDL_hid_set_nonblocking(device, 1);
	hidOutput(false, false, false, false, false);
}

void hidTerm()
{
	if (device != nullptr)
	{
		hidOutput(false, false, false, false, false);
		SDL_hid_close(device);
		device = nullptr;
	}
	SDL_hid_exit();
}

void hidInput()
{
	if (device == nullptr)
		return;
	u8 data[256];
	for (;;)
	{
		int read = SDL_hid_read(device, data, sizeof(data));
		if (read <= 0)
		{
			if (read < 0)
				WARN_LOG(INPUT, "hid_read failed");
			break;
		}
		if (read >= 10)
		{
			// addControl(group, name, offset, pack, bitmask, isEncoder, callback)
			//									I: u32
			//									H: u16
			if (data[0] == 1) // short msg
			{
				// MessageShort.addControl("deck1", "!jog_wheel", 0x01, "I")
				// MessageShort.addControl("deck2", "!jog_wheel", 0x05, "I")
				// MessageShort.addControl("deck1", "!jog_touch", 0x11, "B", 0x01)
				// MessageShort.addControl("deck2", "!jog_touch", 0x11, "B", 0x02)
				// MessageShort.addControl("deck1", "!play", 0x0D, "B", 0x01);
				// MessageShort.addControl("deck2", "!play", 0x0C, "B", 0x01);
				for (int i = 0; i < 2; i++)
				{
					u32 jog = *(u32 *)&data[i * 4 + 1];
					u8 tickval = jog & 0xff;
					u16 timeval = jog >> 16;
					if (lastJogTime[i] > timeval) {
						// We looped around.  Adjust current time so that subtraction works.
						timeval += 0x10000;
					}
					int time_delta = timeval - lastJogTime[i];
					if (time_delta == 0)
						// Spinning too fast to detect speed!  By not dividing we are guessing it took 1ms.
						time_delta = 1;
					int tick_delta = 0;
					if (lastJogTick[i] >= 200 && tickval <= 100)
						tick_delta = tickval + 256 - lastJogTick[i];
					else if (lastJogTick[i] <= 100 && tickval >= 200)
						tick_delta = tickval - lastJogTick[i] - 256;
					else
						tick_delta = tickval - lastJogTick[i];

					lastJogTick[i] = tickval;
					lastJogTime[i] = timeval;
					jogWheelsDelta[i] += tick_delta;
					//printf("%s wheel %d\n", i == 0 ? "left" : "right", jogWheelsDelta[i]);

					jogWheelsTouched[i] = data[0x11] & (1 << i);
				}
				if ((data[0xd] & 1) || (data[0xc] & 1))
					kcode[0] &= ~DC_BTN_START;
				else
					kcode[0] |= DC_BTN_START;
			}
			else
			{
				// reportID=2: long msg
				//MessageLong.addControl("[Master]", "crossfader", 0x07, "H");
				crossFader = *(u16 *)&data[7] - 2048;
				//printf("cross fader %d\n", crossFader);
			}
		}
	}
}

void hidOutput(bool play, bool spotL, bool spotR, bool backL, bool backR)
{
	if (device == nullptr)
		return;
	u8 msg[0x3E]{};
	msg[0] = 0x81;
	msg[0x20] = play ? 0xff : 0;	// deck1 Play
	msg[0x28] = play ? 0xff : 0;	// deck2 Play
	msg[0x2d] = spotL ? 0xff : 0;	// deck1 on air
	msg[0x33] = spotR ? 0xff : 0;	// deck2 on air
	msg[0x2e] = backL ? 0xff : 0;	// deck A
	msg[0x34] = backR ? 0xff : 0;	// deck B
	SDL_hid_write(device, msg, sizeof(msg));
}

#else // SDL < 2.0.18
void hidOutput(bool play, bool spotL, bool spotR, bool backL, bool backR) {}
#endif

#else // !USE_SDL
void hidOutput(bool play, bool spotL, bool spotR, bool backL, bool backR) {}
#endif
