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

#include <glm/glm.hpp>
#include <map>

struct VertexShaderParams
{
	bool gouraud;
	bool naomi2;
	bool divPosZ;

	u32 hash() { return (u32)gouraud | ((u32)naomi2 << 1) | ((u32)divPosZ << 2); }
};

// alpha test, clip test, use alpha, texture, ignore alpha, shader instr, offset, fog, gouraud, bump, clamp, trilinear
struct FragmentShaderParams
{
	bool alphaTest;
	bool insideClipTest;
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
	bool palette;
	bool divPosZ;

	u32 hash()
	{
		return ((u32)alphaTest) | ((u32)insideClipTest << 1) | ((u32)useAlpha << 2)
			| ((u32)texture << 3) | ((u32)ignoreTexAlpha << 4) | (shaderInstr << 5)
			| ((u32)offset << 7) | ((u32)fog << 8) | ((u32)gouraud << 10)
			| ((u32)bumpmap << 11) | ((u32)clamping << 12) | ((u32)trilinear << 13)
			| ((u32)palette << 14) | ((u32)divPosZ << 15);
	}
};

struct ModVolShaderParams
{
	bool naomi2;
	bool divPosZ;

	u32 hash() { return (u32)naomi2 | ((u32)divPosZ << 1); }
};

// std140 alignment required
struct VertexShaderUniforms
{
	glm::mat4 ndcMat;
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
};

// std140 alignment required
struct N2VertexShaderUniforms
{
	glm::mat4 mvMat;
	glm::mat4 normalMat;
	glm::mat4 projMat;
	int envMapping[2];
	int bumpMapping;
	int polyNumber;

	float glossCoef[2];
	int constantColor[2];
};

class ShaderManager
{
public:
	vk::ShaderModule GetVertexShader(const VertexShaderParams& params) { return getShader(vertexShaders, params); }
	vk::ShaderModule GetFragmentShader(const FragmentShaderParams& params) { return getShader(fragmentShaders, params); }
	vk::ShaderModule GetModVolVertexShader(const ModVolShaderParams& params) { return getShader(modVolVertexShaders, params); }

	vk::ShaderModule GetModVolShader(bool divPosZ)
	{
		auto& modVolShader = modVolShaders[divPosZ];
		if (!modVolShader)
			modVolShader = compileModVolFragmentShader(divPosZ);
		return *modVolShader;
	}
	vk::ShaderModule GetQuadVertexShader(bool rotate = false)
	{
		if (rotate)
		{
			if (!quadRotateVertexShader)
				quadRotateVertexShader = compileQuadVertexShader(true);
			return *quadRotateVertexShader;
		}
		else
		{
			if (!quadVertexShader)
				quadVertexShader = compileQuadVertexShader(false);
			return *quadVertexShader;
		}
	}
	vk::ShaderModule GetQuadFragmentShader(bool ignoreTexAlpha)
	{
		if (ignoreTexAlpha)
		{
			if (!quadNoAlphaFragmentShader)
				quadNoAlphaFragmentShader = compileQuadFragmentShader(true);
			return *quadNoAlphaFragmentShader;
		}
		else
		{
			if (!quadFragmentShader)
				quadFragmentShader = compileQuadFragmentShader(false);
			return *quadFragmentShader;
		}
	}
	vk::ShaderModule GetOSDVertexShader()
	{
		if (!osdVertexShader)
			osdVertexShader = compileOSDVertexShader();
		return *osdVertexShader;
	}
	vk::ShaderModule GetOSDFragmentShader()
	{
		if (!osdFragmentShader)
			osdFragmentShader = compileOSDFragmentShader();
		return *osdFragmentShader;
	}

private:
	template<typename T>
	vk::ShaderModule getShader(std::map<u32, vk::UniqueShaderModule>& map, T params)
	{
		u32 h = params.hash();
		auto it = map.find(h);
		if (it != map.end())
			return it->second.get();
		map[h] = compileShader(params);
		return map[h].get();
	}
	vk::UniqueShaderModule compileShader(const VertexShaderParams& params);
	vk::UniqueShaderModule compileShader(const FragmentShaderParams& params);
	vk::UniqueShaderModule compileShader(const ModVolShaderParams& params);
	vk::UniqueShaderModule compileModVolFragmentShader(bool divPosZ);
	vk::UniqueShaderModule compileQuadVertexShader(bool rotate);
	vk::UniqueShaderModule compileQuadFragmentShader(bool ignoreTexAlpha);
	vk::UniqueShaderModule compileOSDVertexShader();
	vk::UniqueShaderModule compileOSDFragmentShader();

	std::map<u32, vk::UniqueShaderModule> vertexShaders;
	std::map<u32, vk::UniqueShaderModule> fragmentShaders;
	std::map<u32, vk::UniqueShaderModule> modVolVertexShaders;
	vk::UniqueShaderModule modVolShaders[2];
	vk::UniqueShaderModule quadVertexShader;
	vk::UniqueShaderModule quadRotateVertexShader;
	vk::UniqueShaderModule quadFragmentShader;
	vk::UniqueShaderModule quadNoAlphaFragmentShader;
	vk::UniqueShaderModule osdVertexShader;
	vk::UniqueShaderModule osdFragmentShader;
};
