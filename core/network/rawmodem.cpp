/*
	Copyright 2026 flyinghead

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
#include "rawmodem.h"
#include "emulator.h"
#include "util/tsqueue.h"
#include "oslib/i18n.h"
#include "oslib/oslib.h"
#include "cfg/cfg.h"

// Inspired by serialbridge.cpp by awarmplace (https://github.com/awarmplace)

namespace net::modbba
{

static TsQueue<u8> toModem;

class GameSocket
{
public:
	GameSocket(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint,
			const std::string& endpointName = "")
		: socket(io_context)
	{
		asio::error_code ec;
		socket.connect(endpoint, ec);
		if (ec)
			throw FlycastException(ec.message());
		os_notify(i18n::T("Connected to game server"), 5000, endpointName.c_str());
		receive();
	}

	void send(u8 b)
	{
		if (sendBufSize == sendBuffer.size()) {
			WARN_LOG(NETWORK, "Output buffer overflow");
			return;
		}
		sendBuffer[sendBufSize++] = b;
		doSend();
	}

protected:
	void receive()
	{
		socket.async_read_some(asio::buffer(recvBuffer),
			[this](const std::error_code& ec, size_t len)
			{
				if (ec || len == 0)
				{
					if (ec && ec != asio::error::eof)
						ERROR_LOG(NETWORK, "Receive error: %s", ec.message().c_str());
					close();
					return;
				}
				for (size_t i = 0; i < len; i++)
					toModem.push(recvBuffer[i]);
				receive();
			});
	}

	void doSend()
	{
		if (sending)
			return;
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

	asio::ip::tcp::socket socket;
	std::array<u8, 1542> recvBuffer;
	std::array<u8, 1542> sendBuffer;
	u32 sendBufSize = 0;
	bool sending = false;
};

class RawModemThread
{
public:
	void start()
	{
		if (thread.joinable())
			return;
		io_context = std::make_unique<asio::io_context>();
		thread = std::thread(&RawModemThread::run, this);
	}

	void stop()
	{
		if (!thread.joinable())
			return;
		io_context->stop();
		thread.join();
		gameSocket.reset();
		io_context.reset();
		os_notify(i18n::T("Network disconnected"), 3000);
	}

	void sendModem(u8 v)
	{
		if (io_context == nullptr || gameSocket == nullptr)
			return;
		io_context->post([this, v]() {
			gameSocket->send(v);
		});
	}

private:
	void run();
	void connect(const std::string& hostname);

	std::thread thread;
	std::unique_ptr<asio::io_context> io_context;
	std::unique_ptr<GameSocket> gameSocket;

	static constexpr uint16_t IP_PORT = 7657;
	friend class RawModemService;
};
static RawModemThread thread;

void RawModemThread::run()
{
	toModem.clear();
	try {
#ifndef LIBRETRO
		connect(config::loadStr("network", "DCNetServer", "dcnet.flyca.st"));
#else
		connect("dcnet.flyca.st");
#endif
		io_context->run();
	} catch (const FlycastException& e) {
		ERROR_LOG(NETWORK, "RawModem connection error: %s", e.what());
		os_notify(i18n::T("Can't connect to game server"), 8000, e.what());
	} catch (const std::runtime_error& e) {
		ERROR_LOG(NETWORK, "RawModemThread::run error: %s", e.what());
	}
}

void RawModemThread::connect(const std::string& hostname)
{
	asio::ip::tcp::resolver resolver(*io_context);
	asio::error_code ec;
	auto it = resolver.resolve(hostname, std::to_string(IP_PORT), ec);
	if (ec)
		throw FlycastException(ec.message());
	if (it.empty())
		throw FlycastException(i18n::Ts("Host not found"));
	asio::ip::tcp::endpoint endpoint = *it.begin();
	gameSocket = std::make_unique<GameSocket>(*io_context, endpoint, hostname);
}

bool RawModemService::start()
{
	emu.setNetworkState(true);
	thread.start();
	return true;
}

void RawModemService::stop()
{
	thread.stop();
	emu.setNetworkState(false);
}

void RawModemService::writeModem(u8 b) {
	thread.sendModem(b);
}

int RawModemService::readModem()
{
	if (toModem.empty())
		return -1;
	else
		return toModem.pop();
}

int RawModemService::modemAvailable() {
	return toModem.size();
}

}	// namespace net::modbba
