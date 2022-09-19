#pragma once

#include <string>
#include <deque>
#include <chrono>
#include <atomic>

#include "gdxsv.pb.h"
#include "gdxsv_backend_tcp.h"
#include "gdxsv_backend_udp.h"
#include "gdxsv_backend_replay.h"
#include "gdxsv_backend_rollback.h"
#include "types.h"

#include "cfg/cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "reios/reios.h"
#include "emulator.h"


class Gdxsv {
public:
    enum class NetMode {
        Offline,
        Lbs,
        McsUdp,
        Replay,
        RollbackTest,
    };

    Gdxsv() : lbs_net(symbols),
              udp_net(symbols, maxlag),
              replay_net(symbols, maxlag),
              rollback_net(symbols, maxlag) {};

    bool Enabled() const;

    bool InGame() const;

    void Reset();

    void Update();

    void HookMainUiLoop();

    void HandleRPC();

    void RestoreOnlinePatch();

    void StartPingTest();

    bool StartReplayFile(const char* path);

    bool StartRollbackTest(const char* param);

private:
    void GcpPingTest();

    static std::string GenerateLoginKey();

    std::vector<u8> GeneratePlatformInfoPacket();

    std::string GeneratePlatformInfoString();

    void WritePatch();

    void ApplyOnlinePatch(bool first_time);

    void WritePatchDisk1();

    void WritePatchDisk2();

    NetMode netmode = NetMode::Offline;
    std::atomic<bool> enabled;
    std::atomic<int> disk;
    std::atomic<int> maxlag;

    std::string server;
    std::string loginkey;
    std::map<std::string, u32> symbols;

    proto::GamePatchList patch_list;
    std::vector<proto::ExtPlayerInfo> ext_player_info;

    GdxsvBackendTcp lbs_net;
    GdxsvBackendUdp udp_net;
    GdxsvBackendReplay replay_net;
    GdxsvBackendRollback rollback_net;

    std::atomic<bool> gcp_ping_test_finished;
    std::map<std::string, int> gcp_ping_test_result;
};

extern Gdxsv gdxsv;
