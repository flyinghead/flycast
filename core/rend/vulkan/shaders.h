/*
 *  Created on: Oct 3, 2019

	Copyright 2019 flyinghead

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
#include "vulkan.h"
#include "SPIRV/GlslangToSpv.h"

struct VertexShaderParams
{
	bool gouraud;
	bool rotate90;

	u32 hash() { return (((u32)gouraud) << 1) | rotate90; }
};

// alpha test, clip test, use alpha, texture, ignore alpha, shader instr, offset, fog, gouraud, bump, clamp, trilinear
struct FragmentShaderParams
{
	bool alphaTest;
	int clipTest;
	bool useAlpha;
	bool texture;
	bool ignoreTexAlpha;
	int shaderInstr;
	bool offset;
	int fog;
	bool gouraud;
	bool bumpmap;
	bool clamping;
	bool trilinear;

	u32 hash()
	{
		return ((u32)alphaTest) | ((clipTest + 1) << 1) | ((u32)useAlpha << 3)
			| ((u32)texture << 4) | ((u32)ignoreTexAlpha << 5) | (shaderInstr << 6)
			| ((u32)offset << 8) | ((u32)fog << 9) | ((u32)gouraud << 11)
			| ((u32)bumpmap << 12) | ((u32)clamping << 13) | ((u32)trilinear << 14);
	}
};

// std140 alignment required
struct VertexShaderUniforms
{
	float scale[4];
	float extra_depth_scale;
};

// std140 alignment required
struct FragmentShaderUniforms
{
	float colorClampMin[4];
	float colorClampMax[4];
	float sp_FOG_COL_RAM[4];	// Only using 3 elements but easier for std140
	float sp_FOG_COL_VERT[4];	// same comment
	float cp_AlphaTestValue;
	float sp_FOG_DENSITY;
	float extra_depth_scale;
};

class ShaderManager
{
public:
	void Init()
	{
		verify(glslang::InitializeProcess());
	}
	void Term()
	{
		glslang::FinalizeProcess();
	}
	vk::ShaderModule GetVertexShader(const VertexShaderParams& params) { return getShader(vertexShaders, params); }
	vk::ShaderModule GetFragmentShader(const FragmentShaderParams& params) { return getShader(fragmentShaders, params); }
	vk::ShaderModule GetModVolVertexShader()
	{
		if (!modVolVertexShader)
			modVolVertexShader = compileModVolVertexShader();
		return *modVolVertexShader;
	}
	vk::ShaderModule GetModVolShader()
	{
		if (!modVolShader)
			modVolShader = compileModVolFragmentShader();
		return *modVolShader;
	}

private:
	template<typename T>
	vk::ShaderModule getShader(std::map<u32, vk::UniqueShaderModule>& map, T params)
	{
		auto it = map.find(params.hash());
		if (it != map.end())
			return it->second.get();
		map[params.hash()] = compileShader(params);
		return map[params.hash()].get();
	}
	vk::UniqueShaderModule compileShader(const VertexShaderParams& params);
	vk::UniqueShaderModule compileShader(const FragmentShaderParams& params);
	vk::UniqueShaderModule compileModVolVertexShader();
	vk::UniqueShaderModule compileModVolFragmentShader();

	std::map<u32, vk::UniqueShaderModule> vertexShaders;
	std::map<u32, vk::UniqueShaderModule> fragmentShaders;
	vk::UniqueShaderModule modVolVertexShader;
	vk::UniqueShaderModule modVolShader;
};
