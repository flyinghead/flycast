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

#include <asio.hpp> // Must be included first to avoid winsock issues on Windows
#include "dreamconn.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "oslib/oslib.h"
#include "ui/gui.h"
#include <cfg/option.h>
#include <SDL.h>
#include <iomanip>
#include <sstream>

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
#include <dirent.h>
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


//! DreamConn implementation class
//! This is here mainly so asio.hpp can be included in this source file instead of the header.
class DreamConnImp {
public:
	DreamConnImp(int bus, bool isForPhysicalController) :
		bus(bus),
		isForPhysicalController(isForPhysicalController)
	{}

	~DreamConnImp() = default;

public:
	int bus = -1;
	bool isForPhysicalController;
	bool maple_io_connected = false;
	u8 expansionDevs = 0;
	asio::ip::tcp::iostream iostream;
	std::mutex send_mutex;
};

DreamConn::DreamConn(int bus, bool isForPhysicalController) :
	mImp(std::make_unique<DreamConnImp>(bus, isForPhysicalController)) {
}

DreamConn::~DreamConn() {
	disconnect();
}

bool DreamConn::isForPhysicalController() {
	return mImp->isForPhysicalController;
}

bool DreamConn::send(const MapleMsg& msg) {
	std::lock_guard<std::mutex> lock(mImp->send_mutex); // Ensure thread safety for send operations
	return send_no_lock(msg);
}

bool DreamConn::send_no_lock(const MapleMsg& msg) {
	if (!mImp->maple_io_connected)
		return false;

	auto ec = sendMsg(msg, mImp->iostream);
	if (ec) {
		WARN_LOG(INPUT, "DreamcastController[%d] send failed: %s", mImp->bus, ec.message().c_str());
		disconnect();
		return false;
	}
	return true;
}

bool DreamConn::send(const MapleMsg& txMsg, MapleMsg& rxMsg) {
	std::lock_guard<std::mutex> lock(mImp->send_mutex); // Ensure thread safety for send operations

	if (!send_no_lock(txMsg)) {
		return false;
	}
	return receiveMsg(rxMsg, mImp->iostream);
}

int DreamConn::getBus() const {
	return mImp->bus;
}

bool DreamConn::hasVmu() const {
	return mImp->expansionDevs & 1;
}

bool DreamConn::hasRumble() const {
	return mImp->expansionDevs & 2;
}

void DreamConn::changeBus(int newBus) {
	if (newBus != mImp->bus) {
		// A different TCP port is used depending on the bus. We'll need to disconnect from the current port.
		// The caller will call connect() again if appropriate.
		disconnect();
		mImp->bus = newBus;
	}
}

bool DreamConn::needsRefresh() {
	if (!isConnected())
		return false;

	std::lock_guard<std::mutex> lock(mImp->send_mutex);

	// Check if there is a refresh message waiting in the socket buffer.
	// Avoid reading (consuming) any other kind of message in this context.
	const int REFRESH_MESSAGE_SIZE = 13;
	asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(mImp->iostream.socket());
	if (sock.available() < REFRESH_MESSAGE_SIZE)
		return false;

	char buffer[REFRESH_MESSAGE_SIZE];
	int bytesPeeked = recv(sock.native_handle(), buffer, REFRESH_MESSAGE_SIZE, MSG_PEEK);
	if (bytesPeeked != REFRESH_MESSAGE_SIZE)
		return false;

	MapleMsg message;
	sscanf(buffer, "%hhx %hhx %hhx %hhx", &message.command, &message.destAP, &message.originAP, &message.size);
	if (message.command == 0xff && message.destAP == 0xff && message.originAP == 0xff && message.size == 0xff) {
		// It is a refresh message, so consume it.
		receiveMsg(message, mImp->iostream);

		if (!updateExpansionDevs())
			return false;

		return true;
	}
	return false;
}

