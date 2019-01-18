#pragma once
#include <SDL2/SDL.h>

extern void input_sdl_init();
extern void input_sdl_handle(u32 port);
extern void input_sdl_rumble(u32 port, u16 pow_strong, u16 pow_weak);
extern void sdl_window_create();
extern void sdl_window_set_text(const char* text);
