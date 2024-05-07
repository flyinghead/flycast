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
#include "ui/gui.h"
#include "ui/gui_util.h"
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
constexpr int VMU_WIDTH = 96;
constexpr int VMU_HEIGHT = 64;
constexpr int VMU_PADDING = 8;

OpenGLDriver::OpenGLDriver()
{
	ImGui_ImplOpenGL3_Init();
	EventManager::listen(Event::Resume, emuEventCallback, this);
	EventManager::listen(Event::Terminate, emuEventCallback, this);
}

OpenGLDriver::~OpenGLDriver()
{
	EventManager::unlisten(Event::Resume, emuEventCallback, this);
	EventManager::unlisten(Event::Terminate, emuEventCallback, this);

	std::vector<GLuint> texIds;
	texIds.reserve(1 + textures.size());
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
	updateVmuTextures();
	const ScaledVec2 size(VMU_WIDTH, VMU_HEIGHT);
	const float padding = uiScaled(VMU_PADDING);
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	for (int i = 0; i < 8; i++)
	{
		if (!vmu_lcd_status[i])
			continue;

		int x = vmu_coords[i][0];
		int y = vmu_coords[i][1];
		ImVec2 pos;
		if (x == 0)
			pos.x = padding;
		else
			pos.x = ImGui::GetIO().DisplaySize.x - size.x - padding;
		if (y == 0)
		{
			pos.y = padding;
			if (i & 1)
				pos.y += size.y + padding;
		}
		else
		{
			pos.y = ImGui::GetIO().DisplaySize.y - size.y - padding;
			if (i & 1)
				pos.y -= size.y + padding;
		}
		ImVec2 pos_b = pos + size;
		dl->AddImage(vmu_lcd_tex_ids[i], pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), 0xC0ffffff);
	}
}

void OpenGLDriver::displayCrosshairs()
{
	if (!gameStarted)
		return;
	if (!crosshairsNeeded())
		return;

	if (crosshairTexId == ImTextureID())
		crosshairTexId = updateTexture("__crosshair", (const u8 *)getCrosshairTextureData(), 16, 16, true);

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
		pos.x -= (config::CrosshairSize * settings.display.uiScale) / 2.f;
		pos.y += (config::CrosshairSize * settings.display.uiScale) / 2.f;
		ImVec2 pos_b(pos.x + config::CrosshairSize * settings.display.uiScale, pos.y - config::CrosshairSize * settings.display.uiScale);

		ImGui::GetWindowDrawList()->AddImage(crosshairTexId, pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), config::CrosshairColor[i]);
	}
	ImGui::End();
}

void OpenGLDriver::newFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
}

void OpenGLDriver::renderDrawData(ImDrawData* drawData, bool gui_open)
{
	ImGui_ImplOpenGL3_RenderDrawData(drawData);
	if (gui_open)
		frameRendered = true;
}

void OpenGLDriver::present()
{
	if (frameRendered)
		theGLContext.swap();
	frameRendered = false;
}

ImTextureID OpenGLDriver::updateTexture(const std::string& name, const u8 *data, int width, int height, bool nearestSampling)
{
	ImTextureID oldId = getTexture(name);
	if (oldId != ImTextureID())
		glcache.DeleteTextures(1, (GLuint *)&oldId);
	GLuint texId = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texId);
	if (nearestSampling) {
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else {
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
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
