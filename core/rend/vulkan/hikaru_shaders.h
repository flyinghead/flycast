/*
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
// Sega Hikaru shaders, independent of the PVR / Naomi 2 shaders
#pragma once
#include "vulkan.h"

// std140 alignment required
// Push constants of the full-screen passes (frame blender, final picture)
struct HikaruScreenConstants
{
	float blendFactor[4];
	float scroll[3][4];
	int slotKind[4];
	float keyed3D[4];
};

struct HikaruDrawUniforms
{
	float modelview0[16];
	float modelview1[16];
	float projection[16];	// eye space to screen pixels (640x480, y down) times w
	float params[4];		// x: 0x501 pixel pattern
};

class HikaruShaderManager
{
public:
	vk::ShaderModule GetVertexShader()
	{
		if (!vertexShader)
			vertexShader = compileVertexShader();
		return *vertexShader;
	}
	vk::ShaderModule GetFragmentShader(bool textured)
	{
		vk::UniqueShaderModule& shader = fragmentShaders[textured];
		if (!shader)
			shader = compileFragmentShader(textured);
		return *shader;
	}
	vk::ShaderModule GetScreenVertexShader()
	{
		if (!screenVertexShader)
			screenVertexShader = compileScreenVertexShader();
		return *screenVertexShader;
	}
	// The frame blender or the final picture
	vk::ShaderModule GetScreenFragmentShader(bool composite)
	{
		vk::UniqueShaderModule& shader = screenFragmentShaders[composite];
		if (!shader)
			shader = compileScreenFragmentShader(composite);
		return *shader;
	}

	void term()
	{
		vertexShader.reset();
		fragmentShaders[0].reset();
		fragmentShaders[1].reset();
		screenVertexShader.reset();
		screenFragmentShaders[0].reset();
		screenFragmentShaders[1].reset();
	}

private:
	vk::UniqueShaderModule compileVertexShader();
	vk::UniqueShaderModule compileFragmentShader(bool textured);
	vk::UniqueShaderModule compileScreenVertexShader();
	vk::UniqueShaderModule compileScreenFragmentShader(bool composite);

	vk::UniqueShaderModule vertexShader;
	vk::UniqueShaderModule fragmentShaders[2];
	vk::UniqueShaderModule screenVertexShader;
	vk::UniqueShaderModule screenFragmentShaders[2];	// [composite]
};
