/*
	Copyright 2025 flyinghead

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
#include "types.h"
#include <asio.hpp>
#include "netservice.h"
#include "util/tsqueue.h"
#include "oslib/oslib.h"
#include "emulator.h"
#include "hw/bba/bba.h"
#include "cfg/option.h"
#include "stdclass.h"
#ifndef LIBRETRO
#include "cfg/cfg.h"
#endif

#include <thread>
#include <memory>
#include <array>
#include <chrono>
#ifndef __ANDROID__
//#define WIRESHARK_DUMP 1
#endif

namespace net::modbba
{

static TsQueue<u8> toModem;

class DCNetService : public Service
{
public:
	bool start() override;
	void stop() override;

	void writeModem(u8 b) override;
	int readModem() override;
	int modemAvailable() override;

	void receiveEthFrame(const u8 *frame, u32 size) override;
};

template<typename SocketT>
class PPPSocket
{
public:
	PPPSocket(asio::io_context& io_context, const typename SocketT::endpoint_type& endpoint,
			const std::string& endpointName = "")
		: socket(io_context)
	{
		asio::error_code ec;
		socket.connect(endpoint, ec);
		if (ec)
			throw FlycastException(ec.message().c_str());
		os_notify("Connected to DCNet with modem", 5000, endpointName.c_str());
		receive();
	}

	~PPPSocket() {
		if (dumpfp != nullptr)
			fclose(dumpfp);
	}

	void send(u8 b)
	{
		if (sendBufSize == sendBuffer.size()) {
			WARN_LOG(NETWORK, "PPP output buffer overflow");
			return;
		}
		sendBuffer[sendBufSize++] = b;
		doSend();
	}

private:
	void receive()
	{
		socket.async_read_some(asio::buffer(recvBuffer),
			[this](const std::error_code& ec, size_t len)
			{
				if (ec || len == 0)
				{
					if (ec)
						ERROR_LOG(NETWORK, "Receive error: %s", ec.message().c_str());
					close();
					return;
				}
				pppdump(recvBuffer.data(), len, false);
				for (size_t i = 0; i < len; i++)
					toModem.push(recvBuffer[i]);
				receive();
			});
	}

	void doSend()
	{
		if (sending)
			return;
		pppdump(sendBuffer.data(), sendBufSize, true);
		sending = true;
		asio::async_write(socket, asio::buffer(sendBuffer, sendBufSize),
			[this](const std::error_code& ec, size_t len)
			{
				if (ec)
				{
					ERROR_LOG(NETWORK, "Send error: %s", ec.message().c_str());
					close();
					return;
				}
				sending = false;
				sendBufSize -= len;
				if (sendBufSize > 0) {
					memmove(&sendBuffer[0], &sendBuffer[len], sendBufSize);
					doSend();
				}
			});
	}

	void close() {
		std::error_code ignored;
		socket.close(ignored);
	}

	void pppdump(uint8_t *buf, int len, bool egress)
	{
#ifdef WIRESHARK_DUMP
		if (!len)
			return;
		if (dumpfp == nullptr)
		{
			dumpfp = fopen("ppp.dump", "a");
			if (dumpfp == nullptr)
	            return;
			time_t now;
			time(&now);
			u32 reset_time = ntohl((u32)now);
	        fputc(7, dumpfp);                    // Reset time
	        fwrite(&reset_time, sizeof(reset_time), 1, dumpfp);
	        dump_last_time_ms = getTimeMs();
		}

		u32 delta = getTimeMs() / 100 - dump_last_time_ms / 100;
		if (delta < 256) {
			fputc(6, dumpfp);					// Time step (short)
			fwrite(&delta, 1, 1, dumpfp);
		}
		else
		{
			delta = ntohl(delta);
			fputc(5, dumpfp);					// Time step (long)
			fwrite(&delta, sizeof(delta), 1, dumpfp);
		}
		dump_last_time_ms = getTimeMs();
		fputc(egress ? 1 : 2, dumpfp);			// Sent/received data
		uint16_t slen = htons(len);
		fwrite(&slen, 2, 1, dumpfp);
		fwrite(buf, 1, len, dumpfp);
#endif
	}

	SocketT socket;
	std::array<u8, 1542> recvBuffer;
	std::array<u8, 1542> sendBuffer;
	u32 sendBufSize = 0;
	bool sending = false;

	FILE *dumpfp = nullptr;
	u64 dump_last_time_ms;
};

using PPPTcpSocket = PPPSocket<asio::ip::tcp::socket>;

class EthSocket
{
public:
	EthSocket(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint,
			const std::string& endpointName = "")
		: socket(io_context)
	{
		asio::error_code ec;
		socket.connect(endpoint, ec);
		if (ec)
			throw FlycastException(ec.message().c_str());
		os_notify("Connected to DCNet with Ethernet", 5000, endpointName.c_str());
		receive();
		u8 prolog[] = { 'D', 'C', 'N', 'E', 'T', 1 };
		send(prolog, sizeof(prolog));
	}

	~EthSocket() {
		if (dumpfp != nullptr)
			fclose(dumpfp);
	}

	void send(const u8 *frame, u32 size)
	{
		if (sendBufferIdx + size >= sendBuffer.size()) {
			WARN_LOG(NETWORK, "Dropped out frame (buffer:%d + %d bytes). Increase send buffer size\n", sendBufferIdx, size);
			return;
		}
		if (size >= 32) // skip prolog
			ethdump(frame, size);
		*(u16 *)&sendBuffer[sendBufferIdx] = size;
		sendBufferIdx += 2;
		memcpy(&sendBuffer[sendBufferIdx], frame, size);
		sendBufferIdx += size;
		doSend();
	}

private:
	using iterator = asio::buffers_iterator<asio::const_buffers_1>;

	std::pair<iterator, bool>
	static packetMatcher(iterator begin, iterator end)
	{
		if (end - begin < 3)
			return std::make_pair(begin, false);
		iterator i = begin;
		uint16_t len = (uint8_t)*i++;
		len |= uint8_t(*i++) << 8;
		len += 2;
		if (end - begin < len)
			return std::make_pair(begin, false);
		return std::make_pair(begin + len, true);
	}

	void receive()
	{
		asio::async_read_until(socket, asio::dynamic_vector_buffer(recvBuffer), packetMatcher,
			[this](const std::error_code& ec, size_t len)
			{
				if (ec || len == 0)
				{
					if (ec)
						ERROR_LOG(NETWORK, "Receive error: %s", ec.message().c_str());
					std::error_code ignored;
					socket.close(ignored);
					return;
				}
				/*
				verify(len - 2 == *(u16 *)&recvBuffer[0]);
				printf("In frame: dest %02x:%02x:%02x:%02x:%02x:%02x "
					   "src %02x:%02x:%02x:%02x:%02x:%02x, ethertype %04x, size %d bytes\n",
					   recvBuffer[2], recvBuffer[3], recvBuffer[4], recvBuffer[5], recvBuffer[6], recvBuffer[7],
					   recvBuffer[8], recvBuffer[9], recvBuffer[10], recvBuffer[11], recvBuffer[12], recvBuffer[13],
					*(u16 *)&recvBuffer[14], (int)len - 2);
				*/
				ethdump(&recvBuffer[2], len - 2);
				bba_recv_frame(&recvBuffer[2], len - 2);
				if (len < recvBuffer.size())
					recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + len);
				else
					recvBuffer.clear();
				receive();
			});
	}

	void doSend()
	{
		if (sending)
			return;
		sending = true;
		asio::async_write(socket, asio::buffer(sendBuffer, sendBufferIdx),
			[this](const std::error_code& ec, size_t len)
			{
				sending = false;
				if (ec)
				{
					ERROR_LOG(NETWORK, "Send error: %s", ec.message().c_str());
					std::error_code ignored;
					socket.close(ignored);
					return;
				}
				sendBufferIdx -= len;
				if (sendBufferIdx != 0) {
					memmove(sendBuffer.data(), sendBuffer.data() + len, sendBufferIdx);
					doSend();
				}
			});
	}

	void ethdump(const uint8_t *frame, int size)
	{
#ifdef WIRESHARK_DUMP
		if (dumpfp == nullptr)
		{
			dumpfp = fopen("bba.pcapng", "wb");
			if (dumpfp == nullptr)
			{
				const char *home = getenv("HOME");
				if (home != nullptr)
				{
					std::string path = home + std::string("/bba.pcapng");
					dumpfp = fopen(path.c_str(), "wb");
				}
				if (dumpfp == nullptr)
					return;
			}
			u32 blockType = 0x0A0D0D0A; // Section Header Block
			fwrite(&blockType, sizeof(blockType), 1, dumpfp);
			u32 blockLen = 28;
			fwrite(&blockLen, sizeof(blockLen), 1, dumpfp);
			u32 magic = 0x1A2B3C4D;
			fwrite(&magic, sizeof(magic), 1, dumpfp);
			u32 version = 1; // 1.0
			fwrite(&version, sizeof(version), 1, dumpfp);
			u64 sectionLength = ~0; // unspecified
			fwrite(&sectionLength, sizeof(sectionLength), 1, dumpfp);
			fwrite(&blockLen, sizeof(blockLen), 1, dumpfp);

			blockType = 1; // Interface Description Block
			fwrite(&blockType, sizeof(blockType), 1, dumpfp);
			blockLen = 20;
			fwrite(&blockLen, sizeof(blockLen), 1, dumpfp);
			const u32 linkType = 1; // Ethernet
			fwrite(&linkType, sizeof(linkType), 1, dumpfp);
			const u32 snapLen = 0; // no limit
			fwrite(&snapLen, sizeof(snapLen), 1, dumpfp);
			// TODO options? if name, ip/mac address
			fwrite(&blockLen, sizeof(blockLen), 1, dumpfp);
		}
		const u32 blockType = 6; // Extended Packet Block
		fwrite(&blockType, sizeof(blockType), 1, dumpfp);
		u32 roundedSize = ((size + 3) & ~3) + 32;
		fwrite(&roundedSize, sizeof(roundedSize), 1, dumpfp);
		u32 ifId = 0;
		fwrite(&ifId, sizeof(ifId), 1, dumpfp);
		u64 now = getTimeMs() * 1000;
		fwrite((u32 *)&now + 1, 4, 1, dumpfp);
		fwrite(&now, 4, 1, dumpfp);
		fwrite(&size, sizeof(size), 1, dumpfp);
		fwrite(&size, sizeof(size), 1, dumpfp);
		fwrite(frame, 1, size, dumpfp);
		fwrite(frame, 1, roundedSize - size - 32, dumpfp);
		fwrite(&roundedSize, sizeof(roundedSize), 1, dumpfp);
#endif
	}

	asio::ip::tcp::socket socket;
	std::vector<u8> recvBuffer;
	std::array<u8, 1600> sendBuffer;
	u32 sendBufferIdx = 0;
	bool sending = false;

	FILE *dumpfp = nullptr;
};

