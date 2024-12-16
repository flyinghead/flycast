/*
	Copyright 2024 flyinghead

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
#include "dreamconn.h"

#if defined(_WIN32) && !defined(TARGET_UWP)
#include "hw/maple/maple_devs.h"

void createDreamConnDevices(DreamConn& dreamconn);

struct MapleMsg
{
	u8 command;
	u8 destAP;
	u8 originAP;
	u8 size;
	u8 data[1024];

	u32 getDataSize() const {
		return size * 4;
	}

	template<typename T>
	void setData(const T& p) {
		memcpy(data, &p, sizeof(T));
		this->size = (sizeof(T) + 3) / 4;
	}

	bool send(sock_t sock) {
		u32 sz = getDataSize() + 4;
		return ::send(sock, this, sz, 0) == sz;
	}
	bool receive(sock_t sock)
	{
		if (::recv(sock, this, 4, 0) != 4)
			return false;
		if (getDataSize() == 0)
			return true;
		return ::recv(sock, data, getDataSize(), 0) == getDataSize();
	}
};
static_assert(sizeof(MapleMsg) == 1028);

void DreamConn::connect()
{
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!VALID(sock))
		return;
	set_recv_timeout(sock, 1000);
	sockaddr_in addr {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(BASE_PORT + bus);
	if (::connect(sock, (sockaddr *)&addr, sizeof(addr)) != 0)
	{
		WARN_LOG(INPUT, "DreamConn[%d] connection failed", bus);
		disconnect();
		return;
	}
	// Now get the controller configuration
	MapleMsg msg;
	msg.command = MDCF_GetCondition;
	msg.destAP = (bus << 6) | 0x20;
	msg.originAP = bus << 6;
	msg.setData(MFID_0_Input);
	if (!msg.send(sock))
	{
		WARN_LOG(INPUT, "DreamConn[%d] communication failed", bus);
		disconnect();
		return;
	}
	if (!msg.receive(sock)) {
		WARN_LOG(INPUT, "DreamConn[%d] read timeout", bus);
		disconnect();
		return;
	}
	expansionDevs = msg.originAP & 0x1f;
	NOTICE_LOG(INPUT, "Connected to DreamConn[%d]: VMU:%d, Rumble Pack:%d", bus, hasVmu(), hasRumble());

	EventManager::listen(Event::Resume, handleEvent, this);
}

void DreamConn::disconnect()
{
	EventManager::unlisten(Event::Resume, handleEvent, this);
	if (VALID(sock)) {
		NOTICE_LOG(INPUT, "Disconnected from DreamConn[%d]", bus);
		closesocket(sock);
	}
	sock = INVALID_SOCKET;
}

bool DreamConn::send(const u8* data, int size)
{
	if (VALID(sock))
		return ::send(sock, data, size, 0) == size;
	else
		return false;
}

void DreamConn::handleEvent(Event event, void *arg) {
	createDreamConnDevices(*static_cast<DreamConn*>(arg));
}

#else

void DreamConn::connect() {
}
void DreamConn::disconnect() {
}

#endif
