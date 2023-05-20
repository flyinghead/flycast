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
#include "netdimm.h"
#include "naomi.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/mem/addrspace.h"
#include "network/net_platform.h"
#include "serialize.h"
#include "naomi_roms.h"
#include "naomi_regs.h"

//#define HTTP_TRACE

const char *SERVER_NAME = "vfnet.duckdns.org";

NetDimm *NetDimm::Instance;

NetDimm::NetDimm(u32 size) : GDCartridge(size)
{
	schedId = sh4_sched_register(0, schedCallback);
	Instance = this;
	if (serverIp == 0)
	{
		hostent *hp = gethostbyname(SERVER_NAME);
		if (hp != nullptr && hp->h_length > 0) {
			memcpy(&serverIp, hp->h_addr_list[0], sizeof(serverIp));
			NOTICE_LOG(NAOMI, "%s IP is %x", SERVER_NAME, serverIp);
		}
	}
}

NetDimm::~NetDimm()
{
	sh4_sched_unregister(schedId);
	Instance = nullptr;
}

void NetDimm::Init(LoadProgress *progress, std::vector<u8> *digest)
{
	GDCartridge::Init(progress, digest);
	dimmBufferOffset = dimm_data_size - 16 * 1024 * 1024;
	finalTuned = strcmp(game->name, "vf4tuned") == 0;
}

u32 NetDimm::ReadMem(u32 address, u32 size)
{
	switch (address)
	{
	case NAOMI_DIMM_COMMAND:
		DEBUG_LOG(NAOMI, "DIMM COMMAND read -> %x", dimm_command);
		return dimm_command;
	case NAOMI_DIMM_OFFSETL:
		DEBUG_LOG(NAOMI, "DIMM OFFSETL read -> %x", dimm_offsetl);
		return dimm_offsetl;
	case NAOMI_DIMM_PARAMETERL:
		DEBUG_LOG(NAOMI, "DIMM PARAMETERL read -> %x", dimm_parameterl);
		return dimm_parameterl;
	case NAOMI_DIMM_PARAMETERH:
		DEBUG_LOG(NAOMI, "DIMM PARAMETERH read -> %x", dimm_parameterh);
		return dimm_parameterh;
	case NAOMI_DIMM_STATUS:
		{
			u32 rc =  DIMM_STATUS & ~(((SB_ISTEXT >> 3) & 1) << 8);
			static u32 lastRc;
			if (rc != lastRc)
				DEBUG_LOG(NAOMI, "DIMM STATUS read -> %x", rc);
			lastRc = rc;
			return rc;
		}
	default:
		return GDCartridge::ReadMem(address, size);
	}
}

void NetDimm::WriteMem(u32 address, u32 data, u32 size)
{
	switch (address)
	{
	case NAOMI_DIMM_COMMAND:
		dimm_command = data;
		DEBUG_LOG(NAOMI, "DIMM COMMAND Write<%d>: %x", size, data);
		return;

	case NAOMI_DIMM_OFFSETL:
		dimm_offsetl = data;
		DEBUG_LOG(NAOMI, "DIMM OFFSETL Write<%d>: %x", size, data);
		return;
	case NAOMI_DIMM_PARAMETERL:
		dimm_parameterl = data;
		DEBUG_LOG(NAOMI, "DIMM PARAMETERL Write<%d>: %x", size, data);
		return;
	case NAOMI_DIMM_PARAMETERH:
		dimm_parameterh = data;
		DEBUG_LOG(NAOMI, "DIMM PARAMETERH Write<%d>: %x", size, data);
		return;

	case NAOMI_DIMM_STATUS:
		DEBUG_LOG(NAOMI, "DIMM STATUS Write<%d>: %x", size, data);
		if (data & 0x100)
			// write 0 seems ignored
			asic_CancelInterrupt(holly_EXP_PCI);
		if ((data & 1) == 0)
			// irq to dimm
			process();
		return;

	default:
		GDCartridge::WriteMem(address, data, size);
		return;
	}
}

bool NetDimm::Write(u32 offset, u32 size, u32 data)
{
//	u8 b0 = data;
//	u8 b1 = data >> 8;
//	INFO_LOG(NAOMI, "Write<%d>%x: %x %c %c", size, offset, data, b0 == 0 ? ' ' : b0, b1 == 0 ? ' ' : b1);
	if (dimm_data != nullptr)
	{
		u32 addr = offset & (dimm_data_size - 1);
		memcpy(&dimm_data[addr], &data, std::min(size, dimm_data_size - addr));
	}
	return true;
}

