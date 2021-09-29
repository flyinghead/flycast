/*
	Created on: Apr 12, 2020

	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "naomi_network.h"

#include "types.h"
#include <array>
#include <chrono>
#include <thread>
#include "rend/gui.h"
#include "hw/naomi/naomi_cart.h"
#include "hw/naomi/naomi_flashrom.h"
#include "cfg/option.h"
#include "emulator.h"

#ifdef _MSC_VER
#if defined(_WIN64)
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif
#endif

NaomiNetwork naomiNetwork;

sock_t NaomiNetwork::createAndBind(int protocol)
{
	sock_t sock = socket(AF_INET, protocol == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM, protocol);
	if (!VALID(sock))
	{
		ERROR_LOG(NETWORK, "Cannot create server socket");
		return sock;
	}
	int option = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(option));

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(SERVER_PORT);

	if (::bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		ERROR_LOG(NETWORK, "NaomiServer: bind() failed. errno=%d", get_last_error());
		closeSocket(sock);
	}
	else
		set_non_blocking(sock);

	return sock;
}

bool NaomiNetwork::init()
{
	if (!config::NetworkEnable)
		return false;
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
	{
		ERROR_LOG(NETWORK, "WSAStartup failed. errno=%d", get_last_error());
		return false;
	}
#endif
	if (config::ActAsServer)
	{
		miniupnp.Init();
		miniupnp.AddPortMapping(SERVER_PORT, true);
		return createBeaconSocket() && createServerSocket();
	}
	else
		return true;
}

bool NaomiNetwork::createServerSocket()
{
	if (VALID(server_sock))
		return true;

	server_sock = createAndBind(IPPROTO_TCP);
	if (!VALID(server_sock))
		return false;

	if (listen(server_sock, 5) < 0)
	{
		ERROR_LOG(NETWORK, "NaomiServer: listen() failed. errno=%d", get_last_error());
		closeSocket(server_sock);
		return false;
	}
	return true;
}

bool NaomiNetwork::createBeaconSocket()
{
	if (!VALID(beacon_sock))
		beacon_sock = createAndBind(IPPROTO_UDP);

    return VALID(beacon_sock);
}

void NaomiNetwork::processBeacon()
{
	// Receive broadcast queries on beacon socket and reply
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	memset(&addr, 0, sizeof(addr));
	char buf[6];
	ssize_t n;
	do {
		memset(buf, '\0', sizeof(buf));
		if ((n = recvfrom(beacon_sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen)) == -1)
		{
			if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
				WARN_LOG(NETWORK, "NaomiServer: Error receiving datagram. errno=%d", get_last_error());
		}
		else
		{
			DEBUG_LOG(NETWORK, "NaomiServer: beacon received %ld bytes", (long)n);
			if (n == sizeof(buf) && !strncmp(buf, "flycast", n))
				sendto(beacon_sock, buf, n, 0, (const struct sockaddr *)&addr, addrlen);
		}
	} while (n != -1);
}

bool NaomiNetwork::findServer()
{
    // Automatically find the adhoc server on the local network using broadcast
	sock_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (!VALID(sockfd))
    {
        ERROR_LOG(NETWORK, "Datagram socket creation error. errno=%d", get_last_error());
        return false;
    }

    // Allow broadcast packets to be sent
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast)) == -1)
    {
        ERROR_LOG(NETWORK, "setsockopt(SO_BROADCAST) failed. errno=%d", get_last_error());
        closesocket(sockfd);
        return false;
    }

    // Set a 500ms timeout on recv call
    if (!set_recv_timeout(sockfd, 500))
    {
        ERROR_LOG(NETWORK, "setsockopt(SO_RCVTIMEO) failed. errno=%d", get_last_error());
        closesocket(sockfd);
        return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;          // host byte order
    addr.sin_port = htons(SERVER_PORT); // short, network byte order
    addr.sin_addr.s_addr = INADDR_BROADCAST;
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

    struct sockaddr server_addr;

    for (int i = 0; i < 3; i++)
    {
        if (sendto(sockfd, "flycast", 6, 0, (struct sockaddr *)&addr, sizeof addr) == -1)
        {
            WARN_LOG(NETWORK, "Send datagram failed. errno=%d", get_last_error());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        char buf[6];
        memset(&server_addr, '\0', sizeof(server_addr));
        socklen_t addrlen = sizeof(server_addr);
        if (recvfrom(sockfd, buf, sizeof(buf), 0, &server_addr, &addrlen) == -1)
        {
            if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
                WARN_LOG(NETWORK, "Recv datagram failed. errno=%d", get_last_error());
            else
                INFO_LOG(NETWORK, "Recv datagram timeout. i=%d", i);
            continue;
        }
        server_ip = ((struct sockaddr_in *)&server_addr)->sin_addr;
        char addressBuffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &server_ip, addressBuffer, INET_ADDRSTRLEN);
        server_name = addressBuffer;
        break;
    }
    closesocket(sockfd);
    if (server_ip.s_addr == INADDR_NONE)
    {
        WARN_LOG(NETWORK, "Network Error: Can't find ad-hoc server on local network");
        gui_display_notification("No server found", 8000);
        return false;
    }
    INFO_LOG(NETWORK, "Found ad-hoc server at %s", server_name.c_str());

    return true;
}

bool NaomiNetwork::startNetwork()
{
	if (!init())
		return false;

	slot_id = 0;
	slot_count = 0;
	slaves.clear();
	got_token = false;

	using namespace std::chrono;
	const auto timeout = seconds(20);

	if (config::ActAsServer)
	{
		NOTICE_LOG(NETWORK, "Waiting for slave connections");
		steady_clock::time_point start_time = steady_clock::now();

		while (steady_clock::now() - start_time < timeout)
		{
			if (network_stopping)
			{
				for (auto& slave : slaves)
					if (VALID(slave.socket))
						closeSocket(slave.socket);
				return false;
			}
			std::string notif = slaves.empty() ? "Waiting for players..."
					: std::to_string(slaves.size()) + " player(s) connected. Waiting...";
			gui_display_notification(notif.c_str(), timeout.count() * 2000);

			processBeacon();

			struct sockaddr_in src_addr;
			socklen_t addr_len = sizeof(src_addr);
			memset(&src_addr, 0, addr_len);
			sock_t clientSock = accept(server_sock, (struct sockaddr *)&src_addr, &addr_len);
			if (!VALID(clientSock))
			{
				if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
					perror("accept");
			}
			else
			{
				NOTICE_LOG(NETWORK, "Slave connection accepted");
				set_non_blocking(clientSock);
				set_tcp_nodelay(clientSock);
				std::lock_guard<std::mutex> lock(mutex);
				slaves.emplace_back(clientSock);
			}
			const auto now = steady_clock::now();
			u32 waiting_slaves = 0;
			for (auto& slave : slaves)
			{
				if (slave.state == ClientState::Waiting)
					waiting_slaves++;
				else if (slave.state == ClientState::Connected)
				{
					char buffer[8];
					ssize_t l = ::recv(slave.socket, buffer, sizeof(buffer), 0);
					if (l < (int)sizeof(buffer) && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
					{
						// error
						INFO_LOG(NETWORK, "Slave socket recv error. errno=%d", get_last_error());
						closeSocket(slave.socket);
					}
					else if (l == -1 && now - slave.state_time > milliseconds(100))
					{
						// timeout
						INFO_LOG(NETWORK, "Slave socket Connected timeout");
						closeSocket(slave.socket);
					}
					else if (l == (int)sizeof(buffer))
					{
						if (memcmp(buffer, naomi_game_id, sizeof(buffer)))
						{
							// wrong game
							WARN_LOG(NETWORK, "Wrong game id received: %.8s", buffer);
							closeSocket(slave.socket);
						}
						else
						{
							slave.set_state(ClientState::Waiting);
							waiting_slaves++;
						}
					}
				}
			}
			{
				std::lock_guard<std::mutex> lock(mutex);
				slaves.erase(std::remove_if(slaves.begin(),
				                              slaves.end(),
				                              [](const Slave& slave){ return !VALID(slave.socket); }),
							slaves.end());
			}
			if (waiting_slaves == 3 || (start_now && !slaves.empty() && waiting_slaves == slaves.size()))
				break;
			std::this_thread::sleep_for(milliseconds(100));
		}
		slot_id = 0;
		slot_count = slaves.size() + 1;
		u8 buf[2] = { (u8)slot_count, 0 };
		int slot_num = 1;
		{
			for (auto& slave : slaves)
			{
				buf[1] = { (u8)slot_num };
				slot_num++;
				::send(slave.socket, (const char *)buf, 2, 0);
				slave.set_state(ClientState::Starting);
			}
		}
		NOTICE_LOG(NETWORK, "Master starting: %zd slaves", slaves.size());
		if (!slaves.empty())
		{
			gui_display_notification("Starting game", 2000);
			SetNaomiNetworkConfig(0);

			return true;
		}
		else
		{
			gui_display_notification("No player connected", 8000);
			return false;
		}
	}
	else
	{
		if (!config::NetworkServer.get().empty())
		{
			struct addrinfo *resultAddr;
			if (getaddrinfo(config::NetworkServer.get().c_str(), 0, nullptr, &resultAddr))
				WARN_LOG(NETWORK, "Server %s is unknown", config::NetworkServer.get().c_str());
			else
				for (struct addrinfo *ptr = resultAddr; ptr != nullptr; ptr = ptr->ai_next)
					if (ptr->ai_family == AF_INET)
					{
						server_ip = ((sockaddr_in *)ptr->ai_addr)->sin_addr;
						break;
					}
		}

		NOTICE_LOG(NETWORK, "Connecting to server");
		gui_display_notification("Connecting to server", 10000);
		steady_clock::time_point start_time = steady_clock::now();

		while (!network_stopping && steady_clock::now() - start_time < timeout)
		{
			if (server_ip.s_addr == INADDR_NONE && !findServer())
				continue;

			client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			struct sockaddr_in src_addr;
			src_addr.sin_family = AF_INET;
			src_addr.sin_addr = server_ip;
			src_addr.sin_port = htons(SERVER_PORT);
			if (::connect(client_sock, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0)
			{
				ERROR_LOG(NETWORK, "Socket connect failed");
				closeSocket(client_sock);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			else
			{
				gui_display_notification("Waiting for server to start", 10000);
				set_tcp_nodelay(client_sock);
				::send(client_sock, naomi_game_id, 8, 0);
				set_recv_timeout(client_sock, (int)std::chrono::milliseconds(timeout * 2).count());
				u8 buf[2];
				if (::recv(client_sock, (char *)buf, 2, 0) < 2)
				{
					ERROR_LOG(NETWORK, "recv failed: errno=%d", get_last_error());
					closeSocket(client_sock);
					gui_display_notification("Server failed to start", 10000);

					return false;
				}
				slot_count = buf[0];
				slot_id = buf[1];
				got_token = slot_id == 1;
				set_non_blocking(client_sock);
				std::string notif = "Connected as slot " + std::to_string(slot_id);
				gui_display_notification(notif.c_str(), 2000);
				SetNaomiNetworkConfig(slot_id);


				return true;
			}
		}
		return false;
	}
}

bool NaomiNetwork::syncNetwork()
{
	using namespace std::chrono;
	const auto timeout = seconds(10);

	if (config::ActAsServer)
	{
		steady_clock::time_point start_time = steady_clock::now();

		bool all_slaves_ready = false;
		while (steady_clock::now() - start_time < timeout && !all_slaves_ready)
		{
			all_slaves_ready = true;
			for (auto& slave : slaves)
				if (slave.state != ClientState::Ready)
				{
					char buf[4];
					ssize_t l = ::recv(slave.socket, buf, sizeof(buf), 0);
					if (l < 4 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
					{
						INFO_LOG(NETWORK, "Socket recv failed. errno=%d", get_last_error());
						closeSocket(slave.socket);
						return false;
					}
					if (l == 4)
					{
						if (memcmp(buf, "REDY", 4))
						{
							INFO_LOG(NETWORK, "Synchronization failed");
							closeSocket(slave.socket);
							return false;
						}
						slave.set_state(ClientState::Ready);
					}
					else
						all_slaves_ready = false;
				}
			if (network_stopping)
				return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		for (auto& slave : slaves)
		{
			ssize_t l = ::send(slave.socket, "GO!!", 4, 0);
			if (l < 4)
			{
				INFO_LOG(NETWORK, "Socket send failed. errno=%d", get_last_error());
				closeSocket(slave.socket);
				return false;
			}
			slave.set_state(ClientState::Online);
		}
		gui_display_notification("Network started", 5000);

		return true;
	}
	else
	{
		// Tell master we're ready
		ssize_t l = ::send(client_sock, "REDY", 4 ,0);
		if (l < 4)
		{
			WARN_LOG(NETWORK, "Socket send failed. errno=%d", get_last_error());
			closeSocket(client_sock);
			return false;
		}
		steady_clock::time_point start_time = steady_clock::now();

		while (steady_clock::now() - start_time < timeout)
		{
			// Wait for the go
			char buf[4];
			l = ::recv(client_sock, buf, sizeof(buf), 0);
			if (l < 4 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
			{
				INFO_LOG(NETWORK, "Socket recv failed. errno=%d", get_last_error());
				closeSocket(client_sock);
				return false;
			}
			else if (l == 4)
			{
				if (memcmp(buf, "GO!!", 4))
				{
					INFO_LOG(NETWORK, "Synchronization failed");
					closeSocket(client_sock);
					return false;
				}
				gui_display_notification("Network started", 5000);
				return true;
			}
			if (network_stopping)
			{
				closeSocket(client_sock);
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		INFO_LOG(NETWORK, "Socket recv timeout");
		closeSocket(client_sock);
		return false;
	}
}

void NaomiNetwork::pipeSlaves()
{
	if (!isMaster() || slot_count < 3)
		return;
	char buf[16384];
	for (auto it = slaves.begin(); it != slaves.end() - 1; it++)
	{
		if (!VALID(it->socket) || !VALID((it + 1)->socket))
			// TODO keep link on
			continue;
		ssize_t l = ::recv(it->socket, buf, sizeof(buf), 0);
		if (l <= 0)
		{
			if (get_last_error() == L_EAGAIN || get_last_error() == L_EWOULDBLOCK)
				continue;
			WARN_LOG(NETWORK, "pipeSlaves: receive failed. errno=%d", get_last_error());
			closeSocket(it->socket);
			continue;
		}
		ssize_t l2 = ::send((it + 1)->socket, buf, l, 0);
		if (l2 != l)
		{
			WARN_LOG(NETWORK, "pipeSlaves: send failed. errno=%d", get_last_error());
			closeSocket((it + 1)->socket);
		}
	}
}

bool NaomiNetwork::receive(u8 *data, u32 size)
{
	sock_t sockfd = INVALID_SOCKET;
	if (isMaster())
		sockfd = slaves.empty() ? INVALID_SOCKET : slaves.back().socket;
	else
		sockfd = client_sock;
	if (!VALID(sockfd))
		return false;

	ssize_t received = 0;
	while (received != size)
	{
		ssize_t l = ::recv(sockfd, (char*)(data + received), size - received, 0);
		if (l <= 0)
		{
			if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
			{
				WARN_LOG(NETWORK, "receiveNetwork: read failed. errno=%d", get_last_error());
				if (isMaster())
				{
					closeSocket(slaves.back().socket);
					got_token = false;
				}
				else
					shutdown();
				return false;
			}
			else if (received == 0)
				return false;
		}
		else
			received += l;
		if (network_stopping)
			return false;
	}
	DEBUG_LOG(NETWORK, "[%d] Received %d bytes", slot_id, size);
	got_token = true;
	return true;
}

void NaomiNetwork::send(u8 *data, u32 size)
{
	if (!got_token)
		return;

	sock_t sockfd;
	if (isMaster())
		sockfd = slaves.empty() ? INVALID_SOCKET : slaves.front().socket;
	else
		sockfd = client_sock;
	if (!VALID(sockfd))
		return;

	if (::send(sockfd, (const char *)data, size, 0) < size)
	{
		if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
		{
			WARN_LOG(NETWORK, "send failed. errno=%d", get_last_error());
			if (isMaster())
				closeSocket(slaves.front().socket);
			else
				shutdown();
		}
		return;
	}
	else
	{
		DEBUG_LOG(NETWORK, "[%d] Sent %d bytes", slot_id, size);
		got_token = false;
	}
}

void NaomiNetwork::shutdown()
{
	network_stopping = true;
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (auto& slave : slaves)
			closeSocket(slave.socket);
	}
	if (VALID(client_sock))
		closeSocket(client_sock);
	emu.setNetworkState(false);
}

void NaomiNetwork::terminate()
{
	shutdown();
	if (config::ActAsServer)
		miniupnp.Term();
	if (VALID(beacon_sock))
		closeSocket(beacon_sock);
	if (VALID(server_sock))
		closeSocket(server_sock);
}

std::future<bool> NaomiNetwork::startNetworkAsync()
{
	network_stopping = false;
	start_now = false;
	return std::async(std::launch::async, [this] {
		bool res = startNetwork();
		emu.setNetworkState(res);
		return res;
	});
}

// Sets the game network config using MIE eeprom or bbsram:
// Node -1 disables network
// Node 0 is master, nodes 1+ are slave
void SetNaomiNetworkConfig(int node)
{
	if (!strcmp("ALIEN FRONT", naomi_game_id))
	{
		// no way to disable the network
		write_naomi_eeprom(0x3f, node == 0 ? 0 : 1);
	}
	else if (!strcmp("MOBILE SUIT GUNDAM JAPAN", naomi_game_id) // gundmct
			|| !strcmp("MOBILE SUIT GUNDAM DELUXE JAPAN", naomi_game_id)) // gundmxgd
	{
		write_naomi_eeprom(0x38, node == -1 ? 2
				: node == 0 ? 0 : 1);
	}
	else if (!strcmp(" BIOHAZARD  GUN SURVIVOR2", naomi_game_id))
	{
		// FIXME need default flash
		write_naomi_flash(0x21c, node == 0 ? 0 : 1);	// CPU ID - 1
		write_naomi_flash(0x22a, node == -1 ? 0 : 1);	// comm link on
	}
	else if (!strcmp("HEAVY METAL JAPAN", naomi_game_id))
	{
		write_naomi_eeprom(0x31, node == -1 ? 0 : node == 0 ? 1 : 2);
	}
	else if (!strcmp("OUTTRIGGER     JAPAN", naomi_game_id))
	{
		// FIXME need default flash
		write_naomi_flash(0x21a, node == -1 ? 0 : 1);	// network on
		write_naomi_flash(0x21b, node);					// node id
	}
	else if (!strcmp("SLASHOUT JAPAN VERSION", naomi_game_id))
	{
		write_naomi_eeprom(0x30, node == -1 ? 0
				: node == 0 ? 1 : 2);
	}
	else if (!strcmp("SPAWN JAPAN", naomi_game_id))
	{
		write_naomi_eeprom(0x44, node == -1 ? 0 : 1);	// network on
		write_naomi_eeprom(0x30, node <= 0 ? 1 : 2);	// node id
	}
	else if (!strcmp("SPIKERS BATTLE JAPAN VERSION", naomi_game_id))
	{
		write_naomi_eeprom(0x30, node == -1 ? 0
				: node == 0 ? 1 : 2);
	}
	else if (!strcmp("VIRTUAL-ON ORATORIO TANGRAM", naomi_game_id))
	{
		write_naomi_eeprom(0x45, node == -1 ? 3
				: node == 0 ? 0 : 1);
		write_naomi_eeprom(0x47, node == 0 ? 0 : 1);
	}
	else if (!strcmp("WAVE RUNNER GP", naomi_game_id))
	{
		write_naomi_eeprom(0x33, node);
		write_naomi_eeprom(0x35, node == -1 ? 2
				: node == 0 ? 0 : 1);
	}
	else if (!strcmp("WORLD KICKS", naomi_game_id))
	{
		// FIXME need default flash
		write_naomi_flash(0x224, node == -1 ? 0 : 1);	// network on
		write_naomi_flash(0x220, node == 0 ? 0 : 1);	// node id
	}
}

bool NaomiNetworkSupported()
{
	static const std::array<const char *, 12> games = {
		"ALIEN FRONT", "MOBILE SUIT GUNDAM JAPAN", "MOBILE SUIT GUNDAM DELUXE JAPAN", " BIOHAZARD  GUN SURVIVOR2",
		"HEAVY METAL JAPAN", "OUTTRIGGER     JAPAN", "SLASHOUT JAPAN VERSION", "SPAWN JAPAN",
		"SPIKERS BATTLE JAPAN VERSION", "VIRTUAL-ON ORATORIO TANGRAM", "WAVE RUNNER GP", "WORLD KICKS"
	};
	if (!config::NetworkEnable)
		return false;
	for (auto game : games)
		if (!strcmp(game, naomi_game_id))
			return true;

	return false;
}
