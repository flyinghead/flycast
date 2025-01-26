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

void createDreamConnDevices(std::shared_ptr<DreamConn> dreamconn, bool gameStart);

static asio::error_code sendMsg(const MapleMsg& msg, asio::ip::tcp::iostream& stream, asio::io_context& io_context, asio::serial_port& serial_handler, int dreamcastControllerType)
{
	std::ostringstream s;
	s.fill('0');
	if (dreamcastControllerType == TYPE_DREAMCASTCONTROLLERUSB)
	{
		// Messages to Dreamcast Controller USB need to be prefixed to trigger the correct parser
		s << "X ";
	}
		
	s << std::hex << std::uppercase
		<< std::setw(2) << (u32)msg.command << " "
		<< std::setw(2) << (u32)msg.destAP << " "
		<< std::setw(2) << (u32)msg.originAP << " "
		<< std::setw(2) << (u32)msg.size;
	const u32 sz = msg.getDataSize();
	for (u32 i = 0; i < sz; i++)
		s << " " << std::setw(2) << (u32)msg.data[i];
	s << "\r\n";
	
	asio::error_code ec;
	
	if (dreamcastControllerType == TYPE_DREAMCONN)
	{
		if (!stream)
			return asio::error::not_connected;
		asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(stream.socket());
		asio::write(sock, asio::buffer(s.str()), ec);
	}
	else if (dreamcastControllerType == TYPE_DREAMCASTCONTROLLERUSB)
	{
		io_context.run();
		io_context.reset();
		
		if (!serial_handler.is_open())
			return asio::error::not_connected;
		asio::async_write(serial_handler, asio::buffer(s.str()), asio::transfer_exactly(s.str().size()), [ &serial_handler](const asio::error_code& error, size_t bytes_transferred)
		{
			if (error) {
				serial_handler.cancel();
			}
		});
	}
	
	return ec;
}

static bool receiveMsg(MapleMsg& msg, std::istream& stream, asio::serial_port& serial_handler, int dreamcastControllerType)
{
	std::string response;
	
	if (dreamcastControllerType == TYPE_DREAMCONN)
	{
		if (!std::getline(stream, response))
			return false;
		sscanf(response.c_str(), "%hhx %hhx %hhx %hhx", &msg.command, &msg.destAP, &msg.originAP, &msg.size);
		if ((msg.getDataSize() - 1) * 3 + 13 >= response.length())
			return false;
		for (unsigned i = 0; i < msg.getDataSize(); i++)
			sscanf(&response[i * 3 + 12], "%hhx", &msg.data[i]);
		return !stream.fail();
	}
	else if (dreamcastControllerType == TYPE_DREAMCASTCONTROLLERUSB)
	{
		asio::error_code ec;
		
		char c;
		for (int i = 0; i < 2; ++i)
		{
			// discard the first message as we are interested in the second only which returns the controller configuration
			response = "";
			while (serial_handler.read_some(asio::buffer(&c, 1), ec) > 0)
			{
				if (!serial_handler.is_open())
					return false;
				if (c == '\n')
					break;
				response += c;
			}
			response.pop_back();
		}
		
		sscanf(response.c_str(), "%hhx %hhx %hhx %hhx", &msg.command, &msg.destAP, &msg.originAP, &msg.size);
		
		if (!ec && serial_handler.is_open())
			return true;
		else
			return false;
	}
	
	return false;
}

static std::string getFirstSerialDevice() {
	
	// On Windows, we get the first serial device matching our VID/PID
#if defined(_WIN32)
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
	if (deviceInfoSet == INVALID_HANDLE_VALUE) {
		return "";
	}

	SP_DEVINFO_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
		DWORD dataType, bufferSize = 0;
		SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, &dataType, NULL, 0, &bufferSize);

		if (bufferSize > 0) {
			std::vector<char> buffer(bufferSize);
			if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, &dataType, (PBYTE)buffer.data(), bufferSize, NULL)) {
				std::string hardwareId(buffer.begin(), buffer.end());
				if (hardwareId.find("VID_1209") != std::string::npos && hardwareId.find("PID_2F07") != std::string::npos) {
					HKEY deviceKey = SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
					if (deviceKey != INVALID_HANDLE_VALUE) {
						char portName[256];
						DWORD portNameSize = sizeof(portName);
						if (RegQueryValueEx(deviceKey, "PortName", NULL, NULL, (LPBYTE)portName, &portNameSize) == ERROR_SUCCESS) {
							RegCloseKey(deviceKey);
							SetupDiDestroyDeviceInfoList(deviceInfoSet);
							return std::string(portName);
						}
						RegCloseKey(deviceKey);
					}
				}
			}
		}
	}
	
	SetupDiDestroyDeviceInfoList(deviceInfoSet);
	return "";
#endif
	
	// On MacOS/Linux, we get the first serial device matching the device prefix
	std::string device_prefix = "";
#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
	
#if defined(__linux__)
	device_prefix = "ttyACM";
#elif (defined(__APPLE__) && defined(TARGET_OS_MAC))
	device_prefix = "tty.usbmodem";
#endif
	
	std::string path = "/dev/";
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir(path.c_str())) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			std::string device = ent->d_name;
			if (device.find(device_prefix) != std::string::npos) {
				closedir(dir);
				return path + device;
			}
		}
		closedir(dir);
	}
	return "";
#endif
}

