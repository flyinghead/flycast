/*
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
#include <asio.hpp> // Must be included first to avoid winsock issues on Windows
#include "dreampotato.h"
#include "maplelink.h"
#include "emulator.h"
#include "cfg/option.h"
#include <memory>
#include <array>
#include <iomanip>

asio::error_code sendMsg(const MapleMsg& msg, asio::ip::tcp::iostream& stream)
{
	static char buffer[1024 * 3 + 2];
	char *p = buffer;
	p += sprintf(p, "%02X %02X %02X %02X", msg.command, msg.destAP, msg.originAP, msg.size);
	const u32 sz = msg.getDataSize();
	for (u32 i = 0; i < sz; i++)
		p += sprintf(p, " %02X", msg.data[i]);
	strcpy(p, "\r\n");
	p += 2;
	asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(stream.socket());
	asio::error_code ec;
	asio::write(sock, asio::buffer(buffer, p - buffer), ec);
	return ec;
}

bool receiveMsg(MapleMsg& msg, std::istream& stream)
{
	std::string response;
	if (!std::getline(stream, response))
		return false;
	sscanf(response.c_str(), "%hhx %hhx %hhx %hhx", &msg.command, &msg.destAP, &msg.originAP, &msg.size);
	if ((msg.getDataSize() - 1) * 3 + 13 >= response.length())
		return false;
	for (unsigned i = 0; i < msg.getDataSize(); i++)
		sscanf(&response[i * 3 + 12], "%hhx", &msg.data[i]);
	return !stream.fail();
}

namespace dreampotato
{

class DreamPotato : public BaseMapleLink
{
	int bus;
	asio::ip::tcp::iostream iostream;
	bool connected = false;
	static constexpr u16 BASE_PORT = 37393;

public:
	DreamPotato(int bus)
		: BaseMapleLink(true), bus(bus)
	{}

	bool send(const MapleMsg& msg) override;
	bool sendReceive(const MapleMsg& txMsg, MapleMsg& rxMsg) override;

	bool isConnected() override {
		return connected;
	}

	void connect();
	void disconnect();
	void init();
	void term();
};

static std::array<std::shared_ptr<DreamPotato>, 4> Potatoes;

void DreamPotato::init()
{
	connect();
	registerLink(bus, 0);
}

void DreamPotato::term()
{
	disconnect();
	unregisterLink(bus, 0);
}

void DreamPotato::connect()
{
	if (connected)
		return;
	iostream = asio::ip::tcp::iostream("::1", std::to_string(BASE_PORT + bus), asio::ip::resolver_base::flags::numeric_host);
	if (!iostream)
		iostream = asio::ip::tcp::iostream("127.0.0.1", std::to_string(BASE_PORT + bus), asio::ip::resolver_base::flags::numeric_host);
	if (!iostream) {
		WARN_LOG(INPUT, "DreamPotato[%d] connection failed: %s", bus, iostream.error().message().c_str());
		disconnect();
		return;
	}
	connected = true;
	iostream.expires_from_now(std::chrono::milliseconds(500));

	MapleMsg txMsg;
	txMsg.command = MDC_DeviceRequest;
	txMsg.destAP = (bus << 6) | 1;
	txMsg.originAP = bus << 6;
	MapleMsg rxMsg;
	// Note: connected *must* be true or infinite recursion loop
	if (!sendReceive(txMsg, rxMsg))
		// error has been logged and disconnect() called
		return;
	const u32 fnCode = rxMsg.readData<u32>();
	if ((fnCode & MFID_1_Storage) == 0)
	{
		WARN_LOG(INPUT, "Unrecognized response from DreamPotato");
		disconnect();
		return;
	}

	NOTICE_LOG(INPUT, "Connected to DreamPotato[%d]", bus);
}

void DreamPotato::disconnect()
{
	if (!connected)
		return;
	if (storageEnabled())
		disableStorage();
	connected = false;
	iostream.close();
	NOTICE_LOG(INPUT, "Disconnected from DreamPotato[%d]", bus);
}

bool DreamPotato::send(const MapleMsg& msg)
{
	connect();
	if (!connected)
		return false;

	iostream.expires_from_now(std::chrono::milliseconds(100));
	asio::error_code ec = sendMsg(msg, iostream);
	if (ec)
	{
		WARN_LOG(INPUT, "DreamPotato[%d] send failed: %s", bus, ec.message().c_str());
		disconnect();
		return false;
	}
	return true;
}

bool DreamPotato::sendReceive(const MapleMsg& txMsg, MapleMsg& rxMsg)
{
	if (!send(txMsg))
		return false;

	if (!receiveMsg(rxMsg, iostream)) {
		WARN_LOG(INPUT, "DreamPotato[%d] receive failed", bus);
		disconnect();
		return false;
	}
	return true;
}

// Instantiate DreamPotato devices where needed and delete the others
void update()
{
	for (unsigned bus = 0; bus < Potatoes.size(); bus++)
	{
		auto& potato = Potatoes[bus];
		if (config::NetworkExpansionDevices[bus][0] == 1
				&& config::MapleExpansionDevices[bus][0] == MDT_SegaVMU
				&& maple_getPortCount(config::MapleMainDevices[bus]) >= 1)
		{
			if (potato == nullptr) {
				potato = std::make_shared<DreamPotato>(bus);
				potato->init();
			}
		}
		else if (potato != nullptr) {
			potato->term();
			potato = nullptr;
		}
	}
}

void term()
{
	for (unsigned bus = 0; bus < Potatoes.size(); bus++)
	{
		auto& potato = Potatoes[bus];
		if (potato != nullptr) {
			potato->term();
			potato = nullptr;
		}
	}
}

} // namespace dreampotato
