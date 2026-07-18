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
#include "oslib/oslib.h"
#include "util/shared_this.h"
#include <asio.hpp>

//#define HTTP_TRACE

const char *SERVER_NAME = "vfnet.flyca.st";

#ifndef ENOTSUP
#define ENOTSUP 0
#endif
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT 0
#endif
#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT 0
#endif
#ifndef ESHUTDOWN
#define ESHUTDOWN 0
#endif
#ifndef ETOOMANYREFS
#define ETOOMANYREFS 0
#endif
#ifndef ENOTBLK
#define ENOTBLK 0
#endif
#ifndef EHOSTDOWN
#define EHOSTDOWN 0
#endif
#ifndef EWOULDBLOCK
 #ifdef EAGAIN
  #define EWOULDBLOCK EAGAIN
 #else
  #define EWOULDBLOCK 0
 #endif
#endif
#ifndef ENOSR
#define ENOSR 0
#endif
#ifndef ENOSTR
#define ENOSTR 0
#endif
#ifndef ENODATA
#define ENODATA 0
#endif
#ifndef ETIME
#define ETIME 0
#endif

// VxWorks "UNIX" error codes
constexpr int errorCodes[] = {
	0,
	/*
	 * POSIX Error codes
	 */
	EPERM,			/* Not owner */
	ENOENT,			/* No such file or directory */
	ESRCH,			/* No such process */
	EINTR,			/* Interrupted system call */
	EIO,			/* I/O error */
	ENXIO,			/* No such device or address */
	E2BIG,			/* Arg list too long */
	ENOEXEC,		/* Exec format error */
	EBADF,			/* Bad file number */
	ECHILD,			/* No children */
	EAGAIN,			/* No more processes */
	ENOMEM,			/* Not enough core */
	EACCES,			/* Permission denied */
	EFAULT,			/* Bad address */
	ENOTEMPTY,		/* Directory not empty */
	EBUSY,			/* Mount device busy */
	EEXIST,			/* File exists */
	EXDEV,			/* Cross-device link */
	ENODEV,			/* No such device */
	ENOTDIR,		/* Not a directory*/
	EISDIR,			/* Is a directory */
	EINVAL,			/* Invalid argument */
	ENFILE,			/* File table overflow */
	EMFILE,			/* Too many open files */
	ENOTTY,			/* Not a typewriter */
	ENAMETOOLONG,	/* File name too long */
	EFBIG,			/* File too large */
	ENOSPC,			/* No space left on device */
	ESPIPE,			/* Illegal seek */
	EROFS,			/* Read-only file system */
	EMLINK,			/* Too many links */
	EPIPE,			/* Broken pipe */
	EDEADLK,		/* Resource deadlock avoided */
	ENOLCK,			/* No locks available */
	ENOTSUP,		/* Unsupported value */
	EMSGSIZE,		/* Message size */
	/* ANSI math software */
	EDOM,			/* Argument too large */
	ERANGE,			/* Result too large */

	/* ipc/network software */
	0,
	/* argument errors */
	EDESTADDRREQ,	/* Destination address required */
	EPROTOTYPE,		/* Protocol wrong type for socket */
	ENOPROTOOPT,	/* Protocol not available */
	EPROTONOSUPPORT,/* Protocol not supported */
	ESOCKTNOSUPPORT,/* Socket type not supported */
	EOPNOTSUPP,		/* Operation not supported on socket */
	EPFNOSUPPORT,	/* Protocol family not supported */
	EAFNOSUPPORT,	/* Addr family not supported */
	EADDRINUSE,		/* Address already in use */
	EADDRNOTAVAIL,	/* Can't assign requested address */
	ENOTSOCK,		/* Socket operation on non-socket */

	/* operational errors */
	ENETUNREACH,		/* Network is unreachable */
	ENETRESET,		/* Network dropped connection on reset*/
	ECONNABORTED,	/* Software caused connection abort */
	ECONNRESET,		/* Connection reset by peer */
	ENOBUFS,		/* No buffer space available */
	EISCONN,		/* Socket is already connected */
	ENOTCONN,		/* Socket is not connected */
	ESHUTDOWN,		/* Can't send after socket shutdown */
	ETOOMANYREFS,	/* Too many references: can't splice */
	ETIMEDOUT,		/* Connection timed out */
	ECONNREFUSED,	/* Connection refused */
	ENETDOWN,		/* Network is down */
	ETXTBSY,		/* Text file busy */
	ELOOP,			/* Too many levels of symbolic links */
	EHOSTUNREACH,	/* No route to host */
	ENOTBLK,		/* Block device required */
	EHOSTDOWN,		/* Host is down */

	/* non-blocking and interrupt i/o */
	EINPROGRESS,	/* Operation now in progress */
	EALREADY,		/* Operation already in progress */
	EWOULDBLOCK,	/* Operation would block */
	ENOSYS,			/* Function not implemented */

	/* aio errors (should be under posix) */
	ECANCELED,		/* Operation canceled */
	0,

	/* specific STREAMS errno values */
	ENOSR,			/* Insufficient memory */
	ENOSTR,			/* STREAMS device required */
	EPROTO,			/* Generic STREAMS error */
	EBADMSG,		/* Invalid STREAMS message */
	ENODATA,		/* Missing expected message data */
	ETIME,			/* STREAMS timeout occurred */
	ENOMSG,			/* Unexpected message type */
};
static_assert(std::size(errorCodes) == 81);

