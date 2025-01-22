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
#include <asio.hpp>
#include "types.h"
#include "netservice.h"
#include "util/tsqueue.h"
#include "oslib/oslib.h"
#include "emulator.h"
#include <thread>
#include <memory>
#include <array>
#ifndef __ANDROID__
//#define DEBUG_PPP 1
#endif
#ifdef DEBUG_PPP
#include <sys/time.h>
#endif

namespace net::modbba
{

static TsQueue<u8> in_buffer;
static TsQueue<u8> out_buffer;

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

class Socket
{
public:
	Socket(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint)
		: socket(io_context), timer(io_context)
	{
		socket.connect(endpoint);
		os_notify("Connected to DCNet cloud", 5000);
		receive();
		sendIfAny({});
	}

	~Socket() {
		if (dumpfp != nullptr)
			fclose(dumpfp);
	}

private:
	void receive() {
		socket.async_read_some(asio::buffer(recvBuffer),
				std::bind(&Socket::onRecv, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
	void onRecv(asio::error_code ec, size_t len)
	{
		if (ec) {
			ERROR_LOG(NETWORK, "onRecv error: %s", ec.message().c_str());
			return;
		}
		pppdump(recvBuffer.data(), len, false);
		for (size_t i = 0; i < len; i++)
			in_buffer.push(recvBuffer[i]);
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
			for (; !out_buffer.empty() && sendBufSize < sendBuffer.size(); sendBufSize++)
			{
				sendBuffer[sendBufSize] = out_buffer.pop();
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
						std::bind(&Socket::onSent, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
				sending = true;
			}
		}

		timer.expires_at(timer.expiry() + asio::chrono::milliseconds(5));
		timer.async_wait(std::bind(&Socket::sendIfAny, this, asio::placeholders::error));
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
#ifdef DEBUG_PPP
		if (!len)
			return;
		const auto& timems = []() -> time_t {
			timeval tv;
			gettimeofday(&tv, nullptr);
			return tv.tv_sec * 1000 + tv.tv_usec / 1000;
		};
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
	        dump_last_time_ms = timems();
		}

		u32 delta = (timems() - dump_last_time_ms) / 100;
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
		dump_last_time_ms = timems();

		fputc(egress ? 1 : 2, dumpfp);			// Sent/received data

		uint16_t slen = htons(len);
		fwrite(&slen, 2, 1, dumpfp);

		fwrite(buf, 1, len, dumpfp);
#endif
	}

	asio::ip::tcp::socket socket;
	asio::steady_timer timer;
	std::array<u8, 1500> recvBuffer;
	std::array<u8, 1500> sendBuffer;
	u32 sendBufSize = 0;
	bool sending = false;

	FILE *dumpfp = nullptr;
	time_t dump_last_time_ms;
};

class DCNetThread
{
public:
	void start()
	{
		verify(!thread.joinable());
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
	}

private:
	void run();

	std::thread thread;
	std::unique_ptr<asio::io_context> io_context;
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
	out_buffer.push(b);
}

int DCNetService::readModem()
{
	if (in_buffer.empty())
		return -1;
	else
		return in_buffer.pop();
}

int DCNetService::modemAvailable() {
	return in_buffer.size();
}

void DCNetService::receiveEthFrame(unsigned char const*, unsigned int) {
	// TODO
}


void DCNetThread::run()
{
	try {
		asio::ip::tcp::resolver resolver(*io_context);
		auto it = resolver.resolve("dcnet.flyca.st", "7654");
		if (it.empty())
			throw std::runtime_error("Can't find dcnet host");
		asio::ip::tcp::endpoint endpoint = *it.begin();

		Socket socket(*io_context, endpoint);
		io_context->run();
	} catch (const std::runtime_error& e) {
		ERROR_LOG(NETWORK, "DCNetThread::run error: %s", e.what());
	}
}

}
