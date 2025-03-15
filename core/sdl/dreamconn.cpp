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

#ifdef USE_DREAMCASTCONTROLLER
#include "hw/maple/maple_devs.h"
#include "ui/gui.h"
#include <cfg/option.h>
#include <SDL.h>
#include <asio.hpp>
#include <iomanip>
#include <sstream>

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
#include <dirent.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <setupapi.h>
#endif

static asio::error_code sendMsg(const MapleMsg& msg, asio::ip::tcp::iostream& stream)
{
	std::ostringstream s;
	s.fill('0');
	s << std::hex << std::uppercase
		<< std::setw(2) << (u32)msg.command << " "
		<< std::setw(2) << (u32)msg.destAP << " "
		<< std::setw(2) << (u32)msg.originAP << " "
		<< std::setw(2) << (u32)msg.size;
	const u32 sz = msg.getDataSize();
	for (u32 i = 0; i < sz; i++)
		s << " " << std::setw(2) << (u32)msg.data[i];
	s << "\r\n";

	asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(stream.socket());
	asio::error_code ec;
	asio::write(sock, asio::buffer(s.str()), ec);
	return ec;
}

static bool receiveMsg(MapleMsg& msg, std::istream& stream)
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





DreamConn::DreamConn(int bus) : bus(bus) {
}

DreamConn::~DreamConn() {
	disconnect();
}

bool DreamConn::send(const MapleMsg& msg) {
	std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations

	asio::error_code ec;

	if (maple_io_connected)
		ec = sendMsg(msg, iostream);
	else
		return false;
	if (ec) {
		maple_io_connected = false;
		WARN_LOG(INPUT, "DreamcastController[%d] send failed: %s", bus, ec.message().c_str());
		disconnect();
		return false;
	}
	return true;
}

bool DreamConn::send(const MapleMsg& txMsg, MapleMsg& rxMsg) {
	std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations

	if (!send(txMsg)) {
		return false;
	}
	return receiveMsg(rxMsg, iostream);
}

void DreamConn::changeBus(int newBus) {
	bus = newBus;
}

void DreamConn::connect() {
	maple_io_connected = false;

	asio::error_code ec;

#if !defined(_WIN32)
	WARN_LOG(INPUT, "DreamcastController[%d] connection failed: DreamConn+ / DreamConn S Controller supported on Windows only", bus);
	return;
#endif

	iostream = asio::ip::tcp::iostream("localhost", std::to_string(BASE_PORT + bus));
	if (!iostream) {
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, iostream.error().message().c_str());
		disconnect();
		return;
	}
	iostream.expires_from_now(std::chrono::seconds(1));


	// Now get the controller configuration
	MapleMsg msg;
	msg.command = MDCF_GetCondition;
	msg.destAP = (bus << 6) | 0x20;
	msg.originAP = bus << 6;
	msg.setData(MFID_0_Input);

	ec = sendMsg(msg, iostream);
	if (ec)
	{
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, ec.message().c_str());
		disconnect();
		return;
	}
	if (!receiveMsg(msg, iostream)) {
		WARN_LOG(INPUT, "DreamcastController[%d] read timeout", bus);
		disconnect();
		return;
	}

	iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow

	expansionDevs = msg.originAP & 0x1f;

	config::MapleExpansionDevices[bus][0] = hasVmu() ? MDT_SegaVMU : MDT_None;
	config::MapleExpansionDevices[bus][1] = hasRumble() ? MDT_PurupuruPack : MDT_None;

	if (hasVmu() || hasRumble())
	{
		NOTICE_LOG(INPUT, "Connected to DreamcastController[%d]: Type:%s, VMU:%d, Rumble Pack:%d", bus, getName().c_str(), hasVmu(), hasRumble());
		maple_io_connected = true;
	}
	else
	{
		WARN_LOG(INPUT, "DreamcastController[%d] connection: no VMU or Rumble Pack connected", bus);
		disconnect();
		return;
	}
}

void DreamConn::disconnect() {
	if (iostream)
		iostream.close();

	maple_io_connected = false;

	NOTICE_LOG(INPUT, "Disconnected from DreamcastController[%d]", bus);
}

#endif
