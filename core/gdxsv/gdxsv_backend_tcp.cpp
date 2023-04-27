#include "gdxsv_backend_tcp.h"

#include "gdx_rpc.h"
#include "libs.h"

void GdxsvBackendTcp::Reset() {
	tcp_client_.Close();
	lbs_msg_reader_.Clear();
	callback_lbs_packet_ = nullptr;
}

bool GdxsvBackendTcp::Connect(const std::string &host, u16 port) {
	bool ok = tcp_client_.Connect(host.c_str(), port);
	if (!ok) {
		WARN_LOG(COMMON, "Failed to connect with TCP %s:%d\n", host.c_str(), port);
		return false;
	}

	tcp_client_.SetNonBlocking();
	return true;
}

bool GdxsvBackendTcp::IsConnected() const { return tcp_client_.IsConnected(); }

void GdxsvBackendTcp::Close() { tcp_client_.Close(); }

int GdxsvBackendTcp::Send(const std::vector<u8> &packet) { return tcp_client_.Send((const char *)packet.data(), packet.size()); }

const std::string &GdxsvBackendTcp::LocalIP() const { return tcp_client_.local_ip(); }

const std::string &GdxsvBackendTcp::RemoteHost() const { return tcp_client_.host(); }

const int GdxsvBackendTcp::RemotePort() const { return tcp_client_.port(); }

u32 GdxsvBackendTcp::OnSockRead(u32 addr, u32 size) {
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

u32 GdxsvBackendTcp::OnSockWrite(u32 addr, u32 size) {
	u8 buf[InetBufSize];
	u32 n = std::min<u32>(InetBufSize, size);
	for (int i = 0; i < n; ++i) {
		buf[i] = gdxsv_ReadMem8(addr + i);
	}
	return tcp_client_.Send((char *)buf, n);
}

u32 GdxsvBackendTcp::OnSockPoll() { return tcp_client_.ReadableSize(); }

void GdxsvBackendTcp::callback_lbs_packet(std::function<void(const LbsMessage &)> callback) { callback_lbs_packet_ = std::move(callback); }
