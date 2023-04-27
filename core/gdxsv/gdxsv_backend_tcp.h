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
	void callback_lbs_packet(std::function<void(const LbsMessage &)> callback);

   private:
	TcpClient tcp_client_;
	LbsMessage lbs_msg_;
	LbsMessageReader lbs_msg_reader_;
	std::function<void(const LbsMessage &)> callback_lbs_packet_;
};