static int convertError(int error)
{
	for (unsigned i = 0; i < std::size(errorCodes); i++)
		if (errorCodes[i] == error)
			return i;
	return 5;	// EIO
}

class NetDimmServer
{
public:
	NetDimmServer(NetDimm& netdimm)
		: netdimm(netdimm)
	{
		io_context = std::make_unique<asio::io_context>();
		thread = std::thread(&NetDimmServer::serverThread, this);
	}
	~NetDimmServer()
	{
		if (thread.joinable())
		{
			io_context->stop();
			thread.join();
			io_context.reset();
		}
	}

private:
	struct PacketView
	{
		PacketView(const u8 *data)
			: data(data)
		{}
		u8 opcode() const { return data[3]; }
		u8 flags() const { return data[2]; }
		u16 payloadLen() const { return read16(0); }
		const u8 *payload() const { return data + 4; }

		u16 read16(u32 offset) const {
			return data[offset] + (data[offset + 1] << 8);
		}
		u32 read32(u32 offset) const
		{
			return data[offset]
					+ (data[offset + 1] << 8)
					+ (data[offset + 2] << 16)
					+ (data[offset + 3] << 24);
		}

		const u8 *data;
	};
	struct Packet
	{
		Packet(u8 opcode, u8 flags = 0)
		{
			data.resize(4);
			data[3] = opcode;
			data[2] = flags;
		}

		void push(u8 v) { data.push_back(v); }
		void push16(u16 v) {
			data.push_back(v & 0xff);
			data.push_back(v >> 8);
		}
		void push32(u32 v)
		{
			data.push_back(v & 0xff);
			data.push_back(v >> 8);
			data.push_back(v >> 16);
			data.push_back(v >> 24);
		}
		void push(const u8 *p, u32 len) {
			data.insert(data.end(), p, p + len);
		}

		void skip(u32 n) {
			data.resize(data.size() + n);
		}

		const std::vector<u8>& finalize() {
			data[0] = data.size() - 4;
			data[1] = (data.size() - 4) >> 8;
			return data;
		}
		std::vector<u8> data;
	};

	class Connection : public SharedThis<Connection>
	{
	public:
		asio::ip::tcp::socket& getSocket() {
			return socket;
		}

		void start() {
			asio::async_read_until(socket, asio::dynamic_buffer(message, 64_KB), packetMatcher,
					std::bind(&Connection::handlePacket, shared_from_this(),
									asio::placeholders::error,
									asio::placeholders::bytes_transferred));
		}

		void send(const std::vector<u8>& msg)
		{
			if (!outMessage.empty()) {
				WARN_LOG(NAOMI, "Message dropped: %x. Already sending: %x", msg[3], outMessage[3]);
				return;
			}
			outMessage = msg;
			asio::async_write(socket, asio::buffer(outMessage),
				std::bind(&Connection::writeDone, shared_from_this(),
						asio::placeholders::error,
						asio::placeholders::bytes_transferred));
		}