int NetDimm::schedCallback(int tag, int sch_cycl, int jitter)
{
	return Instance->schedCallback();
}

int NetDimm::schedCallback()
{
	fd_set readFds {};
	fd_set writeFds {};
	fd_set exceptFds {};
	int nfds = -1;
	for (Socket& socket : sockets)
	{
		if (socket.connecting || socket.sending)
		{
			FD_SET(socket.fd, &writeFds);
			FD_SET(socket.fd, &exceptFds);
			nfds = std::max(nfds, (int)socket.fd);
		}
		if (socket.receiving)
		{
			FD_SET(socket.fd, &readFds);
			FD_SET(socket.fd, &exceptFds);
			nfds = std::max(nfds, (int)socket.fd);
		}
	}
	if (nfds > -1)
	{
		if (SB_ISTEXT & 8)	// holly_EXP_PCI
			return POLL_CYCLES;

		timeval tv {};
		int rc = select(nfds + 1, &readFds, &writeFds, &exceptFds, &tv);

		for (Socket& socket : sockets)
		{
			if (socket.connecting)
			{
				if (rc > 0)
				{
					int so_error;
					socklen_t len = sizeof(so_error);

					getsockopt(socket.fd, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len);
					if (so_error != L_EINPROGRESS && so_error != L_EWOULDBLOCK)
					{
						INFO_LOG(NAOMI, "connect(%d) completed -> %d", socket.fd, so_error);
						socket.connecting = false;
						socket.lastError = so_error;
						returnToNaomi(so_error != 0, &socket - &sockets[0] + 1, so_error);
						break;
					}
				}
				if (socket.connectTimeout > 0 && socket.connectTime + socket.connectTimeout <= sh4_sched_now64())
				{
					WARN_LOG(NAOMI, "connect(%d) timeout", socket.fd);
					socket.connecting = false;
					socket.lastError = ECONNREFUSED;
					returnToNaomi(true, &socket - &sockets[0] + 1, ECONNREFUSED);	// error code?
					break;
				}
			}
			else if (socket.receiving)
			{
				if (rc > 0)
				{
					rc = recv(socket.fd, (char *)socket.recvData, socket.recvLen, 0);
					if (rc == -1)
					{
						int error = get_last_error();
						if (error != L_EAGAIN && error != L_EWOULDBLOCK)
						{
							socket.lastError = get_last_error();
							socket.receiving = false;
						}
					}
#ifdef HTTP_TRACE
					else if (rc > 0)
					{
						fwrite(socket.recvData, 1, rc, stdout);
						fflush(stdout);
					}
#endif
					INFO_LOG(NAOMI, "recv(%d, %d) -> %d", (int)(&socket - &sockets[0] + 1), socket.recvLen, rc);
					if (rc >= 0)
						socket.receiving = false;
					if (!socket.receiving)
					{
						returnToNaomi(rc == -1, &socket - &sockets[0] + 1, rc);
						break;
					}
				}
				if (socket.recvTimeout > 0 && socket.recvTime + socket.recvTimeout <= sh4_sched_now64())
				{
					WARN_LOG(NAOMI, "recv(%d) timeout", socket.fd);
					socket.receiving = false;
					socket.lastError = ETIMEDOUT;
					returnToNaomi(true, &socket - &sockets[0] + 1, ETIMEDOUT);	// error code?
					break;
				}
			}
			else if (socket.sending)
			{
				if (rc > 0)
				{
					rc = send(socket.fd, (const char *)socket.sendData, socket.sendLen, 0);
					if (rc == -1)
					{
						int error = get_last_error();
						if (error != L_EAGAIN && error != L_EWOULDBLOCK)
						{
							socket.lastError = get_last_error();
							socket.sending = false;
						}
					}
#ifdef HTTP_TRACE
					else if (rc > 0)
					{
						fwrite(socket.sendData, 1, rc, stdout);
						fflush(stdout);
					}
#endif
					INFO_LOG(NAOMI, "send(%d, %d) -> %d", (int)(&socket - &sockets[0] + 1), socket.sendLen, rc);
					if (rc >= 0)
						socket.sending = false;
					if (!socket.sending)
					{
						returnToNaomi(rc == -1, &socket - &sockets[0] + 1, rc);
						break;
					}
				}
				if (socket.sendTimeout > 0 && socket.sendTime + socket.sendTimeout <= sh4_sched_now64())
				{
					WARN_LOG(NAOMI, "send(%d) timeout", socket.fd);
					socket.sending = false;
					socket.lastError = ETIMEDOUT;
					returnToNaomi(true, &socket - &sockets[0] + 1, ETIMEDOUT);	// error code?
					break;
				}
			}
		}
		return isBusy() ? POLL_CYCLES : SH4_MAIN_CLOCK;
	}
	if (SB_ISTEXT & 8)	// holly_EXP_PCI
		return SH4_MAIN_CLOCK;
//	if (dnsInProgress)
//	{
//		NOTICE_LOG(NAOMI, "getIpByDns completed");
//		returnToNaomi(false, 0, serverIp);
//		dnsInProgress = false;
//		return SH4_MAIN_CLOCK;
//	}

	// regularly peek the test request address
	peek<u32>(0xc01fc08);
	asic_RaiseInterrupt(holly_EXP_PCI);

	u32 testRequest = addrspace::read32(0xc01fc08);
	if (testRequest & 1)
	{
		// bios dimm test
		addrspace::write32(0xc01fc08, testRequest & ~1);
		bool isMem;
		char *p = (char *)addrspace::writeConst(0xc01fd00, isMem, 4);
		strcpy(p, "CHECKING DIMM BD");
		p = (char *)addrspace::writeConst(0xc01fd10, isMem, 4);
		strcpy(p, "DIMM0 - GOOD");
		p = (char *)addrspace::writeConst(0xc01fd20, isMem, 4);
		strcpy(p, "DIMM1 - GOOD");
		p = (char *)addrspace::writeConst(0xc01fd30, isMem, 4);
		strcpy(p, "--- COMPLETED---");
		addrspace::write32(0xc01fc0c, 0x0317a264);
	}
	else if (testRequest & 0x40)
	{
		// when entering vf4 test mode
		addrspace::write32(0xc01fc08, testRequest & ~0x40);
		addrspace::write32(0xc01fc60, htonl(0xc0a80101));	// FIXME ip address (192.168.1.1)
		addrspace::write32(0xc01fc0c, 0x03170264);
		INFO_LOG(NAOMI, "TEST REQUEST %x", testRequest);
	}
	else if (testRequest & 0x400)
	{
		// when entering vf4 test mode
		addrspace::write32(0xc01fc08, testRequest & ~0x400);
		addrspace::write32(0xc01fc70, 0x08080808);	// FIXME dns2??? we might be off by one; and this would be dns1?
		addrspace::write32(0xc01fc0c, 0x03170264);
		INFO_LOG(NAOMI, "TEST REQUEST %x", testRequest);
	}
	else if (testRequest & 0x10000)
	{
		// bios network settings
		addrspace::write32(0xc01fc08, testRequest & ~0x10000);
		// TODO save to PIC?
		addrspace::write32(0xc01fc0c, 0x03170264);
	}
	else if (testRequest & 0x20000)
	{
		// network test
		addrspace::write32(0xc01fc08, testRequest & ~0x20000);
		bool isMem;
		char *p = (char *)addrspace::writeConst(0xc01fd00, isMem, 4);
		strcpy(p, "CHECKING NETWORK");
		p = (char *)addrspace::writeConst(0xc01fd10, isMem, 4);
		strcpy(p, "PRETENDING... :P");
		p = (char *)addrspace::writeConst(0xc01fd20, isMem, 4);
		strcpy(p, "--- COMPLETED---");
		addrspace::write32(0xc01fc0c, 0x03170264);
	}
	else if (testRequest != 0)
	{
		addrspace::write32(0xc01fc08, 0);
		addrspace::write32(0xc01fc0c, 0x03170100);
		INFO_LOG(NAOMI, "TEST REQUEST %x", testRequest);
	}

	return SH4_MAIN_CLOCK;
}

