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
    if (disk == 2) {
        // WriteMem8_nommu(0x0c024cae, 9);
        // WriteMem8_nommu(0x0c024caf, 9);
        // WriteMem8_nommu(0x0c024cb0, 9);
        // WriteMem8_nommu(0x0c024cb1, 9);
        // WriteMem8_nommu(0x0c024cb2, 9);
        // WriteMem8_nommu(0x0c024cb3, 9);
        // WriteMem8_nommu(0x0c024cbc, 9);
        // WriteMem8_nommu(0x0c024cbd, 0);
        dump_memory("HOGE", 0x8c024cae + 0x00010000, 8);
    }

    NOTICE_LOG(COMMON, "gdxsv disk:%d server:%s loginkey:%s maxlag:%d", disk, server.c_str(), loginkey.c_str(), maxlag);
}

void Gdxsv::Update() {
    if (!enabled) {
        return;
    }

    int f1 = ReadMem8_nommu(symbols["w:initialized"]) == 0;
    if (f1) {
        NOTICE_LOG(COMMON, "Rewrite patch %d", f1);
        if (disk == 2) {
            WritePatchDisk2();
        }
    }

    if (ReadMem32_nommu(symbols["r:print_buf_pos"])) {
        int n = ReadMem32_nommu(symbols["r:print_buf_pos"]);
        n = std::min(n, (int) sizeof(dump_buf));
        for (int i = 0; i < n; i++) {
            dump_buf[i] = ReadMem8_nommu(symbols["r:print_buf"] + i);
        }
        dump_buf[n] = 0;
        WriteMem32_nommu(symbols["r:print_buf_pos"], 0);
        NOTICE_LOG(COMMON, "%s", dump_buf);
    }

    /*
    dump_memory("r:initialized", symbols["r:initialized"], 4);
    dump_memory("w:initialized", symbols["w:initialized"], 4);
    dump_memory("w:gdx_main", symbols["w:gdx_main"], 64);
    dump_memory("r:gdx_main", symbols["r:gdx_main"], 64);
     */

    const u32 offset1 = 0x00000000 + 0x00010000;
    const u32 offset2 = 0x80000000 + 0x00010000;

    /*
    int cnt = 0;
    for (long long i = 0x8C4f0000; i < 0x8C4f0000 + 1000000; i++) {
        if (ReadMem8_nommu(i) == 0) {
            cnt++;
        } else {
            cnt = 0;
        }
        if (8192 <= cnt) {
            NOTICE_LOG(COMMON, "EMPTY AREA %08x-%08x", i-cnt+1, i);
            break;
        }
    }
    */
    if (disk == 2) {
        /*
        dump_memory("0x8c024cae-1",0x8c024cae + offset1, 8);
        dump_memory("0x8c024cae-2",0x8c024cae + offset2, 8);

        iriteMem8_nommu(offset2 + 0x0c024cae, 0x09);
        WriteMem8_nommu(offset2 + 0x0c024caf, 0x00);
        WriteMem8_nommu(offset2 + 0x0c024cb0, 0x09);
        WriteMem8_nommu(offset2 + 0x0c024cb1, 0x00);
        WriteMem8_nommu(offset2 + 0x0c024cb2, 0x09);
        WriteMem8_nommu(offset2 + 0x0c024cb3, 0x00);
         */
        /*
        WriteMem8_nommu(0x0c024cae, 9);
        WriteMem8_nommu(0x0c024caf, 9);
        WriteMem8_nommu(0x0c024cb0, 9);
        WriteMem8_nommu(0x0c024cb1, 9);
        WriteMem8_nommu(0x0c024cb2, 9);
        WriteMem8_nommu(0x0c024cb3, 9);

        WriteMem8_nommu(0x0c024cbc, 9);
        WriteMem8_nommu(0x0c024cbd, 0);
         */
    }
    // dump_memory("0x0c01d7a8", 0x0c01d7a8, 4);
    // dump_memory("0x0c036678-0", 0x0c036678, 4);
    // dump_memory("0x0c036678-1", 0x0c036678 + offset1, 4);
    // dump_memory("0x0c036678-2", 0x0c036678 + offset2, 4);

#ifdef MODEM_DEBUG_NO
    ppp_read_dump.last_data_mtx.lock();
    const auto label_0x0C3AB512 = 0x0C3AB512 - 0x0C000000;
    const auto label_0x0C3AB93C = 0x0C3AB93C - 0x0C000000;
    const auto label_0x0C394524 = 0x0C394524 - 0x0C000000;
    const auto label_0x0C394524_1472 = 0x0C394524 + 0x1472 - 0x0C000000;
    const auto label_0x0C3AB984 = 0x0C3AB984 - 0x0C000000; // InetBuf
    // const auto label_0x0C3AB938 = 0x0C3AB938 - 0x0C000000;
    // const auto label_0x0C3AB538 = 0x0C3AB538 - 0x0C000000;

    if (0 < ppp_read_dump.last_data_len) {
        /*
        memcpy(gdxsv_memory, mem_b.data, std::min((unsigned int)sizeof(gdxsv_memory), mem_b.size));
        unsigned char* pos = gdxsv_memory;
        for (int i = 1;; i++) {
            pos = std::search(pos, gdxsv_memory + sizeof(gdxsv_memory),
                              ppp_read_dump.last_data, ppp_read_dump.last_data + ppp_read_dump.last_data_len);
            if (pos == gdxsv_memory + sizeof(gdxsv_memory)) {
                break;
            }
            NOTICE_LOG(COMMON, "found (%d): addr %08x index %08x", i, pos, pos - (uint64_t) gdxsv_memory);
            pos++;
        }

        static char data[128] = {0};
        for (int i = 0; i < 127; ++i) {
            sprintf(reinterpret_cast<char *>(&data[0] + i * 2), "%02x", int(gdxsv_memory[label_0x0C394524_1472 + i]));
        }
        NOTICE_LOG(COMMON, "label_0x0C394524_1472 %s", data);
         */

        static char data[0x200 * 2] = {0};
        /*
        for (int i = 0; i < 4; ++i) {
            sprintf(reinterpret_cast<char *>(&data[0] + i * 2), "%02x", int(mem_b.data[label_0x0C3AB538 + i]));
        }
        NOTICE_LOG(COMMON, "readbuf_size:%s", data);
         */

        dump_memory("netbuf", &mem_b.data[label_0x0C3AB984], 0x200);

        for (int i = 0; i < 0x200; ++i) {
            sprintf(reinterpret_cast<char *>(&data[0] + i * 2), "%02x", int(virt_ram_base[0x0C3AB538 + i]));
        }
        NOTICE_LOG(COMMON, "recvbuf:%s", data);

        int bufsize = ReadMem32_nommu(0x0c2fa9ac);
        NOTICE_LOG(COMMON, "bufsize:%d", bufsize);
        for (int i = 0; i < std::min(bufsize, 0x200); ++i) {
            sprintf(reinterpret_cast<char *>(&data[0] + i * 2), "%02x", int(virt_ram_base[0x0c2fa9b0 + i]));
        }
        NOTICE_LOG(COMMON, "buf    :%s", data);

        ppp_read_dump.last_data_len = 0;
    }
    ppp_read_dump.last_data_mtx.unlock();
#endif

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
            q.head = ReadMem16_nommu(symbols["r:gdx_rxq"]);
            q.tail = ReadMem16_nommu(symbols["r:gdx_rxq"] + 2);
            u32 buf_addr = symbols["r:gdx_rxq"] + 4;

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
            WriteMem16_nommu(symbols["r:gdx_rxq"] + 2, q.tail);
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
                        WriteMem32_nommu(symbols["r:gdx_rxq"], 0); // BAD
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
