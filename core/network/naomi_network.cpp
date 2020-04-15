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
#include <chrono>
#include <thread>
#include "rend/gui.h"

sock_t NaomiNetwork::createAndBind(int protocol)
{
	sock_t sock = socket(AF_INET, protocol == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM, protocol);
	if (sock == INVALID_SOCKET)
	{
		ERROR_LOG(NETWORK, "Cannot create server socket");
		return INVALID_SOCKET;
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
		closesocket(sock);
		sock = INVALID_SOCKET;
	}
	else
		set_non_blocking(sock);

	return sock;
}

bool NaomiNetwork::init()
{
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
	{
		ERROR_LOG(NETWORK, "WSAStartup failed. errno=%d", get_last_error());
		return false;
	}
#endif
	if (settings.network.ActAsServer)
		return createBeaconSocket() && createServerSocket();
	else
		return true;
}

bool NaomiNetwork::createServerSocket()
{
	if (server_sock != INVALID_SOCKET)
		return true;

	server_sock = createAndBind(IPPROTO_TCP);
	if (server_sock == INVALID_SOCKET)
		return false;

	if (listen(server_sock, 5) < 0)
	{
		ERROR_LOG(NETWORK, "NaomiServer: listen() failed. errno=%d", get_last_error());
		closesocket(server_sock);
		server_sock = INVALID_SOCKET;
		return false;
	}
	return true;
}

bool NaomiNetwork::createBeaconSocket()
{
	if (beacon_sock == INVALID_SOCKET)
		beacon_sock = createAndBind(IPPROTO_UDP);

    return beacon_sock != INVALID_SOCKET;
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
			DEBUG_LOG(NETWORK, "NaomiServer: beacon received %ld bytes", n);
			if (n == sizeof(buf) && !strncmp(buf, "flycast", n))
				sendto(beacon_sock, buf, n, 0, (const struct sockaddr *)&addr, addrlen);
		}
	} while (n != -1);
}

bool NaomiNetwork::findServer()
{
    // Automatically find the adhoc server on the local network using broadcast
	sock_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET)
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

    // Set a 1 sec timeout on recv call
    if (!set_recv_timeout(sockfd, 1000))
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
	network_stopping = false;
	if (!init())
		return false;

	slot_id = 0;
	slot_count = 0;
	packet_number = 0;
	slaves.clear();
	got_token = false;

	using namespace std::chrono;
	const auto timeout = seconds(10);

	if (settings.network.ActAsServer)
	{
		NOTICE_LOG(NETWORK, "Waiting for slave connections");
		steady_clock::time_point start_time = steady_clock::now();

		while (steady_clock::now() - start_time < timeout)
		{
			if (network_stopping)
			{
				for (auto clientSock : slaves)
					if (clientSock != INVALID_SOCKET)
						closesocket(clientSock);
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
			if (clientSock == INVALID_SOCKET)
			{
				if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
					perror("accept");
			}
			else
			{
				NOTICE_LOG(NETWORK, "Slave connection accepted");
				std::lock_guard<std::mutex> lock(mutex);
				slaves.push_back(clientSock);
				if (slaves.size() == 3)
					break;
			}
			std::this_thread::sleep_for(milliseconds(100));
		}
		slot_id = 0;
		slot_count = slaves.size() + 1;
		u8 buf[2] = { (u8)slot_count, 0 };
		int slot_num = 1;
		{
			for (int socket : slaves)
			{
				buf[1] = { (u8)slot_num };
				slot_num++;
				::send(socket, (const char *)buf, 2, 0);
				set_non_blocking(socket);
				set_tcp_nodelay(socket);
			}
		}
		NOTICE_LOG(NETWORK, "Master starting: %zd slaves", slaves.size());
		if (slot_count > 1)
			gui_display_notification("Starting game", 2000);
		else
			gui_display_notification("No player connected", 8000);

		return !slaves.empty();
	}
	else
	{
		if (!settings.network.server.empty())
		{
			struct addrinfo *resultAddr;
			if (getaddrinfo(settings.network.server.c_str(), 0, nullptr, &resultAddr))
				WARN_LOG(NETWORK, "Server %s is unknown", settings.network.server.c_str());
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

		while (client_sock == INVALID_SOCKET && !network_stopping
				&& steady_clock::now() - start_time < timeout)
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
				closesocket(client_sock);
				client_sock = INVALID_SOCKET;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			else
			{
				gui_display_notification("Waiting for server to start", 10000);
				set_recv_timeout(client_sock, (int)std::chrono::milliseconds(timeout * 2).count());
				u8 buf[2];
				if (::recv(client_sock, (char *)buf, 2, 0) < 2)
				{
					ERROR_LOG(NETWORK, "recv failed: errno=%d", get_last_error());
					closesocket(client_sock);
					client_sock = INVALID_SOCKET;
					gui_display_notification("Server failed to start", 10000);

					return false;
				}
				slot_count = buf[0];
				slot_id = buf[1];
				got_token = slot_id == 1;
				set_tcp_nodelay(client_sock);
				set_non_blocking(client_sock);
				std::string notif = "Connected as slot " + std::to_string(slot_id);
				gui_display_notification(notif.c_str(), 2000);

				return true;
			}
		}
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
		if (*it == INVALID_SOCKET || *(it + 1) == INVALID_SOCKET)
			// TODO keep link on
			continue;
		ssize_t l = ::recv(*it, buf, sizeof(buf), 0);
		if (l <= 0)
		{
			if (get_last_error() == L_EAGAIN || get_last_error() == L_EWOULDBLOCK)
				continue;
			WARN_LOG(NETWORK, "pipeSlaves: receive failed. errno=%d", get_last_error());
			closesocket(*it);
			*it = INVALID_SOCKET;
			continue;
		}
		ssize_t l2 = ::send(*(it + 1), buf, l, 0);
		if (l2 != l)
		{
			WARN_LOG(NETWORK, "pipeSlaves: send failed. errno=%d", get_last_error());
			closesocket(*(it + 1));
			*(it + 1) = INVALID_SOCKET;
		}
	}
}

