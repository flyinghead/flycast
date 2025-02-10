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

#include <thread>
#include <memory>
#include <array>
#ifndef __ANDROID__
//#define WIRESHARK_DUMP 1
#endif

namespace net::modbba
{

static TsQueue<u8> toModem;
static TsQueue<u8> fromModem;

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
	PPPSocket(asio::io_context& io_context, const typename SocketT::endpoint_type& endpoint)
		: socket(io_context), timer(io_context)
	{
		socket.connect(endpoint);
		os_notify("Connected to DCNet with modem", 5000);
		receive();
		sendIfAny({});
	}

	~PPPSocket() {
		if (dumpfp != nullptr)
			fclose(dumpfp);
	}

private:
	void receive() {
		socket.async_read_some(asio::buffer(recvBuffer),
				std::bind(&PPPSocket::onRecv, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
	void onRecv(asio::error_code ec, size_t len)
	{
		if (ec) {
			ERROR_LOG(NETWORK, "onRecv error: %s", ec.message().c_str());
			return;
		}
		pppdump(recvBuffer.data(), len, false);
		for (size_t i = 0; i < len; i++)
			toModem.push(recvBuffer[i]);
		receive();
	}

	void sendIfAny(const std::error_code& ec)
	{
		if (ec) {
			ERROR_LOG(NETWORK, "sendIfAny timer error: %s", ec.message().c_str());
			return;
		}
		if (!sending)
		{
			for (; !fromModem.empty() && sendBufSize < sendBuffer.size(); sendBufSize++)
			{
				sendBuffer[sendBufSize] = fromModem.pop();
				if (sendBufSize != 0 && sendBuffer[sendBufSize] == 0x7e) {
					sendBufSize++;
					break;
				}
			}
			if ((sendBufSize > 1 && sendBuffer[sendBufSize - 1] == 0x7e)
					|| sendBufSize == sendBuffer.size())
			{
				pppdump(sendBuffer.data(), sendBufSize, true);
				asio::async_write(socket, asio::buffer(sendBuffer, sendBufSize),
						std::bind(&PPPSocket::onSent, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
				sending = true;
			}
		}

		timer.expires_at(timer.expiry() + asio::chrono::milliseconds(5));
		timer.async_wait(std::bind(&PPPSocket::sendIfAny, this, asio::placeholders::error));
	}

	void onSent(asio::error_code ec, size_t len)
	{
		sending = false;
		sendBufSize = 0;
		if (ec) {
			ERROR_LOG(NETWORK, "onRecv error: %s", ec.message().c_str());
			return;
		}
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
	asio::steady_timer timer;
	std::array<u8, 1542> recvBuffer;
	std::array<u8, 1542> sendBuffer;
	u32 sendBufSize = 0;
	bool sending = false;

	FILE *dumpfp = nullptr;
	u64 dump_last_time_ms;
};

class EthSocket
{
public:
	EthSocket(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint)
		: socket(io_context)
	{
		socket.connect(endpoint);
		os_notify("Connected to DCNet with ethernet", 5000);
		receive();
		u8 prolog[] = { 'D', 'C', 'N', 'E', 'T', 1 };
		send(prolog, sizeof(prolog));
		Instance = this;
	}

	~EthSocket() {
		if (dumpfp != nullptr)
			fclose(dumpfp);
		Instance = nullptr;
	}

	void send(const u8 *frame, u32 size)
	{
		if (sendBufferIdx + size >= sendBuffer.size()) {
			WARN_LOG(NETWORK, "Dropped out frame (buffer:%d + %d bytes). Increase send buffer size\n", sendBufferIdx, size);
			return;
		}
		if (Instance != nullptr)
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

	void receive() {
		asio::async_read_until(socket, asio::dynamic_vector_buffer(recvBuffer), packetMatcher,
				std::bind(&EthSocket::onRecv, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
	void onRecv(asio::error_code ec, size_t len)
	{
		if (ec || len == 0)
		{
			if (ec)
				ERROR_LOG(NETWORK, "onRecv error: %s", ec.message().c_str());
			socket.close(ec);
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
	}

	void doSend()
	{
		if (sending)
			return;
		sending = true;
		asio::async_write(socket, asio::buffer(sendBuffer, sendBufferIdx),
				std::bind(&EthSocket::onSent, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
	}

	void onSent(asio::error_code ec, size_t len)
	{
		sending = false;
		if (ec) {
			ERROR_LOG(NETWORK, "onRecv error: %s", ec.message().c_str());
			socket.close(ec);
			return;
		}
		sendBufferIdx -= len;
		if (sendBufferIdx != 0) {
			memmove(sendBuffer.data(), sendBuffer.data() + len, sendBufferIdx);
			doSend();
		}
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

public:
	static EthSocket *Instance;
};
EthSocket *EthSocket::Instance;

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
		io_context.reset();
		os_notify("DCNet disconnected", 3000);
	}

private:
	void run();

	std::thread thread;
	std::unique_ptr<asio::io_context> io_context;
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
	fromModem.push(b);
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
		while (options - frame < len && *options != 0xff) {
			if (*options == 53		// message type
				&& options[2] == 7)	// release
			{
				stop();
				return;
			}
			options += options[1] + 2;
		}
	}
    if (EthSocket::Instance != nullptr)
    {
    	std::vector<u8> vbuf(frame, frame + len);
    	thread.io_context->post([vbuf]() {
    		EthSocket::Instance->send(vbuf.data(), vbuf.size());
    	});
    }
    else {
    	// restart the thread if previously stopped
    	start();
    }
}

void DCNetThread::run()
{
	try {
		std::string port;
		if (config::EmulateBBA)
			port = "7655";
		else
			port = "7654";
		asio::ip::tcp::resolver resolver(*io_context);
		auto it = resolver.resolve("dcnet.flyca.st", port);
		if (it.empty())
			throw std::runtime_error("Can't find dcnet host");
		asio::ip::tcp::endpoint endpoint = *it.begin();

		if (config::EmulateBBA) {
			EthSocket socket(*io_context, endpoint);
			io_context->run();
		}
		else {
			PPPSocket<asio::ip::tcp::socket> socket(*io_context, endpoint);
			io_context->run();
		}
	} catch (const std::runtime_error& e) {
		ERROR_LOG(NETWORK, "DCNetThread::run error: %s", e.what());
	}
}

}
