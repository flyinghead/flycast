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
#include "dx11_shaders.h"
#include "dx11context.h"
#include "stdclass.h"
#include "dx11_naomi2.h"
#include <xxhash.h>

const char * const VertexShader = R"(
#if pp_Gouraud == 1
#define INTERPOLATION
#else
#define INTERPOLATION nointerpolation
#endif

struct VertexIn
{
	float4 pos : POSITION;
	float4 col : COLOR0;
	float4 spec : COLOR1;
	float2 uv : TEXCOORD0;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float4 uv : TEXCOORD0;
	INTERPOLATION float4 col : COLOR0;
	INTERPOLATION float4 spec : COLOR1;
};

cbuffer constantBuffer : register(b0)
{
	float4x4 transMatrix;
	float4 leftPlane;
	float4 topPlane;
	float4 rightPlane;
	float4 bottomPlane;
};

[clipplanes(leftPlane, topPlane, rightPlane, bottomPlane)]
VertexOut main(in VertexIn vin)
{
	VertexOut vo;
	vo.pos = mul(transMatrix, float4(vin.pos.xyz, 1.f));
#if DIV_POS_Z == 1
	vo.pos /= vo.pos.z;
	vo.pos.z = vo.pos.w;
#endif
	vo.col = vin.col;
	vo.spec = vin.spec;
#if pp_Gouraud == 1 && DIV_POS_Z != 1
	vo.col *= vo.pos.z;
	vo.spec *= vo.pos.z;
#endif
	vo.uv.xyz = float3(vin.uv, 0.f);

#if DIV_POS_Z == 1
	vo.uv.w = vo.pos.w;
#else
	vo.uv.xy *= vo.pos.z;
	vo.uv.w = vo.pos.z;
	vo.pos.w = 1.f;
	vo.pos.z = 0.f;
#endif

	return vo;
}

)";

const char *ModVolVertexShader = R"(
struct VertexIn
{
	float4 pos : POSITION;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float4 uv : TEXCOORD0;
};

cbuffer constantBuffer : register(b0)
{
	float4x4 transMatrix;
	float4 leftPlane;
	float4 topPlane;
	float4 rightPlane;
	float4 bottomPlane;
};

VertexOut main(in VertexIn vin)
{
	VertexOut vo;
	vo.pos = mul(transMatrix, float4(vin.pos.xyz, 1.f));
#if DIV_POS_Z == 1
	vo.pos /= vo.pos.z;
	vo.pos.z = vo.pos.w;
	vo.uv = float4(0.f, 0.f, 0.f, vo.pos.w);
#else
	vo.uv = float4(0.f, 0.f, 0.f, vo.pos.z);
	vo.pos.w = 1.f;
	vo.pos.z = 0.f;
#endif
	return vo;
}

)";

extern const char * const PixelShaderCommon = R"(
#if pp_Gouraud == 1
#define INTERPOLATION
#else
#define INTERPOLATION nointerpolation
#endif

#define PI 3.1415926f

Texture2D texture0 : register(t0);
sampler sampler0 : register(s0);

Texture2D paletteTexture : register(t1);
sampler paletteSampler : register(s1);

Texture2D fogTexture : register(t2);
sampler fogSampler : register(s2);

float fog_mode2(float w)
{
	float z = clamp(
#if DIV_POS_Z == 1
					fogDensity / w
#else
					fogDensity * w
#endif
									, 1.0f, 255.9999f);
	float exp = floor(log2(z));
	float m = z * 16.0f / pow(2.0, exp) - 16.0f;
	float idx = floor(m) + exp * 16.0f + 0.5f;
	float4 fogCoef = fogTexture.Sample(fogSampler, float2(idx / 128.0f, 0.75f - (m - floor(m)) / 2.0f));
	return fogCoef.a;
}

float4 clampColor(float4 color)
{
#if FogClamping == 1
	return clamp(color, colorClampMin, colorClampMax);
#else
	return color;
#endif
}

#if pp_Palette != 0

float4 getPaletteEntry(float colIdx)
{
	uint colorIdx = int(floor(colIdx * 255.0f + 0.5f) + paletteIndex);
    float2 c = float2((fmod(float(colorIdx), 32.0f) * 2.0f + 1.0f) / 64.0f, (float(colorIdx / 32) * 2.0f + 1.0f) / 64.0f);
	return paletteTexture.Sample(paletteSampler, c);
}

