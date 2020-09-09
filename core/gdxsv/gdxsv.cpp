#include "gdxsv.h"

#include <sstream>
#include <iomanip>
#include <random>

#include "network/net_platform.h"
#include "packet.h"
#include "packet_reader.h"
#include "packet_writer.h"
#include "gdx_queue.h"
#include "version.h"

Gdxsv::~Gdxsv() {
    tcp_client.Close();
    net_terminate = true;
    if (net_thread.joinable()) {
        net_thread.join();
    }
}

bool Gdxsv::Enabled() {
    return enabled;
}

void Gdxsv::Reset() {
    if (settings.dreamcast.ContentPath.empty()) {
        settings.dreamcast.ContentPath.emplace_back("./");
    }

    auto game_id = std::string(ip_meta.product_number, sizeof(ip_meta.product_number));
    if (game_id != "T13306M   ") {
        enabled = 0;
        return;
    }
    enabled = 1;

    if (!net_thread.joinable()) {
        NOTICE_LOG(COMMON, "start net thread");
        net_thread = std::thread([this]() {
            UpdateNetwork();
            NOTICE_LOG(COMMON, "end net thread");
        });
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        ERROR_LOG(COMMON, "WSAStartup failed. errno=%d", get_last_error());
        return;
    }
#endif

    server = cfgLoadStr("gdxsv", "server", "zdxsv.net");
    maxlag = cfgLoadInt("gdxsv", "maxlag", 8); // Note: This should be not configurable. This is for development.
    loginkey = cfgLoadStr("gdxsv", "loginkey", "");
    bool overwriteconf = cfgLoadBool("gdxsv", "overwriteconf", true);

    if (loginkey.empty()) {
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
    if (disk_num == "1") disk = 1;
    if (disk_num == "2") disk = 2;
    tcp_client.Close();
    udp_client.Close();
    NOTICE_LOG(COMMON, "gdxsv disk:%d server:%s loginkey:%s maxlag:%d", disk, server.c_str(), loginkey.c_str(), maxlag);
}

void Gdxsv::Update() {
    if (!enabled) return;
    WritePatch();

    u8 dump_buf[1024];
    if (ReadMem32_nommu(symbols["print_buf_pos"])) {
        int n = ReadMem32_nommu(symbols["print_buf_pos"]);
        n = std::min(n, (int) sizeof(dump_buf));
        for (int i = 0; i < n; i++) {
            dump_buf[i] = ReadMem8_nommu(symbols["print_buf"] + i);
        }
        dump_buf[n] = 0;
        WriteMem32_nommu(symbols["print_buf_pos"], 0);
        WriteMem32_nommu(symbols["print_buf"], 0);
        NOTICE_LOG(COMMON, "%s", dump_buf);
    }
}

std::vector<u8> Gdxsv::GeneratePlatformInfoPacket() {
    std::vector<u8> packet = {
            0x81,
            0xFF,
            0x99, 0x50,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0xff, 0xff, 0xff};
    std::stringstream ss;
    ss << "flycast=" << REICAST_VERSION << "\n";
    ss << "git_hash=" << GIT_HASH << "\n";
    ss << "build_date=" << BUILD_DATE << "\n";
    ss << "cpu=" <<
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
    ss << "os=" <<
       #ifdef __ANDROID__
       "Android"
       #elif HOST_OS == OS_LINUX
       "Linux"
       #elif defined(__APPLE__)
       #ifdef TARGET_IPHONE
       "iOS"
       #else
       "OSX"
       #endif
       #elif defined(_WIN32)
       "Windows"
       #else
       "Unknown"
       #endif
       << "\n";
    ss << "disk" << disk << "\n";
    ss << "maxlag" << maxlag << "\n";
    ss << "patch_id" << symbols[":patch_id"] << "\n";
    auto s = ss.str();
    packet.push_back((s.size() >> 8) & 0xffu);
    packet.push_back(s.size() & 0xffu);
    std::copy(begin(s), end(s), std::back_inserter(packet));
    u16 payload_size = (u16) (packet.size() - 12);
    packet[4] = (payload_size >> 8) & 0xffu;
    packet[5] = payload_size & 0xffu;
    return packet;
}

void Gdxsv::SyncNetwork(bool write) {
    if (write) {
        gdx_queue q;
        u32 gdx_txq_addr = symbols["gdx_txq"];
        if (gdx_txq_addr == 0) return;
        u32 buf_addr = gdx_txq_addr + 4;
        q.head = ReadMem16_nommu(gdx_txq_addr);
        q.tail = ReadMem16_nommu(gdx_txq_addr + 2);
        int n = gdx_queue_size(&q);
        if (0 < n) {
            send_buf_mtx.lock();
            for (int i = 0; i < n; ++i) {
                send_buf.push_back(ReadMem8_nommu(buf_addr + q.head));
                gdx_queue_pop(&q);
            }
            send_buf_mtx.unlock();
            WriteMem16_nommu(gdx_txq_addr, q.head);
        }
    } else {
        gdx_rpc_t gdx_rpc;
        u32 gdx_rpc_addr = symbols["gdx_rpc"];
        if (gdx_rpc_addr == 0) return;
        gdx_rpc.request = ReadMem32_nommu(gdx_rpc_addr);
        if (gdx_rpc.request) {
            gdx_rpc.response = ReadMem32_nommu(gdx_rpc_addr + 4);
            gdx_rpc.param1 = ReadMem32_nommu(gdx_rpc_addr + 8);
            gdx_rpc.param2 = ReadMem32_nommu(gdx_rpc_addr + 12);
            gdx_rpc.param3 = ReadMem32_nommu(gdx_rpc_addr + 16);
            gdx_rpc.param4 = ReadMem32_nommu(gdx_rpc_addr + 20);

            if (gdx_rpc.request == GDXRPC_TCP_OPEN) {
                recv_buf_mtx.lock();
                recv_buf.clear();
                recv_buf_mtx.unlock();

                send_buf_mtx.lock();
                send_buf.clear();
                send_buf_mtx.unlock();

                u32 tolobby = gdx_rpc.param1;
                u32 host_ip = gdx_rpc.param2;
                u32 port_no = gdx_rpc.param3;

                std::string host = server;
                u16 port = port_no;

                if (tolobby == 1) {
                    bool ok = tcp_client.Connect(host.c_str(), port);
                    if (!ok) {
                        WARN_LOG(COMMON, "Failed to connect %s:%d", host.c_str(), port);
                    }

                    auto packet = GeneratePlatformInfoPacket();
                    send_buf_mtx.lock();
                    send_buf.clear();
                    std::copy(begin(packet), end(packet), std::back_inserter(send_buf));
                    send_buf_mtx.unlock();
                } else {
                    char addr_buf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &host_ip, addr_buf, INET_ADDRSTRLEN);
                    host = std::string(addr_buf);
                    bool ok = udp_client.Connect(host.c_str(), port);
                    if (ok) {
                        start_session_exchange = true;
                        recv_buf_mtx.lock();
                        recv_buf.assign(
                                {0x0e, 0x61, 0x00, 0x22, 0x10, 0x31, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd});
                        recv_buf_mtx.unlock();
                    } else {
                        WARN_LOG(COMMON, "Failed to connect %s:%d", host.c_str(), port);
                    }
                }
            }

            if (gdx_rpc.request == GDXRPC_TCP_CLOSE) {
                tcp_client.Close();
                udp_client.Close();

                recv_buf_mtx.lock();
                recv_buf.clear();
                recv_buf_mtx.unlock();

                send_buf_mtx.lock();
                send_buf.clear();
                send_buf_mtx.unlock();
            }

            WriteMem32_nommu(gdx_rpc_addr, 0);
            WriteMem32_nommu(gdx_rpc_addr + 4, 0);
            WriteMem32_nommu(gdx_rpc_addr + 8, 0);
            WriteMem32_nommu(gdx_rpc_addr + 12, 0);
            WriteMem32_nommu(gdx_rpc_addr + 16, 0);
            WriteMem32_nommu(gdx_rpc_addr + 20, 0);
        }

        WriteMem32_nommu(symbols["is_online"], tcp_client.IsConnected() || udp_client.IsConnected());

        recv_buf_mtx.lock();
        int n = recv_buf.size();
        recv_buf_mtx.unlock();
        if (0 < n) {
            gdx_queue q;
            u32 gdx_rxq_addr = symbols["gdx_rxq"];
            u32 buf_addr = gdx_rxq_addr + 4;
            q.head = ReadMem16_nommu(gdx_rxq_addr);
            q.tail = ReadMem16_nommu(gdx_rxq_addr + 2);

            u8 buf[GDX_QUEUE_SIZE];
            recv_buf_mtx.lock();
            int n = std::min<int>(recv_buf.size(), gdx_queue_avail(&q));
            for (int i = 0; i < n; ++i) {
                WriteMem8_nommu(buf_addr + q.tail, recv_buf.front());
                recv_buf.pop_front();
                gdx_queue_push(&q, 0);
            }
            recv_buf_mtx.unlock();
            WriteMem16_nommu(gdx_rxq_addr + 2, q.tail);
        }
    }
}

