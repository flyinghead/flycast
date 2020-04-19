#pragma once
#include "types.h"

static inline u32 maple_GetBusId(u32 addr)
{
	return addr >> 6;
}

u32 maple_GetPort(u32 addr);
u32 maple_GetAttachedDevices(u32 bus);

//device : 0 .. 4 -> subdevice , 5 -> main device :)
static inline u32 maple_GetAddress(u32 bus, u32 port)
{
	u32 rv = bus << 6;
	rv |= 1 << port;

	return rv;
}
