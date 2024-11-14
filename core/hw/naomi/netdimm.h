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
#include "gdcartridge.h"
#include "network/net_platform.h"

class NetDimm : public GDCartridge
{
public:
	NetDimm(u32 size);

	void Init(LoadProgress *progress = nullptr, std::vector<u8> *digest = nullptr) override;

	bool Write(u32 offset, u32 size, u32 data) override;

	void Deserialize(Deserializer &deser) override;

protected:
	void process() override;
	int schedCallback() override;

private:
	void systemCmd(int cmd);
	void netCmd(int cmd);

	template<typename T>
	void poke(u32 address, T value)
	{
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4);
		int size;
		switch (sizeof(T))
		{
		case 1:
			size = 8;
			break;
		case 2:
			size = 9;
			break;
		case 4:
			size = 10;
			break;
		}
		dimm_command = ((address >> 16) & 0x1ff) | (size << 9) | 0x8000;
		dimm_offsetl = address & 0xffff;
		dimm_parameterl = value & 0xffff;
		dimm_parameterh = value >> 16;
	}

	sock_t getSocket(int idx)
	{
		if (idx < 1 || idx > (int)sockets.size())
			return INVALID_SOCKET;
		return sockets[idx - 1].fd;
	}

	bool isBusy() const
	{
		for (const Socket& sock : sockets)
			if (sock.isBusy())
				return true;
		return false;
	}

	struct Socket {
		Socket() = default;
		Socket(sock_t fd) : fd(fd) {}

		int close()
		{
			int rc = 0;
			if (fd != INVALID_SOCKET)
				rc = ::closesocket(fd);
			fd = INVALID_SOCKET;
			connecting = false;
			receiving = false;
			sending = false;
			connectTimeout = 0;
			connectTime = 0;
			sendTimeout = 0;
			sendTime = 0;
			recvTimeout = 0;
			recvTime = 0;
			return rc;
		}

		bool isClosed() const {
			return fd == INVALID_SOCKET;
		}

		bool isBusy() const {
			return connecting || receiving || sending;
		}

		sock_t fd = INVALID_SOCKET;
		bool connecting = false;
		bool receiving = false;
		u8 *recvData = nullptr;
		u32 recvLen = 0;
		bool sending = false;
		const u8 *sendData = nullptr;
		u32 sendLen = 0;
		u64 connectTimeout = 0;
		u64 connectTime = 0;
		u64 sendTimeout = 0;
		u64 sendTime = 0;
		u64 recvTimeout = 0;
		u64 recvTime = 0;
		int lastError = 0;
	};
	std::vector<Socket> sockets;
	bool dnsInProgress = false;
	u32 serverIp = 0; //0x0100007f for testing only
	bool finalTuned = false;

	u32 dimmBufferOffset = 0x0f000000;

	static constexpr int POLL_CYCLES = SH4_MAIN_CLOCK / 60;
};
