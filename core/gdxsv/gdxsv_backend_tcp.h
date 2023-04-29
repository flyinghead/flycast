#pragma once

#include <functional>
#include <string>
#include <vector>

#include "gdxsv_network.h"
#include "lbs_message.h"

class GdxsvBackendTcp {
   public:
	void Reset();
	bool Connect(const std::string &host, u16 port);
	bool IsConnected() const;
	void Close();
	int Send(const std::vector<u8> &packet);
	const std::string &LocalIP() const;
	const std::string &RemoteHost() const;
	const int RemotePort() const;
	u32 OnSockRead(u32 addr, u32 size);
	u32 OnSockWrite(u32 addr, u32 size);
	u32 OnSockPoll();
	void lbs_packet_filter(std::function<bool(const LbsMessage &)> callback) { lbs_packet_filter_ = std::move(callback); }

   private:
	TcpClient tcp_client_;
	LbsMessage lbs_msg_;
	LbsMessageReader rx_msg_reader_;
	LbsMessageReader tx_msg_reader_;
	std::deque<u8> recv_buf_;
	std::function<bool(const LbsMessage &)> lbs_packet_filter_;
};