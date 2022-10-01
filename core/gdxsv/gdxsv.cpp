#include "gdxsv.h"

#include <xxhash.h>

#include <random>
#include <sstream>

#include "cfg/cfg.h"
#include "emulator.h"
#include "gdxsv_translation.h"
#include "hw/sh4/sh4_mem.h"
#include "lzma/CpuArch.h"
#include "network/ggpo.h"
#include "oslib/oslib.h"
#include "reios/reios.h"
#include "version.h"

bool Gdxsv::InGame() const {
    return enabled && !testmode && (netmode == NetMode::McsUdp || netmode == NetMode::McsRollback);
}

bool Gdxsv::Enabled() const { return enabled; }

void Gdxsv::DisplayOSD() {
    rollback_net.DisplayOSD();
}

void Gdxsv::Reset() {
    lbs_net.Reset();
    udp_net.Reset();
    RestoreOnlinePatch();

    if (upnp_init.valid()) {
        upnp_init = std::future<bool>();
        upnp.Term();
    }

    // Automatically add ContentPath if it is empty.
    if (config::ContentPath.get().empty()) {
        config::ContentPath.get().push_back("./");
    }

    auto game_id = std::string(ip_meta.product_number, sizeof(ip_meta.product_number));
    if (game_id != "T13306M   ") {
        enabled = false;
        return;
    }
    enabled = true;

    server = cfgLoadStr("gdxsv", "server", "zdxsv.net");
    loginkey = cfgLoadStr("gdxsv", "loginkey", "");

    if (loginkey.empty()) {
        loginkey = GenerateLoginKey();
    }

    cfgSaveStr("gdxsv", "server", server.c_str());
    cfgSaveStr("gdxsv", "loginkey", loginkey.c_str());

    std::string disk_num(ip_meta.disk_num, 1);
    if (disk_num == "1") disk = 1;
    if (disk_num == "2") disk = 2;

    udp_port = config::GdxLocalPort;
    if (config::EnableUPnP) {
        upnp_init = std::async(std::launch::async, [this]() { return upnp.Init(); });
    }

#ifdef __APPLE__
    signal(SIGPIPE, SIG_IGN);
#endif

    NOTICE_LOG(COMMON, "gdxsv disk:%d server:%s loginkey:%s udp_port:%d", (int)disk, server.c_str(), loginkey.c_str(),
               udp_port);

    lbs_net.callback_lbs_packet([this](const LbsMessage &lbs_msg) {
        if (lbs_msg.command == LbsMessage::lbsUserRegist || lbs_msg.command == LbsMessage::lbsUserDecide) {
            std::string id(6, ' ');
            for (int i = 0; i < 6; i++) {
                id[i] = lbs_msg.body[2 + i];
            }
            user_id = id;
        }
        if (lbs_msg.command == LbsMessage::lbsUserRegist || lbs_msg.command == LbsMessage::lbsUserDecide ||
            lbs_msg.command == LbsMessage::lbsLineCheck) {
            if (udp.Initialized()) {
                if (!lbs_remote.is_open()) {
                    lbs_remote.Open(lbs_net.RemoteHost().c_str(), lbs_net.RemotePort());
                }

                proto::Packet pkt;
                pkt.set_type(proto::MessageType::HelloLbs);
                pkt.mutable_hello_lbs_data()->set_user_id(user_id);
                char buf[128];
                if (pkt.SerializePartialToArray((void *)buf, (int)sizeof(buf))) {
                    udp.SendTo((const char *)buf, pkt.GetCachedSize(), lbs_remote);
                } else {
                    ERROR_LOG(COMMON, "packet serialize error");
                }
            }

            lbs_net.Send(GeneratePlatformInfoPacket());
        }
        if (lbs_msg.command == LbsMessage::lbsP2PMatching) {
            proto::P2PMatching matching;
            if (matching.ParseFromArray(lbs_msg.body.data(), lbs_msg.body.size())) {
                int port = udp.bind_port();
                udp.Close();
                lbs_remote.Close();
                rollback_net.Prepare(matching, port);
            } else {
                ERROR_LOG(COMMON, "p2p matching deserialize error");
            }
        }
        if (lbs_msg.command == LbsMessage::lbsReadyBattle) {
            // Reset current patches for no-patched game
            RestoreOnlinePatch();
        }
        if (lbs_msg.command == LbsMessage::lbsGamePatch) {
            // Reset current patches and update patch_list
            RestoreOnlinePatch();
            if (patch_list.ParseFromArray(lbs_msg.body.data(), lbs_msg.body.size())) {
                ApplyOnlinePatch(true);
            } else {
                ERROR_LOG(COMMON, "patch_list deserialize error");
            }
        }
        if (lbs_msg.command == LbsMessage::lbsBattleUserCount && disk == 2 &&
            GdxsvLanguage::Language() != GdxsvLanguage::Lang::Disabled) {
            u32 battle_user_count =
                u32(lbs_msg.body[0]) << 24 | u32(lbs_msg.body[1]) << 16 | u32(lbs_msg.body[2]) << 8 | lbs_msg.body[3];
            const u32 offset = 0x8C000000 + 0x00010000;
            gdxsv_WriteMem32(offset + 0x3839FC, battle_user_count);
        }
    });
}

