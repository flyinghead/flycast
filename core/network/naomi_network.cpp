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
#include "naomi_network.h"
#include "hw/naomi/naomi_flashrom.h"
#include "cfg/option.h"
#include "rend/gui.h"

#include <chrono>
#include <thread>

NaomiNetwork naomiNetwork;

bool NaomiNetwork::init()
{
	if (!config::NetworkEnable)
		return false;
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

	return true;
}

void NaomiNetwork::createSocket()
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
}

bool NaomiNetwork::startNetwork()
{
	if (!init())
		return false;

	slotId = 0;
	slotCount = 0;
	slaves.clear();

	using namespace std::chrono;

	if (config::ActAsServer)
	{
		enableNetworkBroadcast(true);
		const auto timeout = seconds(20);
		NOTICE_LOG(NETWORK, "Waiting for slave connections");
		steady_clock::time_point start_time = steady_clock::now();

		while (steady_clock::now() - start_time < timeout)
		{
			if (networkStopping)
				return false;

			std::string notif = slaves.empty() ? "Waiting for players..."
					: std::to_string(slaves.size()) + " player(s) connected. Waiting...";
			gui_display_notification(notif.c_str(), timeout.count() * 2000);

			poll();

			if (slaves.size() == 3 || (_startNow && !slaves.empty()))
				break;
			std::this_thread::sleep_for(milliseconds(20));
		}
		enableNetworkBroadcast(false);
		if (!slaves.empty())
		{
			NOTICE_LOG(NETWORK, "Master starting: %zd slaves", slaves.size());
			_startNow = true;
			slotCount = slaves.size() + 1;
			Packet packet(Start);
			packet.start.nodeCount = slotCount;
			for (auto& slave : slaves)
				send(&slave.addr, &packet, packet.size());

			nextPeer = slaves[0].addr;

			gui_display_notification("Starting game", 2000);
			SetNaomiNetworkConfig(0);

			return true;
		}
		gui_display_notification("No player connected", 8000);
	}
	else
	{
		const auto timeout = seconds(30);
		serverIp = INADDR_BROADCAST;
		u16 serverPort = SERVER_PORT;
		if (!config::NetworkServer.get().empty())
		{
			auto pos = config::NetworkServer.get().find_last_of(':');
			std::string server;
			if (pos != std::string::npos)
			{
				serverPort = atoi(config::NetworkServer.get().substr(pos + 1).c_str());
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
						serverIp = ((sockaddr_in *)ptr->ai_addr)->sin_addr.s_addr;
						break;
					}
				freeaddrinfo(resultAddr);
			}
		}

		NOTICE_LOG(NETWORK, "Connecting to server");
		gui_display_notification("Connecting to server", 10000);
		steady_clock::time_point start_time = steady_clock::now();

		while (!networkStopping && !_startNow && steady_clock::now() - start_time < timeout)
		{
			if (slotId == 0)
			{
				Packet packet(SyncReq);
				sockaddr_in serverAddr{};
				serverAddr.sin_family = AF_INET;
				serverAddr.sin_port = htons(serverPort);
				serverAddr.sin_addr.s_addr = serverIp;
				send(&serverAddr, &packet, packet.size());
			}
			std::this_thread::sleep_for(milliseconds(10));
			poll();
		}
		if (!networkStopping && _startNow)
		{
			SetNaomiNetworkConfig(slotId);
			return true;
		}
	}
	return false;
}

