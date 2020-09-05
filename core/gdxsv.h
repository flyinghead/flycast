#pragma once

#include <string>
#include <deque>
#include <chrono>

#include "types.h"
#include "cfg/cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "reios/reios.h"

class Gdxsv
{
public:
    bool Enabled();
	void Reset();
	void Update();
    void UpdateNetwork();

private:
	std::string GenerateLoginKey();
    void WritePatchDisk1();
    void WritePatchDisk2();
	int enabled;
	int disk;
	u8 maxlag;
	std::string server;
	std::string loginkey;
	std::map<std::string, u32> symbols;
    std::chrono::system_clock::time_point last_update;
};

extern Gdxsv gdxsv;