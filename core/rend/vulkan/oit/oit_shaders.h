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
#include "cfg/option.h"

#include <map>

enum class Pass { Depth, Color, OIT };

class OITShaderManager
{
public:
	struct VertexShaderParams
	{
		bool gouraud;
		bool naomi2;
		bool lightOn;
		bool twoVolume;
		bool texture;
		bool divPosZ;

		u32 hash() { return (u32)gouraud | ((u32)naomi2 << 1) | ((u32)lightOn << 2)
				| ((u32)twoVolume << 3) | ((u32)texture << 4) | ((u32)divPosZ << 5); }
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
		bool divPosZ;
		Pass pass;

		u32 hash()
		{
			return ((u32)alphaTest) | ((u32)insideClipTest << 1) | ((u32)useAlpha << 2)
				| ((u32)texture << 3) | ((u32)ignoreTexAlpha << 4) | (shaderInstr << 5)
				| ((u32)offset << 7) | ((u32)fog << 8) | ((u32)gouraud << 10)
				| ((u32)bumpmap << 11) | ((u32)clamping << 12) | ((u32)twoVolume << 13)
				| ((u32)palette << 14) | ((int)pass << 15) | ((u32)divPosZ << 17);
		}
	};

	struct ModVolShaderParams
	{
		bool naomi2;
		bool divPosZ;

		u32 hash() { return (u32)naomi2 | ((u32)divPosZ << 1); }
	};

	struct TrModVolShaderParams
	{
		ModVolMode mode;
		bool divPosZ;

		u32 hash() { return (u32)mode | ((u32)divPosZ << 3); }
	};

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

	vk::ShaderModule GetTrModVolShader(const TrModVolShaderParams& params) { return getShader(trModVolShaders, params); }

	vk::ShaderModule GetFinalShader()
	{
		if (!finalFragmentShader || maxLayers != config::PerPixelLayers)
		{
			if (maxLayers != config::PerPixelLayers)
				trModVolShaders.clear();
			finalFragmentShader = compileFinalShader();
			maxLayers = config::PerPixelLayers;
		}
		return *finalFragmentShader;
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

	void term()
	{
		vertexShaders.clear();
		fragmentShaders.clear();
		modVolVertexShaders.clear();
		modVolShaders[0].reset();
		modVolShaders[1].reset();
		trModVolShaders.clear();

		finalVertexShader.reset();
		finalFragmentShader.reset();
		clearShader.reset();
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
	vk::UniqueShaderModule compileShader(const TrModVolShaderParams& params);
	vk::UniqueShaderModule compileFinalShader();
	vk::UniqueShaderModule compileFinalVertexShader();
	vk::UniqueShaderModule compileClearShader();

	std::map<u32, vk::UniqueShaderModule> vertexShaders;
	std::map<u32, vk::UniqueShaderModule> fragmentShaders;
	std::map<u32, vk::UniqueShaderModule> modVolVertexShaders;
	vk::UniqueShaderModule modVolShaders[2];
	std::map<u32, vk::UniqueShaderModule> trModVolShaders;

	vk::UniqueShaderModule finalVertexShader;
	vk::UniqueShaderModule finalFragmentShader;
	vk::UniqueShaderModule clearShader;
	int maxLayers = 0;
};

