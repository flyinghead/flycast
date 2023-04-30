#pragma once
#include <cstdio>
#include <string>
#include <vector>

#include "rend/boxart/http_client.h"

// Functions provided to the emulator by the gdxsv module.

void gdxsv_emu_flycast_init();

void gdxsv_emu_start();

void gdxsv_emu_reset();

void gdxsv_emu_vblank();

void gdxsv_emu_mainui_loop();

void gdxsv_emu_rpc();

void gdxsv_emu_savestate(int slot);

void gdxsv_emu_loadstate(int slot);

bool gdxsv_ingame();

bool gdxsv_emu_menu_open();

bool gdxsv_widescreen_hack_enabled();

void gdxsv_emu_gui_display();

void gdxsv_emu_gui_settings();

void gdxsv_gui_display_osd();

void gdxsv_crash_append_log(FILE* f);

void gdxsv_crash_append_tag(const std::string& logfile, std::vector<http::PostField>& post_fields);
