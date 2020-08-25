/*
	Created on: Apr 20, 2020

	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <atomic>

void InitSettings();
void LoadSettings(bool game_specific);
void SaveSettings();

extern std::atomic<bool> loading_canceled;

int reicast_init(int argc, char* argv[]);
void dc_reset(bool hard);
void dc_init();
void* dc_run(void*);
void dc_term();
void dc_stop();
void dc_request_reset();
void dc_exit();
void dc_resume();
void dc_savestate();
void dc_loadstate();
void dc_load_game(const char *path);
bool dc_is_load_done();
void dc_cancel_load();
void dc_get_load_status();
bool dc_is_running();
