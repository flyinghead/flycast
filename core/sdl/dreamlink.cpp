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

#include "dreamconn.h"
#include "dreampicoport.h"

#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "ui/gui.h"
#include "oslib/i18n.h"
#include <cfg/option.h>
#include <SDL.h>
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

DreamLink::DreamLink(bool storageSupported)
	: BaseMapleLink(storageSupported)
{
	EventManager::listen(Event::Terminate, DreamLink::eventTerminate, this);
}

DreamLink::~DreamLink()
{
	EventManager::unlisten(Event::Terminate, DreamLink::eventTerminate, this);
}

void DreamLink::eventTerminate(Event event, void *param)
{
	DreamLink *self = static_cast<DreamLink *>(param);
	self->gameTermination();
}

bool DreamLinkGamepad::isDreamcastController(int deviceIndex)
{
	char guid_str[33] {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid_str, sizeof(guid_str));
	NOTICE_LOG(INPUT, "GUID: %s VID:%c%c%c%c PID:%c%c%c%c", guid_str,
			guid_str[10], guid_str[11], guid_str[8], guid_str[9],
			guid_str[18], guid_str[19], guid_str[16], guid_str[17]);

#ifdef USE_DREAMCONN
	if (DreamConnGamepad::identify(deviceIndex))
		return true;
#endif
	if (DreamPicoPortGamepad::identify(deviceIndex))
		return true;
	return false;
}

DreamLinkGamepad::DreamLinkGamepad(std::shared_ptr<DreamLink> dreamlink, int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick), dreamlink(dreamlink)
{
	verify(dreamlink != nullptr);
}

void DreamLinkGamepad::close()
{
	if (dreamlink != nullptr)
	{
		dreamlink->disconnect();
		dreamlink.reset();
		// Make sure settings are open in case disconnection happened mid-game
		if (!gui_is_open())
			gui_open_settings();
	}
	SDLGamepad::close();
}

const char* DreamLinkGamepad::dreamLinkStatus()
{
	using namespace i18n;
	return dreamlink->isConnected() ? T("Connected") : T("Disconnected");
}

void DreamLinkGamepad::set_maple_port(int port)
{
	int oldPort = maple_port();
	if (oldPort == port)
		return;

	SDLGamepad::set_maple_port(port);

	dreamlink->changeBus(port);
}

void DreamLinkGamepad::registered()
{
	SDLGamepad::registered();
	dreamlink->connect();
}

void DreamLinkGamepad::resetMappingToDefault(bool arcade, bool gamepad) {
	SDLGamepad::resetMappingToDefault(arcade, gamepad);
	if (input_mapper) {
		setCustomMapping(input_mapper);
		setBaseDefaultMapping(input_mapper);
	}
}

std::shared_ptr<InputMapping> DreamLinkGamepad::getDefaultMapping() {
	std::shared_ptr<InputMapping> mapping = SDLGamepad::getDefaultMapping();
	if (mapping) {
		setCustomMapping(mapping);
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

std::shared_ptr<DreamLinkGamepad> createDreamLinkGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
{
	if (DreamPicoPortGamepad::identify(joystick_idx))
		return std::make_shared<DreamPicoPortGamepad>(maple_port, joystick_idx, sdl_joystick);
#ifdef USE_DREAMCONN
	else if (DreamConnGamepad::identify(joystick_idx))
		return std::make_shared<DreamConnGamepad>(maple_port, joystick_idx, sdl_joystick);
#endif
	return nullptr;
}
