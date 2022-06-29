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
#include "naomi2.h"

const char* N2VertexShader = R"(

uniform mat4 mvMat;
uniform mat4 normalMat;
uniform mat4 projMat;
uniform mat4 ndcMat;
uniform int envMapping[2];
uniform int bumpMapping;
uniform int pp_Number;

// Vertex input
in vec3 in_pos;
#if POSITION_ONLY == 0
in vec4 in_base;
in vec4 in_offs;
in vec2 in_uv;
in vec3 in_normal;
#if pp_TwoVolumes == 1
in vec4 in_base1;
in vec4 in_offs1;
in vec2 in_uv1;
#endif
// output
INTERPOLATION out highp vec4 vtx_base;
INTERPOLATION out highp vec4 vtx_offs;
#if pp_TwoVolumes == 1
INTERPOLATION out vec4 vtx_base1;
INTERPOLATION out vec4 vtx_offs1;
out vec2 vtx_uv1;
#endif
#endif
out highp vec3 vtx_uv;
#ifdef OIT_RENDER
flat out uint vtx_index;
#endif

void wDivide(inout vec4 vpos)
{
	vpos = vec4(vpos.xy / vpos.w, 1.0 / vpos.w, 1.0);
	vpos = ndcMat * vpos;
#if POSITION_ONLY == 1
	vtx_uv = vec3(0.0, 0.0, vpos.z);
#else
#if pp_Gouraud == 1
	vtx_base *= vpos.z;
	vtx_offs *= vpos.z;
#if pp_TwoVolumes == 1
	vtx_base1 *= vpos.z;
	vtx_offs1 *= vpos.z;
#endif
#endif
	vtx_uv = vec3(vtx_uv.xy * vpos.z, vpos.z);
#if pp_TwoVolumes == 1
	vtx_uv1 *= vpos.z;
#endif
#endif
	vpos.w = 1.0;
	vpos.z = 0.0;
}

void main()
{
	vec4 vpos = mvMat * vec4(in_pos, 1.0);
#if POSITION_ONLY == 0
	vtx_base = in_base;
	vtx_offs = in_offs;
	#if LIGHT_ON == 1
	vec3 vnorm = normalize(mat3(normalMat) * in_normal);
	#endif
	#if pp_TwoVolumes == 1
		vtx_base1 = in_base1;
		vtx_offs1 = in_offs1;
		vtx_uv1 = in_uv1;
		#if LIGHT_ON == 1
			// FIXME need offset0 and offset1 for bump maps
			if (bumpMapping == 1)
				computeBumpMap(vtx_offs, vtx_offs1, vpos.xyz, in_normal, normalMat);
			else
			{
				computeColors(vtx_base1, vtx_offs1, 1, vpos.xyz, vnorm);
				#if pp_Texture == 0
					vtx_base1 += vtx_offs1;
				#endif
			}
			if (envMapping[1] == 1)
				computeEnvMap(vtx_uv1.xy, vpos.xyz, vnorm);
		#endif
	#endif
	#if LIGHT_ON == 1
		if (bumpMapping == 0)
		{
			computeColors(vtx_base, vtx_offs, 0, vpos.xyz, vnorm);
			#if pp_Texture == 0
					vtx_base += vtx_offs;
			#endif
		}
	#endif
	vtx_uv.xy = in_uv;
	#if LIGHT_ON == 1
		if (envMapping[0] == 1)
			computeEnvMap(vtx_uv.xy, vpos.xyz, vnorm);
	#endif
#endif

	vpos = projMat * vpos;
	wDivide(vpos);

#ifdef OIT_RENDER
	vtx_index = uint(pp_Number) + uint(gl_VertexID);
#endif

	gl_Position = vpos;
}

)";

const char* N2ColorShader = R"(
#define PI 3.1415926

#define LMODE_SINGLE_SIDED 0
#define LMODE_DOUBLE_SIDED 1
#define LMODE_DOUBLE_SIDED_WITH_TOLERANCE 2
#define LMODE_SPECIAL_EFFECT 3
#define LMODE_THIN_SURFACE 4
#define LMODE_BUMP_MAP 5

