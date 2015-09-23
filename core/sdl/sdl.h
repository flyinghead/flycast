#pragma once
#include <SDL/SDL.h>
extern void* sdl_glc;
extern void input_sdl_init();
extern void input_sdl_handle(u32 port);
extern void sdl_window_create();
extern void sdl_window_set_text(const char* text);
