/*
    This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef LIBRETRO
#include "nswitch.h"
#include "stdclass.h"
#include "sdl/sdl.h"
#include "log/LogManager.h"
#include "emulator.h"
#include "rend/mainui.h"
#include "oslib/directory.h"

int main(int argc, char *argv[])
{
	socketInitializeDefault();
	nxlinkStdio();
	//appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);

	LogManager::Init();

	// Set directories
	flycast::mkdir("/flycast", 0755);
	flycast::mkdir("/flycast/data", 0755);
	set_user_config_dir("/flycast/");
	set_user_data_dir("/flycast/data/");

	add_system_config_dir("/flycast");
	add_system_config_dir("./");
	add_system_data_dir("/flycast/data/");
	add_system_data_dir("./");
	add_system_data_dir("data/");

	if (flycast_init(argc, argv))
		die("Flycast initialization failed");

	mainui_loop();

	sdl_window_destroy();
	flycast_term();

	socketExit();

	return 0;
}

void os_SetupInput()
{
	input_sdl_init();
}

void os_TermInput()
{
	input_sdl_quit();
}

void UpdateInputState()
{
	input_sdl_handle();
}

void os_DoEvents()
{
}

void os_CreateWindow()
{
	sdl_window_create();
}

#endif	//!LIBRETRO
