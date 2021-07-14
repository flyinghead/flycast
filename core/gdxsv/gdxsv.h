#pragma once

#include <string>
#include <deque>
#include <chrono>
#include <atomic>

#include "types.h"
#include "cfg/cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "reios/reios.h"

#include "gdxsv.pb.h"
#include "gdxsv_backend_tcp.h"
#include "gdxsv_backend_udp.h"

class Gdxsv {
public:
    Gdxsv() : lbs_net(symbols), udp_net(symbols, maxlag) {};

    ~Gdxsv();

    bool Enabled() const;

    bool InGame() const;

    void Reset();

    void Update();

    void SyncNetwork(bool write);

    bool UpdateAvailable();

    void OpenDownloadPage();

    void DismissUpdateDialog();

    std::string LatestVersion();

    void RestoreOnlinePatch();
private:
    void GcpPingTest(); // run on network thread

    static std::string GenerateLoginKey();

    std::vector<u8> GeneratePlatformInfoPacket();

    std::string GeneratePlatformInfoString();

    void WritePatch();

    void ApplyOnlinePatch(bool first_time);

    void WritePatchDisk1();

    void WritePatchDisk2();

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

    std::thread gcp_ping_test_thread;
    std::atomic<bool> gcp_ping_test_finished;
    std::map<std::string, int> gcp_ping_test_result;

    void handleReleaseJSON(const std::string &json);

    bool update_available = false;
    std::string latest_version_tag;
};

extern Gdxsv gdxsv;
