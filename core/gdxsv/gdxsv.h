#pragma once

#include <string>
#include <deque>
#include <chrono>
#include <atomic>

#include "gdxsv_network.h"
#include "types.h"
#include "cfg/cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "reios/reios.h"

class Gdxsv {
public:
    Gdxsv() = default;

    ~Gdxsv();

    bool Enabled();

    void Reset();

    void Update();

    void SyncNetwork(bool write);

private:
    void UpdateNetwork(); // run on network thread
    static std::string GenerateLoginKey();

    std::vector<u8> GeneratePlatformInfoPacket();

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
    std::atomic<bool> start_session_exchange;
    std::thread net_thread;
    std::mutex send_buf_mtx;
    std::deque<u8> send_buf;
    std::mutex recv_buf_mtx;
    std::deque<u8> recv_buf;

    TcpClient tcp_client;
    UdpClient udp_client;
};

extern Gdxsv gdxsv;