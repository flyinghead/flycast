/*
	Copyright 2023 flyinghead

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
#include "hw/sh4/modules/modules.h"
#include "net_platform.h"
#include "cfg/option.h"
#include "miniupnp.h"
#include "hw/sh4/sh4_sched.h"
#include "naomi_network.h"
#include "net_handshake.h"
#include <deque>

class NullModemPipe : public SerialPort::Pipe
{
public:
	class Exception : public FlycastException
	{
	public:
		Exception(const std::string& reason) : FlycastException(reason) {}
	};

	// Serial TX
	void write(u8 data) override
	{
		u8 packet[2] = { 'D', data };
		int rc = sendto(sock, (const char *)&packet[0], sizeof(packet), 0, (const sockaddr *)&peerAddress, sizeof(peerAddress));
		if (rc != sizeof(packet))
			ERROR_LOG(NETWORK, "sendto: %d errno %d", rc, get_last_error());
		DEBUG_LOG(NETWORK, "Write %02x %c (buf rx %d)", data, data, (int)rxBuffer.size());
	}

	void sendBreak() override
	{
		const char b = 'B';
		int rc = sendto(sock, &b, 1, 0, (const sockaddr *)&peerAddress, sizeof(peerAddress));
		if (rc != 1)
			ERROR_LOG(NETWORK, "sendto: %d errno %d", rc, get_last_error());
		DEBUG_LOG(NETWORK, "Send Break");
	}

	// RX buffer Size
	int available() override {
		poll();
		checkBreak();
		int realSize = 0;
		for (u32 b : rxBuffer)
			if (b != (u32)~0)
				realSize++;
		return realSize;
	}

	// Serial RX
	u8 read() override
	{
		poll();
		if (rxBuffer.empty()) {
			WARN_LOG(NETWORK, "NetPipe: empty read");
			return 0;
		}
		u8 b = rxBuffer.front();
		rxBuffer.pop_front();
		DEBUG_LOG(NETWORK, "Read %02x (buf rx %d)", b, (int)rxBuffer.size());
		checkBreak();

		return b;
	}

	~NullModemPipe()
	{
		shutdown();
	}

	bool init()
	{
#ifdef _WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
		{
			ERROR_LOG(NETWORK, "WSAStartup failed. errno=%d", get_last_error());
			throw Exception("WSAStartup failed");
		}
#endif
		if (config::EnableUPnP)
		{
			miniupnp.Init();
			miniupnp.AddPortMapping(config::LocalPort, true);
		}

		createSocket();
		SCIFSerialPort::Instance().setPipe(this);

		return true;
	}

	void shutdown()
	{
		enableNetworkBroadcast(false);
		if (VALID(sock))
			closesocket(sock);
		sock = INVALID_SOCKET;
		SCIFSerialPort::Instance().setPipe(nullptr);
	}

private:
	void checkBreak()
	{
		if (!rxBuffer.empty() && rxBuffer.front() == (u32)~0) {
			SCIFSerialPort::Instance().receiveBreak();
			rxBuffer.pop_front();
		}
	}

	void poll()
	{
		if (lastPoll == sh4_sched_now64())
			return;
		lastPoll = sh4_sched_now64();
		u8 data[0x100];
		sockaddr_in addr;
		while (true)
		{
			socklen_t len = sizeof(addr);
			int rc = recvfrom(sock, (char *)data, sizeof(data), 0, (sockaddr *)&addr, &len);
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
			if (peerAddress.sin_addr.s_addr == INADDR_BROADCAST)
			{
				if (addr.sin_port != htons(config::LocalPort) || !is_local_address(addr.sin_addr.s_addr))
				{
					peerAddress.sin_addr.s_addr = addr.sin_addr.s_addr;
					peerAddress.sin_port = addr.sin_port;
					enableNetworkBroadcast(false);
					NOTICE_LOG(NETWORK, "Data received from peer %x:%d", htonl(addr.sin_addr.s_addr), htons(addr.sin_port));
				}
				else
				{
					// this is coming from us so ignore it
					continue;
				}
			}
			if (rc == 2)
			{
				if (data[0] != 'D')
					ERROR_LOG(NETWORK, "Unexpected packet '%c'", data[0]);
				else
					rxBuffer.push_back(data[1]);
			}
			else if (rc == 1)
			{
				if (data[0] != 'B')
					ERROR_LOG(NETWORK, "Unexpected packet '%c'", data[0]);
				else
					rxBuffer.push_back(~0);
			}
		}
	}

	void createSocket()
	{
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET)
		{
			ERROR_LOG(NETWORK, "Socket creation failed: errno %d", get_last_error());
			throw Exception("Socket creation failed");
		}
		int option = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(option));

		sockaddr_in serveraddr{};
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(config::LocalPort);

		if (::bind(sock, (sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
		{
			ERROR_LOG(NETWORK, "NaomiServer: bind() failed. errno=%d", get_last_error());
			closesocket(sock);

			throw Exception("Socket bind failed");
		}
		set_non_blocking(sock);

	    // Allow broadcast packets to be sent
	    int broadcast = 1;
	    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast)) == -1)
	        WARN_LOG(NETWORK, "setsockopt(SO_BROADCAST) failed. errno=%d", get_last_error());

	    peerAddress.sin_family = AF_INET;
	    peerAddress.sin_addr.s_addr = INADDR_BROADCAST;
	    peerAddress.sin_port = htons(NaomiNetwork::SERVER_PORT);
		if (!config::NetworkServer.get().empty()
				// ignore server name if acting as server (maxspeed)
				&& (!config::ActAsServer || settings.platform.isConsole()))
		{
			auto pos = config::NetworkServer.get().find_last_of(':');
			std::string server;
			if (pos != std::string::npos)
			{
				peerAddress.sin_port = htons(atoi(config::NetworkServer.get().substr(pos + 1).c_str()));
				server = config::NetworkServer.get().substr(0, pos);
			}
			else
				server = config::NetworkServer;
			addrinfo *resultAddr;
			if (getaddrinfo(server.c_str(), 0, nullptr, &resultAddr))
				WARN_LOG(NETWORK, "Server %s is unknown", server.c_str());
			else
			{
				for (addrinfo *ptr = resultAddr; ptr != nullptr; ptr = ptr->ai_next)
					if (ptr->ai_family == AF_INET)
					{
						peerAddress.sin_addr.s_addr = ((sockaddr_in *)ptr->ai_addr)->sin_addr.s_addr;
						break;
					}
				freeaddrinfo(resultAddr);
			}
		}
		else
			enableNetworkBroadcast(true);
	}

	sock_t sock = INVALID_SOCKET;
	MiniUPnP miniupnp;
	std::deque<u32> rxBuffer;
	sockaddr_in peerAddress{};
	u64 lastPoll = 0;
};

class BattleCableHandshake : public NetworkHandshake
{
public:
	std::future<bool> start() override {
		std::promise<bool> promise;
		promise.set_value(pipe.init());
		return promise.get_future();
	}
	void stop() override {
		pipe.shutdown();
	}
	bool canStartNow() override {
		return true;
	}
	void startNow() override {}

private:
	NullModemPipe pipe;
};