void Gdxsv::UpdateNetwork() {
    u8 buf[GDX_QUEUE_SIZE];
    bool updated = false;

    static const int kFirstMessageSize = 20;
    MessageBuffer message_buf;
    MessageFilter message_filter;
    Packet pkt;
    std::string session_id;

    while (!net_terminate) {
        if (!updated) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        updated = false;
        if (!tcp_client.IsConnected() && !udp_client.IsConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        if (start_session_exchange) {
            start_session_exchange = false;
            session_id.clear();
            message_buf.Clear();
            message_filter.Clear();

            // get session_id from client
            for (int i = 0; i < 60; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                send_buf_mtx.lock();
                int n = send_buf.size();
                send_buf_mtx.unlock();
                if (n < kFirstMessageSize) {
                    continue;
                }

                send_buf_mtx.lock();
                for (int i = 12; i < kFirstMessageSize; ++i) {
                    session_id.push_back((char) send_buf[i]);
                }
                for (int i = 0; i < kFirstMessageSize; ++i) {
                    send_buf.pop_front();
                }
                send_buf_mtx.unlock();
                break;
            }

            NOTICE_LOG(COMMON, "session_id:%s", session_id.c_str());

            // send session_id to server
            if (session_id.size()) {
                pkt.clear();
                pkt.set_type(proto::MessageType::HelloServer);
                pkt.mutable_hello_server_data().mutable_session_id().set(session_id.c_str(), session_id.size());
                auto w = PacketWriter(buf, sizeof(buf));
                pkt.serialize(w);

                for (int i = 0; i < 10; ++i) {
                    udp_client.Send((const char *) buf, w.get_size());
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    u8 buf2[1024];
                    Packet pkt2;
                    int n = udp_client.Recv((char *) buf2, sizeof(buf2));
                    if (0 < n) {
                        auto r = PacketReader(buf2, n);
                        if (pkt2.deserialize(r) == ::EmbeddedProto::Error::NO_ERRORS) {
                            if (pkt2.hello_server_data().get_ok()) {
                                NOTICE_LOG(COMMON, "session validation OK");
                                break;
                            } else {
                                WARN_LOG(COMMON, "session validation NG");
                            }
                        }
                    }
                }
            }
        }

        send_buf_mtx.lock();
        int n = send_buf.size();
        if (n == 0) {
            send_buf_mtx.unlock();
        } else {
            n = std::min<int>(n, sizeof(buf));
            for (int i = 0; i < n; ++i) {
                buf[i] = send_buf.front();
                send_buf.pop_front();
            }
            send_buf_mtx.unlock();

            if (tcp_client.IsConnected()) {
                int m = tcp_client.Send((char *) buf, n);
                if (m < n) {
                    send_buf_mtx.lock();
                    for (int i = n - 1; m <= i; --i) {
                        send_buf.push_front(buf[i]);
                    }
                    send_buf_mtx.unlock();
                }
            } else if (udp_client.IsConnected()) {
                if (message_buf.CanPush()) {
                    message_buf.PushBattleMessage(session_id, buf, n);
                    pkt.clear();
                    pkt.set_type(proto::MessageType::Battle);
                    message_buf.FillSendData(pkt);
                    auto w = PacketWriter(buf, sizeof(buf));
                    auto err = pkt.serialize(w);
                    if (err == ::EmbeddedProto::Error::NO_ERRORS) {
                        NOTICE_LOG(COMMON, "packet serialized: %d", w.get_size());
                        udp_client.Send((const char *) buf, w.get_size());
                    } else {
                        ERROR_LOG(COMMON, "packet serialize error %d", err);
                    }
                } else {
                    send_buf_mtx.lock();
                    for (int i = n - 1; 0 <= i; --i) {
                        send_buf.push_front(buf[i]);
                    }
                    send_buf_mtx.unlock();
                }
            }
            updated = true;
        }

        if (tcp_client.IsConnected()) {
            n = tcp_client.ReadableSize();
            if (0 < n) {
                n = std::min<int>(n, sizeof(buf));
                n = tcp_client.Recv((char *) buf, n);
                if (0 < n) {
                    recv_buf_mtx.lock();
                    for (int i = 0; i < n; ++i) {
                        recv_buf.push_back(buf[i]);
                    }
                    recv_buf_mtx.unlock();
                    updated = true;
                }
            }
        } else if (udp_client.IsConnected()) {
            n = udp_client.ReadableSize();
            if (0 < n) {
                n = std::min<int>(n, sizeof(buf));
                n = udp_client.Recv((char *) buf, n);
                if (0 < n) {
                    auto r = PacketReader(buf, n);
                    pkt.clear();
                    if (pkt.deserialize(r) == ::EmbeddedProto::Error::NO_ERRORS) {
                        switch (pkt.get_type()) {
                            case proto::None:
                                break;
                            case proto::HelloServer:
                                break;
                            case proto::Ping:
                                break;
                            case proto::Pong:
                                break;
                            case proto::Battle:
                                message_buf.ApplySeqAck(pkt.get_seq(), pkt.get_ack());
                                recv_buf_mtx.lock();
                                const auto &msgs = pkt.get_battle_data();
                                for (int i = 0; i < msgs.get_length(); ++i) {
                                    if (message_filter.Filter(msgs.get_const(i))) {
                                        const auto &body = msgs.get_const(i).body();
                                        for (int j = 0; j < body.get_length(); ++j) {
                                            recv_buf.push_back(body.get_const(j));
                                        }
                                    }
                                }
                                recv_buf_mtx.unlock();
                                break;
                        }
                    } else {
                        // TODO
                    }
                    updated = true;
                }
            }
        }
    }
}

std::string Gdxsv::GenerateLoginKey() {
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

void Gdxsv::WritePatch() {
    if (disk == 1) WritePatchDisk1();
    if (disk == 2) WritePatchDisk2();
    if (symbols["patch_id"] == 0 || ReadMem32_nommu(symbols["patch_id"]) != symbols[":patch_id"]) {
        NOTICE_LOG(COMMON, "patch %d %d", ReadMem32_nommu(symbols["patch_id"]), symbols[":patch_id"]);

#include "gdxsv_patch.h"
    }
}

void Gdxsv::WritePatchDisk1() {
    const u32 offset = 0x8C000000 + 0x00010000;

    // Reduce max lag-frame
    WriteMem8_nommu(offset + 0x00047f60, maxlag);
    WriteMem8_nommu(offset + 0x00047f66, maxlag);

    // Modem connection fix
    const char *atm1 = "ATM1\r                                ";
    for (int i = 0; i < strlen(atm1); ++i) {
        WriteMem8_nommu(offset + 0x0015e703 + i, u8(atm1[i]));
    }

    // Overwrite serve address (max 20 chars)
    for (int i = 0; i < 20; ++i) {
        WriteMem8_nommu(offset + 0x0015e788 + i, (i < server.length()) ? u8(server[i]) : u8(0));
    }

    // Skip form validation
    WriteMem16_nommu(offset + 0x0003b0c4, u16(9)); // nop
    WriteMem16_nommu(offset + 0x0003b0cc, u16(9)); // nop
    WriteMem16_nommu(offset + 0x0003b0d4, u16(9)); // nop
    WriteMem16_nommu(offset + 0x0003b0dc, u16(9)); // nop

    // Write LoginKey
    if (ReadMem8_nommu(offset - 0x10000 + 0x002f6924) == 0) {
        for (int i = 0; i < std::min(loginkey.length(), size_t(8)) + 1; ++i) {
            WriteMem8_nommu(offset - 0x10000 + 0x002f6924 + i,
                            (i < loginkey.length()) ? u8(loginkey[i]) : u8(0));
        }
    }

}

void Gdxsv::WritePatchDisk2() {
    const u32 offset = 0x8C000000 + 0x00010000;

    // Reduce max lag-frame
    WriteMem8_nommu(offset + 0x00035348, maxlag);
    WriteMem8_nommu(offset + 0x0003534e, maxlag);

    // Modem connection fix
    const char *atm1 = "ATM1\r                                ";
    for (int i = 0; i < strlen(atm1); ++i) {
        WriteMem8_nommu(offset + 0x001be7c7 + i, u8(atm1[i]));
    }

    // Overwrite serve address (max 20 chars)
    for (int i = 0; i < 20; ++i) {
        WriteMem8_nommu(offset + 0x001be84c + i, (i < server.length()) ? u8(server[i]) : u8(0));
    }

    // Skip form validation
    WriteMem16_nommu(offset + 0x000284f0, u16(9)); // nop
    WriteMem16_nommu(offset + 0x000284f8, u16(9)); // nop
    WriteMem16_nommu(offset + 0x00028500, u16(9)); // nop
    WriteMem16_nommu(offset + 0x00028508, u16(9)); // nop

    // Write LoginKey
    if (ReadMem8_nommu(offset - 0x10000 + 0x00392064) == 0) {
        for (int i = 0; i < std::min(loginkey.length(), size_t(8)) + 1; ++i) {
            WriteMem8_nommu(offset - 0x10000 + 0x00392064 + i,
                            (i < loginkey.length()) ? u8(loginkey[i]) : u8(0));
        }
    }
}

Gdxsv gdxsv;