// Sends a message to query for expansion devices.
// Disconnects upon failure.
bool DreamConn::updateExpansionDevs() {
	// Now get the controller configuration
	MapleMsg msg;
	msg.command = MDCF_GetCondition;
	msg.destAP = (mImp->bus << 6) | 0x20;
	msg.originAP = mImp->bus << 6;
	msg.setData(MFID_0_Input);

	auto ec = sendMsg(msg, mImp->iostream);
	if (ec)
	{
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", mImp->bus, ec.message().c_str());
		disconnect();
		return false;
	}
	if (!receiveMsg(msg, mImp->iostream)) {
		WARN_LOG(INPUT, "DreamcastController[%d] read timeout", mImp->bus);
		disconnect();
		return false;
	}

	mImp->expansionDevs = msg.originAP & 0x1f;
	config::MapleExpansionDevices[mImp->bus][0] = hasVmu() ? MDT_SegaVMU : MDT_None;
	config::MapleExpansionDevices[mImp->bus][1] = hasRumble() ? MDT_PurupuruPack : MDT_None;
	return true;
}

bool DreamConn::isSocketDisconnected() {
	std::lock_guard<std::mutex> lock(mImp->send_mutex);

	// A socket was disconnected if 'select()' says the socket is ready to read, and a subsequent 'recv()' fails or says 0 bytes available to read.
	auto& sock = static_cast<asio::ip::tcp::socket&>(mImp->iostream.socket());
	auto nativeHandle = sock.native_handle();
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(nativeHandle, &readfds);
	timeval timeout = { 0, 0 };
	// nfds should be set to the highest-numbered file descriptor plus 1.
	// See https://www.man7.org/linux/man-pages/man2/select.2.html
	int nfds = nativeHandle + 1;
	int nReady = select(nfds, &readfds, nullptr, nullptr, &timeout);
	bool socketIsReady = nReady > 0 && FD_ISSET(nativeHandle, &readfds);
	if (!socketIsReady)
		return false;

	char dest;
	int len = recv(nativeHandle, &dest, sizeof(dest), MSG_PEEK);
	return len <= 0;
}

bool DreamConn::isConnected() {
	if (!mImp->maple_io_connected)
		return false;

	if (isSocketDisconnected()) {
		NOTICE_LOG(INPUT, "DreamLink server disconnected bus[%d]", mImp->bus);
		disconnect();
		return false;
	}

	return true;
}

void DreamConn::connect() {
	mImp->maple_io_connected = false;

#if !defined(_WIN32)
	if (isForPhysicalController()) {
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: DreamConn+ / DreamConn S Controller supported on Windows only", bus);
		return;
	}
#endif

	mImp->iostream = asio::ip::tcp::iostream("localhost", std::to_string(BASE_PORT + mImp->bus));
	if (!mImp->iostream) {
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", mImp->bus, mImp->iostream.error().message().c_str());
		disconnect();
		return;
	}
	mImp->iostream.expires_from_now(std::chrono::seconds(1));

	if (!updateExpansionDevs())
		return;

	mImp->iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow

	// Remain connected even if no devices were found, so that connecting a device later will be detected
	NOTICE_LOG(INPUT, "Connected to DreamcastController[%d]: Type:%s, VMU:%d, Rumble Pack:%d", mImp->bus, getName().c_str(), hasVmu(), hasRumble());
	mImp->maple_io_connected = true;
}

void DreamConn::disconnect() {
	// Already disconnected
	if (!mImp->maple_io_connected)
		return;

	mImp->maple_io_connected = false;
	if (mImp->iostream)
		mImp->iostream.close();

	// Notify the user of the disconnect
	NOTICE_LOG(INPUT, "Disconnected from DreamcastController[%d]", mImp->bus);
	char buf[128];
	snprintf(buf, sizeof(buf), "WARNING: DreamLink disconnected from port %c", 'A' + mImp->bus);
	os_notify(buf, 6000);

	tearDownDreamLinkDevices(shared_from_this());
	maple_ReconnectDevices();
}

void DreamConn::gameTermination() {
	// Clear the remote VMU screen
	MapleMsg msg;
	msg.command = MDCF_BlockWrite;
	msg.destAP = (mImp->bus << 6) | 0x20;
	msg.originAP = mImp->bus << 6;

	u32 localData[0x32];
	memset(localData, 0, sizeof(localData));
	localData[0] = MFID_2_LCD;
	msg.setData(localData);

	send(msg);
}
