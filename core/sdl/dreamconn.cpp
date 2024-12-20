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

#if defined(_WIN32) && !defined(TARGET_UWP)
#include "hw/maple/maple_devs.h"
#include <SDL.h>
#include <iomanip>
#include <sstream>

void createDreamConnDevices(std::shared_ptr<DreamConn> dreamconn, bool gameStart);

bool MapleMsg::send(sock_t sock) const
{
	std::ostringstream out;
	out.fill('0');
	out << std::hex << std::uppercase
		<< std::setw(2) << (u32)command << " "
		<< std::setw(2) << (u32)destAP << " "
		<< std::setw(2) << (u32)originAP << " "
		<< std::setw(2) << (u32)size;
	const u32 sz = getDataSize();
	for (u32 i = 0; i < sz; i++)
		out << " " << std::setw(2) << (u32)data[i];
	out << "\r\n";
	std::string s = out.str();
	return ::send(sock, s.c_str(), s.length(), 0) == (int)s.length();
}

bool MapleMsg::receive(sock_t sock)
{
	std::string str(11, ' ');
	if (::recv(sock, (char *)str.data(), str.length(), 0) != (int)str.length())
		return false;
	sscanf(str.c_str(), "%hhx %hhx %hhx %hhx", &command, &destAP, &originAP, &size);
	str = std::string(getDataSize() * 3 + 2, ' ');
	if (::recv(sock, (char *)str.data(), str.length(), 0) != (int)str.length())
		return false;
	for (unsigned i = 0; i < getDataSize(); i++)
		sscanf(&str[i * 3 + 1], "%hhx", &data[i]);
	return true;
}

void DreamConn::connect()
{
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!VALID(sock))
		return;
	set_recv_timeout(sock, 1000);
	sockaddr_in addr {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(BASE_PORT + bus);
	if (::connect(sock, (sockaddr *)&addr, sizeof(addr)) != 0)
	{
		WARN_LOG(INPUT, "DreamConn[%d] connection failed", bus);
		disconnect();
		return;
	}
	// Now get the controller configuration
	MapleMsg msg;
	msg.command = MDCF_GetCondition;
	msg.destAP = (bus << 6) | 0x20;
	msg.originAP = bus << 6;
	msg.setData(MFID_0_Input);
	if (!msg.send(sock))
	{
		WARN_LOG(INPUT, "DreamConn[%d] communication failed", bus);
		disconnect();
		return;
	}
	if (!msg.receive(sock)) {
		WARN_LOG(INPUT, "DreamConn[%d] read timeout", bus);
		disconnect();
		return;
	}
	expansionDevs = msg.originAP & 0x1f;
	NOTICE_LOG(INPUT, "Connected to DreamConn[%d]: VMU:%d, Rumble Pack:%d", bus, hasVmu(), hasRumble());
}

void DreamConn::disconnect()
{
	if (VALID(sock)) {
		NOTICE_LOG(INPUT, "Disconnected from DreamConn[%d]", bus);
		closesocket(sock);
	}
	sock = INVALID_SOCKET;
}

bool DreamConn::send(const MapleMsg& msg)
{
	if (VALID(sock))
		return msg.send(sock);
	else
		return false;
}

bool DreamConnGamepad::isDreamConn(int deviceIndex)
{
	char guid_str[33] {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid_str, sizeof(guid_str));
	NOTICE_LOG(INPUT, "GUID: %s VID:%c%c%c%c PID:%c%c%c%c", guid_str,
			guid_str[10], guid_str[11], guid_str[8], guid_str[9],
			guid_str[18], guid_str[19], guid_str[16], guid_str[17]);
	// DreamConn VID:4457 PID:4443
	return memcmp("5744000043440000", guid_str + 8, 16) == 0;
}

DreamConnGamepad::DreamConnGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick)
{
	EventManager::listen(Event::Start, handleEvent, this);
	EventManager::listen(Event::LoadState, handleEvent, this);
}

DreamConnGamepad::~DreamConnGamepad() {
	EventManager::unlisten(Event::Start, handleEvent, this);
	EventManager::unlisten(Event::LoadState, handleEvent, this);
}

void DreamConnGamepad::set_maple_port(int port)
{
	if (port < 0 || port >= 4) {
		dreamconn.reset();
	}
	else if (dreamconn == nullptr || dreamconn->getBus() != port) {
		dreamconn.reset();
		dreamconn = std::make_shared<DreamConn>(port);
	}
	SDLGamepad::set_maple_port(port);
}

void DreamConnGamepad::handleEvent(Event event, void *arg)
{
	DreamConnGamepad *gamepad = static_cast<DreamConnGamepad*>(arg);
	if (gamepad->dreamconn != nullptr)
		createDreamConnDevices(gamepad->dreamconn, event == Event::Start);
}

#else

void DreamConn::connect() {
}
void DreamConn::disconnect() {
}

bool DreamConnGamepad::isDreamConn(int deviceIndex) {
	return false;
}
DreamConnGamepad::DreamConnGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick) {
}
DreamConnGamepad::~DreamConnGamepad() {
}
void DreamConnGamepad::set_maple_port(int port) {
	SDLGamepad::set_maple_port(port);
}
#endif