void Gdxsv::Update() {
    if (!enabled) return;

    if (InGame()) {
        settings.input.fastForwardMode = false;
    }

    if (!ggpo::active()) {
        // Don't edit memory at vsync if ggpo::active
        WritePatch();
    }
}

void Gdxsv::HookMainUiLoop() {
    if (enabled) {
        if (netmode == NetMode::McsRollback) {
			gdxsv.rollback_net.OnMainUiLoop();
        }
    }
}

std::string Gdxsv::GeneratePlatformInfoString() {
    std::stringstream ss;
    ss << "cpu="
       <<
#if HOST_CPU == CPU_X86
        "x86"
#elif HOST_CPU == CPU_ARM
        "ARM"
#elif HOST_CPU == CPU_MIPS
        "MIPS"
#elif HOST_CPU == CPU_X64
        "x86/64"
#elif HOST_CPU == CPU_GENERIC
        "Generic"
#elif HOST_CPU == CPU_ARM64
        "ARM64"
#else
        "Unknown"
#endif
       << "\n";
    ss << "os="
       <<
#ifdef __ANDROID__
        "Android"
#elif defined(__unix__)
        "Linux"
#elif defined(__APPLE__)
#ifdef TARGET_IPHONE
        "iOS"
#else
        "macOS"
#endif
#elif defined(_WIN32)
        "Windows"
#else
        "Unknown"
#endif
       << "\n";
    ss << "flycast=" << GIT_VERSION << "\n";
    ss << "git_hash=" << GIT_HASH << "\n";
    ss << "build_date=" << BUILD_DATE << "\n";
    ss << "disk=" << (int)disk << "\n";
    ss << "wireless=" << (int)(os_GetConnectionMedium() == "Wireless") << "\n";
    ss << "patch_id=" << symbols[":patch_id"] << "\n";
    ss << "local_ip=" << lbs_net.LocalIP() << "\n";
    ss << "udp_port=" << udp_port << "\n";
    std::string machine_id = os_GetMachineID();
    if (machine_id.length()) {
        auto digest = XXH64(machine_id.c_str(), machine_id.size(), 37);
        ss << "machine_id=" << std::hex << digest << std::dec << "\n";
    }

    if (gcp_ping_test_finished) {
        for (const auto &res : gcp_ping_test_result) {
            ss << res.first << "=" << res.second << "\n";
        }
    }
    return ss.str();
}

std::vector<u8> Gdxsv::GeneratePlatformInfoPacket() {
    std::vector<u8> packet = {0x81, 0xff, 0x99, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff};
    auto s = GeneratePlatformInfoString();
    packet.push_back((s.size() >> 8) & 0xffu);
    packet.push_back(s.size() & 0xffu);
    std::copy(std::begin(s), std::end(s), std::back_inserter(packet));
    std::vector<u8> e_loginkey(loginkey.size());
    static const int magic[] = {0x46, 0xcf, 0x2d, 0x55};
    for (int i = 0; i < e_loginkey.size(); ++i) e_loginkey[i] ^= loginkey[i] ^ magic[i & 3];
    packet.push_back((e_loginkey.size() >> 8) & 0xffu);
    packet.push_back(e_loginkey.size() & 0xffu);
    std::copy(std::begin(e_loginkey), std::end(e_loginkey), std::back_inserter(packet));
    u16 payload_size = (u16)(packet.size() - 12);
    packet[4] = (payload_size >> 8) & 0xffu;
    packet[5] = payload_size & 0xffu;
    return packet;
}

