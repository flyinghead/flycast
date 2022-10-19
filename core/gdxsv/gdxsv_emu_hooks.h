#pragma once

void gdxsv_flycast_init();

void gdxsv_prepare_crashlog(const char* dump_dir, const char* minidump_id);

void gdxsv_emu_start();

void gdxsv_emu_reset();

void gdxsv_emu_update();

void gdxsv_emu_rpc();

void gdxsv_emu_savestate(int slot);

void gdxsv_emu_loadstate(int slot);

bool gdxsv_emu_enabled();

bool gdxsv_emu_ingame();

bool gdxsv_widescreen_hack_enabled();

void gdxsv_update_popup();

void gdxsv_emu_settings();

void gdxsv_mainui_loop();

void gdxsv_gui_display_osd();
