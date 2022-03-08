/*
	Copyright 2022 flyinghead

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
#include "dx11_naomi2.h"

const char * const DX11N2VertexShader = R"(
#if pp_Gouraud == 1
#define INTERPOLATION
#else
#define INTERPOLATION nointerpolation
#endif

struct VertexIn
{
	float4 pos : POSITION;
#if POSITION_ONLY == 0
	float4 col : COLOR0;
	float4 spec : COLOR1;
	float2 uv : TEXCOORD0;
#if pp_TwoVolumes == 1
	float4 col1 : COLOR2;
	float4 spec1 : COLOR3;
	float2 uv1 : TEXCOORD1;
#endif
	float3 normal: NORMAL;
	uint vertexId : SV_VertexID;
#endif
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float4 uv : TEXCOORD0;
#if POSITION_ONLY == 0
	INTERPOLATION float4 col : COLOR0;
	INTERPOLATION float4 spec : COLOR1;
#if pp_TwoVolumes == 1
	float2 uv1 : TEXCOORD1;
	INTERPOLATION float4 col1 : COLOR2;
	INTERPOLATION float4 spec1 : COLOR3;
#endif
	nointerpolation uint index : BLENDINDICES0;
#endif
};

cbuffer shaderConstants : register(b0)
{
	float4x4 ndcMat;
	float4 leftPlane;
	float4 topPlane;
	float4 rightPlane;
	float4 bottomPlane;
};

cbuffer polyConstants : register(b1)
{
	float4x4 mvMat;
	float4x4 normalMat;
	float4x4 projMat;
	int envMapping0;
	int envMapping1;
	int bumpMapping;
	int polyNumber;

	float4 glossCoef;
	int4 constantColor;
	int4 model_diff_spec;	// diffuse0, diffuse1, specular0, specular1
};

void computeColors(inout float4 baseCol, inout float4 offsetCol, in int volIdx, in float3 position, in float3 normal);
void computeEnvMap(inout float2 uv, in float3 normal);
void computeBumpMap(inout float4 color0, in float4 color1, in float3 position, in float3 normal, in float4x4 normalMat);

[clipplanes(leftPlane, topPlane, rightPlane, bottomPlane)]
VertexOut main(in VertexIn vin)
{
	VertexOut vo;
	vo.pos = mul(mvMat, float4(vin.pos.xyz, 1.f));
#if POSITION_ONLY == 0
	vo.col = vin.col;
	vo.spec = vin.spec;
	#if LIGHT_ON == 1
	float3 vnorm = normalize(mul((float3x3)normalMat, vin.normal));
	#endif
	#if pp_TwoVolumes == 1
		vo.col1 = vin.col1;
		vo.spec1 = vin.spec1;
		vo.uv1 = vin.uv1;
		#if LIGHT_ON == 1
			// FIXME need offset0 and offset1 for bump maps
			if (bumpMapping == 1)
				computeBumpMap(vo.spec, vo.spec1, vo.pos.xyz, vnorm, normalMat);
			else
			{
				computeColors(vo.col1, vo.spec1, 1, vo.pos.xyz, vnorm);
				#if pp_Texture == 0
					vo.col1 += vo.spec1;
				#endif
			}
			if (envMapping1 == 1)
				computeEnvMap(vo.uv1.xy, vnorm);
		#endif
	#endif
	#if LIGHT_ON == 1
		if (bumpMapping == 0)
		{
			computeColors(vo.col, vo.spec, 0, vo.pos.xyz, vnorm);
			#if pp_Texture == 0
					vo.col += vo.spec;
			#endif
		}
	#endif
	vo.uv.xy = vin.uv;
	#if LIGHT_ON == 1
		if (envMapping0 == 1)
			computeEnvMap(vo.uv.xy, vnorm);
	#endif
	vo.index = (uint(polyNumber) << 18) + vin.vertexId;
#endif

	vo.pos = mul(projMat, vo.pos);

	vo.pos = float4(vo.pos.xy / vo.pos.w, 1.f / vo.pos.w, 1.f);
	vo.pos = mul(ndcMat, vo.pos);
#if POSITION_ONLY == 1
	vo.uv = float4(0.f, 0.f, 0.f, vo.pos.z);
#else
#if pp_Gouraud == 1
	vo.col *= vo.pos.z;
	vo.spec *= vo.pos.z;
#if pp_TwoVolumes == 1
	vo.col1 *= vo.pos.z;
	vo.spec1 *= vo.pos.z;
#endif
#endif
	vo.uv = float4(vo.uv.xy * vo.pos.z, 0.f, vo.pos.z);
#if pp_TwoVolumes == 1
	vo.uv1 *= vo.pos.z;
#endif
#endif
	vo.pos.w = 1.f;
	vo.pos.z = 0.f;

	return vo;
}

)";

const char * const DX11N2ColorShader = R"(
#define PI 3.1415926f

#define LMODE_SINGLE_SIDED 0
#define LMODE_DOUBLE_SIDED 1
#define LMODE_DOUBLE_SIDED_WITH_TOLERANCE 2
#define LMODE_SPECIAL_EFFECT 3
#define LMODE_THIN_SURFACE 4
#define LMODE_BUMP_MAP 5

#define ROUTING_BASEDIFF_BASESPEC_ADD 0
#define ROUTING_BASEDIFF_OFFSSPEC_ADD 1
#define ROUTING_OFFSDIFF_BASESPEC_ADD 2
#define ROUTING_OFFSDIFF_OFFSSPEC_ADD 3
#define ROUTING_ALPHADIFF_ADD 4
#define ROUTING_ALPHAATTEN_ADD 5
#define ROUTING_FOGDIFF_ADD 6
#define ROUTING_FOGATTENUATION_ADD 7
#define ROUTING_BASEDIFF_BASESPEC_SUB 8
#define ROUTING_BASEDIFF_OFFSSPEC_SUB 9
#define ROUTING_OFFSDIFF_BASESPEC_SUB 10
#define ROUTING_OFFSDIFF_OFFSSPEC_SUB 11
#define ROUTING_ALPHADIFF_SUB 12
#define ROUTING_ALPHAATTEN_SUB 13

struct N2Light
{
	float4 color;
	float4 direction;
	float4 position;
	int parallel;
	int routing;
	int dmode;
	int smode;
	int4 diffuse_specular;		// diffuse0, diffuse1, specular0, specular1
	float attnDistA;
	float attnDistB;
	float attnAngleA;
	float attnAngleB;
	int distAttnMode;
	int3 _pad;
};

cbuffer lightConstants : register(b2)
{
	N2Light lights[16];
	int lightCount;
	float4 ambientBase[2];
	float4 ambientOffset[2];
	int4 ambientMaterial;		// base0, base1, offset0, offset1
	int useBaseOver;
	int bumpId0;
	int bumpId1;
}

void computeColors(inout float4 baseCol, inout float4 offsetCol, in int volIdx, in float3 position, in float3 normal)
{
	if (constantColor[volIdx] == 1)
		return;
	float3 diffuse = float3(0.f, 0.f, 0.f);
	float3 specular = float3(0.f, 0.f, 0.f);
	float diffuseAlpha = 0.f;
	float specularAlpha = 0.f;
	float3 reflectDir = reflect(normalize(position), normal);
	const float BASE_FACTOR = 1.45f;

	for (int i = 0; i < lightCount; i++)
	{
		N2Light light = lights[i];
		float3 lightDir; // direction to the light
		float3 lightColor = light.color.rgb;
		if (light.parallel == 1)
		{
			lightDir = normalize(light.direction.xyz);
		}
		else
		{
			lightDir = normalize(light.position.xyz - position);
			if (light.attnDistA != 1.f || light.attnDistB != 0.f)
			{
				float distance = length(light.position.xyz - position);
				if (light.distAttnMode == 0)
					distance = 1.f / distance;
				lightColor *= saturate(light.attnDistB * distance + light.attnDistA);
			}
			if (light.attnAngleA != 1.f || light.attnAngleB != 0.f)
			{
				float3 spotDir = light.direction.xyz;
				float cosAngle = 1.f - max(0.f, dot(lightDir, spotDir));
				lightColor *= saturate(cosAngle * light.attnAngleB + light.attnAngleA);
			}
		}
		int routing = light.routing;
		if (light.diffuse_specular[volIdx] == 1) // If light contributes to diffuse
		{
			float factor = BASE_FACTOR;
			switch (light.dmode)
			{
			case LMODE_SINGLE_SIDED:
				factor *= max(dot(normal, lightDir), 0.f);
				break;
			case LMODE_DOUBLE_SIDED:
				factor *= abs(dot(normal, lightDir));
				break;
			}
			if (routing == ROUTING_ALPHADIFF_SUB)
				diffuseAlpha -= lightColor.r * factor;
			else if (routing == ROUTING_BASEDIFF_BASESPEC_ADD || routing == ROUTING_BASEDIFF_OFFSSPEC_ADD)
				diffuse += lightColor * factor;
			if (routing == ROUTING_OFFSDIFF_BASESPEC_ADD || routing == ROUTING_OFFSDIFF_OFFSSPEC_ADD)
				specular += lightColor * factor;
		}
		if (light.diffuse_specular[2 + volIdx] == 1) // If light contributes to specular
		{
			float factor = BASE_FACTOR;
			switch (light.smode)
			{
			case LMODE_SINGLE_SIDED:
				factor *= saturate(pow(max(dot(lightDir, reflectDir), 0.f), glossCoef[volIdx]));
				break;
			case LMODE_DOUBLE_SIDED:
				factor *= saturate(pow(abs(dot(lightDir, reflectDir)), glossCoef[volIdx]));
				break;
			}
			if (routing == ROUTING_ALPHADIFF_SUB)
				specularAlpha -= lightColor.r * factor;
			else if (routing == ROUTING_OFFSDIFF_OFFSSPEC_ADD || routing == ROUTING_BASEDIFF_OFFSSPEC_ADD)
				specular += lightColor * factor;
			if (routing == ROUTING_BASEDIFF_BASESPEC_ADD || routing == ROUTING_OFFSDIFF_BASESPEC_ADD)
				diffuse += lightColor * factor;
		}
	}
	// ambient with material
	if (ambientMaterial[volIdx] == 1)
		diffuse += ambientBase[volIdx].rgb;
	if (ambientMaterial[volIdx + 2] == 1)
		specular += ambientOffset[volIdx].rgb;

	if (model_diff_spec[volIdx] == 1)
		baseCol.rgb *= diffuse;
	if (model_diff_spec[volIdx + 2] == 1)
		offsetCol.rgb *= specular;

	// ambient w/o material
	if (ambientMaterial[volIdx] == 0 && model_diff_spec[volIdx] == 1)
		baseCol.rgb += ambientBase[volIdx].rgb;
	if (ambientMaterial[volIdx + 2] == 0 && model_diff_spec[volIdx + 2] == 1)
		offsetCol.rgb += ambientOffset[volIdx].rgb;

	baseCol.a += diffuseAlpha;
	offsetCol.a += specularAlpha;
	if (useBaseOver == 1)
	{
		float4 overflow = max(0.f, baseCol - 1.f);
		offsetCol += overflow;
	}
	baseCol = saturate(baseCol);
	offsetCol = saturate(offsetCol);
}

void computeEnvMap(inout float2 uv, in float3 normal)
{
	// Cheap env mapping
	uv += normal.xy / 2.f + 0.5f;
	uv = saturate(uv);
}

void computeBumpMap(inout float4 color0, in float4 color1, in float3 position, in float3 normal, in float4x4 normalMat)
{
	// TODO
	//if (bumpId0 == -1)
		return;
	float3 tangent = color0.xyz;
	if (tangent.x > 0.5f)
		tangent.x -= 1.f;
	if (tangent.y > 0.5f)
		tangent.y -= 1.f;
	if (tangent.z > 0.5f)
		tangent.z -= 1.f;
	tangent = normalize(mul(normalMat, float4(tangent, 0.f))).xyz;
	float3 bitangent = color1.xyz;
	if (bitangent.x > 0.5f)
		bitangent.x -= 1.f;
	if (bitangent.y > 0.5f)
		bitangent.y -= 1.f;
	if (bitangent.z > 0.5f)
		bitangent.z -= 1.f;
	bitangent = normalize(mul(normalMat, float4(bitangent, 0.f))).xyz;

	float scaleDegree = color0.w;
	float scaleOffset = color1.w;

	N2Light light = lights[bumpId0];
	float3 lightDir; // direction to the light
	if (light.parallel == 1)
		lightDir = normalize(light.direction.xyz);
	else
		lightDir = normalize(light.position.xyz - position);

	float n = dot(lightDir, normal);
	float cosQ = dot(lightDir, tangent);
	float sinQ = dot(lightDir, bitangent);

	float sinT = saturate(n);
	float k1 = 1.f - scaleDegree;
	float k2 = scaleDegree * sinT;
	float k3 = scaleDegree * sqrt(1.f - sinT * sinT); // cos T

	float q = acos(cosQ);
	if (sinQ < 0.f)
		q = 2.f * PI - q;

	color0.r = k2;
	color0.g = k3;
	color0.b = q / PI / 2.f;
	color0.a = k1;
}

)";

struct N2PolyConstants
{
	float mvMat[4][4];		// 0
	float normalMat[4][4];	// 64
	float projMat[4][4];	// 128
	int envMapping[2];		// 192
	int bumpMapping;		// 200
	int polyNumber;			// 204

	float glossCoef[4];		// 208
	int constantColor[4];	// 224
	// int4 model_diff_spec
	int modelDiffuse[2];	// 240
	int modelSpecular[2];	// 248
							// 256
};
static_assert(sizeof(N2PolyConstants) == 256, "sizeof(N2PolyConstants) should be 256");

struct DX11N2Light
{
	float color[4];			// 0
	float direction[4];		// 16
	float position[4];		// 32
	int parallel;			// 48
	int routing;			// 52
	int dmode;				// 56
	int smode;				// 60
	// int4 diffuse_specular
	int diffuse[2];			// 64
	int specular[2];		// 72
	float attnDistA;		// 80
	float attnDistB;		// 84
	float attnAngleA;		// 88
	float attnAngleB;		// 92
	int distAttnMode;		// 96
	int _pad[3];
							// 112
};
static_assert(sizeof(DX11N2Light) == 112, "sizeof(DX11N2Light) should be 112");

struct N2LightConstants
{
	DX11N2Light lights[16];			// 0
	int lightCount;					// 1792
	int _pad0[3];
	float ambientBase[2][4];		// 1808
	float ambientOffset[2][4];		// 1840
	// int4 ambientMaterial
	int ambientMaterialBase[2];		// 1872
	int ambientMaterialOffset[2];	// 1880
	int useBaseOver;				// 1888
	int bumpId1;					// 1892
	int bumpId2;					// 1896
	int _pad3;						// 1900
									// 1904
};
static_assert(sizeof(N2LightConstants) == 1904, "sizeof(N2LightConstants) should be 1904");

void  Naomi2Helper::init(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext> deviceContext)
{
	this->deviceContext = deviceContext;
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(N2PolyConstants);
	desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(device->CreateBuffer(&desc, nullptr, &polyConstantsBuffer.get())))
		WARN_LOG(RENDERER, "Per-polygon constants buffer creation failed");

	desc.ByteWidth = sizeof(N2LightConstants);
	desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
	if (FAILED(device->CreateBuffer(&desc, nullptr, &lightConstantsBuffer.get())))
		WARN_LOG(RENDERER, "Light constants buffer creation failed");
	resetCache();
}

void Naomi2Helper::setConstants(const PolyParam& pp, u32 polyNumber)
{
	N2PolyConstants polyConstants;
	memcpy(polyConstants.mvMat, pp.mvMatrix, sizeof(polyConstants.mvMat));
	memcpy(polyConstants.normalMat, pp.normalMatrix, sizeof(polyConstants.normalMat));
	memcpy(polyConstants.projMat, pp.projMatrix, sizeof(polyConstants.projMat));
	polyConstants.envMapping[0] = pp.envMapping[0];
	polyConstants.envMapping[1] = pp.envMapping[1];
	polyConstants.bumpMapping = pp.pcw.Texture == 1 && pp.tcw.PixelFmt == PixelBumpMap;
	polyConstants.polyNumber = polyNumber;
	for (size_t i = 0; i < 2; i++)
	{
		polyConstants.glossCoef[i] = pp.glossCoef[i];
		polyConstants.constantColor[i] = pp.constantColor[i];
		polyConstants.modelDiffuse[i] = pp.diffuseColor[i];
		polyConstants.modelSpecular[i] = pp.specularColor[i];
	}
	setConstBuffer(polyConstantsBuffer, polyConstants);
	deviceContext->VSSetConstantBuffers(1, 1, &polyConstantsBuffer.get());

	if (pp.lightModel != lastModel)
	{
		lastModel = pp.lightModel;
		N2LightConstants lightConstants{};
		if (pp.lightModel != nullptr)
		{
			const N2LightModel& lights = *pp.lightModel;
			lightConstants.lightCount = lights.lightCount;
			for (int i = 0; i < lights.lightCount; i++)
			{
				DX11N2Light& light = lightConstants.lights[i];
				memcpy(light.color, lights.lights[i].color, sizeof(light.color));
				memcpy(light.direction, lights.lights[i].direction, sizeof(light.direction));
				memcpy(light.position, lights.lights[i].position, sizeof(light.position));
				light.parallel = lights.lights[i].parallel;
				light.routing = lights.lights[i].routing;
				light.dmode = lights.lights[i].dmode;
				light.smode = lights.lights[i].smode;
				memcpy(light.diffuse, lights.lights[i].diffuse, sizeof(light.diffuse));
				memcpy(light.specular, lights.lights[i].specular, sizeof(light.specular));
				light.attnDistA = lights.lights[i].attnDistA;
				light.attnDistB = lights.lights[i].attnDistB;
				light.attnAngleA = lights.lights[i].attnAngleA;
				light.attnAngleB = lights.lights[i].attnAngleB;
				light.distAttnMode = lights.lights[i].distAttnMode;
			}
			memcpy(lightConstants.ambientBase, lights.ambientBase, sizeof(lightConstants.ambientBase));
			memcpy(lightConstants.ambientOffset, lights.ambientOffset, sizeof(lightConstants.ambientOffset));
			for (int i = 0; i < 2; i++)
			{
				lightConstants.ambientMaterialBase[i] = lights.ambientMaterialBase[i];
				lightConstants.ambientMaterialOffset[i] = lights.ambientMaterialOffset[i];
			}
			lightConstants.useBaseOver = lights.useBaseOver;
			lightConstants.bumpId1 = lights.bumpId1;
			lightConstants.bumpId2 = lights.bumpId2;
		}
		else
		{
			lightConstants.lightCount = 0;
			float white[] { 1.f, 1.f, 1.f, 1.f };
			float black[4]{};
			for (int vol = 0; vol < 2; vol++)
			{
				lightConstants.ambientMaterialBase[vol] = 0;
				lightConstants.ambientMaterialOffset[vol] = 0;
				memcpy(lightConstants.ambientBase[vol], white, sizeof(white));
				memcpy(lightConstants.ambientOffset[vol], black, sizeof(black));
			}
			lightConstants.useBaseOver = 0;
			lightConstants.bumpId1 = -1;
			lightConstants.bumpId2 = -1;
		}
		setConstBuffer(lightConstantsBuffer, lightConstants);
		deviceContext->VSSetConstantBuffers(2, 1, &lightConstantsBuffer.get());
	}
}

void Naomi2Helper::setConstants(const float *mvMatrix, const float *projMatrix)
{
	N2PolyConstants polyConstants;
	memcpy(polyConstants.mvMat, mvMatrix, sizeof(polyConstants.mvMat));
	memcpy(polyConstants.projMat, projMatrix, sizeof(polyConstants.projMat));
	setConstBuffer(polyConstantsBuffer, polyConstants);
	deviceContext->VSSetConstantBuffers(1, 1, &polyConstantsBuffer.get());
}