void DreamConn::connect()
{
	maple_io_connected = false;
	
	asio::error_code ec;
	
	switch (dreamcastControllerType) {
		case TYPE_DREAMCONN:
		{
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
			break;
		}
		case TYPE_DREAMCASTCONTROLLERUSB:
		{
			// the serial port isn't ready at this point, so we need to sleep briefly
			// we probably should have a better way to handle this
#if defined(_WIN32)
			Sleep(500);
#elif defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
			usleep(500000);
#endif

			serial_handler = asio::serial_port(io_context);
			
			std::string serial_device = "";
			
			// use user-configured serial device if available, fallback to first available
			if (cfgLoadStr("input", "DreamcastControllerUsbSerialDevice", "default") != "default") {
				serial_device = cfgLoadStr("input", "DreamcastControllerUsbSerialDevice", "default");
				INFO_LOG(INPUT, "DreamcastController[%d] connecting to user-configured serial device: %s", bus, serial_device.c_str());
			}
			else
			{
				serial_device = getFirstSerialDevice();
				INFO_LOG(INPUT, "DreamcastController[%d] connecting to autoselected serial device: %s", bus, serial_device.c_str());
			}

			serial_handler.open(serial_device, ec);
			
			if (ec || !serial_handler.is_open()) {
				WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, ec.message().c_str());
				disconnect();
				return;
			}
			break;
		}
		default:
		{
			return;
		}
	}
	
	// Now get the controller configuration
	MapleMsg msg;
	msg.command = MDCF_GetCondition;
	msg.destAP = (bus << 6) | 0x20;
	msg.originAP = bus << 6;
	msg.setData(MFID_0_Input);
	
	ec = sendMsg(msg, iostream, io_context, serial_handler, dreamcastControllerType);
	if (ec)
	{
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, ec.message().c_str());
		disconnect();
		return;
	}
	if (!receiveMsg(msg, iostream, serial_handler, dreamcastControllerType)) {
		WARN_LOG(INPUT, "DreamcastController[%d] read timeout", bus);
		disconnect();
		return;
	}
	if (dreamcastControllerType == TYPE_DREAMCONN)
		iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow

	expansionDevs = msg.originAP & 0x1f;
	
	config::MapleExpansionDevices[bus][0] = hasVmu() ? MDT_SegaVMU : MDT_None;
	config::MapleExpansionDevices[bus][1] = hasRumble() ? MDT_PurupuruPack : MDT_None;
	
	if (hasVmu() || hasRumble())
	{
		NOTICE_LOG(INPUT, "Connected to DreamcastController[%d]: Type:%s, VMU:%d, Rumble Pack:%d", bus, dreamcastControllerType == 1 ? "DreamConn+ / DreamcConn S Controller" : "Dreamcast Controller USB", hasVmu(), hasRumble());
		maple_io_connected = true;
	}
	else
	{
		WARN_LOG(INPUT, "DreamcastController[%d] connection: no VMU or Rumble Pack connected", bus);
		disconnect();
		return;
	}
}

void DreamConn::disconnect()
{
	if (dreamcastControllerType == TYPE_DREAMCONN)
	{
		if (iostream)
			iostream.close();
	}
	else if (dreamcastControllerType == TYPE_DREAMCASTCONTROLLERUSB)
	{
		if (serial_handler.is_open())
			serial_handler.cancel();
		serial_handler.close();
		io_context.stop();
	}
	
	maple_io_connected = false;
	
	NOTICE_LOG(INPUT, "Disconnected from DreamcastController[%d]", bus);
}

bool DreamConn::send(const MapleMsg& msg)
{
	asio::error_code ec;

	if (maple_io_connected)
		ec = sendMsg(msg, iostream, io_context, serial_handler, dreamcastControllerType);
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

bool DreamConnGamepad::isDreamcastController(int deviceIndex)
{
	char guid_str[33] {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid_str, sizeof(guid_str));
	INFO_LOG(INPUT, "GUID: %s VID:%c%c%c%c PID:%c%c%c%c", guid_str,
			guid_str[10], guid_str[11], guid_str[8], guid_str[9],
			guid_str[18], guid_str[19], guid_str[16], guid_str[17]);
	
	// DreamConn VID:4457 PID:4443
	// Dreamcast Controller USB VID:1209 PID:2f07
	if (memcmp("5744000043440000", guid_str + 8, 16) == 0 || memcmp("09120000072f0000", guid_str + 8, 16) == 0)
	{
		return true;
	}
	return false;
}

DreamConnGamepad::DreamConnGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick)
{
	char guid_str[33] {};
	
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(joystick_idx), guid_str, sizeof(guid_str));
	
	// DreamConn VID:4457 PID:4443
	// Dreamcast Controller USB VID:1209 PID:2f07
	if (memcmp("5744000043440000", guid_str + 8, 16) == 0)
	{
		dreamcastControllerType = TYPE_DREAMCONN;
		_name = "DreamConn+ / DreamConn S Controller";
	}
	else if (memcmp("09120000072f0000", guid_str + 8, 16) == 0)
	{
		dreamcastControllerType = TYPE_DREAMCASTCONTROLLERUSB;
		_name = "Dreamcast Controller USB";
	}
	
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
		dreamconn = std::make_shared<DreamConn>(port, dreamcastControllerType);
	}
	SDLGamepad::set_maple_port(port);
}

void DreamConnGamepad::handleEvent(Event event, void *arg)
{
	DreamConnGamepad *gamepad = static_cast<DreamConnGamepad*>(arg);
	if (gamepad->dreamconn != nullptr)
		createDreamConnDevices(gamepad->dreamconn, event == Event::Start);
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

bool DreamConnGamepad::isDreamcastController(int deviceIndex) {
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
