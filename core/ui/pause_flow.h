/*
	Copyright 2026 flyinghead

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

#include "gui.h"

namespace pause_flow {

enum class MenuCloseAction {
	ResumeNormally,
	RefreshPausedFrame,
	UnpauseForMissingLastFrame,
};

MenuCloseAction onMenuClosed(bool paused);

void schedulePauseAfterLoadState();
void cancelPauseAfterLoadState();
bool consumePauseAfterLoadState();

bool shouldRedrawPausedOsd(bool hasDrawData);

}
