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
#include "pause_flow.h"

#include "cfg/option.h"
#include "hw/pvr/Renderer_if.h"

#include <atomic>

namespace pause_flow {

static std::atomic<bool> pendingPauseAfterLoadState { false };
static std::atomic<bool> pendingPausedRedraw { false };
static bool hadPausedOverlayDrawData = false;

MenuCloseAction onMenuClosed(bool paused)
{
	if (!paused)
		return MenuCloseAction::ResumeNormally;
	if (renderer == nullptr || !renderer->HasLastFrame())
		return MenuCloseAction::UnpauseForMissingLastFrame;
	pendingPausedRedraw = true;
	return MenuCloseAction::RefreshPausedFrame;
}

void schedulePauseAfterLoadState()
{
	pendingPauseAfterLoadState = config::AutoPauseAfterLoadState;
}

void cancelPauseAfterLoadState()
{
	pendingPauseAfterLoadState = false;
}

bool consumePauseAfterLoadState()
{
	return pendingPauseAfterLoadState.exchange(false);
}

bool shouldRedrawPausedOsd(bool hasDrawData)
{
	const bool redrawPausedFrame = pendingPausedRedraw.exchange(false);
	const bool shouldRedraw = redrawPausedFrame || hasDrawData || hadPausedOverlayDrawData;
	hadPausedOverlayDrawData = hasDrawData;
	return shouldRedraw;
}

}
