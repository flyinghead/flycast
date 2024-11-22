/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifdef __ANDROID__

#include "gui_android.h"
#include "gui.h"
#include "stdclass.h"
#include "imgui.h"
#include "gui_util.h"
#include "rend/osd.h"
#include "imgui_driver.h"
#include "input/gamepad.h"
#include "input/gamepad_device.h"
#include "oslib/resources.h"
#include <stb_image.h>

namespace vgamepad
{

struct Control
{
	float x;
	float y;
	float w;
	float h;
	float u0;
	float v0;
	float u1;
	float v1;
};
static Control Controls[_Count];
static bool Visible = true;
static float AlphaTrans;

void displayCommands()
{
    centerNextWindow();

    ImGui::Begin("Virtual Joystick", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);

	if (ImGui::Button("Save", ScaledVec2(150, 50)))
	{
		stopEditing(false);
		gui_setState(GuiState::Settings);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset", ScaledVec2(150, 50)))
	{
		resetEditing();
		startEditing();
		gui_setState(GuiState::VJoyEdit);
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel", ScaledVec2(150, 50)))
	{
		stopEditing(true);
		gui_setState(GuiState::Settings);
	}
    ImGui::End();
}

static u8 *loadOSDButtons(int &width, int &height)
{
	int n;
	stbi_set_flip_vertically_on_load(1);

	FILE *file = nowide::fopen(get_readonly_data_path("buttons.png").c_str(), "rb");
	u8 *image_data = nullptr;
	if (file != nullptr)
	{
		image_data = stbi_load_from_file(file, &width, &height, &n, STBI_rgb_alpha);
		std::fclose(file);
	}
	if (image_data == nullptr)
	{
		size_t size;
		std::unique_ptr<u8[]> data = resource::load("picture/buttons.png", size);
		image_data = stbi_load_from_memory(data.get(), size, &width, &height, &n, STBI_rgb_alpha);
	}
	return image_data;
}

class ImguiVGamepadTexture : public ImguiTexture
{
public:
	ImTextureID getId() override
	{
		ImTextureID id = imguiDriver->getTexture(PATH);
		if (id == ImTextureID())
		{
			int width, height;
			u8 *imgData = loadOSDButtons(width, height);
			if (imgData != nullptr)
			{
				try {
					id = imguiDriver->updateTextureAndAspectRatio(PATH, imgData, width, height, nearestSampling);
				} catch (...) {
					// vulkan can throw during resizing
				}
				free(imgData);
			}
		}
		return id;
	}

private:
	static constexpr char const *PATH = "picture/buttons.png";
};

constexpr float vjoy_sz[2][14] = {
	// L  U  R  D   X  Y  B  A  St  LT RT  Ana Stck FF
	{ 64,64,64,64, 64,64,64,64, 64, 90,90, 128, 64, 64 },
	{ 64,64,64,64, 64,64,64,64, 64, 64,64, 128, 64, 64 },
};

constexpr float OSD_TEX_W = 512.f;
constexpr float OSD_TEX_H = 256.f;

static void setUV()
{
	float u = 0;
	float v = 0;
	int i = 0;

	for (auto& control : Controls)
	{
		control.u0 = (u + 1) / OSD_TEX_W;
		control.v0 = 1.f - (v + 1) / OSD_TEX_H;
		control.u1 = (u + vjoy_sz[0][i] - 1) / OSD_TEX_W;
		control.v1 = 1.f - (v + vjoy_sz[1][i] - 1) / OSD_TEX_H;

		u += vjoy_sz[0][i];
		if (u >= OSD_TEX_W) {
			u -= OSD_TEX_W;
			v += vjoy_sz[1][i];
		}
		i++;
	}
}
static OnLoad _(&setUV);

void show() {
	Visible = true;
}

void hide() {
	Visible = false;
}

void setPosition(ControlId id, float x, float y, float w, float h)
{
	verify(id >= 0 && id < _Count);
	auto& control = Controls[id];
	control.x = x;
	control.y = y;
	control.w = w;
	control.h = h;
}

static void drawButtonDim(ImDrawList *drawList, const Control& control, int state)
{
	ImVec2 pos(control.x, control.y);
	ImVec2 size(control.w, control.h);
	ImVec2 uv0(control.u0, control.v0);
	ImVec2 uv1(control.u1, control.v1);
	float scale_h = settings.display.height / 480.f;
	float offs_x = (settings.display.width - scale_h * 640.f) / 2.f;
	pos *= scale_h;
	size *= scale_h;
	pos.x += offs_x;

	float col = (0.5f - 0.25f * state / 255) * (float)Visible;
	float alpha = (100.f - config::VirtualGamepadTransparency) / 100.f * AlphaTrans;
	AlphaTrans += ((float)Visible - AlphaTrans) / 2;
	ImVec4 color(col, col, col, alpha);

    ImguiVGamepadTexture tex;
	tex.draw(drawList, pos, size, uv0, uv1, color);
}

static void drawButton(ImDrawList *drawList, const Control& control, bool state) {
	drawButtonDim(drawList, control, state ? 0 : 255);
}

void draw()
{
	ImDrawList *drawList = ImGui::GetForegroundDrawList();
	drawButton(drawList, Controls[Left], kcode[0] & DC_DPAD_LEFT);
	drawButton(drawList, Controls[Up], kcode[0] & DC_DPAD_UP);
	drawButton(drawList, Controls[Right], kcode[0] & DC_DPAD_RIGHT);
	drawButton(drawList, Controls[Down], kcode[0] & DC_DPAD_DOWN);

	drawButton(drawList, Controls[X], kcode[0] & (settings.platform.isConsole() ? DC_BTN_X : DC_BTN_C));
	drawButton(drawList, Controls[Y], kcode[0] & (settings.platform.isConsole() ? DC_BTN_Y : DC_BTN_X));
	drawButton(drawList, Controls[B], kcode[0] & DC_BTN_B);
	drawButton(drawList, Controls[A], kcode[0] & DC_BTN_A);

	drawButton(drawList, Controls[Start], kcode[0] & DC_BTN_START);

	drawButtonDim(drawList, Controls[LeftTrigger], lt[0] >> 8);

	drawButtonDim(drawList, Controls[RightTrigger], rt[0] >> 8);

	drawButton(drawList, Controls[AnalogArea], true);
	drawButton(drawList, Controls[AnalogStick], false);

	drawButton(drawList, Controls[FastForward], false);
}

}	// namespace vgamepad

#endif // __ANDROID__
