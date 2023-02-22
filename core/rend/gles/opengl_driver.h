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
#include "emulator.h"
#include <unordered_map>

class OpenGLDriver final : public ImGuiDriver
{
public:
	OpenGLDriver();
	~OpenGLDriver() override;

	void displayVmus() override;
	void displayCrosshairs() override;

	void newFrame() override;
	void renderDrawData(ImDrawData* drawData, bool gui_open) override;
	void present() override;

	void setFrameRendered() override {
		frameRendered = true;
	}

	ImTextureID getTexture(const std::string& name) override {
		auto it = textures.find(name);
		if (it != textures.end())
			return it->second;
		else
			return ImTextureID{};
	}

	ImTextureID updateTexture(const std::string& name, const u8 *data, int width, int height) override;

private:
	void emuEvent(Event event)
	{
		switch (event)
		{
		case Event::Resume:
			gameStarted = true;
			break;
		case Event::Terminate:
			gameStarted = false;
			break;
		default:
			break;
		}
	}
	static void emuEventCallback(Event event, void *p) {
		((OpenGLDriver *)p)->emuEvent(event);
	}

	ImTextureID vmu_lcd_tex_ids[8];
	ImTextureID crosshairTexId = ImTextureID();
	bool gameStarted = false;
	bool frameRendered = false;
	std::unordered_map<std::string, ImTextureID> textures;
};
