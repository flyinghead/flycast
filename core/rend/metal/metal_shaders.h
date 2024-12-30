/*
Copyright 2024 flyinghead

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
#include <Metal/Metal.hpp>

#include "types.h"
#include <glm/glm.hpp>
#include <map>


struct VertexShaderParams
{
    bool gouraud;
    bool naomi2;
    bool divPosZ;

    u32 hash() { return (u32)gouraud | ((u32)naomi2 << 1) | ((u32)divPosZ << 2); }
};

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
    int palette;
    bool divPosZ;
    bool dithering;

    u32 hash()
    {
        return ((u32)alphaTest) | ((u32)insideClipTest << 1) | ((u32)useAlpha << 2)
            | ((u32)texture << 3) | ((u32)ignoreTexAlpha << 4) | (shaderInstr << 5)
            | ((u32)offset << 7) | ((u32)fog << 8) | ((u32)gouraud << 10)
            | ((u32)bumpmap << 11) | ((u32)clamping << 12) | ((u32)trilinear << 13)
            | ((u32)palette << 14) | ((u32)divPosZ << 16) | ((u32)dithering << 17);
    }
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
    float ditherColorMax[4];
    float cp_AlphaTestValue;
    float sp_FOG_DENSITY;
};

class MetalShaders
{
public:
    MetalShaders();

    MTL::Function *GetVertexShader(const VertexShaderParams& params) { return getShader(vertexShaders, params); }
    MTL::Function *GetFragmentShader(const FragmentShaderParams& params) { return getShader(fragmentShaders, params); }

    void term()
    {
        for (auto &[u, func] : vertexShaders) {
            func->release();
        }

        vertexShaders.clear();

        for (auto &[u, func] : fragmentShaders) {
            func->release();
        }

        fragmentShaders.clear();

        vertexShaderLibrary->release();
        fragmentShaderLibrary->release();

        vertexShaderConstants->release();
        fragmentShaderConstants->release();
    }

private:
    MTL::Library *vertexShaderLibrary;
    MTL::Library *fragmentShaderLibrary;
    MTL::FunctionConstantValues *vertexShaderConstants;
    MTL::FunctionConstantValues *fragmentShaderConstants;

    template<typename T>
    MTL::Function *getShader(std::map<u32, MTL::Function*> &map, T params)
    {
        u32 h = params.hash();
        auto it = map.find(h);
        if (it != map.end())
            return it->second;
        map[h] = compileShader(params);
        return map[h];
    }
    MTL::Function *compileShader(const VertexShaderParams& params);
    MTL::Function *compileShader(const FragmentShaderParams& params);

    std::map<u32, MTL::Function*> vertexShaders;
    std::map<u32, MTL::Function*> fragmentShaders;
};