#endif

#if pp_Palette == 1

float4 palettePixel(Texture2D tex, sampler texSampler, float2 coords)
{
	return getPaletteEntry(tex.Sample(texSampler, coords).a);
}

#elif pp_Palette == 2

float4 palettePixelBilinear(Texture2D tex, sampler texSampler, float2 coords)
{
	float2 textureSize;
	tex.GetDimensions(textureSize.x, textureSize.y);
	float2 pixCoord = coords * textureSize - 0.5f;				// coordinates of top left pixel
	float2 originPixCoord = floor(pixCoord);

	float2 sampleUV = (originPixCoord + 0.5f) / textureSize;	// UV coordinates of center of top left pixel

    // Sample from all surrounding texels
    float4 c00 = getPaletteEntry(tex.Sample(texSampler, sampleUV).a);
    float4 c01 = getPaletteEntry(tex.Sample(texSampler, sampleUV, int2(0, 1)).a);
    float4 c11 = getPaletteEntry(tex.Sample(texSampler, sampleUV, int2(1, 1)).a);
    float4 c10 = getPaletteEntry(tex.Sample(texSampler, sampleUV, int2(1, 0)).a);

	float2 weight = pixCoord - originPixCoord;

    // Bi-linear mixing
    float4 temp0 = lerp(c00, c10, weight.x);
    float4 temp1 = lerp(c01, c11, weight.x);
    return lerp(temp0, temp1, weight.y);
}

#endif

)";

const char * const PixelShader = R"(

cbuffer constantBuffer : register(b0)
{
	float4 colorClampMin;
	float4 colorClampMax;
	float4 FOG_COL_VERT;
	float4 FOG_COL_RAM;
	float4 ditherColorMax;
	float fogDensity;
	float shadowScale;
	float alphaTestValue;
};

cbuffer polyConstantBuffer : register(b1)
{
	float4 clipTest;
	float paletteIndex;
	float trilinearAlpha;
};

#include "pixel_common.hlsl"

struct Pixel 
{
	float4 pos : SV_POSITION;
	float4 uv : TEXCOORD0;
	INTERPOLATION float4 col : COLOR0;
	INTERPOLATION float4 spec : COLOR1;
};

struct PSO
{
	float4 col : SV_TARGET;
	float z : SV_DEPTH;
};

PSO main(in Pixel inpix)
{
#if pp_ClipInside == 1
	// Clip inside the box
	if (inpix.pos.x >= clipTest.x && inpix.pos.x <= clipTest.z
			&& inpix.pos.y >= clipTest.y && inpix.pos.y <= clipTest.w)
		discard;
#endif
	float4 color = inpix.col;
	#if pp_BumpMap == 1 || pp_Offset == 1
		float4 specular = inpix.spec;
	#endif
	#if pp_Gouraud == 1 && DIV_POS_Z != 1
		color /= inpix.uv.w;
		#if pp_BumpMap == 1 || pp_Offset == 1
			specular /= inpix.uv.w;
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
		float2 uv = inpix.uv.xy;
		#if DIV_POS_Z != 1
			uv /= inpix.uv.w;
		#endif
		#if pp_Palette == 0
			float4 texcol = texture0.Sample(sampler0, uv);
		#elif pp_Palette == 1
			float4 texcol = palettePixel(texture0, sampler0, uv);
		#else
			float4 texcol = palettePixelBilinear(texture0, sampler0, uv);
		#endif
		
		#if pp_BumpMap == 1
			float s = PI / 2.0f * (texcol.a * 15.0f * 16.0f + texcol.r * 15.0f) / 255.0f;
			float r = 2.0f * PI * (texcol.g * 15.0f * 16.0f + texcol.b * 15.0f) / 255.0f;
			texcol[3] = clamp(specular.a + specular.r * sin(s) + specular.g * cos(s) * cos(r - 2.0f * PI * specular.b), 0.0f, 1.0f);
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
			color.rgb += specular.rgb;
		#endif
	}
	#endif
	
	color = clampColor(color);
	
	#if pp_FogCtrl == 0
		color.rgb = lerp(color.rgb, FOG_COL_RAM.rgb, fog_mode2(inpix.uv.w)); 
	#endif
	#if pp_FogCtrl == 1 && pp_Offset == 1 && pp_BumpMap == 0
		color.rgb = lerp(color.rgb, FOG_COL_VERT.rgb, specular.a);
	#endif
	
	#if pp_TriLinear == 1
	color *= trilinearAlpha;
	#endif

	#if cp_AlphaTest == 1
		color.a = round(color.a * 255.0f) * 0.0039215688593685626983642578125; // 1 / 255
		if (alphaTestValue > color.a)
			discard;
		color.a = 1.0f;
	#endif

