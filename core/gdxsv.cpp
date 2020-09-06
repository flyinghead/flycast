#include <sstream>
#include <iomanip>
#include <random>

#include "gdxsv.h"
#include "network/net_platform.h"

namespace {
    enum {
        RPC_TCP_OPEN = 1,
        RPC_TCP_CLOSE = 2,
    };

    struct gdx_rpc_t {
        u32 request;
        u32 response;

        u32 param1;
        u32 param2;
        u32 param3;
        u32 param4;
        u8 name1[128];
        u8 name2[128];
    };

    static const int GDX_QUEUE_SIZE = 1024;

    struct gdx_queue {
        u16 head;
        u16 tail;
        u8 buf[GDX_QUEUE_SIZE];
    };

    u32 gdx_queue_init(struct gdx_queue *q) {
        q->head = 0;
        q->tail = 0;
    }

    u32 gdx_queue_size(struct gdx_queue *q) {
        return (q->tail + GDX_QUEUE_SIZE - q->head) % GDX_QUEUE_SIZE;
    }

    u32 gdx_queue_avail(struct gdx_queue *q) {
        return GDX_QUEUE_SIZE - gdx_queue_size(q) - 1;
    }

    void gdx_queue_push(struct gdx_queue *q, u8 data) {
        q->buf[q->tail] = data;
        q->tail = (q->tail + 1) % GDX_QUEUE_SIZE;
    }

    u8 gdx_queue_pop(struct gdx_queue *q) {
        u8 ret = q->buf[q->head];
        q->head = (q->head + 1) % GDX_QUEUE_SIZE;
        return ret;
    }

    char dump_buf[4096];

    void dump_memory_file() {
        auto name = get_writable_data_path("gdxsv-dump.bin");
        auto fp = fopen(name.c_str(), "wb");
        fwrite(mem_b.data, sizeof(u8), mem_b.size, fp);
        fclose(fp);
    }

    class GdxTcpClient {
        sock_t sock = INVALID_SOCKET;

    public:
        bool do_connect(const char *host, int port) {
            NOTICE_LOG(COMMON, "do_connect : %s:%d", host, port);

            sock_t new_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (new_sock == INVALID_SOCKET) {
                WARN_LOG(COMMON, "do_connect fail 1 %d", get_last_error());
                return false;
            }
            auto host_entry = gethostbyname(host);
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr = *((LPIN_ADDR) host_entry->h_addr_list[0]);
            addr.sin_port = htons(port);
            set_recv_timeout(new_sock, 5000);
            if (::connect(new_sock, (const sockaddr *) &addr, sizeof(addr)) != NO_ERROR) {
                WARN_LOG(COMMON, "do_connect fail 2 %d", get_last_error());
                return false;
            }

            if (sock != INVALID_SOCKET) {
                closesocket(sock);
            }

            set_tcp_nodelay(new_sock);
            set_recv_timeout(new_sock, 1);
            sock = new_sock;
            return true;
        }

        int is_connected() {
            return sock != INVALID_SOCKET;
        }

        int do_recv(char *buf, int len) {
            int n = ::recv(sock, buf, len, 0);
            if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
                NOTICE_LOG(COMMON, "recv failed. errno=%d", get_last_error());
                do_close();
            }
            if (n < 0) return 0;
            return n;
        }

        int do_send(const char *buf, int len) {
            int n = ::send(sock, buf, len, 0);
            if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
                NOTICE_LOG(COMMON, "send failed. errno=%d", get_last_error());
                do_close();
            }
            if (n < 0) return 0;
            return n;
        }

        void do_close() {
            if (sock != INVALID_SOCKET) {
                closesocket(sock);
                sock = INVALID_SOCKET;
            }
        }

        u32 readable_size() {
            u_long n = 0;
#ifndef _WIN32
            ioctl(sock,FIONREAD,&n)
#else
            ioctlsocket(sock, FIONREAD, &n);
#endif
            return u32(n);
        }
    };

    GdxTcpClient tcp_client;
}

Gdxsv::Gdxsv() : enabled(false), disk(0), maxlag(10), net_terminate(false) {
}

Gdxsv::~Gdxsv() {
    tcp_client.do_close();
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
        settings.dreamcast.ContentPath.push_back("./");
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

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        ERROR_LOG(COMMON, "WSAStartup failed. errno=%d", get_last_error());
        return;
    }

    server = cfgLoadStr("gdxsv", "server", "zdxsv.net");
    maxlag = cfgLoadInt("gdxsv", "maxlag", 8); // Note: This should be not configurable. This is for development.
    loginkey = cfgLoadStr("gdxsv", "loginkey", "");
    bool overwriteconf = cfgLoadBool("gdxsv", "overwriteconf", true);

    if (loginkey.empty()) {
        loginkey = GenerateLoginKey();
    }

    if (overwriteconf) {
        NOTICE_LOG(COMMON, "Overwrite configs for gdxsv");

        settings.aica.BufferSize = 512;
        settings.pvr.SynchronousRender = false;
    }

    cfgSaveStr("gdxsv", "server", server.c_str());
    cfgSaveStr("gdxsv", "loginkey", loginkey.c_str());
    cfgSaveBool("gdxsv", "overwritedconf", overwriteconf);

    std::string disk_num(ip_meta.disk_num, 1);
    if (disk_num == "1") {
        disk = 1;
        WritePatchDisk1();
    }
    if (disk_num == "2") {
        disk = 2;
        WritePatchDisk2();
    }

    tcp_client.do_close();
    NOTICE_LOG(COMMON, "gdxsv disk:%d server:%s loginkey:%s maxlag:%d", disk, server.c_str(), loginkey.c_str(), maxlag);
}