class AccessPointFinder
{
public:
	AccessPointFinder(asio::io_context& io_context)
		: io_context(io_context), socket(io_context, asio::ip::udp::endpoint()),
		  timer(io_context)
	{
	}

	template<typename Handler>
	void find(const Handler& handler)
	{
		this->handler = std::function<void(const std::error_code&, const asio::ip::address&, const std::string&)>(handler);
		try {
			asio::ip::udp::resolver resolver(io_context);
			auto it = resolver.resolve("dcnet.flyca.st", std::to_string(PORT));
			if (it.empty()) {
				finish();
				return;
			}
			mainEndpoint = *it.begin();

			std::array<uint8_t, 5> buf;
			memcpy(&buf[0], &MAGIC, sizeof(MAGIC));
			buf[4] = DISCOVER;	// discover access points
			socket.send_to(asio::buffer(buf), mainEndpoint);

			timer.expires_after(asio::chrono::milliseconds(500));
			timer.async_wait([this](const std::error_code& ec)
			{
				if (ec)
					return;
				// Re-ping access points that didn't answer after 500 ms
				for (const auto& ap : accessPoints) {
					if (ap.count == 0)
						sendPing(ap.endpoint);
				}
				timer.expires_after(asio::chrono::milliseconds(500));
				timer.async_wait([this](const std::error_code& ec)
				{
					// 1 sec final timeout
					if (ec)
						return;
					std::error_code err;
					socket.close(err);
					finish();
				});
			});
			receiveAccessPoints();
		} catch (const std::system_error& e) {
			finish(e.code());
		}
	}

private:
	void receiveAccessPoints()
	{
		socket.async_receive_from(asio::buffer(recvbuf), recvEndpoint, [this](const std::error_code& ec, size_t len)
			{
				if (recvEndpoint != mainEndpoint || len < 5
						|| memcmp(&recvbuf[0], &MAGIC, sizeof(MAGIC)) || recvbuf[4] != DISCOVER) {
					// Unexpected or invalid packet
					receiveAccessPoints();
					return;
				}
				const uint8_t *p = &recvbuf[5];
				while (p - &recvbuf[0] < (int)len)
				{
					accessPoints.emplace_back();
					uint32_t addr;
					memcpy(&addr, p, sizeof(uint32_t));
					accessPoints.back().endpoint = asio::ip::udp::endpoint(asio::ip::address_v4(htonl(addr)), PORT);
					p += 4;
					size_t l = *p++;
					accessPoints.back().name = std::string((const char *)p, (const char *)(p + l));
					p += l;
				}
				if (accessPoints.size() > 1)
				{
					// Need to ping
					for (const auto& ap : accessPoints)
						sendPing(ap.endpoint);
				}
				else {
					finish();
				}
			});
	}