#if DITHERING == 1
	static const float ditherTable[16] = {
		 0.9375f,  0.1875f,  0.75f,  0.0f,   
		 0.4375f,  0.6875f,  0.25f,  0.5f,
		 0.8125f,  0.0625f,  0.875f, 0.125f,
		 0.3125f,  0.5625f,  0.375f, 0.625f	
	};
	float r = ditherTable[int(inpix.pos.y % 4.0f) * 4 + int(inpix.pos.x % 4.0f)] + 0.03125f; // why is this bias needed??
	// 31 for 5-bit color, 63 for 6 bits, 15 for 4 bits
	color += r / ditherColorMax;
	// avoid rounding
	color = floor(color * 255.0f) / 255.0f;
#endif
	PSO pso;
#if DIV_POS_Z == 1
	float w = 100000.0f / inpix.uv.w;
#else
	float w = 100000.0f * inpix.uv.w;
#endif
	pso.z = log2(1.0f + max(w, -0.999999f)) / 34.0f;
	pso.col = color;

	return pso;
}

struct MVPixel 
{
	float4 pos : SV_POSITION;
	float4 uv : TEXCOORD0;
};

PSO modifierVolume(in MVPixel inpix)
{
	PSO pso;
#if DIV_POS_Z == 1
	float w = 100000.0f / inpix.uv.w;
#else
	float w = 100000.0f * inpix.uv.w;
#endif
	pso.z = log2(1.0f + max(w, -0.999999f)) / 34.0f;
	pso.col = float4(0, 0, 0, 1.f - shadowScale);

	return pso;
}
)";

const char * const QuadVertexShader = R"(
struct VertexIn
{
	float2 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VertexOut main(in VertexIn vin)
{
	VertexOut vo;
#if ROTATE == 0
	vo.pos = float4(vin.pos, 0.f, 1.f);
#else
	vo.pos = float4(-vin.pos.y, vin.pos.x, 0.f, 1.f);
#endif
	vo.uv = vin.uv;

	return vo;
}
)";

const char * const QuadPixelShader = R"(
cbuffer constantBuffer : register(b0)
{
	float4 color;
};

struct VertexIn
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

sampler sampler0;
Texture2D texture0;

float4 main(in VertexIn vin) : SV_Target
{
	return color * texture0.Sample(sampler0, vin.uv);
}

)";

struct IncludeManager : public ID3DInclude
{
	HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) override
	{
		if (!strcmp(pFileName, "pixel_common.hlsl"))
		{
			*ppData = PixelShaderCommon;
			*pBytes = (UINT)strlen(PixelShaderCommon);
			return S_OK;
		}
		return E_FAIL;
	}

	HRESULT STDMETHODCALLTYPE Close(LPCVOID pData) override {
		return S_OK;
	}
};

const char * const MacroValues[] { "0", "1", "2", "3" };

enum VertexMacroEnum {
	MacroGouraud,
	MacroDivPosZ,
	MacroPositionOnly,
	MacroTwoVolumes,
	MacroLightOn,
	MacroModifierVolume,
};

static D3D_SHADER_MACRO VertexMacros[]
{
	{ "pp_Gouraud", "1" },
	{ "DIV_POS_Z", "0" },
	{ "POSITION_ONLY", "0" },
	{ "pp_TwoVolumes", "0" },
	{ "LIGHT_ON", "1" },
	{ "MODIFIER_VOLUME", "0" },
	{ nullptr, nullptr }
};

enum PixelMacroEnum {
	MacroTexture = 2,
	MacroUseAlpha,
	MacroIgnoreTexA,
	MacroShadInstr,
	MacroOffset,
	MacroFogCtrl,
	MacroBumpMap,
	MacroFogClamping,
	MacroTriLinear,
	MacroPalette,
	MacroAlphaTest,
	MacroClipInside,
	MacroDithering
};