	private:
		Connection(NetDimm& netdimm, asio::io_context& io_context)
			: netdimm(netdimm), io_context(io_context), socket(io_context) {
		}

		using iterator = asio::buffers_iterator<asio::const_buffers_1>;

		std::pair<iterator, bool>
		static packetMatcher(iterator begin, iterator end)
		{
			if (end - begin < 4)
				return std::make_pair(begin, false);
			iterator it = begin;
			u16 len = *it++;
			len |= *it << 8;
			if (end - begin < 4 + len)
				return std::make_pair(begin, false);
			else
				return std::make_pair(begin + 4 + len, true);
		}

		void handlePacket(const std::error_code& ec, size_t len)
		{
			if (ec)
			{
				if (ec != asio::error::eof && ec != asio::error::connection_reset)
					WARN_LOG(COMMON, "Receive error: %s", ec.message().c_str());
				return;
			}
			if (len < 4) {
				WARN_LOG(COMMON, "Received small packet: %d bytes", (int)len);
				return;
			}
			PacketView pkt(&message[0]);
			switch (pkt.opcode())
			{
			case 1: // NOP
				INFO_LOG(NAOMI, "netdimm server: NOP");
				break;
			case 4:	// upload
			{
				if (pkt.payloadLen() > 10)
				{
					u32 addr = pkt.read32(8);
					u8 *dest = netdimm.getDimmData(addr);
					memcpy(dest, pkt.payload() + 10, len - 14);
				}
				break;
			}
			case 5: // download
			{
				u32 addr = pkt.read32(4);
				u32 size = pkt.read32(8);
				INFO_LOG(NAOMI, "netdimm server: download(%x, %x)", addr, size);
				int seq = 1;
				bool specialAddr = false;
				u32 value;
				if ((addr & 0x3fffffff) == 0x3ffeffe0) {
					// crc status
					value = 2; // 2: CRC over data is correct, game should boot or be running.
					specialAddr = true;
				}
				else if ((addr & 0x3fffffff) == 0x3fff0004) {
					// game size set by set_information
					value = netdimm.getFileSize();
					specialAddr = true;
				}
				if (specialAddr)
				{
					Packet opkt(4, 0x81);
					opkt.push32(1);
					opkt.push32(addr);
					opkt.push16(0); // ?
					opkt.push32(value);
					send(opkt.finalize());
					break;
				}
				while (size > 0)
				{
					u32 chunksz = std::min(size, 8192u);
					Packet opkt(4, chunksz < size ? 0x80 : 0x81);
					opkt.push32(seq);
					opkt.push32(addr);
					opkt.push16(0); // ?
					opkt.push(netdimm.getDimmData(addr), chunksz);
					send(opkt.finalize());
					addr += chunksz;
					size -= chunksz;
					seq++;
				}
				break;
			}
			case 7: // set_mode_host (not implemented on real hw?)
			{
				const u8 set = pkt.data[4];
				const u8 reset = pkt.data[5];
				INFO_LOG(NAOMI, "netdimm server: set_mode_host(%x, %x)", set, reset);
				Packet opkt(7);
				opkt.push32(set & ~reset);
				send(opkt.finalize());
				break;
			}
			// case 8: // set dimm mode (not implemented on real hw?)
			case 9: // close
				INFO_LOG(NAOMI, "netdimm server: closing");
				return;
			case 0xa: // reboot
				WARN_LOG(NAOMI, "netdimm server: reboot requested");
				netdimm.reboot();
				break;
			case 0x10: // peek
			{
				u32 addr = pkt.read32(4);
				u8 type = pkt.read32(8);
				INFO_LOG(NAOMI, "netdimm server: peek(%x, %d)", addr, type);
				u32 val;
				switch (type)
				{
				case 1:
					val = addrspace::read8(addr);
					break;
				case 2:
					val = addrspace::read16(addr);
					break;
				case 3:
				default:
					val = addrspace::read32(addr);
					break;
				}
				Packet opkt(0x10);
				opkt.push32(0);
				opkt.push32(val);
				send(opkt.finalize());
				break;
			}
			// case 0x11: // poke
			case 0x16: // host control read
			{
				INFO_LOG(NAOMI, "netdimm server: host control read");
				netdimm.controlRead([this](u32 address) {
					io_context.post([this, address]() {
						Packet opkt(0x10, 0x81);
						opkt.push32(0);
						opkt.push32(address);
						send(opkt.finalize());
					});
				});
				break;
			}

			case 0x17: // set time limit
				INFO_LOG(NAOMI, "netdimm server: set_time_limit(%d)", pkt.read32(4));
				break;

			case 0x18: // get information
			{
				INFO_LOG(NAOMI, "netdimm server: get information");
				Packet opkt(0x18);
				opkt.push16(0xc);								// unknown. protocol version?
				opkt.push16(0x317);								// dimm fw version
				opkt.push16(netdimm.getDimmSize() / 1_MB - 16);	// available game memory (MB)
				opkt.push16(netdimm.getDimmSize() / 1_MB);		// dimm memory (MB)
				opkt.push32(netdimm.getCrc()); 					// crc
				send(opkt.finalize());
				break;
			}
			case 0x19: // set information
				INFO_LOG(NAOMI, "netdimm server: set_information(crc=%x, len=%x)", pkt.read32(4), pkt.read32(8));
				break;

			case 0x7f: // set key code (ignored)
				INFO_LOG(NAOMI, "netdimm server: set_key_code(%02x %02x %02x %02x...)", pkt.data[4], pkt.data[5], pkt.data[6], pkt.data[7]);
				break;

			default:
				WARN_LOG(NAOMI, "netdimm server: Unknown opcode %x", pkt.opcode());
				break;
			}
			message.erase(message.begin(), message.begin() + len);
			start();
		}

