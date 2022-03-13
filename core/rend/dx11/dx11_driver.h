/*
	Copyright 2021 flyinghead

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
#include "rend/imgui_driver.h"
#include "imgui_impl_dx11.h"
#include "dx11context.h"
#include "rend/gui.h"

class DX11Driver final : public ImGuiDriver
{
public:
    void newFrame() override {
    	ImGui_ImplDX11_NewFrame();
	}

	void renderDrawData(ImDrawData *drawData) override {
		theDX11Context.EndImGuiFrame();
		if (gui_is_open())
			frameRendered = true;
	}

	void present() override {
		if (frameRendered)
			theDX11Context.Present();
		frameRendered = false;
	}

	void setFrameRendered() override {
		frameRendered = true;
	}

private:
	bool frameRendered = false;
};