static D3D_SHADER_MACRO PixelMacros[]
{
	{ "pp_Gouraud", "1" },
	{ "DIV_POS_Z", "0" },
	{ "pp_Texture", "0" },
	{ "pp_UseAlpha", "0" },
	{ "pp_IgnoreTexA", "0" },
	{ "pp_ShadInstr", "0" },
	{ "pp_Offset", "0" },
	{ "pp_FogCtrl", "0" },
	{ "pp_BumpMap", "0" },
	{ "FogClamping", "0" },
	{ "pp_TriLinear", "0" },
	{ "pp_Palette", "0" },
	{ "cp_AlphaTest", "0" },
	{ "pp_ClipInside", "0" },
	{ "DITHERING", "0" },
	{ nullptr, nullptr }
};

const ComPtr<ID3D11PixelShader>& DX11Shaders::getShader(bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr,
		bool pp_Offset, u32 pp_FogCtrl, bool pp_BumpMap, bool fog_clamping,
		bool trilinear, int palette, bool gouraud, bool alphaTest, bool clipInside, bool dithering)
{
	bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
	const u32 hash = (int)pp_Texture
			| (pp_UseAlpha << 1)
			| (pp_IgnoreTexA << 2)
			| (pp_ShadInstr << 3)
			| (pp_Offset << 5)
			| (pp_FogCtrl << 6)
			| (pp_BumpMap << 8)
			| (fog_clamping << 9)
			| (trilinear << 10)
			| (palette << 11)
			| (gouraud << 13)
			| (alphaTest << 14)
			| (clipInside << 15)
			| (divPosZ << 16)
			| (dithering << 17);
	auto& shader = shaders[hash];
	if (shader == nullptr)
	{
		verify(pp_ShadInstr < std::size(MacroValues));
		verify(pp_FogCtrl < std::size(MacroValues));
		PixelMacros[MacroGouraud].Definition = MacroValues[gouraud];
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
		PixelMacros[MacroAlphaTest].Definition = MacroValues[alphaTest];
		PixelMacros[MacroClipInside].Definition = MacroValues[clipInside];
		PixelMacros[MacroDivPosZ].Definition = MacroValues[divPosZ];
		PixelMacros[MacroDithering].Definition = MacroValues[dithering];

		shader = compilePS(PixelShader, "main", PixelMacros);
		verify(shader != nullptr);
	}
	return shader;
}

const ComPtr<ID3D11VertexShader>& DX11Shaders::getVertexShader(bool gouraud, bool naomi2)
{
	bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
	int index = (int)gouraud | ((int)naomi2 << 1) | ((int)divPosZ << 2);
	ComPtr<ID3D11VertexShader>& vertexShader = vertexShaders[index];
	if (!vertexShader)
	{
		VertexMacros[MacroGouraud].Definition = MacroValues[gouraud];
		if (!naomi2)
		{
			VertexMacros[MacroDivPosZ].Definition = MacroValues[divPosZ];
			vertexShader = compileVS(VertexShader, "main", VertexMacros);
		}
		else
		{
			VertexMacros[MacroPositionOnly].Definition = MacroValues[false];
			VertexMacros[MacroTwoVolumes].Definition = MacroValues[false];
			VertexMacros[MacroLightOn].Definition = MacroValues[true];
			VertexMacros[MacroModifierVolume].Definition = MacroValues[false];
			std::string source(DX11N2VertexShader);
			source += std::string("\n") + DX11N2ColorShader;
			vertexShader = compileVS(source.c_str(), "main", VertexMacros);
		}
	}

	return vertexShader;
}

const ComPtr<ID3D11VertexShader>& DX11Shaders::getMVVertexShader(bool naomi2)
{
	bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
	int index = (int)naomi2 | ((int)divPosZ << 1);
	if (!modVolVertexShaders[index])
	{
		if (!naomi2)
		{
			VertexMacros[MacroDivPosZ].Definition = MacroValues[divPosZ];
			modVolVertexShaders[index] = compileVS(ModVolVertexShader, "main", VertexMacros);
		}
		else
		{
			VertexMacros[MacroGouraud].Definition = MacroValues[false];
			VertexMacros[MacroPositionOnly].Definition = MacroValues[true];
			VertexMacros[MacroTwoVolumes].Definition = MacroValues[false];
			VertexMacros[MacroLightOn].Definition = MacroValues[false];
			VertexMacros[MacroModifierVolume].Definition = MacroValues[true];
			modVolVertexShaders[index] = compileVS(DX11N2VertexShader, "main", VertexMacros);
		}
	}

	return modVolVertexShaders[index];
}

