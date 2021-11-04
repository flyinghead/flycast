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
	UpdateInputState();

	if (gui_is_open() || gui_state == GuiState::VJoyEdit)
	{
		gui_display_ui();
		// TODO refactor android vjoy out of renderer
		if (gui_state == GuiState::VJoyEdit && renderer != NULL)
			renderer->DrawOSD(true);
#ifndef TARGET_IPHONE
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
	}
	else
	{
		try {
			if (!emu.render())
				return false;
		} catch (const FlycastException& e) {
			emu.unloadGame();
			gui_stop_game(e.what());
			return false;
		}
	}
	MainFrameCount++;

	return true;
}

void mainui_init()
{
	rend_init_renderer();
	rend_resize_renderer();
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
			else if (config::RendererType.isVulkan())
				VulkanContext::Instance()->Present();
#endif
#ifdef _WIN32
			else if (config::RendererType.isDirectX())
				theDXContext.Present();
#endif
		}

		if (config::RendererType.pendingChange() || forceReinit)
		{
			int api = config::RendererType.isOpenGL() ? 0 : config::RendererType.isVulkan() ? 1 : 2;
			RenderType oldRender = config::RendererType;
			mainui_term();
			config::RendererType.commit();
			int newApi = config::RendererType.isOpenGL() ? 0 : config::RendererType.isVulkan() ? 1 : 2;
			if (newApi != api || forceReinit)
			{
				// Switch between vulkan/opengl/directx (or full reinit)
				RenderType newRender = config::RendererType;
				// restore current render mode to terminate
				config::RendererType = oldRender;
				config::RendererType.commit();
				TermRenderApi();
				// set the new render mode and init
				config::RendererType = newRender;
				config::RendererType.commit();
				InitRenderApi();
			}
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
