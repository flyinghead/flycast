/*
	Created on: Sep 15, 2018

	Copyright 2018 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

bool start_pico();
void stop_pico();
void write_pico(u8 b);
int read_pico();

// #define MODEM_DEBUG

#ifdef MODEM_DEBUG
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <mutex>

class ppp_dumper
{
public:
    ppp_dumper(std::string name) : name(name)
    {
    }

    void push(u8 c)
    {
        buf.push_back(c);

        if (buf.size() <= 2 && c == 0x7e)
        {
            start_time = std::chrono::high_resolution_clock::now();
        }
        bool is_ppp_frame = 2 <= buf.size() && buf.front() == 0x7e && buf.back() == 0x7e;
        if (!is_ppp_frame)
            return;
        end_time = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        for (auto i = 0; i < buf.size(); ++i)
        {
            if (buf[i] == 0x7d)
            {
                buf[i + 1] ^= 0x20;
                buf.erase(buf.begin() + i);
            }
        }
        auto it_0x21 = std::find(buf.begin(), buf.end(), (u8)0x21);
        if (it_0x21 != buf.end() && (it_0x21 - buf.begin()) <= 6 && 40 <= buf.size())
        { // IPv4
            int ip_h = 1 + (it_0x21 - buf.begin());
            int ip_header_size = (buf[ip_h] & 0x0f) * 4;
            int ip_size = int(buf[ip_h + 2]) << 8 | buf[ip_h + 3];
            int proto_id = buf[ip_h + 9];
            std::string proto = std::to_string(proto_id);
            if (proto_id == 6)
                proto = "tcp";
            if (proto_id == 17)
                proto = "udp";

            if (proto == "tcp")
            {
                int tcp_h = ip_h + ip_header_size;
                int src_port = int(buf[tcp_h + 0]) << 8 | buf[tcp_h + 1];
                int dst_port = int(buf[tcp_h + 2]) << 8 | buf[tcp_h + 3];
                // int seq, ack;
                int tcp_header_size = (buf[tcp_h + 12] >> 4) * 4;
                const int n = (ip_h + ip_size) - (tcp_h + tcp_header_size);
                if (0 < n)
                {
                    /*
                    char ipdir[128] = {0};
                    sprintf(ipdir, "tcp:%d.%d.%d.%d:%d > %d.%d.%d.%d:%d",
                        buf[ip_h + 12], buf[ip_h + 13], buf[ip_h + 14], buf[ip_h + 15], src_port,
                        buf[ip_h + 16], buf[ip_h + 17], buf[ip_h + 18], buf[ip_h + 19], dst_port);
                    */
                    std::vector<char> data(16 + n * 2, 0);
                    for (int i = 0; i < n; ++i)
                    {
                        sprintf(&data[0] + i * 2, "%02x", int(buf[tcp_h + tcp_header_size + i]));
                    }
                    if (2 <= ms) {
                        WARN_LOG(MODEM, "[pppdumper][%s][%ldms]data:%s", name.c_str(), ms, data.begin());
                    }
                }
            }
        }
        else
        {
            std::stringstream data;
            for (int i = 0; i < buf.size() - 3; ++i)
            {
                data << std::hex << std::setfill('0') << std::setw(2) << int(buf[i]);
            }
            if (2 <= ms) {
                WARN_LOG(MODEM, "[pppdumper][%s][%dms] Unknown data:%s", name.c_str(), ms, data.str().c_str());
            }
        }
        buf.clear();
    }

private:
    std::vector<u8> buf;
    std::string name;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
};

extern ppp_dumper ppp_read_dump;
extern ppp_dumper ppp_write_dump;
extern ppp_dumper modem_read_dump;
extern ppp_dumper modem_write_dump;

class safe_timer {
public:
    safe_timer(std::string name) : name(name)
    {
    }

    void start() {
        mtx.lock();
        started = true;
        start_time = std::chrono::high_resolution_clock::now();
        mtx.unlock();
    }

    bool stop() {
        long long ms = 0;
        mtx.lock();
        if (started) {
            auto end_time = std::chrono::high_resolution_clock::now();
            ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            started = false;
        }
        mtx.unlock();
        if (5 < ms) {
            INFO_LOG(MODEM, "%s:%ldms", name.c_str(), ms);
            return true;
        }
        return false;
    }
private:
    std::string name;
    std::mutex mtx;
    bool started;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
};
extern safe_timer modem_write_pico_read_timer;
#endif