void Gdxsv::HandleRPC() {
    u32 gdx_rpc_addr = symbols["gdx_rpc"];
    if (gdx_rpc_addr == 0) {
        return;
    }

    u32 response = 0;
    gdx_rpc_t gdx_rpc{};
    gdx_rpc.request = gdxsv_ReadMem32(gdx_rpc_addr);
    gdx_rpc.response = gdxsv_ReadMem32(gdx_rpc_addr + 4);
    gdx_rpc.param1 = gdxsv_ReadMem32(gdx_rpc_addr + 8);
    gdx_rpc.param2 = gdxsv_ReadMem32(gdx_rpc_addr + 12);
    gdx_rpc.param3 = gdxsv_ReadMem32(gdx_rpc_addr + 16);
    gdx_rpc.param4 = gdxsv_ReadMem32(gdx_rpc_addr + 20);

    if (gdx_rpc.request == GDX_RPC_SOCK_OPEN) {
        u32 tolobby = gdx_rpc.param1;
        u32 host_ip = gdx_rpc.param2;
        u32 port_no = gdx_rpc.param3;

        std::string host = server;
        u16 port = port_no;

        if (netmode == NetMode::Replay) {
            replay_net.Open();
        } else if (tolobby == 1) {
            udp_net.CloseMcsRemoteWithReason("cl_to_lobby");
            if (lbs_net.Connect(host, port)) {
                netmode = NetMode::Lbs;
                lbs_net.Send(GeneratePlatformInfoPacket());

                lbs_remote.Open(host.c_str(), port);
                if (udp.Bind(udp_port)) {
                    if (udp_port != udp.bind_port()) {
                        config::GdxLocalPort = udp_port = udp.bind_port();
                    }

                    if (config::EnableUPnP && upnp_port != udp.bind_port()) {
                        upnp_port = udp.bind_port();
                        port_mapping =
                            std::async(std::launch::async, [this]() { return upnp.AddPortMapping(upnp_port, false); });
                    }
                }
            } else {
                netmode = NetMode::Offline;
            }
        } else {
            lbs_net.Close();
            if (~host_ip == 0) {
                lbs_remote.Close();
                udp.Close();
                rollback_net.Open();
                netmode = NetMode::McsRollback;
            } else {
                char addr_buf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &host_ip, addr_buf, INET_ADDRSTRLEN);
                host = std::string(addr_buf);
                if (udp_net.Connect(host, port)) {
                    netmode = NetMode::McsUdp;
                } else {
                    netmode = NetMode::Offline;
                }
            }
        }
    }

    if (gdx_rpc.request == GDX_RPC_SOCK_CLOSE) {
        if (netmode == NetMode::Replay) {
            replay_net.Close();
        } else if (netmode == NetMode::McsRollback) {
            rollback_net.Close();
            netmode = NetMode::Offline;
        } else {
            lbs_net.Close();

            if (gdx_rpc.param2 == 0) {
                udp_net.CloseMcsRemoteWithReason("cl_app_close");
            } else if (gdx_rpc.param2 == 1) {
                udp_net.CloseMcsRemoteWithReason("cl_ppp_close");
            } else if (gdx_rpc.param2 == 2) {
                udp_net.CloseMcsRemoteWithReason("cl_soft_reset");
            } else {
                udp_net.CloseMcsRemoteWithReason("cl_tcp_close");
            }

            netmode = NetMode::Offline;
        }
    }

    if (gdx_rpc.request == GDX_RPC_SOCK_READ) {
        if (netmode == NetMode::Lbs) {
            response = lbs_net.OnSockRead(gdx_rpc.param1, gdx_rpc.param2);
        } else if (netmode == NetMode::McsUdp) {
            response = udp_net.OnSockRead(gdx_rpc.param1, gdx_rpc.param2);
        } else if (netmode == NetMode::Replay) {
            response = replay_net.OnSockRead(gdx_rpc.param1, gdx_rpc.param2);
        } else if (netmode == NetMode::McsRollback) {
            response = rollback_net.OnSockRead(gdx_rpc.param1, gdx_rpc.param2);
        }
    }

    if (gdx_rpc.request == GDX_RPC_SOCK_WRITE) {
        if (netmode == NetMode::Lbs) {
            response = lbs_net.OnSockWrite(gdx_rpc.param1, gdx_rpc.param2);
        } else if (netmode == NetMode::McsUdp) {
            response = udp_net.OnSockWrite(gdx_rpc.param1, gdx_rpc.param2);
        } else if (netmode == NetMode::Replay) {
            response = replay_net.OnSockWrite(gdx_rpc.param1, gdx_rpc.param2);
        } else if (netmode == NetMode::McsRollback) {
            response = rollback_net.OnSockWrite(gdx_rpc.param1, gdx_rpc.param2);
        }
    }

    if (gdx_rpc.request == GDX_RPC_SOCK_POLL) {
        if (netmode == NetMode::Lbs) {
            response = lbs_net.OnSockPoll();
        } else if (netmode == NetMode::McsUdp) {
            response = udp_net.OnSockPoll();
        } else if (netmode == NetMode::Replay) {
            response = replay_net.OnSockPoll();
        } else if (netmode == NetMode::McsRollback) {
            response = rollback_net.OnSockPoll();
        }
    }

    gdxsv_WriteMem32(gdx_rpc_addr, 0);
    gdxsv_WriteMem32(gdx_rpc_addr + 4, response);
    gdxsv_WriteMem32(gdx_rpc_addr + 8, 0);
    gdxsv_WriteMem32(gdx_rpc_addr + 12, 0);
    gdxsv_WriteMem32(gdx_rpc_addr + 16, 0);
    gdxsv_WriteMem32(gdx_rpc_addr + 20, 0);

    gdxsv_WriteMem32(symbols["is_online"], netmode != NetMode::Offline);
}