		void writeDone(const std::error_code& ec, size_t len)
		{
			if (ec)
				WARN_LOG(COMMON, "Write error: %s", ec.message().c_str());
			outMessage.clear();
		}

		NetDimm& netdimm;
		asio::io_context& io_context;
		asio::ip::tcp::socket socket;
		std::vector<u8> message;
		std::vector<u8> outMessage;
		friend super;
	};

	class TcpAcceptor
	{
	public:
		TcpAcceptor(NetDimm& netdimm, asio::io_context& io_context)
			: netdimm(netdimm), io_context(io_context),
			  acceptor(asio::ip::tcp::acceptor(io_context,
					asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 10703)))
		{
			asio::socket_base::reuse_address option(true);
			acceptor.set_option(option);
			start();
		}

	private:
		void start()
		{
			Connection::Ptr newConnection = Connection::create(netdimm, io_context);

			acceptor.async_accept(newConnection->getSocket(),
					std::bind(&TcpAcceptor::handleAccept, this, newConnection, asio::placeholders::error));
		}

		void handleAccept(Connection::Ptr newConnection, const std::error_code& error)
		{
			if (!error)
				newConnection->start();
			start();
		}

		NetDimm& netdimm;
		asio::io_context& io_context;
		asio::ip::tcp::acceptor acceptor;
	};

	void serverThread()
	{
		ThreadName _("NetDimmServer");
		INFO_LOG(NAOMI, "NetDimmServer started");
		try
		{
			TcpAcceptor server(netdimm, *io_context);
			io_context->run();
		}
		catch (const std::exception& e) {
			ERROR_LOG(NAOMI, "NetDimmServer exception: %s", e.what());
		}
		INFO_LOG(NAOMI, "NetDimmServer stopped");
	}

	NetDimm& netdimm;
	std::thread thread;
	std::unique_ptr<asio::io_context> io_context;
};

NetDimm::NetDimm(u32 size) : GDCartridge(size)
{
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
}

void NetDimm::Init(LoadProgress *progress, std::vector<u8> *digest)
{
	if (strncmp(game->name, "wccf", 4) == 0)
		fullLoad = true;
	GDCartridge::Init(progress, digest);
	dimmBufferOffset = dimm_data_size - 16_MB;
	finalTuned = strcmp(game->name, "vf4tuned") == 0;
	if (strncmp(game->name, "wccf", 4) == 0)
		server = std::make_unique<NetDimmServer>(*this);
}

