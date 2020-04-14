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
#pragma once
#include "types.h"
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include "net_platform.h"

class NaomiNetwork
{
public:
	~NaomiNetwork() { terminate(); }
	bool init();
	bool startNetwork();
	void pipeSlaves();
	bool receive(u8 *data, u32 size);
	void send(u8 *data, u32 size);
	void shutdown();	// thread-safe
	void terminate();	// thread-safe
	int slotCount() const { return slot_count; }
	int slotId() const { return slot_id; }
	u16 packetNumber() const { return packet_number; }
	bool hasToken() const { return got_token; }

private:
	bool createServerSocket();
	bool createBeaconSocket();
	void processBeacon();
	bool findServer();
	int createAndBind(int protocol);
	bool isMaster() const { return slot_id == 0; }

	struct in_addr server_ip{ INADDR_NONE };
	std::string server_name;
	// server stuff
	sock_t server_sock = INVALID_SOCKET;
	sock_t beacon_sock = INVALID_SOCKET;
	std::vector<sock_t> slaves;
	// client stuff
	sock_t client_sock = INVALID_SOCKET;
	// common stuff
	int slot_count = 0;
	int slot_id = 0;
	bool got_token = false;
	u16 packet_number = 0;
	std::atomic<bool> network_stopping{ false };
	std::mutex mutex;

	static const uint16_t SERVER_PORT = 37391;
};
