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
#include "hw/maple/maple_if.h"
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
#include <oslib/oslib.h>

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





DreamConn::DreamConn(int bus, bool isForPhysicalController) : bus(bus), _isForPhysicalController(isForPhysicalController) {
}

DreamConn::~DreamConn() {
	disconnect();
}

bool DreamConn::isForPhysicalController() {
	return _isForPhysicalController;
}

bool DreamConn::send(const MapleMsg& msg) {
	std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations
	return send_no_lock(msg);
}

bool DreamConn::send_no_lock(const MapleMsg& msg) {
	if (!maple_io_connected)
		return false;

	auto ec = sendMsg(msg, iostream);
	if (ec) {
		WARN_LOG(INPUT, "DreamcastController[%d] send failed: %s", bus, ec.message().c_str());
		disconnect();
		return false;
	}
	return true;
}

bool DreamConn::send(const MapleMsg& txMsg, MapleMsg& rxMsg) {
	std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations

	if (!send_no_lock(txMsg)) {
		return false;
	}
	return receiveMsg(rxMsg, iostream);
}

void DreamConn::changeBus(int newBus) {
	bus = newBus;
}

void DreamConn::refreshIfNeeded() {
	if (!isConnected())
		return;

	std::lock_guard<std::mutex> lock(send_mutex);

	// Check if there is a refresh message waiting in the socket buffer.
	// Avoid reading (consuming) any other kind of message in this context.
	const int REFRESH_MESSAGE_SIZE = 13;
	asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(iostream.socket());
	if (sock.available() < REFRESH_MESSAGE_SIZE)
		return;

	char buffer[REFRESH_MESSAGE_SIZE];
	int bytesPeeked = recv(sock.native_handle(), buffer, REFRESH_MESSAGE_SIZE, MSG_PEEK);
	if (bytesPeeked != REFRESH_MESSAGE_SIZE)
		return;

	MapleMsg message;
	sscanf(buffer, "%hhx %hhx %hhx %hhx", &message.command, &message.destAP, &message.originAP, &message.size);
	if (message.command == 0xff && message.destAP == 0xff && message.originAP == 0xff && message.size == 0xff) {
		// It is a refresh message, so consume it.
		receiveMsg(message, iostream);

		if (!updateExpansionDevs())
			return;

		NOTICE_LOG(INPUT, "Refreshing DreamLink devices bus[%d]: Type:%s, VMU:%d, Rumble Pack:%d", bus, getName().c_str(), hasVmu(), hasRumble());
		dreamLinkNeedsRefresh[bus] = true;
		tearDownDreamLinkDevices(shared_from_this());
		maple_ReconnectDevices();
	}
}

bool DreamConn::updateExpansionDevs() {
	// Now get the controller configuration
	MapleMsg msg;
	msg.command = MDCF_GetCondition;
	msg.destAP = (bus << 6) | 0x20;
	msg.originAP = bus << 6;
	msg.setData(MFID_0_Input);

	auto ec = sendMsg(msg, iostream);
	if (ec)
	{
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, ec.message().c_str());
		disconnect();
		return false;
	}
	if (!receiveMsg(msg, iostream)) {
		WARN_LOG(INPUT, "DreamcastController[%d] read timeout", bus);
		disconnect();
		return false;
	}

	expansionDevs = msg.originAP & 0x1f;
	config::MapleExpansionDevices[bus][0] = hasVmu() ? MDT_SegaVMU : MDT_None;
	config::MapleExpansionDevices[bus][1] = hasRumble() ? MDT_PurupuruPack : MDT_None;
	return true;
}

static bool isSocketDisconnected(asio::ip::tcp::socket& sock, int available) {
	// Socket is disconnected if 0 bytes are available to read and 'select' considers the socket ready to read
	if (sock.available() != 0)
		return false;

	auto nativeHandle = sock.native_handle();
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(nativeHandle, &readfds);
	timeval timeout = { 0, 0 };
	// nfds should be set to the highest-numbered file descriptor plus 1.
	// See https://www.man7.org/linux/man-pages/man2/select.2.html
	int nfds = nativeHandle + 1;
	int nReady = select(nfds, &readfds, nullptr, nullptr, &timeout);
	return nReady > 0 && FD_ISSET(nativeHandle, &readfds);
}

bool DreamConn::isConnected() {
	if (!maple_io_connected)
		return false;

	auto& sock = static_cast<asio::ip::tcp::socket&>(iostream.socket());
	int available = sock.available();
	if (isSocketDisconnected(sock, available)) {
		NOTICE_LOG(INPUT, "DreamLink server disconnected bus[%d]", bus);
		disconnect();
		return false;
	}

	return true;
}

void DreamConn::connect() {
	maple_io_connected = false;

#if !defined(_WIN32)
	if (isForPhysicalController()) {
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: DreamConn+ / DreamConn S Controller supported on Windows only", bus);
		return;
	}
#endif

	iostream = asio::ip::tcp::iostream("localhost", std::to_string(BASE_PORT + bus));
	if (!iostream) {
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, iostream.error().message().c_str());
		disconnect();
		return;
	}
	iostream.expires_from_now(std::chrono::seconds(1));

	if (!updateExpansionDevs())
		return;

	iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow

	// Remain connected even if no devices were found, so that connecting a device later will be detected
	NOTICE_LOG(INPUT, "Connected to DreamcastController[%d]: Type:%s, VMU:%d, Rumble Pack:%d", bus, getName().c_str(), hasVmu(), hasRumble());
	maple_io_connected = true;
}

void DreamConn::disconnect() {
	// Already disconnected
	if (!maple_io_connected)
		return;

	maple_io_connected = false;
	if (iostream)
		iostream.close();

	// Notify the user of the disconnect
	NOTICE_LOG(INPUT, "Disconnected from DreamcastController[%d]", bus);
	char buf[128];
	snprintf(buf, sizeof(buf), "WARNING: DreamLink disconnected from port %c", 'A' + bus);
	os_notify(buf, 6000);

	tearDownDreamLinkDevices(shared_from_this());
	maple_ReconnectDevices();
}

#endif