bool NaomiNetwork::receive(const sockaddr_in *addr, const Packet *packet, u32 size)
{
	DEBUG_LOG(NETWORK, "Received port %d pckt %d size %x", ntohs(addr->sin_port), packet->type, size - (u32)packet->size(0));
	switch (packet->type)
	{
	case SyncReq:
		if (config::ActAsServer && !_startNow)
		{
			Slave *slave = nullptr;
			for (auto& s : slaves)
				if (s.addr.sin_port == addr->sin_port && s.addr.sin_addr.s_addr == addr->sin_addr.s_addr)
				{
					slave = &s;
					break;
				}
			if (slave == nullptr)
			{
				slaves.push_back(Slave());
				slave = &slaves.back();
				slave->state = 0; // unused
				slave->addr = *addr;
			}
			Packet reply(SyncReply);
			reply.sync.nodeId = (u16)(slave - &slaves[0] + 1);
			if (slave - &slaves[0] + 1 < (int)slaves.size())
			{
				Slave *nextSlave = &slaves[slave - &slaves[0] + 1];
				reply.sync.nextNodeIp = nextSlave->addr.sin_addr.s_addr;
				reply.sync.nextNodePort = nextSlave->addr.sin_port;
			}
			else
			{
				//FIXME local ip?
				reply.sync.nextNodeIp = 0;
				reply.sync.nextNodePort = htons(config::LocalPort);
			}
			send(addr, &reply, reply.size());

			if (reply.sync.nodeId > 1)
			{
				// notify previous slave of nextNode change
				reply.sync.nextNodeIp = addr->sin_addr.s_addr;
				reply.sync.nextNodePort = addr->sin_port;
				reply.sync.nodeId--;
				slave = &slaves[reply.sync.nodeId - 1];
				send(&slave->addr, &reply, reply.size());
			}
		}
		break;

	case SyncReply:
		if (!config::ActAsServer && !_startNow)
		{
			serverIp = addr->sin_addr.s_addr;
			slotId = packet->sync.nodeId;
			nextPeer.sin_family = AF_INET;
			nextPeer.sin_port = packet->sync.nextNodePort;
			nextPeer.sin_addr.s_addr = packet->sync.nextNodeIp == 0 ? addr->sin_addr.s_addr : packet->sync.nextNodeIp;
			std::string notif = "Connected as slot " + std::to_string(slotId);
			gui_display_notification(notif.c_str(), 2000);
		}
		break;

	case Start:
		if (!_startNow)
		{
			slotCount = packet->start.nodeCount;
			sendAck(addr);
			_startNow = true;
		}
		break;

	case Data:
		if (!receivedData.empty())
			INFO_LOG(NETWORK, "Received packet overwritten");
		receivedData.resize(size - packet->size(0));
		memcpy(receivedData.data(), packet->data.payload, receivedData.size());
		packetNumber = packet->data.packetNumber;
		// TODO? sendAck(peer, port);
		return true;

	case Ack:
		break;

	case NAck:
		WARN_LOG(NETWORK, "NAK received");
		throw Exception("NAK received");
		break;

	default:
		WARN_LOG(NETWORK, "Unknown packet type %d", packet->type);
		throw Exception("Unknown packet type ");
		break;
	}

	return false;
}

