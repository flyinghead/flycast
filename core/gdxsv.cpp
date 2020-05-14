#include <sstream>
#include <iomanip>
#include <random>

#include "gdxsv.h"
#include "Log/Log.h"

// hardware SEGA SEGAKATANA  maker SEGA ENTERPRISES ks 3DC7  type GD-ROM num 1/2   area J        ctrl 27BB dev A vga 1 wince 0 product T13306M    version V1.000 date 20020221 boot 1ST_READ.BIN     softco SEGA LC-T-133    name MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX
// hardware SEGA SEGAKATANA  maker SEGA ENTERPRISES ks 3DC7  type GD-ROM num 2/2   area J        ctrl 27BB dev A vga 1 wince 0 product T13306M    version V1.000 date 20020221 boot 1ST_READ.BIN     softco SEGA LC-T-133    name MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX

void Gdxsv::Reset()
{
    if (settings.dreamcast.ContentPath.empty()) {
        settings.dreamcast.ContentPath.push_back("./");
    }

    auto game_id = std::string(ip_meta.product_number, sizeof(ip_meta.product_number));
	if (game_id != "T13306M   ")
	{
		enabled = 0;
		return;
	}

	enabled = 1;

    server = cfgLoadStr("gdxsv", "server", "zdxsv.net");
	maxlag = cfgLoadInt("gdxsv", "maxlag", 8); // Note: This should be not configurable. This is for development.
	loginkey = cfgLoadStr("gdxsv", "loginkey", "");
    bool overwriteconf = cfgLoadBool("gdxsv", "overwriteconf", true);

	if (loginkey.empty())
	{
		loginkey = GenerateLoginKey();
	}

    if (overwriteconf) {
        NOTICE_LOG(COMMON, "Overwrite configs for gdxsv");

        settings.aica.BufferSize = 529;
        settings.pvr.SynchronousRender = false;
    }

    cfgSaveStr("gdxsv", "server", server.c_str());
	cfgSaveStr("gdxsv", "loginkey", loginkey.c_str());
    cfgSaveBool("gdxsv", "overwritedconf", overwriteconf);

	std::string disk_num(ip_meta.disk_num, 1);
	if (disk_num == "1")
	{
		disk = 1;
	}
	if (disk_num == "2")
	{
		disk = 2;
	}

	NOTICE_LOG(COMMON, "gdxsv disk:%d server:%s loginkey:%s maxlag:%d", disk, server.c_str(), loginkey.c_str(), maxlag);
}

void Gdxsv::Update()
{
	if (!enabled)
	{
		return;
	}

	if (disk == 1)
	{
		const u32 offset = 0x8C000000 + 0x00010000;

		// Reduce max lag-frame
		WriteMem8_nommu(offset + 0x00047f60, maxlag);
		WriteMem8_nommu(offset + 0x00047f66, maxlag);

		// Modem connection fix
		const char *atm1 = "ATM1\r                                ";
		for (int i = 0; i < strlen(atm1); ++i)
		{
			WriteMem8_nommu(offset + 0x0015e703 + i, u8(atm1[i]));
		}

		// Overwrite serve address (max 20 chars)
		for (int i = 0; i < 20; ++i)
		{
			WriteMem8_nommu(offset + 0x0015e788 + i, (i < server.length()) ? u8(server[i]) : u8(0));
		}

		// Skip form validation
		WriteMem16_nommu(offset + 0x0003b0c4, u16(9)); // nop
		WriteMem16_nommu(offset + 0x0003b0cc, u16(9)); // nop
		WriteMem16_nommu(offset + 0x0003b0d4, u16(9)); // nop
		WriteMem16_nommu(offset + 0x0003b0dc, u16(9)); // nop

		// Write LoginKey
		if (ReadMem8_nommu(offset - 0x10000 + 0x002f6924) == 0)
		{
			for (int i = 0; i < std::min(loginkey.length(), size_t(8)) + 1; ++i)
			{
				WriteMem8_nommu(offset - 0x10000 + 0x002f6924 + i, (i < loginkey.length()) ? u8(loginkey[i]) : u8(0));
			}
		}
	}

	if (disk == 2)
	{
		const u32 offset = 0x8C000000 + 0x00010000;

		// Reduce max lag-frame
		WriteMem8_nommu(offset + 0x00035348, maxlag);
		WriteMem8_nommu(offset + 0x0003534e, maxlag);

		// Modem connection fix
		const char *atm1 = "ATM1\r                                ";
		for (int i = 0; i < strlen(atm1); ++i)
		{
			WriteMem8_nommu(offset + 0x001be7c7 + i, u8(atm1[i]));
		}

		// Overwrite serve address (max 20 chars)
		for (int i = 0; i < 20; ++i)
		{
			WriteMem8_nommu(offset + 0x001be84c + i, (i < server.length()) ? u8(server[i]) : u8(0));
		}

		// Skip form validation
		WriteMem16_nommu(offset + 0x000284f0, u16(9)); // nop
		WriteMem16_nommu(offset + 0x000284f8, u16(9)); // nop
		WriteMem16_nommu(offset + 0x00028500, u16(9)); // nop
		WriteMem16_nommu(offset + 0x00028508, u16(9)); // nop

		// Write LoginKey
		if (ReadMem8_nommu(offset - 0x10000 + 0x00392064) == 0)
		{
			for (int i = 0; i < std::min(loginkey.length(), size_t(8)) + 1; ++i)
			{
				WriteMem8_nommu(offset - 0x10000 + 0x00392064 + i, (i < loginkey.length()) ? u8(loginkey[i]) : u8(0));
			}
		}
	}
}

std::string Gdxsv::GenerateLoginKey()
{
	const int n = 8;
	uint64_t seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::mt19937 gen(seed);
	std::string chars = "0123456789";
	std::uniform_int_distribution<> dist(0, chars.length() - 1);
	std::string key(n, 0);
	std::generate_n(key.begin(), n, [&]() {
		return chars[dist(gen)];
	});
	return key;
}

Gdxsv gdxsv;