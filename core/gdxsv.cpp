#include <sstream>
#include <iomanip>
#include <random>

#include "gdxsv.h"
#include "hw/modem/picoppp.h"

// hardware SEGA SEGAKATANA  maker SEGA ENTERPRISES ks 3DC7  type GD-ROM num 1/2   area J        ctrl 27BB dev A vga 1 wince 0 product T13306M    version V1.000 date 20020221 boot 1ST_READ.BIN     softco SEGA LC-T-133    name MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX
// hardware SEGA SEGAKATANA  maker SEGA ENTERPRISES ks 3DC7  type GD-ROM num 2/2   area J        ctrl 27BB dev A vga 1 wince 0 product T13306M    version V1.000 date 20020221 boot 1ST_READ.BIN     softco SEGA LC-T-133    name MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX


namespace {
    static const int GDX_QUEUE_SIZE = 512;

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

    void gdx_queue_push(struct gdx_queue* q, u8 data) {
        q->buf[q->tail] = data;
        q->tail = (q->tail + 1) % GDX_QUEUE_SIZE;
    }

    u8 gdx_queue_pop(struct gdx_queue* q) {
        u8 ret = q->buf[q->head];
        q->head = (q->head + 1) % GDX_QUEUE_SIZE;
        return ret;
    }
}

namespace {
    char dump_buf[4096];
}

void dump_memory_file() {
    auto name = get_writable_data_path("gdxsv-dump.bin");
    auto fp = fopen(name.c_str(), "wb");
    fwrite(mem_b.data, sizeof(u8), mem_b.size, fp);
    fclose(fp);
}

void dump_memory(const char *prefix, u32 addr, size_t size) {
    for (int i = 0; i < size; ++i) {
        sprintf(reinterpret_cast<char *>(dump_buf + i * 2), "%02x", ReadMem8_nommu(addr + i));
    }
    dump_buf[size * 2] = 0;
    NOTICE_LOG(COMMON, "%s : %s", prefix, dump_buf);
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
    }
    if (disk_num == "2") {
        disk = 2;
        WritePatchDisk2();
    }
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

    if (disk == 1) {
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
                WriteMem8_nommu(offset - 0x10000 + 0x002f6924 + i, (i < loginkey.length()) ? u8(loginkey[i]) : u8(0));
            }
        }
    }

    if (disk == 2) {
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
                WriteMem8_nommu(offset - 0x10000 + 0x00392064 + i, (i < loginkey.length()) ? u8(loginkey[i]) : u8(0));
            }
        }

        recv_data_lock.lock();
        if (!recv_data.empty()) {
            gdx_queue q;
            q.head = ReadMem16_nommu(symbols["gdx_rxq"]);
            q.tail = ReadMem16_nommu(symbols["gdx_rxq"] + 2);
            u32 buf_addr = symbols["gdx_rxq"] + 4;

            int count = 0;
            while (gdx_queue_size(&q) + 1 < GDX_QUEUE_SIZE && !recv_data.empty()) {
                u8 data = recv_data.front();
                sprintf(dump_buf + count * 2, "%0x2d", data);
                WriteMem8_nommu(buf_addr + q.tail, data);
                q.tail = (q.tail + 1) % GDX_QUEUE_SIZE;
                recv_data.pop_front();
                count++;
            }
            dump_buf[count * 2] = 0;
            NOTICE_LOG(COMMON, "flycast queue write : %s", dump_buf);
            NOTICE_LOG(COMMON, "flycast queue size:%d head:%d tail:%d", gdx_queue_size(&q), q.head, q.tail);
            WriteMem16_nommu(symbols["gdx_rxq"] + 2, q.tail);
        }
        recv_data_lock.unlock();
    }
}

void Gdxsv::OnPPPRecv(u8 c) {
    if (!enabled) {
        return;
    }
    auto& buf = ppp_recv_buf;
    buf.push_back(c);
    bool is_ppp_frame = 2 <= buf.size() && buf.front() == 0x7e && buf.back() == 0x7e;
    if (is_ppp_frame) {
        for (auto i = 0; i < buf.size(); ++i) {
            if (buf[i] == 0x7d) {
                buf[i + 1] ^= 0x20;
                buf.erase(buf.begin() + i); // order
            }
        }
        auto it_0x21 = std::find(buf.begin(), buf.end(), (u8) 0x21);
        if (it_0x21 != buf.end() && (it_0x21 - buf.begin()) <= 6 && 40 <= buf.size()) {
            int ip_h = 1 + (it_0x21 - buf.begin());
            int ip_header_size = (buf[ip_h] & 0x0f) * 4;
            int ip_size = int(buf[ip_h + 2]) << 8 | buf[ip_h + 3];
            int proto_id = buf[ip_h + 9];
            // tcp
            if (proto_id == 6) {
                int tcp_h = ip_h + ip_header_size;
                // int src_port = int(buf[tcp_h + 0]) << 8 | buf[tcp_h + 1];
                // int dst_port = int(buf[tcp_h + 2]) << 8 | buf[tcp_h + 3];
                uint32_t seq = uint32_t(buf[tcp_h + 4]) << 24 |
                        uint32_t(buf[tcp_h + 5]) << 16 |
                        uint32_t(buf[tcp_h + 6]) << 8 |
                        uint32_t(buf[tcp_h + 7]);
                int tcp_header_size = (buf[tcp_h + 12] >> 4) * 4;
                const int n = (ip_h + ip_size) - (tcp_h + tcp_header_size);
                if (0 < n) {
                    static u8 mcs_first_data[] = {0x0e, 0x61, 0x00, 0x22, 0x10, 0x31};
                    bool is_mcs_first_data = sizeof(mcs_first_data) <= n && memcmp(mcs_first_data, &buf[tcp_h + tcp_header_size], sizeof(mcs_first_data)) == 0;
                    recv_data_lock.lock();
                    if (is_mcs_first_data) {
                        recv_data.clear();
                        WriteMem32_nommu(symbols["gdx_rxq"], 0); // BAD
                        NOTICE_LOG(COMMON, "Clear recv_data");
                        next_seq_no = seq;
                    }
                    if (seq == next_seq_no) {
                        // NOTICE_LOG(COMMON, "good seq_no :%d", seq);
                        for (int i = 0; i < n; ++i) {
                            recv_data.push_back(buf[tcp_h + tcp_header_size + i]);
                        }
                        next_seq_no = seq + n;
                        /*
                        std::vector<char> data(16 + n * 2, 0);
                        for (int i = 0; i < n; ++i) {
                            sprintf(&data[0] + i * 2, "%02x", int(buf[tcp_h + tcp_header_size + i]));
                        }
                        NOTICE_LOG(COMMON, "[gdxsv]recv data:%s", data.begin());
                         */
                    } else {
                        NOTICE_LOG(COMMON, "invalid seq_no expected:%d actual:%d", next_seq_no, seq);
                    }
                    recv_data_lock.unlock();
                }
            }
        }
    }

    if (is_ppp_frame) {
        ppp_recv_buf.clear();
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

void Gdxsv::WritePatchDisk2() {
#include "gdxsv_disk2.patch"
}

Gdxsv gdxsv;
