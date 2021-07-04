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
#include "d3d_shaders.h"

#define SHADER_DEBUG 0 // D3DXSHADER_DEBUG|D3DXSHADER_SKIPOPTIMIZATION

const char *VertexShader = R"(
struct vertex_in
{
	float4 pos : POSITION;
	float4 col : COLOR0;
	float4 spc : COLOR1;
	float2 uv : TEXCOORD0;
};

struct vertex_out
{
	float4 pos : POSITION;
	float4 uv : TEXCOORD0;
	float4 col : COLOR0;
	float4 spc : COLOR1;
};

float4x4 normal_matrix : register(c0);

vertex_out main(in vertex_in vin)
{
	vertex_out vo;
	vo.pos = mul(normal_matrix, vin.pos);
#if pp_Gouraud == 1
	vo.col = vin.col * vo.pos.z;
	vo.spc = vin.spc * vo.pos.z;
#else
	// flat shading: no interpolation
	vo.col = vin.col;
	vo.spc = vin.spc;
#endif
	vo.uv = float4(vin.uv * vo.pos.z, 0, vo.pos.z);

	vo.pos.w = 1.0f;
	vo.pos.z = 0;

	return vo;
}

)";

const char *PixelShader = R"(

#define PI 3.1415926f

struct pixel 
{
	float4 uv : TEXCOORD0;
	float4 col : COLOR0;
#if pp_Texture == 1 && (pp_BumpMap == 1 || pp_Offset == 1)
	float4 offs : COLOR1;
#endif
	
};
 
sampler2D samplr : register(s0);
sampler2D tex_pal : register(s1);
sampler2D fog_table : register(s2);

float4 palette_index : register(c0);
float4 FOG_COL_VERT : register(c1);
float4 FOG_COL_RAM : register(c2);
float4 FOG_DENSITY_SCALE : register(c3);
float4 ClipTest : register(c4);
float4 trilinear_alpha : register(c5);
float4 fog_clamp_min : register(c6);
float4 fog_clamp_max : register(c7);

float fog_mode2(float w)
{
	float z = clamp(w * FOG_DENSITY_SCALE.x, 1.0f, 255.9999f);
	float exp = floor(log2(z));
	float m = z * 16.0f / pow(2.0, exp) - 16.0f;
	float idx = floor(m) + exp * 16.0f + 0.5f;
	float4 fog_coef = tex2D(fog_table, float2(idx / 128.0f, 0.75f - (m - floor(m)) / 2.0f));
	return fog_coef.a;
}

float4 fog_clamp(float4 col)
{
#if FogClamping == 1
	return clamp(col, fog_clamp_min, fog_clamp_max);
#else
	return col;
#endif
}

#if pp_Palette == 1

float4 palettePixel(float4 coords)
{
	int color_idx = int(floor(tex2Dproj(samplr, coords).a * 255.0f + 0.5f) + palette_index.x);
    float2 c = float2((fmod(float(color_idx), 32.0f) * 2.0f + 1.0f) / 64.0f, (float(color_idx / 32) * 2.0f + 1.0f) / 64.0f);
	return tex2D(tex_pal, c);
}

#endif

struct PSO
{
	float4 col : COLOR0;
	float z : DEPTH;
};