// Sets the game network config using MIE eeprom or bbsram:
// Node -1 disables network
// Node 0 is master, nodes 1+ are slave
void SetNaomiNetworkConfig(int node)
{
	const std::string& gameId = settings.content.gameId;
	if (gameId == "ALIEN FRONT")
	{
		// no way to disable the network
		write_naomi_eeprom(0x3f, node == 0 ? 0 : 1);
	}
	else if (gameId == "MOBILE SUIT GUNDAM JAPAN" // gundmct
			|| gameId == "MOBILE SUIT GUNDAM DELUXE JAPAN") // gundmxgd
	{
		write_naomi_eeprom(0x38, node == -1 ? 2
				: node == 0 ? 0 : 1);
	}
	else if (gameId == " BIOHAZARD  GUN SURVIVOR2")
	{
		write_naomi_flash(0x21c, node == 0 ? 0 : 1);	// CPU ID - 1
		write_naomi_flash(0x22a, node == -1 ? 0 : 1);	// comm link on
	}
	else if (gameId == "HEAVY METAL JAPAN")
	{
		write_naomi_eeprom(0x31, node == -1 ? 0 : node == 0 ? 1 : 2);
	}
	else if (gameId == "OUTTRIGGER     JAPAN")
	{
		write_naomi_flash(0x21a, node == -1 ? 0 : 1);	// network on
		write_naomi_flash(0x21b, node);					// node id
	}
	else if (gameId == "SLASHOUT JAPAN VERSION")
	{
		write_naomi_eeprom(0x30, node + 1);
	}
	else if (gameId == "SPAWN JAPAN")
	{
		write_naomi_eeprom(0x44, node == -1 ? 0
				: node == 0 ? 1 : 2);
	}
	else if (gameId == "SPIKERS BATTLE JAPAN VERSION")
	{
		write_naomi_eeprom(0x30, node == -1 ? 0
				: node == 0 ? 1 : 2);
	}
	else if (gameId == "VIRTUAL-ON ORATORIO TANGRAM")
	{
		write_naomi_eeprom(0x45, node == -1 ? 3
				: node == 0 ? 0 : 1);
		write_naomi_eeprom(0x47, node == 0 ? 0 : 1);
	}
	else if (gameId == "WAVE RUNNER GP")
	{
		write_naomi_eeprom(0x33, node);
		write_naomi_eeprom(0x35, node == -1 ? 2
				: node == 0 ? 0 : 1);
	}
	else if (gameId == "WORLD KICKS")
	{
		write_naomi_flash(0x224, node == -1 ? 0 : 1);	// network on
		write_naomi_flash(0x220, node == 0 ? 0 : 1);	// node id
	}
	else if (gameId == "CLUB KART IN JAPAN")
	{
		write_naomi_eeprom(0x34, node + 1); // also 03 = satellite
	}
	else if (gameId == "INITIAL D"
			|| gameId == "INITIAL D Ver.2"
			|| gameId == "INITIAL D Ver.3")
	{
		write_naomi_eeprom(0x34, node == -1 ? 0x02 : node == 0 ? 0x12 : 0x22);
	}
	else if (gameId == "THE KING OF ROUTE66")
	{
		write_naomi_eeprom(0x3d, node == -1 ? 0x44 : node == 0 ? 0x54 : 0x64);
	}
	else if (gameId == "MAXIMUM SPEED")
	{
		configure_maxspeed_flash(node != -1, node == 0);
	}
	else if (gameId == "F355 CHALLENGE JAPAN")
	{
		// FIXME need default flash
		write_naomi_flash(0x230, node == -1 ? 0 : node == 0 ? 1 : 2);
		if (node != -1)
			// car number (0 to 7)
			write_naomi_flash(0x231, node);
		// 0x233: cabinet type (0 deluxe, 1 twin)
		write_naomi_flash(0x233, config::MultiboardSlaves >= 2 ? 0 : 1);
	}
}

bool NaomiNetworkSupported()
{
	static const char * const games[] = {
		"ALIEN FRONT", "MOBILE SUIT GUNDAM JAPAN", "MOBILE SUIT GUNDAM DELUXE JAPAN", " BIOHAZARD  GUN SURVIVOR2",
		"HEAVY METAL JAPAN", "OUTTRIGGER     JAPAN", "SLASHOUT JAPAN VERSION", "SPAWN JAPAN",
		"SPIKERS BATTLE JAPAN VERSION", "VIRTUAL-ON ORATORIO TANGRAM", "WAVE RUNNER GP", "WORLD KICKS",
		"F355 CHALLENGE JAPAN",
		// Naomi 2
		"CLUB KART IN JAPAN", "INITIAL D", "INITIAL D Ver.2", "INITIAL D Ver.3", "THE KING OF ROUTE66",
		"SEGA DRIVING SIMULATOR"
	};
	if (!config::NetworkEnable)
		return false;
	for (auto game : games)
		if (settings.content.gameId == game)
			return true;

	return false;
}
