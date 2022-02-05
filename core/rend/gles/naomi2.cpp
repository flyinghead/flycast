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
uniform int envMapping[2];
uniform int bumpMapping;

// Vertex input
in vec3 in_pos;
#if GEOM_ONLY == 0
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
INTERPOLATION out highp vec4 vs_base;
INTERPOLATION out highp vec4 vs_offs;
NOPERSPECTIVE out highp vec3 vs_uv;
#if pp_TwoVolumes == 1
INTERPOLATION out vec4 vs_base1;
INTERPOLATION out vec4 vs_offs1;
noperspective out vec2 vs_uv1;
#endif
#endif
out float gl_ClipDistance[2];

void main()
{
	vec4 vpos = mvMat * vec4(in_pos, 1.0);
#if GEOM_ONLY == 0
	vs_base = in_base;
	vs_offs = in_offs;
	vec4 vnorm = normalize(normalMat * vec4(in_normal, 0.0));
	#if pp_TwoVolumes == 1
		vs_base1 = in_base1;
		vs_offs1 = in_offs1;
		vs_uv1 = in_uv1;
		// FIXME need offset0 and offset1 for bump maps
		if (bumpMapping == 1)
			computeBumpMap(vs_offs, vs_offs1, normalize(in_normal));
		else
		{
			computeColors(vs_base1, vs_offs1, 1, vpos.xyz, vnorm.xyz);
			#if pp_Texture == 0
				vs_base1 += vs_offs1;
			#endif
		}
		if (envMapping[1] == 1)
			computeEnvMap(vs_uv1.xy, vpos.xyz, vnorm.xyz);
	#endif
	if (bumpMapping == 0)
	{
		computeColors(vs_base, vs_offs, 0, vpos.xyz, vnorm.xyz);
		#if pp_Texture == 0
				vs_base += vs_offs;
		#endif
	}
	vs_uv.xy = in_uv;
	if (envMapping[0] == 1)
		computeEnvMap(vs_uv.xy, vpos.xyz, vnorm.xyz);
#endif

	vpos = projMat * vpos;

	gl_ClipDistance[0] = vpos.w - 0.001; // near FIXME
	gl_ClipDistance[1] = 100000.0 - vpos.w; // far FIXME

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
			if (light.attnDistA != 1.0 && light.attnDistB != 0.0)
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

void computeBumpMap(inout vec4 color0, in vec4 color1, in vec3 normal)
{
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

	// FIXME not right
	float sinT = normal.x;
	float cosQ = tangent.y;
	float sinQ = bitangent.z;

	float k1 = 1.0 - scaleDegree;
	float k2 = scaleDegree * sinT;
	float k3 = scaleDegree * sqrt(1.0 - sinT * sinT);
	float q = acos(cosQ);
	if (sinQ < 0)
		q = 2.0 * PI - q;
	color0.x = q / PI / 2.0;
	color0.y = k3;
	color0.z = k2;
	color0.w = k1;
}

)";

const char *GeometryClippingShader = R"(
layout (triangles) in;
layout (triangle_strip, max_vertices = 12) out;

uniform mat4 ndcMat;

#if GEOM_ONLY == 0
INTERPOLATION in highp vec4 vs_base[3];
INTERPOLATION in highp vec4 vs_offs[3];
NOPERSPECTIVE in highp vec3 vs_uv[3];
#if pp_TwoVolumes == 1
INTERPOLATION in highp vec4 vs_base1[3];
INTERPOLATION in highp vec4 vs_offs1[3];
NOPERSPECTIVE in highp vec2 vs_uv1[3];
#endif

INTERPOLATION out highp vec4 vtx_base;
INTERPOLATION out highp vec4 vtx_offs;
#if pp_TwoVolumes == 1
INTERPOLATION out highp vec4 vtx_base1;
INTERPOLATION out highp vec4 vtx_offs1;
NOPERSPECTIVE out highp vec2 vtx_uv1;
#endif
#endif
NOPERSPECTIVE out highp vec3 vtx_uv; // For depth

struct Vertex
{
	vec4 pos;
	vec4 base;
	vec4 offs;
	vec3 uv;
#if pp_TwoVolumes == 1
	vec4 base1;
	vec4 offs1;
	vec2 uv1;
#endif
	float clipDist[2];
};

Vertex interpolate(in Vertex v0, in Vertex v1, in float d0, in float d1)
{
	Vertex v;
	float f = d0 / (d0 - d1);
	v.pos = mix(v0.pos, v1.pos, f);
#if GEOM_ONLY == 0
	v.base = mix(v0.base, v1.base, f);
	v.offs = mix(v0.offs, v1.offs, f);
	v.uv = mix(v0.uv, v1.uv, f);
#if pp_TwoVolumes == 1
	v.base1 = mix(v0.base1, v1.base1, f);
	v.offs1 = mix(v0.offs1, v1.offs1, f);
	v.uv1 = mix(v0.uv1, v1.uv1, f);
#endif
#endif
	v.clipDist[0] = mix(v0.clipDist[0], v1.clipDist[0], f);
	v.clipDist[1] = mix(v0.clipDist[1], v1.clipDist[1], f);

	return v;
}
 
//
// Efficient Triangle and Quadrilateral Clipping within Shaders. M. McGuire
// Journal of Graphics GPU and Game Tools, November 2011
//
const float clipEpsilon  = 0.00001;
const float clipEpsilon2 = 0.0; // 0.01;

