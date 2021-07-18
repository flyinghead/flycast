// Network implementation for gdxsv lobby server

#pragma once

#include <atomic>
#include <mutex>
#include <utility>
#include "gdx_queue.h"
#include "gdxsv_network.h"
#include "lbs_message.h"

class GdxsvBackendTcp {
public:
    GdxsvBackendTcp(const std::map<std::string, u32> &symbols) : symbols_(symbols) {
    }

    void Reset() {
        tcp_client_.Close();
        lbs_msg_reader_.Clear();
        callback_lbs_packet_ = nullptr;
    }

    bool Connect(const std::string &host, u16 port) {
        bool ok = tcp_client_.Connect(host.c_str(), port);
        if (!ok) {
            WARN_LOG(COMMON, "Failed to connect with TCP %s:%d", host.c_str(), port);
            return false;
        }

        tcp_client_.SetNonBlocking();
        return true;
    }

    bool IsConnected() const {
        return tcp_client_.IsConnected();
    }

    void Close() {
        tcp_client_.Close();
    }

    int Send(const std::vector<u8> &packet) {
        return tcp_client_.Send((const char *) packet.data(), packet.size());
    }

    const std::string &LocalIP() {
        return tcp_client_.local_ip();
    }

    void OnGameWrite() {
        gdx_queue q{};
        u32 gdx_txq_addr = symbols_.at("gdx_txq");
        if (gdx_txq_addr == 0) return;
        q.head = ReadMem16_nommu(gdx_txq_addr);
        q.tail = ReadMem16_nommu(gdx_txq_addr + 2);
        u32 buf_addr = gdx_txq_addr + 4;

        int n = gdx_queue_size(&q);
        if (0 < n) {
            u8 buf[GDX_QUEUE_SIZE] = {};
            for (int i = 0; i < n; ++i) {
                buf[i] = ReadMem8_nommu(buf_addr + q.head);
                gdx_queue_pop(&q); // dummy pop
            }
            WriteMem16_nommu(gdx_txq_addr, q.head);

            int m = tcp_client_.Send((char *) buf, n);
            if (n != m) {
                WARN_LOG(COMMON, "TcpSend failed");
                tcp_client_.Close();
            }
        }
    }

    void OnGameRead() {
        u8 buf[GDX_QUEUE_SIZE];
        u32 gdx_rxq_addr = symbols_.at("gdx_rxq");
        gdx_queue q{};
        q.head = ReadMem16_nommu(gdx_rxq_addr);
        q.tail = ReadMem16_nommu(gdx_rxq_addr + 2);
        u32 buf_addr = gdx_rxq_addr + 4;

        int n = tcp_client_.ReadableSize();
        if (n <= 0) {
            return;
        }
        n = std::min<int>(n, static_cast<int>(gdx_queue_avail(&q)));
        n = tcp_client_.Recv((char *) buf, n);

        if (0 < n) {
            for (int i = 0; i < n; ++i) {
                WriteMem8_nommu(buf_addr + q.tail, buf[i]);
                gdx_queue_push(&q, 0); // dummy push
            }
            WriteMem16_nommu(gdx_rxq_addr + 2, q.tail);

            if (callback_lbs_packet_) {
                lbs_msg_reader_.Write((char *) buf, n);
                while (lbs_msg_reader_.Read(lbs_msg_)) {
                    callback_lbs_packet_(lbs_msg_);
                }
            }
        }
    }

    void callback_lbs_packet(std::function<void(const LbsMessage &)> callback) {
        callback_lbs_packet_ = std::move(callback);
    }

private:
    const std::map<std::string, u32> &symbols_;
    TcpClient tcp_client_;
    LbsMessage lbs_msg_;
    LbsMessageReader lbs_msg_reader_;
    std::function<void(const LbsMessage &)> callback_lbs_packet_;
};
