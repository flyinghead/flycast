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

#ifdef USE_DREAMLINK_DEVICES

#include "dreamconn.h"
#include "dreampicoport.h"

#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
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

void handleEvent(Event event, void* arg)
{
	if (event == Event::Start)
	{
		reconnectDreamLinks();
	}

	for (auto& dreamlink : allDreamLinks)
	{
		if (dreamlink != nullptr)
		{
			if (event != Event::Terminate)
			{
				createDreamLinkDevices(dreamlink, event == Event::Start, event == Event::LoadState);
			}
			else
			{
				dreamlink->gameTermination();
			}
		}
	}
}

void registerDreamLinkEvents()
{
	EventManager::listen(Event::Start, handleEvent, nullptr);
	EventManager::listen(Event::LoadState, handleEvent, nullptr);
	EventManager::listen(Event::Terminate, handleEvent, nullptr);
}

void unregisterDreamLinkEvents()
{
	EventManager::unlisten(Event::Start, handleEvent, nullptr);
	EventManager::unlisten(Event::LoadState, handleEvent, nullptr);
	EventManager::unlisten(Event::Terminate, handleEvent, nullptr);
}

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
		bool isForPhysicalController = true;
		dreamlink = std::make_shared<DreamConn>(maple_port, isForPhysicalController);
	}
	else if (memcmp(DreamPicoPort::VID_PID_GUID, guid_str + 8, 16) == 0)
	{
		dreamlink = std::make_shared<DreamPicoPort>(maple_port, joystick_idx, sdl_joystick);
	}

	if (dreamlink) {
		allDreamLinks[maple_port] = dreamlink;
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

	loadMapping();
}

DreamLinkGamepad::~DreamLinkGamepad() {
	if (dreamlink) {
		tearDownDreamLinkDevices(dreamlink);
		dreamlink.reset();
		allDreamLinks[maple_port()] = nullptr;

		// Make sure settings are open in case disconnection happened mid-game
		if (!gui_is_open()) {
			gui_open_settings();
		}
	}
}

void DreamLinkGamepad::set_maple_port(int port)
{
	if (dreamlink) {
		int oldBus = dreamlink->getBus();
		if (port < 0 || port >= 4) {
			dreamlink->disconnect();
		}
		else if (oldBus != port) {
			// Move the dreamlink from the old to new position
			if (oldBus >= 0 && oldBus < 4) {
				dreamLinkNeedsRefresh[oldBus] = false;
				allDreamLinks[oldBus] = nullptr;
			}

			dreamLinkNeedsRefresh[port] = false;
			allDreamLinks[port] = dreamlink;

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
		createDreamLinkDevices(dreamlink, false, false);
	}
}

void DreamLinkGamepad::resetMappingToDefault(bool arcade, bool gamepad) {
	SDLGamepad::resetMappingToDefault(arcade, gamepad);
	if (input_mapper) {
		if (dreamlink) {
			dreamlink->setDefaultMapping(input_mapper);
		}
		setBaseDefaultMapping(input_mapper);
	}
}
const char *DreamLinkGamepad::get_button_name(u32 code) {
	if (dreamlink) {
		const char* name = dreamlink->getButtonName(code);
		if (name) {
			return name;
		}
	}
	return SDLGamepad::get_button_name(code);
}

const char *DreamLinkGamepad::get_axis_name(u32 code) {
	if (dreamlink) {
		const char* name = dreamlink->getAxisName(code);
		if (name) {
			return name;
		}
	}
	return SDLGamepad::get_axis_name(code);
}

std::shared_ptr<InputMapping> DreamLinkGamepad::getDefaultMapping() {
	std::shared_ptr<InputMapping> mapping = SDLGamepad::getDefaultMapping();
	if (mapping) {
		if (dreamlink) {
			dreamlink->setDefaultMapping(mapping);
		}
		setBaseDefaultMapping(mapping);
	}
	return mapping;
}

void DreamLinkGamepad::setBaseDefaultMapping(const std::shared_ptr<InputMapping>& mapping) const
{
	const u32 leftTrigger = mapping->get_axis_code(maple_port(), DreamcastKey::DC_AXIS_LT).first;
	const u32 rightTrigger = mapping->get_axis_code(maple_port(), DreamcastKey::DC_AXIS_RT).first;
	const u32 startCode = mapping->get_button_code(maple_port(), DreamcastKey::DC_BTN_START);
	if (leftTrigger != InputMapping::InputDef::INVALID_CODE &&
		rightTrigger != InputMapping::InputDef::INVALID_CODE &&
		startCode != InputMapping::InputDef::INVALID_CODE)
	{
		mapping->set_button(DreamcastKey::EMU_BTN_MENU, InputMapping::ButtonCombo{
			InputMapping::InputSet{
				InputMapping::InputDef{leftTrigger, InputMapping::InputDef::InputType::AXIS_POS},
				InputMapping::InputDef{rightTrigger, InputMapping::InputDef::InputType::AXIS_POS},
				InputMapping::InputDef{startCode, InputMapping::InputDef::InputType::BUTTON}
			},
			false
		});
	}
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
void DreamLinkGamepad::resetMappingToDefault(bool arcade, bool gamepad) {
	SDLGamepad::resetMappingToDefault(arcade, gamepad);
}
const char *DreamLinkGamepad::get_button_name(u32 code) {
	return SDLGamepad::get_button_name(code);
}
const char *DreamLinkGamepad::get_axis_name(u32 code) {
	return SDLGamepad::get_axis_name(code);
}
std::shared_ptr<InputMapping> DreamLinkGamepad::getDefaultMapping() {
	return SDLGamepad::getDefaultMapping();
}
void DreamLinkGamepad::setBaseDefaultMapping(const std::shared_ptr<InputMapping>& mapping) const {
}

#endif