/**
 Computes the intersection of triangle v0-v1-v2 with the half-space (x,y,z) * n > 0.
 The result is a convex polygon in v0-v1-v2-v3. Vertex v3 may be degenerate
 and equal to the first vertex. 

 \return number of vertices; 0, 3, or 4
*/
int clip3(in vec3 dist, inout Vertex v0, inout Vertex v1, inout Vertex v2, out Vertex v3)
{
	if (!any(greaterThanEqual(dist, vec3(clipEpsilon2))))
		// All clipped
		return 0;
    
	if (all(greaterThanEqual(dist, vec3(-clipEpsilon)))) {
		// None clipped (original triangle vertices are unmodified)
		v3 = v0;
		return 3;
    }
        
	bvec3 above = greaterThanEqual(dist, vec3(0.0));

	// There are either 1 or 2 vertices above the clipping plane.
	bool nextIsAbove;

	// Find the CCW-most vertex above the plane by cycling
	// the vertices in place.  There are three cases.
	if (above[1] && !above[0]) {
		nextIsAbove = above[2];
		// Cycle once CCW.  Use v3 as a temp
		v3 = v0; v0 = v1; v1 = v2; v2 = v3;
		dist = dist.yzx;
	}
	else if (above[2] && !above[1]) {
		// Cycle once CW.  Use v3 as a temp.
		nextIsAbove = above[0];
		v3 = v2; v2 = v1; v1 = v0; v0 = v3;
		dist = dist.zxy;
	}
	else {
		nextIsAbove = above[1];
	}

	// We always need to clip v2-v0.
	v3 = interpolate(v0, v2, dist[0], dist[2]);

	if (nextIsAbove) {
		v2 = interpolate(v1, v2, dist[1], dist[2]);
		return 4;
	} else {
		v1 = interpolate(v0, v1, dist[0], dist[1]);
		v2 = v3;
		v3 = v0;
		return 3;
	}
}

void wDivide(inout Vertex v)
{
	v.pos = vec4(v.pos.xy / v.pos.w, 1.0 / v.pos.w, 1.0);
	v.pos = ndcMat * v.pos;
#if GEOM_ONLY == 1
	v.uv = vec3(0.0, 0.0, v.pos.z);
#else
#if pp_Gouraud == 1
	v.base *= v.pos.z;
	v.offs *= v.pos.z;
#if pp_TwoVolumes == 1
	v.base1 *= v.pos.z;
	v.offs1 *= v.pos.z;
#endif
#endif
	v.uv = vec3(v.uv.xy * v.pos.z, v.pos.z);
#if pp_TwoVolumes == 1
	v.uv1 *= v.pos.z;
#endif
#endif
	v.pos.w = 1.0;
	v.pos.z = 0.0;
}

void emitVertex(in Vertex v)
{
	wDivide(v);
#if GEOM_ONLY == 0
	vtx_base = v.base;
	vtx_offs = v.offs;
#if pp_TwoVolumes == 1
	vtx_base1 = v.base1;
	vtx_offs1 = v.offs1;
	vtx_uv1 = v.uv1;
#endif
#endif
	vtx_uv = v.uv;
	gl_Position = v.pos;
	EmitVertex();
}

void main()
{
	Vertex vtx[12];
	for (int i = 0; i < 3; i++)
	{
		vtx[i].pos = gl_in[i].gl_Position;
#if GEOM_ONLY == 0
		vtx[i].base = vs_base[i];
		vtx[i].offs = vs_offs[i];
		vtx[i].uv = vs_uv[i];
#if pp_TwoVolumes == 1
		vtx[i].base1 = vs_base1[i];
		vtx[i].offs1 = vs_offs1[i];
		vtx[i].uv1 = vs_uv1[i];
#endif
#endif
		vtx[i].clipDist[0] = gl_in[i].gl_ClipDistance[0];
		vtx[i].clipDist[1] = gl_in[i].gl_ClipDistance[1];
	}
	int vtxCount = 3;

	// near-plane only
	vec3 dist = vec3(vtx[0].clipDist[0], vtx[1].clipDist[0], vtx[2].clipDist[0]);
	Vertex v3;
	int size = clip3(dist, vtx[0], vtx[1], vtx[2], v3);
	if (size == 0)
		vtxCount = 0;
	else if (size == 4)
	{
		vtx[3] = vtx[0];
		vtx[4] = vtx[2];
		vtx[5] = v3;
		vtxCount = 6;
	}

	for (int i = 0; i + 2 < vtxCount; i += 3)
	{
		emitVertex(vtx[i]);
		emitVertex(vtx[i + 1]);
		emitVertex(vtx[i + 2]);
		EndPrimitive();
	}
}

)";

N2VertexSource::N2VertexSource(bool gouraud, bool geometryOnly, bool texture) : OpenGlSource()
{
	addConstant("pp_Gouraud", gouraud);
	addConstant("GEOM_ONLY", geometryOnly);
	addConstant("pp_TwoVolumes", 0);
	addConstant("pp_Texture", (int)texture);

	addSource(VertexCompatShader);
	addSource(GouraudSource);
	if (!geometryOnly)
		addSource(N2ColorShader);
	addSource(N2VertexShader);
}

N2GeometryShader::N2GeometryShader(bool gouraud, bool geometryOnly) : OpenGlSource()
{
	addConstant("pp_Gouraud", gouraud);
	addConstant("GEOM_ONLY", geometryOnly);
	addConstant("pp_TwoVolumes", 0);
	addSource(GouraudSource);
	addSource(GeometryClippingShader);
}
