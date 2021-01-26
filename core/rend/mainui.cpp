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

bool mainui_enabled;
int renderer_changed = -1;	// Signals the renderer thread to switch renderer
u32 MainFrameCount;

void UpdateInputState();

bool mainui_rend_frame()
{
	os_DoEvents();

	if (gui_is_open() || gui_state == VJoyEdit)
	{
		gui_display_ui();
		// TODO refactor android vjoy out of renderer
		if (gui_state == VJoyEdit && renderer != NULL)
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
}

void mainui_term()
{
	rend_term_renderer();
}

void mainui_loop()
{
	mainui_enabled = true;
	renderer_changed = (int)settings.pvr.rend;
	mainui_init();

	while (mainui_enabled)
	{
		if (mainui_rend_frame())
		{
			if (settings.pvr.IsOpenGL())
				theGLContext.Swap();
#ifdef USE_VULKAN
			else
				VulkanContext::Instance()->Present();
#endif
		}

		if (renderer_changed != (int)settings.pvr.rend)
		{
			mainui_term();
			if (renderer_changed == -1
					|| settings.pvr.IsOpenGL() != ((RenderType)renderer_changed == RenderType::OpenGL || (RenderType)renderer_changed == RenderType::OpenGL_OIT))
			{
				// Switch between vulkan and opengl (or full reinit)
				SwitchRenderApi(renderer_changed == -1 ? settings.pvr.rend : (RenderType)renderer_changed);
			}
			else
			{
				settings.pvr.rend = (RenderType)renderer_changed;
			}
			renderer_changed = (int)settings.pvr.rend;
			mainui_init();
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
	renderer_changed = -1;
}
