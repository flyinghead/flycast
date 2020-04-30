#pragma once

#include <string>

#include "types.h"
#include "cfg/cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "reios/reios.h"

// Disc1
// hardware SEGA SEGAKATANA  maker SEGA ENTERPRISES ks 3DC7  type GD-ROM num 1/2   area J        ctrl 27BB dev A vga 1 wince 0 product T13306M    version V1.000 date 20020221 boot 1ST_READ.BIN     softco SEGA LC-T-133    name MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX

// Disc2
// hardware SEGA SEGAKATANA  maker SEGA ENTERPRISES ks 3DC7  type GD-ROM num 2/2   area J        ctrl 27BB dev A vga 1 wince 0 product T13306M    version V1.000 date 20020221 boot 1ST_READ.BIN     softco SEGA LC-T-133    name MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX

class Gdxsv
{
public:
	void Reset();
	void Update();

private:
	std::string GenerateLoginKey();
	int enabled;
	int disk;
	u8 maxlag;
	std::string server;
	std::string loginkey;
};

extern Gdxsv gdxsv;