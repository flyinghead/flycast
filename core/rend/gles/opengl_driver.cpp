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
#include "opengl_driver.h"
#include "imgui_impl_opengl3.h"
#include "wsi/gl_context.h"
#include "rend/osd.h"
#include "rend/gui.h"
#include "glcache.h"
#include "gles.h"

#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

static constexpr int vmu_coords[8][2] = {
		{ 0 , 0 },
		{ 0 , 0 },
		{ 1 , 0 },
		{ 1 , 0 },
		{ 0 , 1 },
		{ 0 , 1 },
		{ 1 , 1 },
		{ 1 , 1 },
};
constexpr int VMU_WIDTH = 70 * 48 / 32;
constexpr int VMU_HEIGHT = 70;
constexpr int VMU_PADDING = 8;

OpenGLDriver::OpenGLDriver()
{
	for (auto& tex : vmu_lcd_tex_ids)
		tex = ImTextureID();
	ImGui_ImplOpenGL3_Init();
	EventManager::listen(Event::Start, emuEventCallback, this);
	EventManager::listen(Event::Terminate, emuEventCallback, this);
}

OpenGLDriver::~OpenGLDriver()
{
	EventManager::unlisten(Event::Start, emuEventCallback, this);
	EventManager::unlisten(Event::Terminate, emuEventCallback, this);

	std::vector<GLuint> texIds;
	texIds.reserve(ARRAY_SIZE(vmu_lcd_tex_ids) + 1 + textures.size());
	for (ImTextureID texId : vmu_lcd_tex_ids)
		if (texId != ImTextureID())
			texIds.push_back((GLuint)(uintptr_t)texId);
	if (crosshairTexId != ImTextureID())
		texIds.push_back((GLuint)(uintptr_t)crosshairTexId);
	for (const auto& it : textures)
		texIds.push_back((GLuint)(uintptr_t)it.second);
	if (!texIds.empty())
		glcache.DeleteTextures(texIds.size(), &texIds[0]);
	ImGui_ImplOpenGL3_Shutdown();
}

void OpenGLDriver::displayVmus()
{
	if (!gameStarted)
		return;
	ImGui::SetNextWindowBgAlpha(0);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin("vmu-window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs
			| ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
	const float width = VMU_WIDTH * settings.display.uiScale;
	const float height = VMU_HEIGHT * settings.display.uiScale;
	const float padding = VMU_PADDING * settings.display.uiScale;
	for (int i = 0; i < 8; i++)
	{
		if (!vmu_lcd_status[i])
			continue;

		if (vmu_lcd_changed[i] || vmu_lcd_tex_ids[i] == ImTextureID())
			vmu_lcd_tex_ids[i] = updateTexture("__vmu" + std::to_string(i), (const u8 *)vmu_lcd_data[i], 48, 32);

		int x = vmu_coords[i][0];
		int y = vmu_coords[i][1];
		ImVec2 pos;
		if (x == 0)
			pos.x = padding;
		else
			pos.x = ImGui::GetIO().DisplaySize.x - width - padding;
		if (y == 0)
		{
			pos.y = padding;
			if (i & 1)
				pos.y += height + padding;
		}
		else
		{
			pos.y = ImGui::GetIO().DisplaySize.y - height - padding;
			if (i & 1)
				pos.y -= height + padding;
		}
		ImVec2 pos_b(pos.x + width, pos.y + height);
		ImGui::GetWindowDrawList()->AddImage(vmu_lcd_tex_ids[i], pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), 0xC0ffffff);
	}
	ImGui::End();
}

void OpenGLDriver::displayCrosshairs()
{
	if (!gameStarted)
		return;
	if (!crosshairsNeeded())
		return;

	if (crosshairTexId == ImTextureID())
		crosshairTexId = updateTexture("__crosshair", (const u8 *)getCrosshairTextureData(), 16, 16);

	ImGui::SetNextWindowBgAlpha(0);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin("xhair-window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs
			| ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
	for (u32 i = 0; i < config::CrosshairColor.size(); i++)
	{
		if (config::CrosshairColor[i] == 0)
			continue;
		if (settings.platform.isConsole() && config::MapleMainDevices[i] != MDT_LightGun)
			continue;

		ImVec2 pos;
		std::tie(pos.x, pos.y) = getCrosshairPosition(i);
		pos.x -= (XHAIR_WIDTH * settings.display.uiScale) / 2.f;
		pos.y += (XHAIR_WIDTH * settings.display.uiScale) / 2.f;
		ImVec2 pos_b(pos.x + XHAIR_WIDTH * settings.display.uiScale, pos.y - XHAIR_HEIGHT * settings.display.uiScale);

		ImGui::GetWindowDrawList()->AddImage(crosshairTexId, pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), config::CrosshairColor[i]);
	}
	ImGui::End();
}

void OpenGLDriver::newFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
}

void OpenGLDriver::renderDrawData(ImDrawData* drawData)
{
	ImGui_ImplOpenGL3_RenderDrawData(drawData);
	if (gui_is_open())
		frameRendered = true;
}

void OpenGLDriver::present()
{
	if (frameRendered)
		theGLContext.swap();
	frameRendered = false;
}

ImTextureID OpenGLDriver::updateTexture(const std::string& name, const u8 *data, int width, int height)
{
	ImTextureID oldId = getTexture(name);
	if (oldId != ImTextureID())
		glcache.DeleteTextures(1, (GLuint *)&oldId);
	GLuint texId = glcache.GenTexture();
    glcache.BindTexture(GL_TEXTURE_2D, texId);
    glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (gl.border_clamp_supported)
	{
		float color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	}
	else
	{
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    return textures[name] = (ImTextureID)(u64)texId;
}
