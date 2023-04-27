#pragma once

#include <string>

#include "gdxsv_network.h"

class GdxsvBackendUdp {
   public:
	void Reset();
	bool Connect(const std::string &host, u16 port);
	bool IsConnected() const;
	void CloseMcsRemoteWithReason(const char *reason);
	u32 OnSockRead(u32 addr, u32 size);
	u32 OnSockWrite(u32 addr, u32 size);
	u32 OnSockPoll();

   private:
	void NetThreadLoop();
	void ClearBuffers();

	bool net_terminate_ = false;
	std::string session_id_;
	UdpClient udp_client_;
	UdpRemote mcs_remote_;
	std::deque<u8> recv_buf_;
	std::deque<u8> send_buf_;
	std::mutex recv_buf_mtx_;
	std::mutex send_buf_mtx_;
};