bool NaomiNetwork::receive(u8 *data, u32 size)
{
	sock_t sockfd = INVALID_SOCKET;
	if (isMaster())
		sockfd = slaves.empty() ? INVALID_SOCKET : slaves.back();
	else
		sockfd = client_sock;
	if (sockfd == INVALID_SOCKET)
		return false;

	u16 pktnum;
	ssize_t l = ::recv(sockfd, (char *)&pktnum, sizeof(pktnum), 0);
	if (l <= 0)
	{
		if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
		{
			WARN_LOG(NETWORK, "receiveNetwork: read failed. errno=%d", get_last_error());
			if (isMaster())
			{
				slaves.back() = INVALID_SOCKET;
				closesocket(sockfd);
				got_token = false;
			}
			else
				shutdown();
		}
		return false;
	}
	packet_number = pktnum;

	ssize_t received = 0;
	while (received != size && !network_stopping)
	{
		l = ::recv(sockfd, (char*)(data + received), size - received, 0);
		if (l <= 0)
		{
			if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
			{
				WARN_LOG(NETWORK, "receiveNetwork: read failed. errno=%d", get_last_error());
				if (isMaster())
				{
					slaves.back() = INVALID_SOCKET;
					closesocket(sockfd);
					got_token = false;
				}
				else
					shutdown();
				return false;
			}
		}
		else
			received += l;
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
		sockfd = slaves.empty() ? INVALID_SOCKET : slaves.front();
	else
		sockfd = client_sock;
	if (sockfd == INVALID_SOCKET)
		return;

	u16 pktnum = packet_number + 1;
	if (::send(sockfd, (const char *)&pktnum, sizeof(pktnum), 0) < sizeof(pktnum))
	{
		if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
		{
			WARN_LOG(NETWORK, "send failed. errno=%d", get_last_error());
			if (isMaster())
			{
				slaves.front() = INVALID_SOCKET;
				closesocket(sockfd);
			}
			else
				shutdown();
		}
		return;
	}
	if (::send(sockfd, (const char *)data, size, 0) < size)
	{
		WARN_LOG(NETWORK, "send failed. errno=%d", get_last_error());
		if (isMaster())
		{
			slaves.front() = INVALID_SOCKET;
			closesocket(sockfd);
		}
		else
			shutdown();
	}
	else
	{
		DEBUG_LOG(NETWORK, "[%d] Sent %d bytes", slot_id, size);
		got_token = false;
		packet_number = pktnum;
	}
}

void NaomiNetwork::shutdown()
{
	network_stopping = true;
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (auto& sock : slaves)
		{
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
	}
	if (client_sock != INVALID_SOCKET)
	{
		closesocket(client_sock);
		client_sock = INVALID_SOCKET;
	}
}

void NaomiNetwork::terminate()
{
	shutdown();
	if (beacon_sock != INVALID_SOCKET)
	{
		closesocket(beacon_sock);
		beacon_sock = INVALID_SOCKET;
	}
	if (server_sock != INVALID_SOCKET)
	{
		closesocket(server_sock);
		server_sock = INVALID_SOCKET;
	}
}
