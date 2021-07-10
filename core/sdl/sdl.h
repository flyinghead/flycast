#pragma once
#include <SDL.h>
#include "types.h"

void input_sdl_init();
void input_sdl_handle();
void sdl_window_create();
void sdl_window_set_text(const char* text);
void sdl_window_destroy();
bool sdl_recreate_window(u32 flags);