bool NetDimm::Write(u32 offset, u32 size, u32 data)
{
	if (dimm_data != nullptr) {
		u32 addr = offset & (dimm_data_size - 1);
		memcpy(&dimm_data[addr], &data, std::min(size, dimm_data_size - addr));
	}
	return true;
}

sock_t NetDimm::getSocket(int idx)
{
	if (idx < 1 || idx > (int)sockets.size())
		return INVALID_SOCKET;
	if (sockets[idx - 1].fd == INVALID_SOCKET)
		sockets[idx - 1].lastError = convertError(EBADF);
	return sockets[idx - 1].fd;

}

int NetDimm::schedCallback()
{
	while (!serverQueue.empty())
	{
		switch (serverQueue.pop())
		{
		case ControlRead:
			dimm_command = 0x8200;
			dimm_offsetl = 0;
			dimm_parameterl = 0;
			dimm_parameterh = 0;
			asic_RaiseInterrupt(holly_EXP_PCI);
			break;
		case Reboot:
			*(u32 *)&dimm_data[dimm_data_size - 0x10020] = 0;
			emu.requestReset();
			serverQueue.clear();
			return 0;
		default:
			break;
		}
	}
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
						socket.lastError = convertError(so_error);
						returnToNaomi(so_error != 0, &socket - &sockets[0] + 1, so_error != 0 ? -1 : 0);
						break;
					}
				}
				if (socket.connectTimeout > 0 && socket.connectTime + socket.connectTimeout <= sh4_sched_now64())
				{
					WARN_LOG(NAOMI, "connect(%d) timeout", socket.fd);
					socket.connecting = false;
					socket.lastError = convertError(ECONNREFUSED);
					returnToNaomi(true, &socket - &sockets[0] + 1, -1);
					break;
				}
			}
			else if (socket.receiving)
			{
				if (rc > 0)
				{
					if (socket.srcAddr != nullptr)
						rc = recvfrom(socket.fd, (char *)socket.recvData, socket.recvLen, 0, socket.srcAddr, socket.addrLen);
					else
						rc = recv(socket.fd, (char *)socket.recvData, socket.recvLen, 0);
					if (rc == -1)
					{
						const int error = get_last_error();
						if (error != L_EAGAIN && error != L_EWOULDBLOCK)
						{
							socket.lastError = convertError(error);
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
					DEBUG_LOG(NAOMI, "recv(%d, %d) -> %d", (int)(&socket - &sockets[0] + 1), socket.recvLen, rc);
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
					socket.lastError = convertError(ETIMEDOUT);
					returnToNaomi(true, &socket - &sockets[0] + 1, -1);
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
						const int error = get_last_error();
						if (error != L_EAGAIN && error != L_EWOULDBLOCK)
						{
							socket.lastError = convertError(error);
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
					DEBUG_LOG(NAOMI, "send(%d, %d) -> %d", (int)(&socket - &sockets[0] + 1), socket.sendLen, rc);
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
					socket.lastError = convertError(ETIMEDOUT);
					returnToNaomi(true, &socket - &sockets[0] + 1, -1);
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

void NetDimm::systemCmd(int cmd)
{
	switch (cmd)
	{
	case 0xf:	// startup
		NOTICE_LOG(NAOMI, "NetDIMM startup");
		// bit 16,17: dimm0 size (none, 128, 256, 512)
		// bit 18,19: dimm1 size
		// bit 27: dhcp done? needed for wccf, else stops on Waiting DHCP offer
		// bit 28: network enabled (network settings appear in bios menu)
		// bit 29: set
		// bit 30: gd-rom connected
		// bit 31: mobile/ppp network?
		// (| 30, 70, F0, 1F0, 3F0, 7F0)
		// | offset >> 20 (dimm buffers offset @ size - 16MB)
		// offset = (64MB << 0-5) - 16MB
		// vf4 forces this value to 0f000000 (256MB) if != 1f000000 (512MB)
		if (dimm_data_size == 512_MB)
			addrspace::write32(0xc01fc04, (3 << 16) | 0x78000000 | (dimmBufferOffset >> 20));	// dimm board config 1 x 512 MB
		else if (dimm_data_size == 256_MB)
			addrspace::write32(0xc01fc04, (2 << 16) | 0x78000000 | (dimmBufferOffset >> 20));	// dimm board config 1 x 256 MB
		else if (dimm_data_size == 128_MB)
			addrspace::write32(0xc01fc04, (1 << 16) | 0x78000000 | (dimmBufferOffset >> 20));	// dimm board config 1 x 128 MB
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
		// DIMM board serial Id
		{
			const u32 *serial = (u32 *)(getGameSerialId() + 0x20);	// get only the serial id
			addrspace::write32(0xc01fc40, *serial++);
			addrspace::write32(0xc01fc44, *serial++);
			addrspace::write32(0xc01fc48, *serial++);
			addrspace::write32(0xc01fc4c, *serial++);
		}
		addrspace::write32(0xc01fc18, 0x10002);	// net mode (2 or 4 is mobile), bit 16 dhcp?
		// network order
		addrspace::write32(0xc01fc60, htonl(0xc0a8011e));	// ip address (192.168.1.30)
		addrspace::write32(0xc01fc64, htonl(0xffffff00));	// netmask
		addrspace::write32(0xc01fc68, htonl(0xc0a80101));	// gateway 192.168.1.1
		addrspace::write32(0xc01fc6c, htonl(0xc0a80101));	// dns1
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

	case 1:		// control read
		if (controlReadCallback)
		{
			DEBUG_LOG(NAOMI, "Control read callback: offset %x parameter %04x %04x", dimm_offsetl, dimm_parameterh, dimm_parameterl);
			controlReadCallback((dimm_parameterh << 16) | dimm_parameterl);
			controlReadCallback = {};
		}
		break;
	case 0:		// nop
	case 3:		// set base address
	case 4:		// peek8
	case 5:		// peek16
	case 6:		// peek32
	case 8:		// poke8
	case 9:		// poke16
	case 10:	// poke32
		// These are callbacks from naomi
		DEBUG_LOG(NAOMI, "System callback command %x offset %x parameter %04x %04x", cmd, dimm_offsetl, dimm_parameterh, dimm_parameterl);
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
		{
			const int sockidx = buffer[1];
			const sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET) {
				INFO_LOG(NAOMI, "bind(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				const int option = 1;
				setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(option));

				sockaddr_in addr = *(const sockaddr_in *)getDimmData(buffer[2]);
				addr.sin_family = AF_INET;
				rc = bind(sockfd, (const sockaddr *)&addr, buffer[3]);
				INFO_LOG(NAOMI, "bind(%d, %s:%d, %d) -> %d", sockidx, inet_ntoa(addr.sin_addr), htons(addr.sin_port), buffer[3], rc);
				if (rc == -1)
					sockets[sockidx - 1].lastError = convertError(get_last_error());
			}
			returnToNaomi(rc == -1, buffer[1], rc);
			break;
		}
	case 3: // close
		{
			const int sockidx = buffer[1];
			const sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET) {
				INFO_LOG(NAOMI, "closesocket(%d) invalid socket", sockidx);
				rc = -1;
			}
			else {
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
					const int error = get_last_error();
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
						sockets[sockidx - 1].lastError = convertError(error);
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
					const int error = get_last_error();
					if (error == L_EAGAIN || error == L_EWOULDBLOCK)
					{
						sockets[sockidx - 1].receiving = true;
						sockets[sockidx - 1].recvTime = sh4_sched_now64();
						sockets[sockidx - 1].recvData = data;
						sockets[sockidx - 1].recvLen = len;
						sockets[sockidx - 1].srcAddr = nullptr;
						sockets[sockidx - 1].addrLen = nullptr;
						sh4_sched_request(schedId, POLL_CYCLES);
						DEBUG_LOG(NAOMI, "recv(%d, %d) delayed", sockidx, len);
						return;
					}
					else
					{
						sockets[sockidx - 1].lastError = convertError(error);
					}
				}
#ifdef HTTP_TRACE
				else if (rc > 0)
				{
					fwrite(data, 1, rc, stdout);
					fflush(stdout);
				}
#endif
				DEBUG_LOG(NAOMI, "recv(%d, %d) -> %d", sockidx, len, rc);
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
					const int error = get_last_error();
					if (error == L_EAGAIN || error == L_EWOULDBLOCK)
					{
						sockets[sockidx - 1].sending = true;
						sockets[sockidx - 1].sendTime = sh4_sched_now64();
						sockets[sockidx - 1].sendData = data;
						sockets[sockidx - 1].sendLen = len;
						sh4_sched_request(schedId, POLL_CYCLES);
						DEBUG_LOG(NAOMI, "send(%d, %d) delayed", sockidx, len);
						return;
					}
					else
					{
						sockets[sockidx - 1].lastError = convertError(error);
					}
				}
				DEBUG_LOG(NAOMI, "send(%d, %d) -> %d", sockidx, len, rc);
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
		{
			const int sockidx = buffer[1];
			const sock_t sockfd = getSocket(sockidx);
			int rc = 0;
			if (sockfd == INVALID_SOCKET) {
				INFO_LOG(NAOMI, "setsockopt(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				int optname = buffer[3];
				const int level = SOL_SOCKET;
				switch (optname)
				{
				case 0x20:
					optname = SO_BROADCAST;
					break;
				case 0x1002:
					optname = SO_RCVBUF;
					break;
				default:
					WARN_LOG(NAOMI, "netdimm: unknown setsockopt option: fd=%d, optname=%x", sockidx, optname);
					rc = -1;
					break;
				}
				if (rc != -1)
				{
					rc = setsockopt(sockfd, level, optname, (const char *)getDimmData(buffer[4]), buffer[5]);
					INFO_LOG(NAOMI, "netdimm: setsockopt(fd=%d, level=%x, optname=%x, optval=%x, optlen=%x) -> %d",
							sockidx, buffer[2], buffer[3], *(u32*)getDimmData(buffer[4]), buffer[5], rc);
					if (rc == -1)
						sockets[sockidx - 1].lastError = convertError(get_last_error());
				}
			}
			returnToNaomi(rc == -1, sockidx, rc);
			break;
		}
	case 15: // getsockopt
		{
			const int sockidx = buffer[1];
			const sock_t sockfd = getSocket(sockidx);
			int rc = 0;
			if (sockfd == INVALID_SOCKET) {
				INFO_LOG(NAOMI, "getsockopt(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				int optname = buffer[3];
				const int level = SOL_SOCKET;
				switch (optname)
				{
				case 0x20:
					optname = SO_BROADCAST;
					break;
				case 0x1002:
					optname = SO_RCVBUF;
					break;
				default:
					WARN_LOG(NAOMI, "netdimm: unknown getsockopt option: fd=%d, optname=%x", sockidx, optname);
					rc = -1;
					break;
				}
				if (rc != -1)
				{
					rc = getsockopt(sockfd, level, optname, (char *)getDimmData(buffer[4]), (socklen_t *)getDimmData(buffer[5]));
					INFO_LOG(NAOMI, "netdimm: getsockopt(fd=%d, level=%x, optname=%x, optval=%x) -> %d",
							sockidx, buffer[2], buffer[3], *(u32*)getDimmData(buffer[4]), rc);
					if (rc == -1)
						sockets[sockidx - 1].lastError = convertError(get_last_error());
				}
			}
			returnToNaomi(rc == -1, sockidx, rc);
			break;
		}
	case 16: // settimeout
		{
			const int sockidx = buffer[1];
			sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET) {
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
		returnToNaomi(false, 0, 0);
		break;
	case 21: // modifyMyIPaddr
		WARN_LOG(NAOMI, "netdimm: modifyMyIPaddr not implemented");
		returnToNaomi(true, 0, -1);
		break;
	case 22: // recvfrom
		{
			const int sockidx = buffer[1];
			const sock_t sockfd = getSocket(sockidx);
			const u32 len = buffer[3];

			int rc;
			if (sockfd == INVALID_SOCKET)
			{
				WARN_LOG(NAOMI, "recv(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				u8 *data = &dimm_data[buffer[2] & (dimm_data_size - 1)];
				sockaddr *src_addr = (sockaddr *)&dimm_data[buffer[5] & (dimm_data_size - 1)];
				socklen_t *addrlen = (socklen_t *)&dimm_data[buffer[6] & (dimm_data_size - 1)];
				rc = recvfrom(sockfd, (char *)data, len, buffer[4], src_addr, addrlen);
				if (rc == -1)
				{
					const int error = get_last_error();
					if (error == L_EAGAIN || error == L_EWOULDBLOCK)
					{
						sockets[sockidx - 1].receiving = true;
						sockets[sockidx - 1].recvTime = sh4_sched_now64();
						sockets[sockidx - 1].recvData = data;
						sockets[sockidx - 1].recvLen = len;
						sockets[sockidx - 1].srcAddr = src_addr;
						sockets[sockidx - 1].addrLen = addrlen;
						sh4_sched_request(schedId, POLL_CYCLES);
						DEBUG_LOG(NAOMI, "recvfrom(%d, %d) delayed", sockidx, len);
						return;
					}
					else {
						sockets[sockidx - 1].lastError = convertError(error);
					}
				}
			}
			DEBUG_LOG(NAOMI, "recvfrom(%d, %d) -> %x", sockidx, len, rc);
			returnToNaomi(rc == -1, sockidx, rc);
			break;
		}
	case 23: // sendto
		{
			const int sockidx = buffer[1];
			const sock_t sockfd = getSocket(sockidx);
			int rc;
			if (sockfd == INVALID_SOCKET) {
				WARN_LOG(NAOMI, "sendto(%d) invalid socket", sockidx);
				rc = -1;
			}
			else
			{
				const void *data = &dimm_data[buffer[2] & (dimm_data_size - 1)];
				sockaddr_in dest_addr = *(sockaddr_in *)&dimm_data[buffer[5] & (dimm_data_size - 1)];
				dest_addr.sin_family = AF_INET;
				rc = sendto(sockfd, (const char *)data, buffer[3], buffer[4], (const sockaddr *)&dest_addr, buffer[6]);
				DEBUG_LOG(NAOMI, "netdimm: sendto(%d, %p, %x, %x, %s, %x) -> %x",
						sockidx, data, buffer[3], buffer[4], inet_ntoa(dest_addr.sin_addr), buffer[6], rc);
				if (rc < 0)
					sockets[sockidx - 1].lastError = convertError(get_last_error());
			}
			returnToNaomi(rc < 0, sockidx, rc);
			break;
		}

	default:
		WARN_LOG(NAOMI, "netdimm: Invalid Net command: %d", cmd);
		returnToNaomi(true, 0, 0);
		break;
	}
}

void NetDimm::process()
{
	DEBUG_LOG(NAOMI, "NetDIMM cmd %04x sock %d offset %04x paramh/l %04x %04x", (dimm_command >> 9) & 0x3f,
			dimm_command & 0xff, dimm_offsetl, dimm_parameterh, dimm_parameterl);

	const int cmdGroup = (dimm_command >> 13) & 3;
	const int cmd = (dimm_command >> 9) & 0xf;
	switch (cmdGroup)
	{
	case 0:	// system commands
		systemCmd(cmd);
		break;
	case 1: // network client
		netCmd(cmd);
		break;
	default:
		WARN_LOG(NAOMI, "Unknown DIMM command group %d cmd %x", cmdGroup, cmd);
		returnToNaomi(true, 0, -1);
		break;
	}
}

void NetDimm::Deserialize(Deserializer &deser)
{
	GDCartridge::Deserialize(deser);
	for (Socket& socket : sockets)
		socket.close();
	if (deser.version() >= Deserializer::V36 && deser.version() < Deserializer::V53)
	{
		// moved to parent class in v53
		deser >> dimm_command;
		deser >> dimm_offsetl;
		deser >> dimm_parameterl;
		deser >> dimm_parameterh;
		sh4_sched_deserialize(deser, schedId);
	}
}

void NetDimm::controlRead(std::function<void(u32)> callback) {
	controlReadCallback = callback;
	serverQueue.push(ControlRead);
}

void NetDimm::reboot() {
	serverQueue.push(Reboot);
}