void NetDimm::returnToNaomi(bool failed, u16 offsetl, u32 parameter)
{
	dimm_command = ((dimm_command & 0x7e00) + 0x400) | (failed ? 0xff : 0x4);
	dimm_offsetl = offsetl;
	dimm_parameterh = parameter >> 16;
	dimm_parameterl = parameter;
	verify(((SB_ISTEXT >> 3) & 1) == 0);
	asic_RaiseInterrupt(holly_EXP_PCI);
}

void NetDimm::systemCmd(int cmd)
{
	switch (cmd)
	{
	case 0xf:	// startup
		NOTICE_LOG(NAOMI, "NetDIMM startup");
		// bit 16,17: dimm0 size (none, 128, 256, 512)
		// bit 18,19: dimm1 size
		// bit 28: network enabled (network settings appear in bios menu)
		// bit 29: set
		// bit 30: gd-rom connected
		// bit 31: mobile/ppp network?
		// (| 30, 70, F0, 1F0, 3F0, 7F0)
		// | offset >> 20 (dimm buffers offset @ size - 16MB)
		// offset = (64MB << 0-5) - 16MB
		// vf4 forces this value to 0f000000 (256MB) if != 1f000000 (512MB)
		if (dimm_data_size == 512 * 1024 * 1024)
			addrspace::write32(0xc01fc04, (3 << 16) | 0x70000000 | (dimmBufferOffset >> 20));	// dimm board config 1 x 512 MB
		else if (dimm_data_size == 256 * 1024 * 1024)
			addrspace::write32(0xc01fc04, (2 << 16) | 0x70000000 | (dimmBufferOffset >> 20));	// dimm board config 1 x 256 MB
		else
			die("Unsupported dimm mem size");
		addrspace::write32(0xc01fc0c, 0x3170000 | 0x264);		// fw version | 100/264/364?
		addrspace::write32(0xc01fc10, 0);
		// additional pokes (initPoke_maybe)
		addrspace::write32(0xc01fc14, 1);
		// new in 3.17
		addrspace::write32(0xc01fc20, 0x78000);
		addrspace::write32(0xc01fc24, 0x3e000a);
		addrspace::write32(0xc01fc28, 0x18077f);
		addrspace::write32(0xc01fc2c, 0x10014);
		// PIC16?
		//addrspace::write32(0xc01fc40, .);
		// ...
		//addrspace::write32(0xc01fc54, .);
		addrspace::write32(0xc01fc18, 0x10002);	// net mode (2 or 4 is mobile), bit 16 dhcp?
		// network order
		addrspace::write32(0xc01fc60, htonl(0xc0a80101));	// ip address (192.168.1.1)
		addrspace::write32(0xc01fc64, htonl(0xffffff00));	// netmask
		addrspace::write32(0xc01fc68, htonl(0xc0a801fe));	// gateway 192.168.1.254
		addrspace::write32(0xc01fc6c, htonl(0xc0a801fe));	// dns1
		addrspace::write32(0xc01fc70, htonl(0x08080808));	// dns2
		addrspace::write32(0xc01fc74, 0);	// ?
		addrspace::write32(0xc01fc78, 0);	// ?
		addrspace::write32(0xc01fc7c, 0);	// ppp ip addr
		addrspace::write32(0xc01fc80, 0);	// ppp param
		addrspace::write32(0xc01fc84, 0);	// ?
		addrspace::write32(0xc01fc88, 0);	// ?
		addrspace::write32(0xc01fc8c, 0);	// ?
		addrspace::write32(0xc01fc90, 0);	// ?
		addrspace::write32(0xc01fc94, 0);	// ?
		// SET_BASE_ADDRESS(0c000000, 0)
		dimm_command = 0x8600;
		dimm_offsetl = 0;
		dimm_parameterl = 0;
		dimm_parameterh = 0x0c00;
		asic_RaiseInterrupt(holly_EXP_PCI);
		sh4_sched_request(schedId, SH4_MAIN_CLOCK);

		break;

	case 0:		// nop
	case 1:		// control read
	case 3:		// set base address
	case 4:		// peek8
	case 5:		// peek16
	case 6:		// peek32
	case 8:		// poke8
	case 9:		// poke16
	case 10:	// poke32
		// These are callbacks from naomi
		INFO_LOG(NAOMI, "System callback command %x", cmd);
		break;

	default:
		WARN_LOG(NAOMI, "Unknown system command %x", cmd);
		break;
	}
}