void Gdxsv::StartPingTest() {
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        GcpPingTest();
    }).detach();
}

void Gdxsv::GcpPingTest() {
    // powered by https://github.com/cloudharmony/network
    static const std::string get_path = "/probe/ping.js";
    static const std::map<std::string, std::string> gcp_region_hosts = {
        {"asia-east1", "asia-east1-gce.cloudharmony.net"},
        {"asia-east2", "asia-east2-gce.cloudharmony.net"},
        {"asia-northeast1", "asia-northeast1-gce.cloudharmony.net"},
        {"asia-northeast2", "asia-northeast2-gce.cloudharmony.net"},
        {"asia-northeast3", "asia-northeast3-gce.cloudharmony.net"},
        // {"asia-south1",             "asia-south1-gce.cloudharmony.net"}, // inactive now.
        {"asia-southeast1", "asia-southeast1-gce.cloudharmony.net"},
        {"australia-southeast1", "australia-southeast1-gce.cloudharmony.net"},
        {"europe-north1", "europe-north1-gce.cloudharmony.net"},
        {"europe-west1", "europe-west1-gce.cloudharmony.net"},
        {"europe-west2", "europe-west2-gce.cloudharmony.net"},
        {"europe-west3", "europe-west3-gce.cloudharmony.net"},
        {"europe-west4", "europe-west4-gce.cloudharmony.net"},
        {"europe-west6", "europe-west6-gce.cloudharmony.net"},
        {"northamerica-northeast1", "northamerica-northeast1-gce.cloudharmony.net"},
        {"southamerica-east1", "southamerica-east1-gce.cloudharmony.net"},
        {"us-central1", "us-central1-gce.cloudharmony.net"},
        {"us-east1", "us-east1-gce.cloudharmony.net"},
        {"us-east4", "us-east4-gce.cloudharmony.net"},
        {"us-west1", "us-west1-gce.cloudharmony.net"},
        {"us-west2", "us-west2-a-gce.cloudharmony.net"},
        {"us-west3", "us-west3-gce.cloudharmony.net"},
    };

    for (const auto &region_host : gcp_region_hosts) {
        gui_display_notification("Ping testing...", 1000);
        TcpClient client;
        std::stringstream ss;
        ss << "HEAD " << get_path << " HTTP/1.1"
           << "\r\n";
        ss << "Host: " << region_host.second << "\r\n";
        ss << "User-Agent: flycast for gdxsv"
           << "\r\n";
        ss << "Accept: */*"
           << "\r\n";
        ss << "\r\n";  // end of header

        if (!client.Connect(region_host.second.c_str(), 80)) {
            ERROR_LOG(COMMON, "connect failed : %s", region_host.first.c_str());
            continue;
        }

        auto request_header = ss.str();
        auto t1 = std::chrono::high_resolution_clock::now();
        int n = client.Send(request_header.c_str(), request_header.size());
        if (n < request_header.size()) {
            ERROR_LOG(COMMON, "send failed : %s", region_host.first.c_str());
            client.Close();
            continue;
        }

        char buf[1024] = {0};
        n = client.Recv(buf, 1024);
        if (n <= 0) {
            ERROR_LOG(COMMON, "recv failed : %s", region_host.first.c_str());
            client.Close();
            continue;
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        int rtt = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        const std::string response_header(buf, n);
        if (response_header.find("200 OK") != std::string::npos) {
            gcp_ping_test_result[region_host.first] = rtt;
            char latency_str[256];
            snprintf(latency_str, 256, "%s : %d[ms]", region_host.first.c_str(), rtt);
            NOTICE_LOG(COMMON, "%s", latency_str);
        } else {
            ERROR_LOG(COMMON, "error response : %s", response_header.c_str());
        }
        client.Close();
    }
    gcp_ping_test_finished = true;
    gui_display_notification("Ping test finished", 3000);
}

std::string Gdxsv::GenerateLoginKey() {
    const int n = 8;
    uint64_t seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937 gen(seed);
    std::string chars = "0123456789";
    std::uniform_int_distribution<> dist(0, chars.length() - 1);
    std::string key(n, 0);
    std::generate_n(key.begin(), n, [&]() { return chars[dist(gen)]; });
    return key;
}

void Gdxsv::ApplyOnlinePatch(bool first_time) {
    for (int i = 0; i < patch_list.patches_size(); ++i) {
        auto &patch = patch_list.patches(i);
        if (patch.write_once() && !first_time) {
            continue;
        }
        if (first_time) {
            NOTICE_LOG(COMMON, "patch apply: %s", patch.name().c_str());
        }
        for (int j = 0; j < patch.codes_size(); ++j) {
            auto &code = patch.codes(j);
            if (code.size() == 8) {
                gdxsv_WriteMem8(code.address(), (u8)(code.changed() & 0xff));
            }
            if (code.size() == 16) {
                gdxsv_WriteMem16(code.address(), (u16)(code.changed() & 0xffff));
            }
            if (code.size() == 32) {
                gdxsv_WriteMem32(code.address(), code.changed());
            }
        }
    }
}

void Gdxsv::RestoreOnlinePatch() {
    for (int i = 0; i < patch_list.patches_size(); ++i) {
        auto &patch = patch_list.patches(i);
        NOTICE_LOG(COMMON, "patch restore: %s", patch.name().c_str());
        for (int j = 0; j < patch.codes_size(); ++j) {
            auto &code = patch.codes(j);
            if (code.size() == 8) {
                gdxsv_WriteMem8(code.address(), (u8)(code.original() & 0xff));
            }
            if (code.size() == 16) {
                gdxsv_WriteMem16(code.address(), (u16)(code.original() & 0xffff));
            }
            if (code.size() == 32) {
                gdxsv_WriteMem32(code.address(), code.original());
            }
        }
    }
    patch_list.clear_patches();
}

void Gdxsv::WritePatch() {
    if (disk == 1) WritePatchDisk1();
    if (disk == 2) WritePatchDisk2();
    if (symbols["patch_id"] == 0 || gdxsv_ReadMem32(symbols["patch_id"]) != symbols[":patch_id"]) {
        NOTICE_LOG(COMMON, "patch %d %d", gdxsv_ReadMem32(symbols["patch_id"]), symbols[":patch_id"]);

#include "gdxsv_patch.inc"

        gdxsv_WriteMem32(symbols["disk"], (int)disk);
    }

    if (symbols["lang_patch_id"] == 0 || gdxsv_ReadMem32(symbols["lang_patch_id"]) != symbols[":lang_patch_id"] ||
        symbols[":lang_patch_lang"] != (u8)GdxsvLanguage::Language()) {
        NOTICE_LOG(COMMON, "lang_patch id=%d prev=%d lang=%d", gdxsv_ReadMem32(symbols["lang_patch_id"]),
                   symbols[":lang_patch_id"], GdxsvLanguage::Language());
#include "gdxsv_translation_patch.inc"
    }
}

void Gdxsv::WritePatchDisk1() {
    const u32 offset = 0x8C000000 + 0x00010000;

    // Max Rebattle Patch
    gdxsv_WriteMem8(0x0c0345b0, 5);

    // Fix cost 300 to 295
    gdxsv_WriteMem16(0x0c1b0fd0, 295);

    // Send key message every frame
    gdxsv_WriteMem8(0x0c310450, 1);

    // Reduce max lag-frame
    gdxsv_WriteMem8(0x0c310451, maxlag);

    // Modem connection fix
    const char *atm1 = "ATM1\r                                ";
    for (int i = 0; i < strlen(atm1); ++i) {
        gdxsv_WriteMem8(offset + 0x0015e703 + i, u8(atm1[i]));
    }

    // Overwrite serve address (max 20 chars)
    for (int i = 0; i < 20; ++i) {
        gdxsv_WriteMem8(offset + 0x0015e788 + i, (i < server.length()) ? u8(server[i]) : u8(0));
    }

    // Skip form validation
    gdxsv_WriteMem16(offset + 0x0003b0c4, u16(9));  // nop
    gdxsv_WriteMem16(offset + 0x0003b0cc, u16(9));  // nop
    gdxsv_WriteMem16(offset + 0x0003b0d4, u16(9));  // nop
    gdxsv_WriteMem16(offset + 0x0003b0dc, u16(9));  // nop

    // Write LoginKey
    if (gdxsv_ReadMem8(offset - 0x10000 + 0x002f6924) == 0) {
        for (int i = 0; i < std::min(loginkey.length(), size_t(8)) + 1; ++i) {
            gdxsv_WriteMem8(offset - 0x10000 + 0x002f6924 + i, (i < loginkey.length()) ? u8(loginkey[i]) : u8(0));
        }
    }

    // Ally HP
    u16 hp_offset = 0x0180;
    if (InGame()) {
        u8 player_index = gdxsv_ReadMem8(0x0c2f6652);
        if (player_index) {
            player_index--;
            // depend on 4 player battle
            u8 ally_index = player_index - (player_index & 1) + !(player_index & 1);
            u16 ally_hp = gdxsv_ReadMem16(0x0c3369d6 + ally_index * 0x2000);
            gdxsv_WriteMem16(0x0c3369d2 + player_index * 0x2000, ally_hp);
        }
        hp_offset -= 2;
    }
    gdxsv_WriteMem16(0x0c01d336, hp_offset);
    gdxsv_WriteMem16(0x0c01d56e, hp_offset);
    gdxsv_WriteMem16(0x0c01d678, hp_offset);
    gdxsv_WriteMem16(0x0c01d89e, hp_offset);

    // Disable soft reset
    gdxsv_WriteMem8(0x0c2f6657, InGame() ? 1 : 0);

    // Online patch
    ApplyOnlinePatch(false);
}

void Gdxsv::WritePatchDisk2() {
    const u32 offset = 0x8C000000 + 0x00010000;

    // Max Rebattle Patch
    gdxsv_WriteMem8(0x0c0219ec, 5);

    // Fix cost 300 to 295
    gdxsv_WriteMem16(0x0c21bfec, 295);
    gdxsv_WriteMem16(0x0c21bff4, 295);
    gdxsv_WriteMem16(0x0c21c034, 295);

    // Send key message every frame
    gdxsv_WriteMem8(0x0c3abb90, 1);

    // Reduce max lag-frame
    gdxsv_WriteMem8(0x0c3abb91, maxlag);

    // Modem connection fix
    const char *atm1 = "ATM1\r                                ";
    for (int i = 0; i < strlen(atm1); ++i) {
        gdxsv_WriteMem8(offset + 0x001be7c7 + i, u8(atm1[i]));
    }

    // Overwrite serve address (max 20 chars)
    for (int i = 0; i < 20; ++i) {
        gdxsv_WriteMem8(offset + 0x001be84c + i, (i < server.length()) ? u8(server[i]) : u8(0));
    }

    // Skip form validation
    gdxsv_WriteMem16(offset + 0x000284f0, u16(9));  // nop
    gdxsv_WriteMem16(offset + 0x000284f8, u16(9));  // nop
    gdxsv_WriteMem16(offset + 0x00028500, u16(9));  // nop
    gdxsv_WriteMem16(offset + 0x00028508, u16(9));  // nop

    // Write LoginKey
    if (gdxsv_ReadMem8(offset - 0x10000 + 0x00392064) == 0) {
        for (int i = 0; i < std::min(loginkey.length(), size_t(8)) + 1; ++i) {
            gdxsv_WriteMem8(offset - 0x10000 + 0x00392064 + i, (i < loginkey.length()) ? u8(loginkey[i]) : u8(0));
        }
    }

    // Ally HP
    u16 hp_offset = 0x0180;
    if (InGame()) {
        u8 player_index = gdxsv_ReadMem8(0x0c391d92);
        if (player_index) {
            player_index--;
            // depend on 4 player battle
            u8 ally_index = player_index - (player_index & 1) + !(player_index & 1);
            u16 ally_hp = gdxsv_ReadMem16(0x0c3d1e56 + ally_index * 0x2000);
            gdxsv_WriteMem16(0x0c3d1e52 + player_index * 0x2000, ally_hp);
        }
        hp_offset -= 2;
    }
    gdxsv_WriteMem16(0x0c11da88, hp_offset);
    gdxsv_WriteMem16(0x0c11dbbc, hp_offset);
    gdxsv_WriteMem16(0x0c11dcc0, hp_offset);
    gdxsv_WriteMem16(0x0c11ddd6, hp_offset);
    gdxsv_WriteMem16(0x0c11df08, hp_offset);
    gdxsv_WriteMem16(0x0c11e01a, hp_offset);

    // Disable soft reset
    gdxsv_WriteMem8(0x0c391d97, InGame() ? 1 : 0);

    // Online patch
    ApplyOnlinePatch(false);

    // Dirty widescreen cheat
    if (config::WidescreenGameHacks.get()) {
        u32 ratio = 0x3faaaaab;  // default 4/3
        int stretching = 100;
        bool update = false;
        if (gdxsv_ReadMem8(0x0c3d16d4) == 2 && gdxsv_ReadMem8(0x0c3d16d5) == 7) {  // In main game part
            // Changing this value outside the game part will break UI layout.
            // For 0x0c3d16d5: 4=load briefing, 5=briefing, 7=battle, 0xd=rebattle/end selection
            if (config::ScreenStretching == 100) {
                // ratio = 0x3fe4b17e; // wide 4/3 * 1.34
                // stretching = 134;
                // Use a little wider than 16/9 because of a glitch at the edges of the screen.
                ratio = 0x40155555;
                stretching = 175;
                update = true;
            }
        } else {
            if (config::ScreenStretching != 100) {
                update = true;
            }
        }
        if (update) {
            config::ScreenStretching.override(stretching);
            gdxsv_WriteMem32(0x0c1e7948, ratio);
            gdxsv_WriteMem32(0x0c1e7958, ratio);
            gdxsv_WriteMem32(0x0c1e7968, ratio);
            gdxsv_WriteMem32(0x0c1e7978, ratio);
        }
    }
}

bool Gdxsv::StartReplayFile(const char *path) {
    testmode = true;
    replay_net.Reset();
    if (replay_net.StartFile(path)) {
        netmode = NetMode::Replay;
        return true;
    }

    auto str = std::string(path);
    if (4 <= str.length() && str.substr(0, 4) == "http") {
        auto resp = os_FetchStringFromURL(str);
        if (0 < resp.size() && replay_net.StartBuffer(resp.data(), resp.size())) {
            netmode = NetMode::Replay;
            return true;
        }
    }
    return false;
}

bool Gdxsv::StartRollbackTest(const char *param) {
    testmode = true;
    rollback_net.Reset();

    if (rollback_net.StartLocalTest(param)) {
        netmode = NetMode::McsRollback;
        return true;
    }

    return false;
}

Gdxsv gdxsv;
