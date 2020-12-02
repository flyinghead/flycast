#pragma once
#include <SDL2/SDL.h>
#include "types.h"

extern void input_sdl_init();
extern void input_sdl_handle();
extern void sdl_window_create();
extern void sdl_window_set_text(const char* text);
extern void sdl_window_destroy();
extern void sdl_recreate_window(u32 flags);

#ifdef _WIN32
#include <windows.h>
HWND sdl_get_native_hwnd();
#endif
