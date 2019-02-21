#pragma once
#include "types.h"

extern void input_evdev_init();
extern void input_evdev_close();
extern bool input_evdev_handle(u32 port);
extern void input_evdev_rumble(u32 port, u16 pow_strong, u16 pow_weak);