void NetDimm::netCmd(int cmd)
{
	u32 *buffer = (u32 *)&dimm_data[dimmBufferOffset + 0x800000 + 0x1000 * (dimm_command & 0xff)];
	cmd = buffer[0];
	switch (cmd)
	{
	case 0: // returnToNaomiRawCmd
		WARN_LOG(NAOMI, "netdimm: returnToNaomiRawCmd not implemented");
		returnToNaomi(true, 0, -1);
		break;
	case 1: // accept
		WARN_LOG(NAOMI, "netdimm: accept not implemented");
		returnToNaomi(true, buffer[1], -1);
		break;
	case 2: // bind
		WARN_LOG(NAOMI, "netdimm: bind not implemented");
		returnToNaomi(true, buffer[1], -1);
		break;
	case 3: // close
		{
			const int sockidx = buffer[1];
			const sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET)
			{
				WARN_LOG(NAOMI, "closesocket(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				rc = sockets[sockidx - 1].close();
				INFO_LOG(NAOMI, "closesocket(%d) %d -> %d", sockidx, sockfd, rc);
			}
			returnToNaomi(rc != 0, sockidx, rc);
			break;
		}
	case 4: // connect
		{
			const int sockidx = buffer[1];
			const sockaddr_in *addr = (const sockaddr_in *)&dimm_data[buffer[2]];
			sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET)
			{
				WARN_LOG(NAOMI, "connect(%d, %x) invalid socket", sockidx, htonl(addr->sin_addr.s_addr));
				rc = -1;
			}
			else
			{
				//socklen_t len = (socklen_t)buffer[3];
				sockaddr_in a {};
				a.sin_family = AF_INET;	// for some reason the family is in network order too. Just ignore it.
				a.sin_port = addr->sin_port;
				a.sin_addr.s_addr = addr->sin_addr.s_addr;
				rc = connect(sockfd, (sockaddr *)&a, sizeof(a));
				if (rc == -1)
				{
					int error = get_last_error();
					if (error == L_EINPROGRESS || error == L_EWOULDBLOCK)
					{
						sockets[sockidx - 1].connecting = true;
						sockets[sockidx - 1].connectTime = sh4_sched_now64();
						sh4_sched_request(schedId, POLL_CYCLES);
						INFO_LOG(NAOMI, "connect(%d, %x:%d) delayed", sockidx, htonl(a.sin_addr.s_addr), htons(a.sin_port));
						return;
					}
					else
					{
						sockets[sockidx - 1].lastError = error;
					}
				}
				else
				{
					if (finalTuned)
						set_non_blocking(sockfd);
				}
				INFO_LOG(NAOMI, "connect(%d, %x:%d) -> %d", sockidx, htonl(a.sin_addr.s_addr), htons(a.sin_port), rc);
			}
			returnToNaomi(rc != 0, sockidx, rc);
			break;
		}
	case 5: // getIpByDns
		{
			char *name = (char *)&dimm_data[buffer[1]];
			INFO_LOG(NAOMI, "getIpByDns %s", name);
			//dnsInProgress = true;
			//sh4_sched_request(schedId, POLL_CYCLES * 10);

			//int len = buffer[2];
			//u32 dns1 = buffer[3];
			//u32 dns2 = buffer[4];
			returnToNaomi(false, 0, serverIp);
			break;
		}
	case 6: // inet_addr
		WARN_LOG(NAOMI, "netdimm: inet_addr not implemented");
		returnToNaomi(true, 0, -1);
		break;
	case 7: // ioctl
		WARN_LOG(NAOMI, "netdimm: ioctl not implemented");
		returnToNaomi(true, buffer[1], -1);
		break;
	case 8: // listen
		WARN_LOG(NAOMI, "netdimm: listen not implemented");
		returnToNaomi(true, buffer[1], -1);
		break;
	case 9: // recv
		{
			const int sockidx = buffer[1];
			sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET)
			{
				WARN_LOG(NAOMI, "recv(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				u32 len = buffer[3];
				u32 offset = buffer[2] & (dimm_data_size - 1);
				u8 *data = &dimm_data[offset];
				rc = recv(sockfd, (char *)data, len, 0);
				if (rc == -1)
				{
					int error = get_last_error();
					if (error == L_EAGAIN || error == L_EWOULDBLOCK)
					{
						sockets[sockidx - 1].receiving = true;
						sockets[sockidx - 1].recvTime = sh4_sched_now64();
						sockets[sockidx - 1].recvData = data;
						sockets[sockidx - 1].recvLen = len;
						sh4_sched_request(schedId, POLL_CYCLES);
						INFO_LOG(NAOMI, "recv(%d, %d) delayed", sockidx, len);
						return;
					}
					else
					{
						sockets[sockidx - 1].lastError = get_last_error();
					}
				}
#ifdef HTTP_TRACE
				else if (rc > 0)
				{
					fwrite(data, 1, rc, stdout);
					fflush(stdout);
				}
#endif
				INFO_LOG(NAOMI, "recv(%d, %d) -> %d", sockidx, len, rc);
			}
			returnToNaomi(rc == -1, sockidx, rc);
			break;
		}
	case 10: // send
		{
			const int sockidx = buffer[1];
			sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET)
			{
				INFO_LOG(NAOMI, "send(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				u32 len = buffer[3];
				u32 offset = buffer[2] & (dimm_data_size - 1);
				u8 *data = &dimm_data[offset];
				rc = send(sockfd, (const char *)data, len, 0);
				if (rc == -1)
				{
					int error = get_last_error();
					if (error == L_EAGAIN || error == L_EWOULDBLOCK)
					{
						sockets[sockidx - 1].sending = true;
						sockets[sockidx - 1].sendTime = sh4_sched_now64();
						sockets[sockidx - 1].sendData = data;
						sockets[sockidx - 1].sendLen = len;
						sh4_sched_request(schedId, POLL_CYCLES);
						INFO_LOG(NAOMI, "send(%d, %d) delayed", sockidx, len);
						return;
					}
					else
					{
						sockets[sockidx - 1].lastError = get_last_error();
					}
				}
				INFO_LOG(NAOMI, "send(%d, %d) -> %d", sockidx, len, rc);
#ifdef HTTP_TRACE
				fwrite(data, 1, len, stdout);
				fflush(stdout);
#endif
			}
			returnToNaomi(rc == -1, sockidx, rc);
			break;
		}
	case 11: // openSocket
		{
			const u32 domain = buffer[1];
			const u32 type = buffer[2];
			const u32 protocol = buffer[3];
			const sock_t fd = socket(domain, type, protocol);
			int sockidx = -1;
			if (fd != INVALID_SOCKET)
			{
				// FIXME async mode still not right with FT
				if (!finalTuned)
					set_non_blocking(fd);
				size_t i = 0;
				for (; i < sockets.size(); i++)
					if (sockets[i].fd == INVALID_SOCKET)
						break;
				if (i == sockets.size())
					sockets.emplace_back(fd);
				else
					sockets[i].fd = fd;
				sockidx = i + 1;
			}
			INFO_LOG(NAOMI, "openSocket(%d, %d, %d) %d -> %d", domain, type, protocol, fd, sockidx);
			returnToNaomi(sockidx == -1, 0, sockidx);
			break;
		}
	case 12: // netSelect
		{
			const u32 readFds = buffer[2];
			const u32 writeFds = buffer[3];
			const u32 exceptFds = buffer[4];
			const u32 timeoutAddr = buffer[4];
			int nfds = -1;
			fd_set read {};
			fd_set write {};
			fd_set except {};
			timeval timeout;

			const auto& setFdsets = [this, buffer, &nfds](u32 fdsOffset, fd_set *fdset)
			{
				if (fdsOffset == 0)
					return;
				fd_set fds;
				memcpy(&fds, &dimm_data[fdsOffset & (dimm_data_size - 1)], std::min<size_t>(sizeof(fds), 32));
				const int count = buffer[1];
				for (int i = 0; i < count; i++)
				{
					if (FD_ISSET(i, &fds))
					{
						sock_t sockfd = getSocket(i);
						if (sockfd != INVALID_SOCKET) {
							FD_SET(sockfd, fdset);
							nfds = std::max(nfds, (int)sockfd);
						}
					}
				}
			};
			setFdsets(readFds, &read);
			setFdsets(writeFds, &write);
			setFdsets(exceptFds, &except);

			if (timeoutAddr != 0)
			{
				timeout.tv_sec = *(u32 *)&dimm_data[timeoutAddr & (dimm_data_size - 1)];
				timeout.tv_usec = *(u32 *)&dimm_data[(timeoutAddr + 4) & (dimm_data_size - 1)];
			}
			int rc = select(nfds + 1, &read, &write, &except, timeoutAddr == 0 ? nullptr : &timeout);
			INFO_LOG(NAOMI, "select(%d, %x, %x, %x, %x) -> %d", nfds, readFds, writeFds, exceptFds, timeoutAddr, rc);
			returnToNaomi(rc == -1, 0, rc);
			break;
		}
	case 13: // shutdown (not implemented on real hardware)
		WARN_LOG(NAOMI, "netdimm: shutdown not implemented");
		returnToNaomi(true, buffer[1], -3);
		break;
	case 14: // setsockopt
		WARN_LOG(NAOMI, "netdimm: setsockopt not implemented");
		returnToNaomi(true, buffer[1], -1);
		break;
	case 15: // getsockopt
		WARN_LOG(NAOMI, "netdimm: getsockopt not implemented");
		returnToNaomi(true, buffer[1], -1);
		break;
	case 16: // settimeout
		{
			const int sockidx = buffer[1];
			sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET)
			{
				WARN_LOG(NAOMI, "settimeout(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				sockets[sockidx - 1].connectTimeout = (u64)buffer[2] * SH4_MAIN_CLOCK / 1000;
				sockets[sockidx - 1].sendTimeout = (u64)buffer[3] * SH4_MAIN_CLOCK / 1000;
				sockets[sockidx - 1].recvTimeout = (u64)buffer[4] * SH4_MAIN_CLOCK / 1000;
				INFO_LOG(NAOMI, "setTimeout(%d, %d, %d, %d)", sockidx, buffer[2], buffer[3], buffer[4]);
				rc = 0;
			}
			returnToNaomi(rc != 0, sockidx, 0);
			break;
		}
	case 17: // geterrno
		{
			int sockidx = buffer[1];
			sock_t sockfd = getSocket(sockidx);
			if (sockfd != INVALID_SOCKET)
			{
				int rc = sockets[sockidx - 1].lastError;
				INFO_LOG(NAOMI, "geterrno(%d) -> %d", sockidx, rc);
				returnToNaomi(false, sockidx, rc);
			}
			else
			{
				returnToNaomi(true, sockidx, -1);
			}
			break;
		}
	case 18: // routeAdd
		WARN_LOG(NAOMI, "netdimm: routeAdd not implemented");
		returnToNaomi(true, 0, -1);
		break;
	case 19: // routeDelete
		WARN_LOG(NAOMI, "netdimm: routeDelete not implemented");
		returnToNaomi(true, 0, -1);
		break;

	case 20: // getParambyDHCP
		WARN_LOG(NAOMI, "netdimm: getParambyDHCP not implemented");
		returnToNaomi(true, 0, -1);
		break;
	case 21: // modifyMyIPaddr
		WARN_LOG(NAOMI, "netdimm: modifyMyIPaddr not implemented");
		returnToNaomi(true, 0, -1);
		break;
	case 22: // recvfrom
		WARN_LOG(NAOMI, "netdimm: recvfrom not implemented");
		returnToNaomi(true, buffer[1], -1);
		break;
	case 23: // sendto
		WARN_LOG(NAOMI, "netdimm: sendto not implemented");
		returnToNaomi(true, buffer[1], -1);
		break;

	default:
		WARN_LOG(NAOMI, "netdimm: Invalid Net command: %d", cmd);
		returnToNaomi(true, 0, 0);
		break;
	}
}