const ComPtr<ID3D11PixelShader>& DX11Shaders::getModVolShader()
{
	if (!modVolShader)
		modVolShader = compilePS(PixelShader, "modifierVolume", PixelMacros);

	return modVolShader;
}

const ComPtr<ID3D11VertexShader>& DX11Shaders::getQuadVertexShader(bool rotate)
{
	ComPtr<ID3D11VertexShader>& shader = rotate ? quadRotateVertexShader : quadVertexShader;
	if (!shader)
	{
		D3D_SHADER_MACRO macros[]
		{
			{ "ROTATE", rotate ? "1" : "0" },
			{ nullptr, nullptr }
		};
		shader = compileVS(QuadVertexShader, "main", macros);
	}

	return shader;
}

const ComPtr<ID3D11PixelShader>& DX11Shaders::getQuadPixelShader()
{
	if (!quadPixelShader)
		quadPixelShader = compilePS(QuadPixelShader, "main", nullptr);

	return quadPixelShader;
}

ComPtr<ID3DBlob> DX11Shaders::compileShader(const char* source, const char* function, const char* profile, const D3D_SHADER_MACRO *pDefines)
{
	u64 hash = hashShader(source, function, profile, pDefines);

	ComPtr<ID3DBlob> shaderBlob;
	if (!lookupShader(hash, shaderBlob))
	{
		ComPtr<ID3DBlob> errorBlob;
		IncludeManager includeManager;

		if (FAILED(this->D3DCompile(source, strlen(source), nullptr, pDefines, &includeManager, function, profile, 0, 0, &shaderBlob.get(), &errorBlob.get())))
			ERROR_LOG(RENDERER, "Shader compilation failed: %s", errorBlob->GetBufferPointer());
		else
			cacheShader(hash, shaderBlob);
	}

	return shaderBlob;
}

ComPtr<ID3D11VertexShader> DX11Shaders::compileVS(const char* source, const char* function, const D3D_SHADER_MACRO *pDefines)
{
	ComPtr<ID3DBlob> blob = compileShader(source, function, "vs_4_0", pDefines);
	ComPtr<ID3D11VertexShader> shader;
	if (blob)
	{
		if (FAILED(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader.get())))
			ERROR_LOG(RENDERER, "Vertex shader creation failed");
	}

	return shader;
}

ComPtr<ID3D11PixelShader> DX11Shaders::compilePS(const char* source, const char* function, const D3D_SHADER_MACRO *pDefines)
{
	ComPtr<ID3DBlob> blob = compileShader(source, function, "ps_4_0", pDefines);
	ComPtr<ID3D11PixelShader> shader;
	if (blob)
	{
		if (FAILED(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader.get())))
			ERROR_LOG(RENDERER, "Pixel shader creation failed");
	}

	return shader;
}

ComPtr<ID3DBlob> DX11Shaders::getVertexShaderBlob()
{
	VertexMacros[MacroGouraud].Definition = MacroValues[true];
	// FIXME code dup
	VertexMacros[MacroPositionOnly].Definition = MacroValues[false];
	VertexMacros[MacroTwoVolumes].Definition = MacroValues[false];
	VertexMacros[MacroModifierVolume].Definition = MacroValues[false];
	std::string source(DX11N2VertexShader);
	source += std::string("\n") + DX11N2ColorShader;
	return compileShader(source.c_str(), "main", "vs_4_0", VertexMacros);
}

ComPtr<ID3DBlob> DX11Shaders::getMVVertexShaderBlob()
{
	// FIXME code dup
	VertexMacros[MacroGouraud].Definition = MacroValues[false];
	VertexMacros[MacroPositionOnly].Definition = MacroValues[true];
	VertexMacros[MacroTwoVolumes].Definition = MacroValues[false];
	VertexMacros[MacroModifierVolume].Definition = MacroValues[true];
	return compileShader(DX11N2VertexShader, "main", "vs_4_0", VertexMacros);
}

ComPtr<ID3DBlob> DX11Shaders::getQuadVertexShaderBlob()
{
	return compileShader(QuadVertexShader, "main", "vs_4_0", nullptr);
}

