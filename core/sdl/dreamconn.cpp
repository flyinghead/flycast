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
#include <cfg/option.h>
#include <SDL.h>
#include <iomanip>

void createDreamConnDevices(std::shared_ptr<DreamConn> dreamconn, bool gameStart);

bool MapleMsg::send(std::ostream& stream) const
{
	stream.fill('0');
	stream << std::hex << std::uppercase
		<< std::setw(2) << (u32)command << " "
		<< std::setw(2) << (u32)destAP << " "
		<< std::setw(2) << (u32)originAP << " "
		<< std::setw(2) << (u32)size;
	const u32 sz = getDataSize();
	for (u32 i = 0; i < sz; i++)
		stream << " " << std::setw(2) << (u32)data[i];
	stream << "\r\n";
	return !stream.fail();
}

bool MapleMsg::receive(std::istream& stream)
{
	std::string response;
	if (!std::getline(stream, response))
		return false;
	sscanf(response.c_str(), "%hhx %hhx %hhx %hhx", &command, &destAP, &originAP, &size);
	if ((getDataSize() - 1) * 3 + 13 >= response.length())
		return false;
	for (unsigned i = 0; i < getDataSize(); i++)
		sscanf(&response[i * 3 + 12], "%hhx", &data[i]);
	return !stream.fail();
}

void DreamConn::connect()
{
	iostream = asio::ip::tcp::iostream("localhost", std::to_string(BASE_PORT + bus));
	if (!iostream) {
		WARN_LOG(INPUT, "DreamConn[%d] connection failed: %s", bus, iostream.error().message().c_str());
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
	if (!msg.send(iostream))
	{
		WARN_LOG(INPUT, "DreamConn[%d] communication failed", bus);
		disconnect();
		return;
	}
	if (!msg.receive(iostream)) {
		WARN_LOG(INPUT, "DreamConn[%d] read timeout", bus);
		disconnect();
		return;
	}
	iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow
	expansionDevs = msg.originAP & 0x1f;
	NOTICE_LOG(INPUT, "Connected to DreamConn[%d]: VMU:%d, Rumble Pack:%d", bus, hasVmu(), hasRumble());
	config::MapleExpansionDevices[bus][0] = hasVmu() ? MDT_SegaVMU : MDT_None;
	config::MapleExpansionDevices[bus][1] = hasRumble() ? MDT_PurupuruPack : MDT_None;
}

void DreamConn::disconnect()
{
	if (iostream) {
		iostream.close();
		NOTICE_LOG(INPUT, "Disconnected from DreamConn[%d]", bus);
	}
}

bool DreamConn::send(const MapleMsg& msg)
{
	if (!iostream)
		return false;
	if (!msg.send(iostream)) {
		WARN_LOG(INPUT, "DreamConn[%d] send failed: %s", bus, iostream.error().message().c_str());
		return false;
	}
	return true;
}

bool DreamConnGamepad::isDreamConn(int deviceIndex)
{
	char guid_str[33] {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid_str, sizeof(guid_str));
	INFO_LOG(INPUT, "GUID: %s VID:%c%c%c%c PID:%c%c%c%c", guid_str,
			guid_str[10], guid_str[11], guid_str[8], guid_str[9],
			guid_str[18], guid_str[19], guid_str[16], guid_str[17]);
	// DreamConn VID:4457 PID:4443
	return memcmp("5744000043440000", guid_str + 8, 16) == 0;
}

DreamConnGamepad::DreamConnGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick)
{
	_name = "DreamConn+ Controller";
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
