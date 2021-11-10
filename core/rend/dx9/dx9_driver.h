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
#include "imgui_impl_dx9.h"
#include "dxcontext.h"

class DX9Driver final : public ImGuiDriver
{
public:
    void newFrame() override {
    	ImGui_ImplDX9_NewFrame();
	}

	void renderDrawData(ImDrawData *drawData) override {
		theDXContext.EndImGuiFrame();
	}

	void present() override {
		theDXContext.Present();
	}
};