	void sendPing(const asio::ip::udp::endpoint& endpoint)
	{
		std::array<uint8_t, 13> buf;
		memcpy(&buf[0], &MAGIC, sizeof(MAGIC));
		buf[4] = PING;
		u64 now = (u64)getTimeMs();
		memcpy(&buf[5], &now, sizeof(u64));
		socket.send_to(asio::buffer(buf), endpoint);
		receivePing();
	}

	void receivePing()
	{
		if (receiving)
			return;
		receiving = true;
		socket.async_receive_from(asio::buffer(recvbuf), recvEndpoint, [this](const std::error_code& ec, size_t len)
			{
				receiving = false;
				if (ec)
				{
					if (ec != asio::error::operation_aborted && ec != asio::error::bad_descriptor)
						INFO_LOG(NETWORK, "receivePing error: %s", ec.message().c_str());
					return;
				}
				if (len != 13 || recvbuf[4] != PONG) {
					receivePing();
					return;
				}
				u64 ts;
				memcpy(&ts, &recvbuf[5], sizeof(ts));
				int ping = getTimeMs() - (time_t)ts;
				for (auto& ap : accessPoints)
				{
					if (ap.endpoint == recvEndpoint)
					{
						ap.ping += ping;
						ap.count++;
						if (ap.count < 3)
							sendPing(ap.endpoint);
						else
							// we have 3 answers from one AP so let's stop here
							finish();
						return;
					}
				}
				receivePing();
			});
	}

