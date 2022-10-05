// Network implementation for gdxsv lobby server

#pragma once

#include <atomic>
#include <mutex>
#include <utility>

#include "gdx_rpc.h"
#include "gdxsv_network.h"
#include "lbs_message.h"
#include "libs.h"

class GdxsvBackendTcp {
   public:
	GdxsvBackendTcp(const std::map<std::string, u32> &symbols) : symbols_(symbols) {}

	void Reset() {
		tcp_client_.Close();
		lbs_msg_reader_.Clear();
		callback_lbs_packet_ = nullptr;
	}

	bool Connect(const std::string &host, u16 port) {
		bool ok = tcp_client_.Connect(host.c_str(), port);
		if (!ok) {
			WARN_LOG(COMMON, "Failed to connect with TCP %s:%d\n", host.c_str(), port);
			return false;
		}

		tcp_client_.SetNonBlocking();
		return true;
	}

	bool IsConnected() const { return tcp_client_.IsConnected(); }

	void Close() { tcp_client_.Close(); }

	int Send(const std::vector<u8> &packet) { return tcp_client_.Send((const char *)packet.data(), packet.size()); }

	const std::string &LocalIP() const { return tcp_client_.local_ip(); }

	const std::string &RemoteHost() const { return tcp_client_.host(); }

	const int RemotePort() const { return tcp_client_.port(); }

	u32 OnSockRead(u32 addr, u32 size) {
		u8 buf[InetBufSize];
		u32 n = std::min(tcp_client_.ReadableSize(), size);
		if (n <= 0) {
			return 0;
		}
		n = tcp_client_.Recv((char *)buf, n);
		if (0 < n) {
			for (int i = 0; i < n; ++i) {
				gdxsv_WriteMem8(addr + i, buf[i]);
			}
			if (callback_lbs_packet_) {
				lbs_msg_reader_.Write((char *)buf, n);
				while (lbs_msg_reader_.Read(lbs_msg_)) {
					callback_lbs_packet_(lbs_msg_);
				}
			}
		}
		return n;
	}

	u32 OnSockWrite(u32 addr, u32 size) {
		u8 buf[InetBufSize];
		u32 n = std::min<u32>(InetBufSize, size);
		for (int i = 0; i < n; ++i) {
			buf[i] = gdxsv_ReadMem8(addr + i);
		}
		return tcp_client_.Send((char *)buf, n);
	}

	u32 OnSockPoll() { return tcp_client_.ReadableSize(); }

	void callback_lbs_packet(std::function<void(const LbsMessage &)> callback) { callback_lbs_packet_ = std::move(callback); }

   private:
	const std::map<std::string, u32> &symbols_;
	TcpClient tcp_client_;
	LbsMessage lbs_msg_;
	LbsMessageReader lbs_msg_reader_;
	std::function<void(const LbsMessage &)> callback_lbs_packet_;
};