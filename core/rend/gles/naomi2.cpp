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
noperspective out vec2 vtx_uv1;
#endif
#endif
NOPERSPECTIVE out highp vec3 vtx_uv;
flat out uint vtx_index;

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
	vec4 vnorm = normalize(normalMat * vec4(in_normal, 0.0));
	#endif
	#if pp_TwoVolumes == 1
		vtx_base1 = in_base1;
		vtx_offs1 = in_offs1;
		vtx_uv1 = in_uv1;
		#if LIGHT_ON == 1
			// FIXME need offset0 and offset1 for bump maps
			if (bumpMapping == 1)
				computeBumpMap(vtx_offs, vtx_offs1, vpos.xyz, vnorm.xyz, normalMat);
			else
			{
				computeColors(vtx_base1, vtx_offs1, 1, vpos.xyz, vnorm.xyz);
				#if pp_Texture == 0
					vtx_base1 += vtx_offs1;
				#endif
			}
			if (envMapping[1] == 1)
				computeEnvMap(vtx_uv1.xy, vpos.xyz, vnorm.xyz);
		#endif
	#endif
	#if LIGHT_ON == 1
		if (bumpMapping == 0)
		{
			computeColors(vtx_base, vtx_offs, 0, vpos.xyz, vnorm.xyz);
			#if pp_Texture == 0
					vtx_base += vtx_offs;
			#endif
		}
	#endif
	vtx_uv.xy = in_uv;
	#if LIGHT_ON == 1
		if (envMapping[0] == 1)
			computeEnvMap(vtx_uv.xy, vpos.xyz, vnorm.xyz);
	#endif
#endif

	vpos = projMat * vpos;
	wDivide(vpos);

	vtx_index = (uint(pp_Number) << 18) + uint(gl_VertexID);

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
uniform int modelDiffuse[2];
uniform int modelSpecular[2];