	void finish(const std::error_code& ec = {})
	{
		std::error_code e;
		socket.close(e);
		timer.cancel(e);
		if (ec) {
			handler(ec, {}, {});
		}
		else if (accessPoints.empty()) {
			handler({}, mainEndpoint.address(), {});
		}
		else
		{
			int bestPing = 1000000;
			const AccessPoint *bestAP = nullptr;
			for (const AccessPoint& ap : accessPoints)
			{
				if (ap.count == 0) {
					INFO_LOG(NETWORK, "AP %s (%s): no answer", ap.name.c_str(), ap.endpoint.address().to_string().c_str());
					continue;
				}
				const int ping = ap.ping / ap.count;
				INFO_LOG(NETWORK, "AP %s (%s): ping %d ms", ap.name.c_str(), ap.endpoint.address().to_string().c_str(), ping);
				if (ping < bestPing) {
					bestPing = ping;
					bestAP = &ap;
				}
			}
			if (bestAP == nullptr)
				bestAP = &accessPoints[0];
			handler({}, bestAP->endpoint.address(), bestAP->name);
		}
	}

	struct AccessPoint
	{
		asio::ip::udp::endpoint endpoint;
		std::string name;
		int ping = 0;
		int count = 0;
	};

	asio::io_context& io_context;
	asio::ip::udp::socket socket;
	std::array<uint8_t, 512> recvbuf;
	asio::ip::udp::endpoint recvEndpoint;
	asio::ip::udp::endpoint mainEndpoint;
	std::vector<AccessPoint> accessPoints;
	bool receiving = false;
	asio::steady_timer timer;
	std::function<void(const std::error_code&, const asio::ip::address&, const std::string&)> handler;

	static constexpr uint16_t PORT = 7655;
	static constexpr uint32_t MAGIC = 0xDC15C001;
	static constexpr uint8_t PING = 1;
	static constexpr uint8_t PONG = 2;
	static constexpr uint8_t DISCOVER = 3;
};

class DCNetThread
{
public:
	void start()
	{
		if (thread.joinable())
			return;
		io_context = std::make_unique<asio::io_context>();
		thread = std::thread(&DCNetThread::run, this);
	}

	void stop()
	{
		if (!thread.joinable())
			return;
		io_context->stop();
		thread.join();
		pppSocket.reset();
		ethSocket.reset();
		io_context.reset();
		os_notify("DCNet disconnected", 3000);
	}

