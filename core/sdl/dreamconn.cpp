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

#ifdef USE_DREAMCONN
#include "hw/maple/maple_devs.h"
#include "ui/gui.h"
#include <cfg/option.h>
#include <SDL.h>
#include <asio.hpp>
#include <iomanip>
#include <sstream>

void createDreamConnDevices(std::shared_ptr<DreamConn> dreamconn, bool gameStart);

static bool sendMsg(const MapleMsg& msg, asio::ip::tcp::iostream& stream)
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
	return !ec;
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
	if (!sendMsg(msg, iostream))
	{
		WARN_LOG(INPUT, "DreamConn[%d] communication failed", bus);
		disconnect();
		return;
	}
	if (!receiveMsg(msg, iostream)) {
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
	if (!sendMsg(msg, iostream)) {
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
	EventManager::listen(EmuEvent::Start, handleEvent, this);
	EventManager::listen(EmuEvent::LoadState, handleEvent, this);
}

DreamConnGamepad::~DreamConnGamepad() {
	EventManager::unlisten(EmuEvent::Start, handleEvent, this);
	EventManager::unlisten(EmuEvent::LoadState, handleEvent, this);
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

void DreamConnGamepad::handleEvent(EmuEvent event, void *arg)
{
	DreamConnGamepad *gamepad = static_cast<DreamConnGamepad*>(arg);
	if (gamepad->dreamconn != nullptr)
		createDreamConnDevices(gamepad->dreamconn, event == EmuEvent::Start);
}

bool DreamConnGamepad::gamepad_btn_input(u32 code, bool pressed)
{
	if (!is_detecting_input() && input_mapper)
	{
		DreamcastKey key = input_mapper->get_button_id(0, code);
		if (key == DC_BTN_START) {
			startPressed = pressed;
			checkKeyCombo();
		}
	}
	else {
		startPressed = false;
	}
	return SDLGamepad::gamepad_btn_input(code, pressed);
}

bool DreamConnGamepad::gamepad_axis_input(u32 code, int value)
{
	if (!is_detecting_input())
	{
		if (code == leftTrigger) {
			ltrigPressed = value > 0;
			checkKeyCombo();
		}
		else if (code == rightTrigger) {
			rtrigPressed = value > 0;
			checkKeyCombo();
		}
	}
	else {
		ltrigPressed = false;
		rtrigPressed = false;
	}
	return SDLGamepad::gamepad_axis_input(code, value);
}

void DreamConnGamepad::checkKeyCombo() {
	if (ltrigPressed && rtrigPressed && startPressed)
		gui_open_settings();
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
bool DreamConnGamepad::gamepad_btn_input(u32 code, bool pressed) {
	return SDLGamepad::gamepad_btn_input(code, pressed);
}
bool DreamConnGamepad::gamepad_axis_input(u32 code, int value) {
	return SDLGamepad::gamepad_axis_input(code, value);
}
#endif