void NetDimm::process()
{
	INFO_LOG(NAOMI, "NetDIMM cmd %04x sock %d offset %04x paramh/l %04x %04x", (dimm_command >> 9) & 0x3f,
			dimm_command & 0xff, dimm_offsetl, dimm_parameterh, dimm_parameterl);

	int cmdGroup = (dimm_command >> 13) & 3;
	int cmd = (dimm_command >> 9) & 0xf;
	switch (cmdGroup)
	{
	case 0:	// system commands
		systemCmd(cmd);
		break;
	case 1: // network client
		netCmd(cmd);
		break;
	default:
		WARN_LOG(NAOMI, "Unknown DIMM command group %d cmd %x\n", cmdGroup, cmd);
		returnToNaomi(true, 0, -1);
		break;
	}
}

void NetDimm::Serialize(Serializer &ser) const
{
	GDCartridge::Serialize(ser);
	ser << dimm_command;
	ser << dimm_offsetl;
	ser << dimm_parameterl;
	ser << dimm_parameterh;
	sh4_sched_serialize(ser, schedId);
}

void NetDimm::Deserialize(Deserializer &deser)
{
	GDCartridge::Deserialize(deser);
	for (Socket& socket : sockets)
		socket.close();
	if (deser.version() >= Deserializer::V36)
	{
		deser >> dimm_command;
		deser >> dimm_offsetl;
		deser >> dimm_parameterl;
		deser >> dimm_parameterh;
		sh4_sched_deserialize(deser, schedId);
	}
}