PSO main(in pixel inpix)
{ 
	// Clip inside the box
/* TODO
	#if pp_ClipInside == 1
		if (VPOS.x >= pp_ClipTest.x && VPOS.x <= pp_ClipTest.z
				&& VPOS.y >= pp_ClipTest.y && VPOS.y <= pp_ClipTest.w)
			discard;
	#endif
*/

#if pp_Gouraud == 1
	float4 color = inpix.col / inpix.uv.w;
	#if pp_Texture == 1 && (pp_BumpMap == 1 || pp_Offset == 1)
		float4 offset = inpix.offs / inpix.uv.w;
	#endif
#else
	float4 color = inpix.col;
	#if pp_Texture == 1 && (pp_BumpMap == 1 || pp_Offset == 1)
		float4 offset = inpix.offs;
	#endif
#endif
	#if pp_UseAlpha == 0
		color.a = 1.0f;
	#endif
	#if pp_FogCtrl == 3
		color = float4(FOG_COL_RAM.rgb, fog_mode2(inpix.uv.w));
	#endif
	#if pp_Texture == 1
	{
		#if pp_Palette == 0
			float4 texcol = tex2Dproj(samplr, inpix.uv);
		#else
			float4 texcol = palettePixel(inpix.uv);
		#endif
		
		#if pp_BumpMap == 1
			float s = PI / 2.0f * (texcol.a * 15.0f * 16.0f + texcol.r * 15.0f) / 255.0f;
			float r = 2.0f * PI * (texcol.g * 15.0f * 16.0f + texcol.b * 15.0f) / 255.0f;
			texcol[3] = clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2.0f * PI * offset.b), 0.0f, 1.0f);
			texcol.rgb = float3(1.0f, 1.0f, 1.0f);	
		#else
			#if pp_IgnoreTexA == 1
				texcol.a = 1.0f;
			#endif
		#endif
		#if pp_ShadInstr == 0
			color = texcol;
		#endif
		#if pp_ShadInstr == 1
			color.rgb *= texcol.rgb;
			color.a = texcol.a;
		#endif
		#if pp_ShadInstr == 2
			color.rgb = lerp(color.rgb, texcol.rgb, texcol.a);
		#endif
		#if  pp_ShadInstr == 3
			color *= texcol;
		#endif
		
		#if pp_Offset == 1 && pp_BumpMap == 0
			color.rgb += offset.rgb;
		#endif
	}
	#endif
	
	color = fog_clamp(color);
	
	#if pp_FogCtrl == 0
		color.rgb = lerp(color.rgb, FOG_COL_RAM.rgb, fog_mode2(inpix.uv.w)); 
	#endif
	#if pp_FogCtrl == 1 && pp_Offset == 1 && pp_BumpMap == 0
		color.rgb = lerp(color.rgb, FOG_COL_VERT.rgb, offset.a);
	#endif
	
	#if pp_TriLinear == 1
	color *= trilinear_alpha;
	#endif

	//color.rgb = float3(inpix.uv.w * FOG_DENSITY_SCALE.x / 128.0f);
	PSO pso;
	float w = inpix.uv.w * 100000.0f;
	pso.z = log2(1.0f + w) / 34.0f;
	pso.col = color;

	return pso;
}

PSO modifierVolume(float4 uv : TEXCOORD0)
{
	PSO pso;
	float w = uv.w * 100000.0f;
	pso.z = log2(1.0f + w) / 34.0f;
	pso.col = float4(0, 0, 0, FOG_DENSITY_SCALE.y);

	return pso;
}
)";

const char *MacroValues[] { "0", "1", "2", "3" };

static D3DXMACRO VertexMacros[]
{
	{ "pp_Gouraud", "1" },
	{ 0, 0 }
};

constexpr u32 MacroTexture = 0;
constexpr u32 MacroOffset = 1;
constexpr u32 MacroShadInstr = 2;
constexpr u32 MacroIgnoreTexA = 3;
constexpr u32 MacroUseAlpha = 4;
constexpr u32 MacroFogCtrl = 5;
constexpr u32 MacroFogClamping = 6;
constexpr u32 MacroPalette = 7;
constexpr u32 MacroBumpMap = 8;
constexpr u32 MacroTriLinear = 9;
constexpr u32 MacroGouraud = 10;

static D3DXMACRO PixelMacros[]
{
	{ "pp_Texture", "0" },
	{ "pp_Offset", "0" },
	{ "pp_ShadInstr", "0" },
	{ "pp_IgnoreTexA", "0" },
	{ "pp_UseAlpha", "0" },
	{ "pp_FogCtrl", "0" },
	{ "FogClamping", "0" },
	{ "pp_Palette", "0" },
	{ "pp_BumpMap", "0" },
	{ "pp_TriLinear", "0" },
	{ "pp_Gouraud", "1" },
	{0, 0}
	//{ "pp_ClipInside", "0" },	// TODO
};

