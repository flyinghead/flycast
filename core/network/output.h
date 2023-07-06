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
#include "emulator.h"
#include "cfg/option.h"

#include <algorithm>
#include <vector>

class NetworkOutput
{
public:
	void init()
	{
		if (!config::NetworkOutput || settings.naomi.slave || settings.naomi.drivingSimSlave == 1)
			return;
		server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		int option = 1;
		setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(option));

		sockaddr_in saddr{};
		socklen_t saddr_len = sizeof(saddr);
		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = INADDR_ANY;
		saddr.sin_port = htons(8000 + settings.naomi.drivingSimSlave);
		if (::bind(server, (sockaddr *)&saddr, saddr_len) < 0)
		{
			perror("bind");
			term();
			return;
		}
		if (listen(server, 5) < 0)
		{
			perror("listen");
			term();
			return;
		}
		set_non_blocking(server);
		EventManager::listen(Event::VBlank, vblankCallback, this);
	}

	void term()
	{
		EventManager::unlisten(Event::VBlank, vblankCallback, this);
		for (sock_t sock : clients)
			closesocket(sock);
		clients.clear();
		if (server != INVALID_SOCKET)
		{
			closesocket(server);
			server = INVALID_SOCKET;
		}
	}

	void reset()
	{
		gameNameSent = false;
	}

	void output(const char *name, u32 value)
	{
		if (!config::NetworkOutput || clients.empty())
			return;
		if (!gameNameSent)
		{
			send("game = " + settings.content.gameId + "\n");
			gameNameSent = true;
		}
		char s[9];
		sprintf(s, "%x", value);
		std::string msg = std::string(name) + " = " + std::string(s) + "\n";	// mame uses \r
		send(msg);
	}

private:
	static void vblankCallback(Event event, void *param) {
		((NetworkOutput *)param)->acceptConnections();
	}

	void acceptConnections()
	{
		sockaddr_in src_addr{};
		socklen_t addr_len = sizeof(src_addr);
		sock_t sockfd = accept(server, (sockaddr *)&src_addr, &addr_len);
		if (sockfd != INVALID_SOCKET)
		{
			set_non_blocking(sockfd);
			clients.push_back(sockfd);
		}
	}

	void send(const std::string& msg)
	{
		std::vector<sock_t> errorSockets;
		for (sock_t sock : clients)
			if (::send(sock, msg.c_str(), msg.length(), 0) < 0)
			{
				int error = get_last_error();
				if (error != L_EWOULDBLOCK && error != L_EAGAIN)
					errorSockets.push_back(sock);
			}
		for (sock_t sock : errorSockets)
		{
			closesocket(sock);
			clients.erase(std::find(clients.begin(), clients.end(), sock));
		}
	}

	sock_t server = INVALID_SOCKET;
	std::vector<sock_t> clients;
	bool gameNameSent = false;
};

extern NetworkOutput networkOutput;

