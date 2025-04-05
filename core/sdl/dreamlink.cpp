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
#include "dreamlink.h"

#ifdef USE_DREAMCASTCONTROLLER

#include "dreamconn.h"
#include "dreampicoport.h"

#include "hw/maple/maple_devs.h"
#include "ui/gui.h"
#include <cfg/option.h>
#include <SDL.h>
#include <asio.hpp>
#include <iomanip>
#include <sstream>
#include <optional>
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
#include <dirent.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <setupapi.h>
#endif

void createDreamLinkDevices(std::shared_ptr<DreamLink> dreamlink, bool gameStart);
void tearDownDreamLinkDevices(std::shared_ptr<DreamLink> dreamlink);

bool DreamLinkGamepad::isDreamcastController(int deviceIndex)
{
	char guid_str[33] {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid_str, sizeof(guid_str));
	NOTICE_LOG(INPUT, "GUID: %s VID:%c%c%c%c PID:%c%c%c%c", guid_str,
			guid_str[10], guid_str[11], guid_str[8], guid_str[9],
			guid_str[18], guid_str[19], guid_str[16], guid_str[17]);

	// DreamConn VID:4457 PID:4443
	// Dreamcast Controller USB VID:1209 PID:2f07
    const char* pid_vid_guid_str = guid_str + 8;
	if (memcmp(DreamConn::VID_PID_GUID, pid_vid_guid_str, 16) == 0 ||
		memcmp(DreamPicoPort::VID_PID_GUID, pid_vid_guid_str, 16) == 0)
	{
		NOTICE_LOG(INPUT, "Dreamcast controller found!");
		return true;
	}
	return false;
}

DreamLinkGamepad::DreamLinkGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick)
{
	char guid_str[33] {};

	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(joystick_idx), guid_str, sizeof(guid_str));
	device_guid = guid_str;

	// DreamConn VID:4457 PID:4443
	// Dreamcast Controller USB VID:1209 PID:2f07
	if (memcmp(DreamConn::VID_PID_GUID, guid_str + 8, 16) == 0)
	{
		dreamlink = std::make_shared<DreamConn>(maple_port);
	}
	else if (memcmp(DreamPicoPort::VID_PID_GUID, guid_str + 8, 16) == 0)
	{
		dreamlink = std::make_shared<DreamPicoPort>(maple_port, joystick_idx, sdl_joystick);
	}

	if (dreamlink) {
		_name = dreamlink->getName();
		int defaultBus = dreamlink->getDefaultBus();
		if (defaultBus >= 0 && defaultBus < 4) {
			set_maple_port(defaultBus);
		}

		std::string uniqueId = dreamlink->getUniqueId();
		if (!uniqueId.empty()) {
			this->_unique_id = uniqueId;
		}
	}

	EventManager::listen(Event::Start, handleEvent, this);
	EventManager::listen(Event::LoadState, handleEvent, this);
    EventManager::listen(Event::Terminate, handleEvent, this);

	loadMapping();
}

DreamLinkGamepad::~DreamLinkGamepad() {
	EventManager::unlisten(Event::Start, handleEvent, this);
	EventManager::unlisten(Event::LoadState, handleEvent, this);
    EventManager::unlisten(Event::Terminate, handleEvent, this);
	if (dreamlink) {
		tearDownDreamLinkDevices(dreamlink);
		dreamlink.reset();

		// Make sure settings are open in case disconnection happened mid-game
		if (!gui_is_open()) {
			gui_open_settings();
		}
	}
}

void DreamLinkGamepad::set_maple_port(int port)
{
	if (dreamlink) {
		if (port < 0 || port >= 4) {
			dreamlink->disconnect();
		}
		else if (dreamlink->getBus() != port) {
			dreamlink->changeBus(port);
			if (is_registered()) {
				dreamlink->connect();
			}
		}
	}
	SDLGamepad::set_maple_port(port);
}

void DreamLinkGamepad::registered()
{
	if (dreamlink)
	{
		dreamlink->connect();

		// Create DreamLink Maple Devices here just in case game is already running
		createDreamLinkDevices(dreamlink, false);
	}
}

void DreamLinkGamepad::handleEvent(Event event, void *arg)
{
	DreamLinkGamepad *gamepad = static_cast<DreamLinkGamepad*>(arg);
	if (gamepad->dreamlink != nullptr && event != Event::Terminate) {
		createDreamLinkDevices(gamepad->dreamlink, event == Event::Start);
	}

    if (gamepad->dreamlink != nullptr && event == Event::Terminate)
    {
		gamepad->dreamlink->gameTermination();
    }
}

bool DreamLinkGamepad::gamepad_btn_input(u32 code, bool pressed)
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

bool DreamLinkGamepad::gamepad_axis_input(u32 code, int value)
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

void DreamLinkGamepad::resetMappingToDefault(bool arcade, bool gamepad) {
	SDLGamepad::resetMappingToDefault(arcade, gamepad);
	dreamlink->setDefaultMapping(input_mapper);
}

std::shared_ptr<InputMapping> DreamLinkGamepad::getDefaultMapping() {
	std::shared_ptr<InputMapping> mapping = SDLGamepad::getDefaultMapping();
	if (mapping && dreamlink) {
		dreamlink->setDefaultMapping(mapping);
	}
	return mapping;
}

void DreamLinkGamepad::checkKeyCombo() {
	if (ltrigPressed && rtrigPressed && startPressed)
		gui_open_settings();
}

#else // USE_DREAMCASTCONTROLLER

bool DreamLinkGamepad::isDreamcastController(int deviceIndex) {
	return false;
}
DreamLinkGamepad::DreamLinkGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick) {
}
DreamLinkGamepad::~DreamLinkGamepad() {
}
void DreamLinkGamepad::set_maple_port(int port) {
	SDLGamepad::set_maple_port(port);
}
void DreamLinkGamepad::registered() {
}
bool DreamLinkGamepad::gamepad_btn_input(u32 code, bool pressed) {
	return SDLGamepad::gamepad_btn_input(code, pressed);
}
bool DreamLinkGamepad::gamepad_axis_input(u32 code, int value) {
	return SDLGamepad::gamepad_axis_input(code, value);
}
void DreamLinkGamepad::resetMappingToDefault(bool arcade, bool gamepad) {
	SDLGamepad::resetMappingToDefault(arcade, gamepad);
}
std::shared_ptr<InputMapping> DreamLinkGamepad::getDefaultMapping() {
	return SDLGamepad::getDefaultMapping();
}

#endif