void Gdxsv::Update() {
    if (!enabled) {
        return;
    }

    if (ReadMem8_nommu(symbols["initialized"]) == 0) {
        NOTICE_LOG(COMMON, "Rewrite patch");
        if (disk == 2) {
            WritePatchDisk2();
        }
    }

    if (ReadMem32_nommu(symbols["print_buf_pos"])) {
        int n = ReadMem32_nommu(symbols["print_buf_pos"]);
        n = std::min(n, (int) sizeof(dump_buf));
        for (int i = 0; i < n; i++) {
            dump_buf[i] = ReadMem8_nommu(symbols["print_buf"] + i);
        }
        dump_buf[n] = 0;
        WriteMem32_nommu(symbols["print_buf_pos"], 0);
        NOTICE_LOG(COMMON, "%s", dump_buf);
    }
}

void Gdxsv::SyncNetwork(bool write) {
    std::lock_guard<std::mutex> net_lock(net_mtx);

    if (write) {
        gdx_queue q;
        u32 gdx_txq_addr = symbols["gdx_txq"];
        if (gdx_txq_addr == 0) return;
        u32 buf_addr = gdx_txq_addr + 4;
        q.head = ReadMem16_nommu(gdx_txq_addr);
        q.tail = ReadMem16_nommu(gdx_txq_addr + 2);
        int n = gdx_queue_size(&q);
        if (0 < n) {
            for (int i = 0; i < n; ++i) {
                send_buf.push_back(ReadMem8_nommu(buf_addr + q.head));
                gdx_queue_pop(&q);
            }
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

            if (gdx_rpc.request == RPC_TCP_OPEN) {
                recv_buf.clear();
                send_buf.clear();

                u32 tolobby = gdx_rpc.param1;
                u32 host_ip = gdx_rpc.param2;
                u32 port_no = gdx_rpc.param3;

                std::string host = server;
                u16 port = port_no;

                if (tolobby != 1) {
                    char addr_buf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &host_ip, addr_buf, INET_ADDRSTRLEN);
                    host = std::string(addr_buf);
                }

                bool ok = tcp_client.do_connect(host.c_str(), port);
                if (!ok) {
                    WARN_LOG(COMMON, "Failed to connect %s:%d", host.c_str(), port);
                }
            }

            if (gdx_rpc.request == RPC_TCP_CLOSE) {
                tcp_client.do_close();
                recv_buf.clear();
                send_buf.clear();
            }

            WriteMem32_nommu(gdx_rpc_addr, 0);
            WriteMem32_nommu(gdx_rpc_addr + 4, 0);
            WriteMem32_nommu(gdx_rpc_addr + 8, 0);
            WriteMem32_nommu(gdx_rpc_addr + 12, 0);
            WriteMem32_nommu(gdx_rpc_addr + 16, 0);
            WriteMem32_nommu(gdx_rpc_addr + 20, 0);
        }

        if (recv_buf.size()) {
            gdx_queue q;
            u32 gdx_rxq_addr = symbols["gdx_rxq"];
            u32 buf_addr = gdx_rxq_addr + 4;
            q.head = ReadMem16_nommu(gdx_rxq_addr);
            q.tail = ReadMem16_nommu(gdx_rxq_addr + 2);

            u8 buf[GDX_QUEUE_SIZE];
            int n = std::min<int>(recv_buf.size(), gdx_queue_avail(&q));
            for (int i = 0; i < n; ++i) {
                WriteMem8_nommu(buf_addr + q.tail, recv_buf.front());
                recv_buf.pop_front();
                gdx_queue_push(&q, 0);
            }
            WriteMem16_nommu(gdx_rxq_addr + 2, q.tail);
        }
    }
}

u32 Gdxsv::UpdateNetwork() {
    u8 buf[GDX_QUEUE_SIZE];
    while (!net_terminate) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!tcp_client.is_connected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        net_mtx.lock();
        if (send_buf.size()) {
            int n = std::min<int>(send_buf.size(), sizeof(buf));
            for (int i = 0; i < n; ++i) {
                buf[i] = send_buf.front();
                send_buf.pop_front();
            }
            int m = tcp_client.do_send((char *) buf, n);
            if (m < n) {
                for (int i = n - 1; m <= i; --i) {
                    send_buf.push_front(buf[i]);
                }
            }
        }
        net_mtx.unlock();

        int n = tcp_client.readable_size();
        if (0 < n) {
            n = std::min(n, GDX_QUEUE_SIZE);
            n = tcp_client.do_recv((char *) buf, n);
            if (0 < n) {
                net_mtx.lock();
                for (int i = 0; i < n; ++i) {
                    recv_buf.push_back(buf[i]);
                }
                net_mtx.unlock();
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

#include "gdxsv_disk2.patch"
}

Gdxsv gdxsv;
