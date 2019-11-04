#include <csignal>
#include <unistd.h>
#include "types.h"

u16 kcode[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
s8 joyx[4], joyy[4];

// FIXME
void* x11_glc;

void os_DebugBreak()
{
	raise(SIGTRAP);
}

void* libPvr_GetRenderTarget()
{
	return nullptr;
}

void* libPvr_GetRenderSurface()
{
	return nullptr;
}

void os_SetupInput()
{
}

void UpdateInputState(u32 port)
{
}

void os_DoEvents()
{
}

void os_CreateWindow()
{
}

int get_mic_data(u8* buffer)
{
	return 0;
}
