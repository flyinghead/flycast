#pragma once

void gdxsv_emu_start();

void gdxsv_emu_reset();

void gdxsv_emu_update();

void gdxsv_emu_rpc();

void gdxsv_emu_savestate(int slot);

void gdxsv_emu_loadstate(int slot);

bool gdxsv_emu_ingame();

void gdxsv_update_popup();

void gdxsv_emu_settings();

void gdxsv_mainui_loop();

