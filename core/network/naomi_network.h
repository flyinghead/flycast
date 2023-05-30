/*
	Copyright 2022 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "types.h"
#include "net_platform.h"
#include "miniupnp.h"
#include "cfg/option.h"
#include "emulator.h"

#include <algorithm>
#include <atomic>
#include <future>
#include <vector>

class NaomiNetwork
{
public:
	class Exception : public FlycastException
	{
	public:
		Exception(const std::string& reason) : FlycastException(reason) {}
	};

	~NaomiNetwork() { shutdown(); }

	std::future<bool> startNetworkAsync()
	{
		networkStopping = false;
		_startNow = false;
		return std::async(std::launch::async, [this] {
			bool res = startNetwork();
			emu.setNetworkState(res);
			return res;
		});
	}

	void shutdown()
	{
		enableNetworkBroadcast(false);
		emu.setNetworkState(false);
		if (sock != INVALID_SOCKET)
		{
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
	}

	bool receive(u8 *data, u32 size, u16 *packetNumber)
	{
		poll();
		if (receivedData.empty())
			return false;

		size = std::min(size, (u32)receivedData.size());
		memcpy(data, receivedData.data(), size);
		receivedData.erase(receivedData.begin(), receivedData.begin() + size);
		*packetNumber = this->packetNumber;

		return true;
	}

	void send(u8 *data, u32 size, u16 packetNumber)
	{
		verify(size < sizeof(Packet::data.payload));
		Packet packet(Data);
		memcpy(packet.data.payload, data, size);
		packet.data.packetNumber = packetNumber;
		send(&nextPeer, &packet, packet.size(size));
	}

	int getSlotCount() const { return slotCount; }
	int getSlotId() const { return slotId; }
	void startNow() {
		if (config::ActAsServer)
			_startNow = true;
	}

private:
	enum PacketType : u16 {
		SyncReq,
		SyncReply,
		Start,
		Data,
		Ack,
		NAck
	};

	#pragma pack(push, 1)
	struct Packet
	{
		Packet(PacketType type = SyncReq) : type(type) {}

		PacketType type;
		union {
			struct {
				u16 nodeId;
				u16 nextNodePort;
				u32 nextNodeIp;
			} sync;
			struct {
				u16 nodeCount;
			} start;
			struct {
				u16 packetNumber;
				u8 payload[0x4000];
			} data;
		};

		size_t size(size_t dataSize = 0) const
		{
			size_t sz = sizeof(type);
			switch (type) {
			case SyncReq:
			case SyncReply:
				sz += sizeof(sync);
				break;
			case Start:
				sz += sizeof(start);
				break;
			case Data:
				sz += sizeof(data.packetNumber) + dataSize;
				break;
			default:
				break;
			}
			return sz;
		}
	};
	#pragma pack(pop)

	bool init();

	void createSocket();

	bool startNetwork();

	void poll()
	{
		Packet packet;
		sockaddr_in addr;
		while (true)
		{
			socklen_t len = sizeof(addr);
			int rc = recvfrom(sock, (char *)&packet, sizeof(packet), 0, (sockaddr *)&addr, &len);
			if (rc == -1)
			{
				int error = get_last_error();
				if (error == L_EWOULDBLOCK || error == L_EAGAIN)
					break;
#ifdef _WIN32
				if (error == WSAECONNRESET)
					// Happens if the previous send resulted in an ICMP Port Unreachable message
					break;
#endif
				throw Exception("Receive error: errno " + std::to_string(error));
			}
			if (rc < (int)packet.size(0))
				throw Exception("Receive error: truncated packet");
			receive(&addr, &packet, rc);
		}
	}

	bool receive(const sockaddr_in *addr, const Packet *packet, u32 size);

	void sendAck(const sockaddr_in *addr, bool ack = true)
	{
		Packet packet(ack ? Ack : NAck);
		send(addr, &packet, packet.size());
	}

	void send(const sockaddr_in *addr, const Packet *packet, u32 size)
	{
		int rc = sendto(sock, (const char *)packet, size, 0,
				(sockaddr *)addr, sizeof(*addr));
		if (rc != (int)size)
			throw Exception("Send failed: errno " + std::to_string(get_last_error()));
		DEBUG_LOG(NETWORK, "Sent port %d pckt %d size %x", ntohs(addr->sin_port), packet->type, size - (u32)packet->size(0));
	}

	sock_t sock = INVALID_SOCKET;
	int slotCount = 0;
	int slotId = 0;
	std::atomic<bool> networkStopping{ false };
	MiniUPnP miniupnp;

	sockaddr_in nextPeer;
	std::vector<u8> receivedData;
	u16 packetNumber = 0;
	bool _startNow = false;

	// Server stuff
	struct Slave
	{
		int state;
		sockaddr_in addr;
	};
	std::vector<Slave> slaves;

	// Client stuff
	u32 serverIp;

public:
	static constexpr u16 SERVER_PORT = 37391;
};
extern NaomiNetwork naomiNetwork;

void SetNaomiNetworkConfig(int node);
bool NaomiNetworkSupported();
