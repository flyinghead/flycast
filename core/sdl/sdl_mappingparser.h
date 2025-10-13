/*
	Copyright 2025 flyinghead

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
#pragma once
#include <SDL.h>
#include <stdlib.h>
#include <string>

// Home-made version of SDL_GameControllerGetBindForAxis that returns additional information on axes

struct SDL_GameControllerButtonBind2
{
    SDL_GameControllerBindType bindType;
    union
    {
        int button;
        struct {
        	int axis;
        	int direction;	// 0:full, 1:positive, -1:negative, 2:reverse full
        } axis;
        struct {
            int hat;
            int hat_mask;
        } hat;
    } value;

};

class SDLControllerMappingParser
{
public:
	SDLControllerMappingParser(SDL_GameController *sdlController)
	{
		char *gcdb = SDL_GameControllerMapping(sdlController);
		if (gcdb == nullptr)
			return;
		gcMapping = gcdb;
		SDL_free(gcdb);
	}

	SDL_GameControllerButtonBind2 getBindForAxis(SDL_GameControllerAxis sdl_axis, int direction = 0)
	{
		SDL_GameControllerButtonBind2 bind{};
		const char *axisName = SDL_GameControllerGetStringForAxis(sdl_axis);
		if (axisName == nullptr)
			return bind;
		std::string axis;
		if (direction > 0)
			axis = '+';
		else if (direction < 0)
			axis = '-';
		axis += std::string(axisName) + ':';
		auto axisPos = gcMapping.find(axis);
		if (axisPos == std::string::npos)
		{
			if (direction != 0) {
				axis = axis.substr(1);
				axisPos = gcMapping.find(axis);
			}
			if (axisPos == std::string::npos)
				return bind;
		}
		auto endPos = gcMapping.find(',', axisPos);
		std::string mapping = gcMapping.substr(axisPos, endPos == std::string::npos ? endPos : endPos - axisPos);
		auto valuePos = axis.length();
		if (valuePos >= mapping.length() - 1)
			return bind;

		if (mapping[valuePos] == 'b')
		{
			// Button
			bind.bindType = SDL_CONTROLLER_BINDTYPE_BUTTON;
			bind.value.button = atoi(mapping.substr(valuePos + 1).c_str());
			return bind;
		}
		if (mapping[valuePos] == 'h')
		{
			// Hat
			bind.value.hat.hat = atoi(mapping.substr(valuePos + 1).c_str());
			auto dotPos = mapping.find('.');
			if (dotPos == std::string::npos)
				return bind;
			bind.value.hat.hat_mask = atoi(mapping.substr(dotPos + 1).c_str());
			bind.bindType = SDL_CONTROLLER_BINDTYPE_HAT;
			return bind;
		}
		// Axis
		if (mapping[valuePos] == '+') {
			bind.value.axis.direction = 1;
			valuePos++;
		}
		if (mapping[valuePos] == '-') {
			bind.value.axis.direction = -1;
			valuePos++;
		}
		if (valuePos >= mapping.length() - 1 || mapping[valuePos] != 'a')
			return bind;
		valuePos++;
		if (mapping.back() == '~')
			bind.value.axis.direction = 2;
		bind.value.axis.axis = atoi(mapping.substr(valuePos).c_str());
		bind.bindType = SDL_CONTROLLER_BINDTYPE_AXIS;

		if (sdl_axis != SDL_CONTROLLER_AXIS_TRIGGERLEFT
				&& sdl_axis != SDL_CONTROLLER_AXIS_TRIGGERRIGHT
				&& (bind.value.axis.direction == 0 || bind.value.axis.direction == 2)
				&& axis[0] != '+'
				&& axis[0] != '-')
		{
			// Full axis mapped to full axis
			if (bind.value.axis.direction == 0)
				// normal
				bind.value.axis.direction = direction;
			else
				// reverse
				bind.value.axis.direction = -direction;
		}

		return bind;
	}

private:
	std::string gcMapping;
};
