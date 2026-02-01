#include "maple_helper.h"
#include "maple_if.h"

u32 maple_GetAttachedDevices(u32 bus)
{
	verify(MapleDevices[bus][5] != nullptr);

	u32 rv = 0;
	for (int i = 0; i < 5; i++)
	{
		auto device = MapleDevices[bus][i];
		if (device != nullptr && device->linkStatus())
			rv |= 1 << i;
	}
	return rv;
}
