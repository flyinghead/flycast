/*
	Copyright 2020 flyinghead

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
#include <chrono>
#include <thread>
#include "mainui.h"
#include "hw/pvr/Renderer_if.h"
#include "gui.h"
#include "oslib/oslib.h"
#include "wsi/context.h"
#include "cfg/option.h"
#include "emulator.h"

bool mainui_enabled;
u32 MainFrameCount;
static bool forceReinit;

void UpdateInputState();

bool mainui_rend_frame()
{
	os_DoEvents();

	if (gui_is_open() || gui_state == GuiState::VJoyEdit)
	{
		gui_display_ui();
		// TODO refactor android vjoy out of renderer
		if (gui_state == GuiState::VJoyEdit && renderer != NULL)
			renderer->DrawOSD(true);
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
	else
	{
		if (!rend_single_frame(mainui_enabled))
		{
			UpdateInputState();
			return false;
		}
	}
	MainFrameCount++;

	return true;
}

void mainui_init()
{
	rend_init_renderer();
	dc_resize_renderer();
}

void mainui_term()
{
	rend_term_renderer();
}

void mainui_loop()
{
	mainui_enabled = true;
	mainui_init();

	while (mainui_enabled)
	{
		if (mainui_rend_frame())
		{
			if (config::RendererType.isOpenGL())
				theGLContext.Swap();
#ifdef USE_VULKAN
			else
				VulkanContext::Instance()->Present();
#endif
		}

		if (config::RendererType.pendingChange() || forceReinit)
		{
			bool openGl = config::RendererType.isOpenGL();
			mainui_term();
			config::RendererType.commit();
			if (openGl != config::RendererType.isOpenGL() || forceReinit)
				// Switch between vulkan and opengl (or full reinit)
				SwitchRenderApi();
			mainui_init();
			forceReinit = false;
		}
	}

	mainui_term();
}

void mainui_stop()
{
	mainui_enabled = false;
}

void mainui_reinit()
{
	forceReinit = true;
}