#define ROUTING_SPEC_TO_OFFSET 1
#define ROUTING_DIFF_TO_OFFSET 2
#define ROUTING_ATTENUATION 1	// not handled
#define ROUTING_FOG 2			// not handled
#define ROUTING_ALPHA 4
#define ROUTING_SUB 8

struct N2Light
{
	vec4 color;
	vec4 direction;	// For parallel/spot
	vec4 position;	// For spot/point
	int parallel;
	int diffuse[2];
	int specular[2];
	int routing;
	int dmode;
	int smode;

	int distAttnMode;	// For spot/point
	float attnDistA;
	float attnDistB;
	float attnAngleA;	// For spot
	float attnAngleB;
};
uniform N2Light lights[16];
uniform int lightCount;

uniform vec4 ambientBase[2];
uniform vec4 ambientOffset[2];
uniform int ambientMaterialBase[2];
uniform int ambientMaterialOffset[2];
uniform int useBaseOver;
uniform int bumpId0;
uniform int bumpId1;

// model attributes
uniform float glossCoef[2];
uniform int constantColor[2];

void computeColors(inout vec4 baseCol, inout vec4 offsetCol, int volIdx, vec3 position, vec3 normal)
{
	if (constantColor[volIdx] == 1)
		return;
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	float diffuseAlpha = 0.0;
	float specularAlpha = 0.0;
	vec3 reflectDir = reflect(normalize(position), normal);
	const float BASE_FACTOR = 2.0;

	for (int i = 0; i < lightCount; i++)
	{
		vec3 lightDir; // direction to the light
		vec3 lightColor = lights[i].color.rgb;
		if (lights[i].parallel == 1)
		{
			lightDir = normalize(lights[i].direction.xyz);
		}
		else
		{
			lightDir = normalize(lights[i].position.xyz - position);
			if (lights[i].attnDistA != 1.0 || lights[i].attnDistB != 0.0)
			{
				float distance = length(lights[i].position.xyz - position);
				if (lights[i].distAttnMode == 0)
					distance = 1.0 / distance;
				lightColor *= clamp(lights[i].attnDistB * distance + lights[i].attnDistA, 0.0, 1.0);
			}
			if (lights[i].attnAngleA != 1.0 || lights[i].attnAngleB != 0.0)
			{
				vec3 spotDir = lights[i].direction.xyz;
				float cosAngle = 1.0 - max(0.0, dot(lightDir, spotDir));
				lightColor *= clamp(cosAngle * lights[i].attnAngleB + lights[i].attnAngleA, 0.0, 1.0);
			}
		}
		if (lights[i].diffuse[volIdx] == 1)
		{
			float factor = (lights[i].routing & ROUTING_SUB) != 0 ? -BASE_FACTOR : BASE_FACTOR;
			if (lights[i].dmode == LMODE_SINGLE_SIDED)
				factor *= max(dot(normal, lightDir), 0.0);
			else if (lights[i].dmode == LMODE_DOUBLE_SIDED)
				factor *= abs(dot(normal, lightDir));

			if ((lights[i].routing & ROUTING_ALPHA) != 0)
				diffuseAlpha += lightColor.r * factor;
			else
			{
				if ((lights[i].routing & ROUTING_DIFF_TO_OFFSET) == 0)
					diffuse += lightColor * factor * baseCol.rgb;
				else
					specular += lightColor * factor * baseCol.rgb;
			}
		}
		if (lights[i].specular[volIdx] == 1)
		{
			float factor = (lights[i].routing & ROUTING_SUB) != 0 ? -BASE_FACTOR : BASE_FACTOR;
			if (lights[i].smode == LMODE_SINGLE_SIDED)
				factor *= clamp(pow(max(dot(lightDir, reflectDir), 0.0), glossCoef[volIdx]), 0.0, 1.0);
			else if (lights[i].smode == LMODE_DOUBLE_SIDED)
				factor *= clamp(pow(abs(dot(lightDir, reflectDir)), glossCoef[volIdx]), 0.0, 1.0);

			if ((lights[i].routing & ROUTING_ALPHA) != 0)
				specularAlpha += lightColor.r * factor;
			else
			{
				if ((lights[i].routing & ROUTING_SPEC_TO_OFFSET) == 0)
					diffuse += lightColor * factor * offsetCol.rgb;
				else
					specular += lightColor * factor * offsetCol.rgb;
			}
		}
	}
	// ambient light
	if (ambientMaterialBase[volIdx] == 1)
		diffuse += ambientBase[volIdx].rgb * baseCol.rgb;
	else
		diffuse += ambientBase[volIdx].rgb;
	if (ambientMaterialOffset[volIdx] == 1)
		specular += ambientOffset[volIdx].rgb * offsetCol.rgb;
	else
		specular += ambientOffset[volIdx].rgb;

	baseCol.rgb = diffuse;
	offsetCol.rgb = specular;

	baseCol.a += diffuseAlpha;
	offsetCol.a += specularAlpha;
	if (useBaseOver == 1)
	{
		vec4 overflow = max(baseCol - vec4(1.0), 0.0);
		offsetCol += overflow;
	}
	baseCol = clamp(baseCol, 0.0, 1.0);
	offsetCol = clamp(offsetCol, 0.0, 1.0);
}