void computeColors(inout vec4 baseCol, inout vec4 offsetCol, in int volIdx, in vec3 position, in vec3 normal)
{
	if (constantColor[volIdx] == 1)
		return;
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	float diffuseAlpha = 0.0;
	float specularAlpha = 0.0;

	for (int i = 0; i < lightCount; i++)
	{
		N2Light light = lights[i];
		vec3 lightDir; // direction to the light
		vec3 lightColor = light.color.rgb;
		if (light.parallel == 1)
		{
			lightDir = normalize(light.direction.xyz);
		}
		else
		{
			lightDir = normalize(light.position.xyz - position);
			if (light.attnDistA != 1.0 || light.attnDistB != 0.0)
			{
				float distance = length(light.position.xyz - position);
				if (light.distAttnMode == 0)
					distance = 1.0 / distance;
				lightColor *= clamp(light.attnDistB * distance + light.attnDistA, 0.0, 1.0);
			}
			if (light.attnAngleA != 1.0 || light.attnAngleB != 0.0)
			{
				vec3 spotDir = light.direction.xyz;
				float cosAngle = 1.0 - max(0.0, dot(lightDir, spotDir));
				lightColor *= clamp(cosAngle * light.attnAngleB + light.attnAngleA, 0.0, 1.0);
			}
		}
		if (light.diffuse[volIdx] == 1)
		{
			float factor;
			switch (light.dmode)
			{
			case LMODE_SINGLE_SIDED:
				factor = max(dot(normal, lightDir), 0.0);
				break;
			case LMODE_DOUBLE_SIDED:
				factor = abs(dot(normal, lightDir));
				break;
			case LMODE_SPECIAL_EFFECT:
			default:
				factor = 1.0;
				break;
			}
			if (light.routing == ROUTING_ALPHADIFF_SUB)
				diffuseAlpha -= lightColor.r * factor;
			else if (light.routing == ROUTING_BASEDIFF_BASESPEC_ADD || light.routing == ROUTING_BASEDIFF_OFFSSPEC_ADD)
				diffuse += lightColor * factor;
			if (light.routing == ROUTING_OFFSDIFF_BASESPEC_ADD || light.routing == ROUTING_OFFSDIFF_OFFSSPEC_ADD)
				specular += lightColor * factor;
		}
		if (light.specular[volIdx] == 1)
		{
			vec3 reflectDir = reflect(-lightDir, normal);
			float factor;
			switch (light.smode)
			{
			case LMODE_SINGLE_SIDED:
				factor = clamp(pow(max(dot(normalize(-position), reflectDir), 0.0), glossCoef[volIdx]), 0.0, 1.0);
				break;
			case LMODE_DOUBLE_SIDED:
				factor = clamp(pow(abs(dot(normalize(-position), reflectDir)), glossCoef[volIdx]), 0.0, 1.0);
				break;
			case LMODE_SPECIAL_EFFECT:
			default:
				factor = 1.0;
				break;
			}
			if (light.routing == ROUTING_ALPHADIFF_SUB)
				specularAlpha -= lightColor.r * factor;
			else if (light.routing == ROUTING_OFFSDIFF_OFFSSPEC_ADD || light.routing == ROUTING_BASEDIFF_OFFSSPEC_ADD)
				specular += lightColor * factor;
			if (light.routing == ROUTING_BASEDIFF_BASESPEC_ADD || light.routing == ROUTING_OFFSDIFF_BASESPEC_ADD)
				diffuse += lightColor * factor;
		}
	}
	// ambient with material
	if (ambientMaterialBase[volIdx] == 1)
		diffuse += ambientBase[volIdx].rgb;
	if (ambientMaterialOffset[volIdx] == 1)
		specular += ambientOffset[volIdx].rgb;

	if (modelDiffuse[volIdx] == 1)
		baseCol.rgb *= diffuse;
	if (modelSpecular[volIdx] == 1)
		offsetCol.rgb *= specular;

	// ambient w/o material
	if (ambientMaterialBase[volIdx] == 0 && modelDiffuse[volIdx] == 1)
		baseCol.rgb += ambientBase[volIdx].rgb;
	if (ambientMaterialOffset[volIdx] == 0 && modelSpecular[volIdx] == 1)
		offsetCol.rgb += ambientOffset[volIdx].rgb;

	baseCol.a = max(0.0, baseCol.a + diffuseAlpha);
	offsetCol.a = max(0.0, offsetCol.a + specularAlpha);
	if (useBaseOver == 1)
	{
		vec4 overflow = max(vec4(0.0), baseCol - vec4(1.0));
		offsetCol += overflow;
	}
}

void computeEnvMap(inout vec2 uv, in vec3 position, in vec3 normal)
{
	// Spherical mapping
	//vec3 r = reflect(normalize(position), normal);
	//float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
	//uv += r.xy / m + 0.5;

	// Cheap env mapping
	uv += normal.xy / 2.0 + 0.5;
	uv = clamp(uv, 0.0, 1.0);
}

void computeBumpMap(inout vec4 color0, in vec4 color1, in vec3 position, in vec3 normal, in mat4 normalMat)
{
	// TODO
	//if (bumpId0 == -1)
		return;
	vec3 tangent = color0.xyz;
	if (tangent.x > 0.5)
		tangent.x -= 1.0;
	if (tangent.y > 0.5)
		tangent.y -= 1.0;
	if (tangent.z > 0.5)
		tangent.z -= 1.0;
	tangent = normalize(normalMat * vec4(tangent, 0.0)).xyz;
	vec3 bitangent = color1.xyz;
	if (bitangent.x > 0.5)
		bitangent.x -= 1.0;
	if (bitangent.y > 0.5)
		bitangent.y -= 1.0;
	if (bitangent.z > 0.5)
		bitangent.z -= 1.0;
	bitangent = normalize(normalMat * vec4(bitangent, 0.0)).xyz;

	float scaleDegree = color0.w;
	float scaleOffset = color1.w;

	N2Light light = lights[bumpId0];
	vec3 lightDir; // direction to the light
	if (light.parallel == 1)
		lightDir = normalize(light.direction.xyz);
	else
		lightDir = normalize(light.position.xyz - position);

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