void DX11Shaders::init(const ComPtr<ID3D11Device>& device, pD3DCompile D3DCompile)
{
	this->device = device;
	this->D3DCompile = D3DCompile;
	enableCache(!theDX11Context.hasShaderCache());
	loadCache(CacheFile);
}

void DX11Shaders::term()
{
	saveCache(CacheFile);
	shaders.clear();
	for (auto& shader : vertexShaders)
		shader.reset();
	modVolShader.reset();
	for (auto& shader : modVolVertexShaders)
		shader.reset();
	quadVertexShader.reset();
	quadRotateVertexShader.reset();
	quadPixelShader.reset();
	device.reset();
}

void CachedDX11Shaders::saveCache(const std::string& filename)
{
	if (!enabled)
		return;
	std::string path = hostfs::getShaderCachePath(filename);
	FILE *fp = nowide::fopen(path.c_str(), "wb");
	if (fp == nullptr)
	{
		WARN_LOG(RENDERER, "Cannot save shader cache to %s", path.c_str());
		return;
	}
	for (const auto& pair : shaderCache)
	{
		if (std::fwrite(&pair.first, sizeof(pair.first), 1, fp) != 1
				|| std::fwrite(&pair.second.size, sizeof(pair.second.size), 1, fp) != 1
				|| std::fwrite(&pair.second.blob[0], 1, pair.second.size, fp) != pair.second.size)
		{
			WARN_LOG(RENDERER, "Error saving shader cache to %s", path.c_str());
			break;
		}
	}
	NOTICE_LOG(RENDERER, "Saved %d shaders to %s", (int)shaderCache.size(), path.c_str());
	std::fclose(fp);
}

void CachedDX11Shaders::loadCache(const std::string& filename)
{
	if (!enabled)
		return;
	std::string path = hostfs::getShaderCachePath(filename);
	FILE *fp = nowide::fopen(path.c_str(), "rb");
	if (fp != nullptr)
	{
		u64 hash;
		u32 size;
		while (true)
		{
			if (std::fread(&hash, sizeof(hash), 1, fp) != 1)
				break;
			if (std::fread(&size, sizeof(size), 1, fp) != 1)
				break;
			std::unique_ptr<u8[]> blob = std::make_unique<u8[]>(size);
			if (std::fread(&blob[0], 1, size, fp) != size)
				break;
			shaderCache[hash] = { size, std::move(blob) };
		}
		std::fclose(fp);
		NOTICE_LOG(RENDERER, "Loaded %d shaders from %s", (int)shaderCache.size(), path.c_str());
	}

}
bool CachedDX11Shaders::lookupShader(u64 hash, ComPtr<ID3DBlob>& blob)
{
	if (!enabled)
		return false;

	auto it = shaderCache.find(hash);
	if (it == shaderCache.end())
		return false;

	D3DCreateBlob(it->second.size, &blob.get());
	memcpy(blob->GetBufferPointer(), &it->second.blob[0], it->second.size);

	return true;
}

void CachedDX11Shaders::cacheShader(u64 hash, const ComPtr<ID3DBlob>& blob)
{
	if (!enabled)
		return;
	u32 size = (u32)blob->GetBufferSize();
	std::unique_ptr<u8[]> data = std::make_unique<u8[]>(size);
	memcpy(&data[0], blob->GetBufferPointer(), size);
	shaderCache[hash] = { size, std::move(data) };
}

u64 CachedDX11Shaders::hashShader(const char* source, const char* function, const char* profile, const D3D_SHADER_MACRO *pDefines, const char *includeFile)
{
	if (!enabled)
		return 0;

	XXH3_state_t *xxh = XXH3_createState();
	XXH3_64bits_reset(xxh);
	XXH3_64bits_update(xxh, source, strlen(source));
	XXH3_64bits_update(xxh, function, strlen(function));
	if (pDefines != nullptr)
		for (const D3D_SHADER_MACRO *pDef = pDefines; pDef->Name != nullptr; pDef++)
		{
			XXH3_64bits_update(xxh, pDef->Name, strlen(pDef->Name));
			XXH3_64bits_update(xxh, pDef->Definition, strlen(pDef->Definition));
		}
	if (includeFile != nullptr)
		XXH3_64bits_update(xxh, includeFile, strlen(includeFile));
	u64 hash = XXH3_64bits_digest(xxh);
	XXH3_freeState(xxh);

	return hash;
}
