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
#include "glcache.h"
#include "gles.h"
#include "hw/pvr/Renderer_if.h"

#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

OpenGLDriver::OpenGLDriver()
{
	ImGui_ImplOpenGL3_Init();
}

OpenGLDriver::~OpenGLDriver()
{
	std::vector<GLuint> texIds;
	texIds.reserve(textures.size());
	for (const auto& it : textures)
		texIds.push_back((GLuint)(uintptr_t)it.second);
	if (!texIds.empty())
		glcache.DeleteTextures(texIds.size(), &texIds[0]);
	ImGui_ImplOpenGL3_Shutdown();
}

void OpenGLDriver::newFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
}

void OpenGLDriver::renderDrawData(ImDrawData* drawData, bool gui_open)
{
	if (gui_open)
	{
#ifndef TARGET_IPHONE
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
		glcache.Disable(GL_SCISSOR_TEST);
		glcache.ClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		if (renderer != nullptr)
			renderer->RenderLastFrame();
	}
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

void OpenGLDriver::deleteTexture(const std::string& name)
{
	auto it = textures.find(name);
	if (it != textures.end()) {
		glcache.DeleteTextures(1, (GLuint *)&it->second);
		textures.erase(it);
	}
}