void computeEnvMap(inout vec2 uv, vec3 position, vec3 normal)
{
	// Spherical mapping
	//vec3 r = reflect(normalize(position), normal);
	//float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
	//uv += r.xy / m + 0.5;

	// Cheap env mapping
	uv += normal.xy / 2.0 + 0.5;
	uv = clamp(uv, 0.0, 1.0);
}

void computeBumpMap(inout vec4 color0, vec4 color1, vec3 position, vec3 normal, mat4 normalMat)
{
	// TODO
	//if (bumpId0 == -1)
		return;
	normal = normalize(normal);
	vec3 tangent = color0.xyz;
	if (tangent.x > 0.5)
		tangent.x -= 1.0;
	if (tangent.y > 0.5)
		tangent.y -= 1.0;
	if (tangent.z > 0.5)
		tangent.z -= 1.0;
	tangent = normalize(tangent);
	vec3 bitangent = color1.xyz;
	if (bitangent.x > 0.5)
		bitangent.x -= 1.0;
	if (bitangent.y > 0.5)
		bitangent.y -= 1.0;
	if (bitangent.z > 0.5)
		bitangent.z -= 1.0;
	bitangent = normalize(bitangent);

	float scaleDegree = color0.w;
	float scaleOffset = color1.w;

	vec3 lightDir; // direction to the light
	if (lights[bumpId0].parallel == 1)
		lightDir = lights[bumpId0].direction.xyz;
	else
		lightDir = lights[bumpId0].position.xyz - position;
	lightDir = normalize(lightDir * mat3(normalMat));

	float n = dot(lightDir, normal);
	float cosQ = dot(lightDir, tangent);
	float sinQ = dot(lightDir, bitangent);

	float sinT = clamp(n, 0.0, 1.0);
	float k1 = 1.0 - scaleDegree;
	float k2 = scaleDegree * sinT;
	float k3 = scaleDegree * sqrt(1.0 - sinT * sinT); // cos T

	float q = acos(cosQ);
	if (sinQ < 0.0)
		q = 2.0 * PI - q;

	color0.r = k2;
	color0.g = k3;
	color0.b = q / PI / 2.0;
	color0.a = k1;
	color0 = clamp(color0, 0.0, 1.0);
}

)";

N2VertexSource::N2VertexSource(bool gouraud, bool geometryOnly, bool texture) : OpenGlSource()
{
	addConstant("pp_Gouraud", gouraud);
	addConstant("POSITION_ONLY", geometryOnly);
	addConstant("pp_TwoVolumes", 0);
	addConstant("pp_Texture", (int)texture);
	addConstant("LIGHT_ON", 1);

	addSource(VertexCompatShader);
	addSource(GouraudSource);
	if (!geometryOnly)
		addSource(N2ColorShader);
	addSource(N2VertexShader);
}
