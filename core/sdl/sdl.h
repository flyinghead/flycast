#pragma once
#include <SDL.h>
#include "types.h"

void input_sdl_init();
void input_sdl_handle();
void input_sdl_quit();
void sdl_window_create();
void sdl_window_destroy();
bool sdl_recreate_window(u32 flags);
bool sdl_update_display_metrics(SDL_Window *window, u32 windowFlags);
void sdl_fix_steamdeck_dpi(SDL_Window *window);
