#pragma once

#include <string>
#include <deque>
#include <chrono>
#include <atomic>

#include "types.h"
#include "cfg/cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "reios/reios.h"

class Gdxsv
{
public:
    Gdxsv();
    ~Gdxsv();
    bool Enabled();
	void Reset();
	void Update();
    void SyncNetwork(bool write);

private:
    void StartNetwork();
    void UpdateNetwork(); // run on network thread
	std::string GenerateLoginKey();
    void WritePatch();
    void WritePatchDisk1();
    void WritePatchDisk2();

    bool enabled;
    int disk;
	u8 maxlag;

	std::string server;
	std::string loginkey;
	std::map<std::string, u32> symbols;

    std::atomic<bool> net_terminate;
    std::thread net_thread;
    std::mutex net_mtx;
    std::deque<u8> send_buf;
    std::deque<u8> recv_buf;
};

extern Gdxsv gdxsv;