const ComPtr<IDirect3DPixelShader9>& D3DShaders::getShader(bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr,
		bool pp_Offset, u32 pp_FogCtrl, bool pp_BumpMap, bool fog_clamping,
		bool trilinear, bool palette, bool gouraud)
{
	u32 hash = (int)pp_Texture
			| (pp_UseAlpha << 1)
			| (pp_IgnoreTexA << 2)
			| (pp_ShadInstr << 3)
			| (pp_Offset << 5)
			| (pp_FogCtrl << 6)
			| (pp_BumpMap << 8)
			| (fog_clamping << 9)
			| (trilinear << 10)
			| (palette << 11)
			| (gouraud << 12);
	auto it = shaders.find(hash);
	if (it == shaders.end())
	{
		verify(pp_ShadInstr < ARRAY_SIZE(MacroValues));
		verify(pp_FogCtrl < ARRAY_SIZE(MacroValues));
		PixelMacros[MacroTexture].Definition = MacroValues[pp_Texture];
		PixelMacros[MacroUseAlpha].Definition = MacroValues[pp_UseAlpha];
		PixelMacros[MacroIgnoreTexA].Definition = MacroValues[pp_IgnoreTexA];
		PixelMacros[MacroShadInstr].Definition = MacroValues[pp_ShadInstr];
		PixelMacros[MacroOffset].Definition = MacroValues[pp_Offset];
		PixelMacros[MacroFogCtrl].Definition = MacroValues[pp_FogCtrl];
		PixelMacros[MacroBumpMap].Definition = MacroValues[pp_BumpMap];
		PixelMacros[MacroFogClamping].Definition = MacroValues[fog_clamping];
		PixelMacros[MacroTriLinear].Definition = MacroValues[trilinear];
		PixelMacros[MacroPalette].Definition = MacroValues[palette];
		PixelMacros[MacroGouraud].Definition = MacroValues[gouraud];
		ComPtr<IDirect3DPixelShader9> shader = compilePS(PixelShader, "main", PixelMacros);
		verify((bool )shader);
		it = shaders.insert(std::make_pair(hash, shader)).first;
	}
	return it->second;
}

const ComPtr<IDirect3DVertexShader9>& D3DShaders::getVertexShader(bool gouraud)
{
	ComPtr<IDirect3DVertexShader9>& vertexShader = gouraud ? gouraudVertexShader : flatVertexShader;
	if (!vertexShader)
	{
		VertexMacros[0].Definition = MacroValues[gouraud];
		vertexShader = compileVS(VertexShader, "main", VertexMacros);
	}

	return vertexShader;
}

const ComPtr<IDirect3DPixelShader9>& D3DShaders::getModVolShader()
{
	if (!modVolShader)
		modVolShader = compilePS(PixelShader, "modifierVolume", PixelMacros);

	return modVolShader;
}

ComPtr<ID3DXBuffer> D3DShaders::compileShader(const char* source, const char* function, const char* profile, const D3DXMACRO* pDefines)
{
	ComPtr<ID3DXBuffer> errors;
	ComPtr<ID3DXBuffer> shader;
	ComPtr<ID3DXConstantTable> constants;
	D3DXCompileShader(source, strlen(source), pDefines, NULL, function, profile, SHADER_DEBUG, &shader.get(), &errors.get(), &constants.get());
	if (errors) {
		char *text = (char *) errors->GetBufferPointer();
		WARN_LOG(RENDERER, "%s", text);
	}
	return shader;
}

ComPtr<IDirect3DVertexShader9> D3DShaders::compileVS(const char* source, const char* function, const D3DXMACRO* pDefines)
{
	ComPtr<ID3DXBuffer> buffer = compileShader(source, function, D3DXGetVertexShaderProfile(device), pDefines);
	ComPtr<IDirect3DVertexShader9> shader;
	if (buffer)
		device->CreateVertexShader((DWORD *)buffer->GetBufferPointer(), &shader.get());

	return shader;
}

ComPtr<IDirect3DPixelShader9> D3DShaders::compilePS(const char* source, const char* function, const D3DXMACRO* pDefines)
{
	ComPtr<ID3DXBuffer> buffer = compileShader(source, function, D3DXGetPixelShaderProfile(device), pDefines);
	ComPtr<IDirect3DPixelShader9> shader;
	if (buffer)
		device->CreatePixelShader((DWORD *)buffer->GetBufferPointer(), &shader.get());

	return shader;
}