	void sendModem(u8 v)
	{
		if (io_context == nullptr || pppSocket == nullptr)
			return;
		io_context->post([this, v]() {
			pppSocket->send(v);
		});
	}
	void sendEthFrame(const u8 *frame, u32 len)
	{
		if (io_context != nullptr && ethSocket != nullptr)
		{
			std::vector<u8> vbuf(frame, frame + len);
			io_context->post([this, vbuf]() {
				ethSocket->send(vbuf.data(), vbuf.size());
			});
		}
		else {
			// restart the thread if previously stopped
			start();
		}
	}

private:
	void run();
	void connect(const asio::ip::address& address = {}, const std::string& apname = {});

	std::thread thread;
	std::unique_ptr<asio::io_context> io_context;
	std::unique_ptr<PPPTcpSocket> pppSocket;
	std::unique_ptr<EthSocket> ethSocket;

	static constexpr uint16_t PPP_PORT = 7654;
	static constexpr uint16_t TAP_PORT = 7655;
	friend DCNetService;
};
static DCNetThread thread;

bool DCNetService::start()
{
	emu.setNetworkState(true);
	thread.start();
	return true;
}

void DCNetService::stop() {
	thread.stop();
	emu.setNetworkState(false);
}

void DCNetService::writeModem(u8 b) {
	thread.sendModem(b);
}

int DCNetService::readModem()
{
	if (toModem.empty())
		return -1;
	else
		return toModem.pop();
}

int DCNetService::modemAvailable() {
	return toModem.size();
}

void DCNetService::receiveEthFrame(u8 const *frame, unsigned int len)
{
	/*
    printf("Out frame: dest %02x:%02x:%02x:%02x:%02x:%02x "
           "src %02x:%02x:%02x:%02x:%02x:%02x, ethertype %04x, size %d bytes %s\n",
        frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
        frame[6], frame[7], frame[8], frame[9], frame[10], frame[11],
        *(u16 *)&frame[12], len, EthSocket::Instance == nullptr ? "LOST" : "");
    */
    // Stop DCNet on DHCP Release
	if (len >= 0x11d
			&& *(u16 *)&frame[0xc] == 0x0008		// type: IPv4
			&& frame[0x17] == 0x11					// UDP
			&& ntohs(*(u16 *)&frame[0x22]) == 68	// src port: dhcpc
			&& ntohs(*(u16 *)&frame[0x24]) == 67)	// dest port: dhcps
	{
		const u8 *options = &frame[0x11a];
		while (options - frame < len && *options != 0xff)
		{
			if (*options == 53		// message type
				&& options[2] == 7)	// release
			{
				stop();
				return;
			}
			options += options[1] + 2;
		}
	}
	thread.sendEthFrame(frame, len);
}

void DCNetThread::connect(const asio::ip::address& address, const std::string& apname)
{
	asio::ip::tcp::endpoint endpoint;
	if (address.is_unspecified())
	{
		std::string hostname = "dcnet.flyca.st";
#ifndef LIBRETRO
		hostname = cfgLoadStr("network", "DCNetServer", hostname);
#endif
		std::string port;
		if (config::EmulateBBA)
			port = std::to_string(TAP_PORT);
		else
			port = std::to_string(PPP_PORT);
		asio::ip::tcp::resolver resolver(*io_context);
		asio::error_code ec;
		auto it = resolver.resolve(hostname, port, ec);
		if (ec)
			throw FlycastException(ec.message());
		if (it.empty())
			throw FlycastException("Host not found");
		endpoint = *it.begin();
	}
	else {
		endpoint.address(address);
		endpoint.port(config::EmulateBBA ? TAP_PORT : PPP_PORT);
	}
	if (config::EmulateBBA)
		ethSocket = std::make_unique<EthSocket>(*io_context, endpoint, apname);
	else
		pppSocket = std::make_unique<PPPTcpSocket>(*io_context, endpoint, apname);
}

void DCNetThread::run()
{
	toModem.clear();
	try {
		std::string hostname;
#ifndef LIBRETRO
		hostname = cfgLoadStr("network", "DCNetServer", "");
		if (!hostname.empty())
			connect();
#endif
		AccessPointFinder finder(*io_context);
		if (hostname.empty())
			finder.find([this](const std::error_code& ec,
					const asio::ip::address& address, const std::string& apname)
				{
					if (ec)
						WARN_LOG(NETWORK, "AP discovery failed: %s", ec.message().c_str());
					this->connect(address, apname);
				});

		io_context->run();
	} catch (const FlycastException& e) {
		ERROR_LOG(NETWORK, "DCNet connection error: %s", e.what());
		os_notify("Can't connect to DCNet", 8000, e.what());
	} catch (const std::runtime_error& e) {
		ERROR_LOG(NETWORK, "DCNetThread::run error: %s", e.what());
	}
}

}
