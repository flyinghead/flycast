/*
    Created on: Oct 27, 2019

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
#include "../vulkan.h"
#include "../utils.h"

enum class Pass { Depth, Color, OIT };

class OITShaderManager
{
public:
	struct VertexShaderParams
	{
		bool gouraud;

		u32 hash() { return (u32)gouraud; }
	};

	// alpha test, clip test, use alpha, texture, ignore alpha, shader instr, offset, fog, gouraud, bump, clamp
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
		bool twoVolume;
		bool palette;
		Pass pass;

		u32 hash()
		{
			return ((u32)alphaTest) | ((u32)insideClipTest << 1) | ((u32)useAlpha << 2)
				| ((u32)texture << 3) | ((u32)ignoreTexAlpha << 4) | (shaderInstr << 5)
				| ((u32)offset << 7) | ((u32)fog << 8) | ((u32)gouraud << 10)
				| ((u32)bumpmap << 11) | ((u32)clamping << 12) | ((u32)twoVolume << 13)
				| ((u32)palette << 14) | ((int)pass << 15);
		}
	};

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
	vk::ShaderModule GetTrModVolShader(ModVolMode mode)
	{
		if (trModVolShaders.empty() || !trModVolShaders[(size_t)mode])
			compileTrModVolFragmentShader(mode);
		return *trModVolShaders[(size_t)mode];
	}

	vk::ShaderModule GetFinalShader()
	{
		if (!finalAutosortShader)
			finalAutosortShader = compileFinalShader();
		return *finalAutosortShader;
	}
	vk::ShaderModule GetFinalVertexShader()
	{
		if (!finalVertexShader)
			finalVertexShader = compileFinalVertexShader();
		return *finalVertexShader;
	}
	vk::ShaderModule GetClearShader()
	{
		if (!clearShader)
			clearShader = compileClearShader();
		return *clearShader;
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
	void compileTrModVolFragmentShader(ModVolMode mode);
	vk::UniqueShaderModule compileFinalShader();
	vk::UniqueShaderModule compileFinalVertexShader();
	vk::UniqueShaderModule compileClearShader();

	std::map<u32, vk::UniqueShaderModule> vertexShaders;
	std::map<u32, vk::UniqueShaderModule> fragmentShaders;
	vk::UniqueShaderModule modVolVertexShader;
	vk::UniqueShaderModule modVolShader;
	std::vector<vk::UniqueShaderModule> trModVolShaders;

	vk::UniqueShaderModule finalVertexShader;
	vk::UniqueShaderModule finalAutosortShader;
	vk::UniqueShaderModule finalSortedShader;
	vk::UniqueShaderModule clearShader;
};

