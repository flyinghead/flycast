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

// FIXME GLES
#ifndef GL_CLIP_DISTANCE0
#define GL_CLIP_DISTANCE0 0x3000
#endif

const char* N2VertexShader = R"(
uniform vec4 depth_scale;
uniform mat4 normal_matrix;
uniform float sp_FOG_DENSITY;

uniform mat4 mvMat;
uniform mat4 projMat;
uniform int envMapping;

// Vertex input
in vec3 in_pos;
#if GEOM_ONLY == 0
in vec4 in_base;
in vec4 in_offs;
in vec2 in_uv;
in vec3 in_normal;
#if TWO_VOLUMES == 1
in vec4 in_base1;
in vec4 in_offs1;
in vec2 in_uv1;
#endif
// output
INTERPOLATION out highp vec4 vs_base;
INTERPOLATION out highp vec4 vs_offs;
NOPERSPECTIVE out highp vec3 vs_uv;
#if TWO_VOLUMES == 1
INTERPOLATION out vec4 vs_base1;
INTERPOLATION out vec4 vs_offs1;
noperspective out vec2 vs_uv1;
#endif
#endif
out float gl_ClipDistance[1];

void main()
{
	vec4 vpos = mvMat * vec4(in_pos, 1.0);
#if GEOM_ONLY == 0
	vs_base = in_base;
	vs_offs = in_offs;
#if TWO_VOLUMES == 1
	vs_base1 = in_base1;
	vs_offs1 = in_offs1;
	vs_uv1 = in_uv1;
#endif
	vec4 vnorm = normalize(mvMat * vec4(in_normal, 0.0));
	computeColors(vs_base, vs_offs, vpos.xyz, vnorm.xyz);
	vs_uv.xy = in_uv;
	if (envMapping == 1)
		computeEnvMap(vs_uv.xy, vpos.xyz, vnorm.xyz);
#endif

	vpos = projMat * vpos;

	gl_ClipDistance[0] = vpos.w - 0.001; // near FIXME

	gl_Position = vpos;
}

)";

const char* N2ColorShader = R"(
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
	int diffuse;
	int specular;
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

uniform vec4 ambientBase;
uniform vec4 ambientOffset;
uniform int ambientMaterial;
uniform int useBaseOver;

// model attributes
uniform float glossCoef0;
uniform float glossCoef1;

void computeColors(inout vec4 baseCol, inout vec4 offsetCol, in vec3 position, in vec3 normal)
{
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
			if (light.attnAngleA != 1.0 && light.attnAngleB != 0.0)
			{
				vec3 spotDir = normalize(light.direction.xyz);
				float cosAngle = max(0.0, dot(-lightDir, spotDir));
				lightColor *= clamp(cosAngle * light.attnAngleB + light.attnAngleA, 0.0, 1.0);
			}
		}
		if (light.diffuse == 1)
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
		if (light.specular == 1)
		{
			vec3 reflectDir = reflect(-lightDir, normal);
			float factor;
			switch (light.smode)
			{
			case LMODE_SINGLE_SIDED:
				factor = clamp(pow(max(dot(normalize(-position), reflectDir), 0.0), glossCoef0), 0.0, 1.0);
				break;
			case LMODE_DOUBLE_SIDED:
				factor = clamp(pow(abs(dot(normalize(-position), reflectDir)), glossCoef0), 0.0, 1.0);
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
	if (ambientMaterial == 1)
	{
		diffuse += ambientBase.rgb;
		specular += ambientOffset.rgb;
	}
	baseCol.rgb *= diffuse;
	offsetCol.rgb *= specular;
	if (ambientMaterial == 0)
	{
		baseCol.rgb += ambientBase.rgb;
		offsetCol.rgb += ambientOffset.rgb;
	}
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

)";

const char *GeometryClippingShader = R"(
layout (triangles) in;
layout (triangle_strip, max_vertices = 6) out;

uniform mat4 normal_matrix;

#if GEOM_ONLY == 0
INTERPOLATION in highp vec4 vs_base[3];
INTERPOLATION in highp vec4 vs_offs[3];
NOPERSPECTIVE in highp vec3 vs_uv[3];
#if TWO_VOLUMES == 1
INTERPOLATION in highp vec4 vs_base1[3];
INTERPOLATION in highp vec4 vs_offs1[3];
NOPERSPECTIVE in highp vec2 vs_uv1[3];
#endif

INTERPOLATION out highp vec4 vtx_base;
INTERPOLATION out highp vec4 vtx_offs;
#if TWO_VOLUMES == 1
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
#if TWO_VOLUMES == 1
	vec4 base1;
	vec4 offs1;
	vec2 uv1;
#endif
	float clipDist;
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
#if TWO_VOLUMES == 1
	v.base1 = mix(v0.base1, v1.base1, f);
	v.offs1 = mix(v0.offs1, v1.offs1, f);
	v.uv1 = mix(v0.uv1, v1.uv1, f);
#endif
#endif
	v.clipDist = mix(v0.clipDist, v1.clipDist, f);

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
	v.pos = normal_matrix * v.pos;
#if GEOM_ONLY == 1
	v.uv = vec3(0.0, 0.0, v.pos.z);
#else
#if pp_Gouraud == 1
	v.base *= v.pos.z;
	v.offs *= v.pos.z;
#if TWO_VOLUMES == 1
	v.base1 *= v.pos.z;
	v.offs1 *= v.pos.z;
#endif
#endif
	v.uv = vec3(v.uv.xy * v.pos.z, v.pos.z);
#if TWO_VOLUMES == 1
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
#if TWO_VOLUMES == 1
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
	Vertex vtx[6];
	vtx[0].pos = gl_in[0].gl_Position;
	vtx[1].pos = gl_in[1].gl_Position;
	vtx[2].pos = gl_in[2].gl_Position;
#if GEOM_ONLY == 0
	vtx[0].base = vs_base[0];
	vtx[0].offs = vs_offs[0];
	vtx[0].uv = vs_uv[0];
	vtx[1].base = vs_base[1];
	vtx[1].offs = vs_offs[1];
	vtx[1].uv = vs_uv[1];
	vtx[2].base = vs_base[2];
	vtx[2].offs = vs_offs[2];
	vtx[2].uv = vs_uv[2];
#if TWO_VOLUMES == 1
	vtx[0].base1 = vs_base1[0];
	vtx[0].offs1 = vs_offs1[0];
	vtx[0].uv1 = vs_uv1[0];
	vtx[1].base1 = vs_base1[1];
	vtx[1].offs1 = vs_offs1[1];
	vtx[1].uv1 = vs_uv1[1];
	vtx[2].base1 = vs_base1[2];
	vtx[2].offs1 = vs_offs1[2];
	vtx[2].uv1 = vs_uv1[2];
#endif
#endif
	int vtxCount = 3;
	vtx[0].clipDist = gl_in[0].gl_ClipDistance[0];
	vtx[1].clipDist = gl_in[1].gl_ClipDistance[0];
	vtx[2].clipDist = gl_in[2].gl_ClipDistance[0];

	// near-plane only
	vec3 dist = vec3(vtx[0].clipDist, vtx[1].clipDist, vtx[2].clipDist);
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

N2VertexSource::N2VertexSource(bool gouraud, bool geometryOnly) : OpenGlSource()
{
	addConstant("pp_Gouraud", gouraud);
	addConstant("GEOM_ONLY", geometryOnly);
	addConstant("TWO_VOLUMES", 0);

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
	addConstant("TWO_VOLUMES", 0);
	addSource(GouraudSource);
	addSource(GeometryClippingShader);
}

static void setLightUniform(const PipelineShader *shader, int lightId, const char *name, int v)
{
	char s[128];
	sprintf(s, "lights[%d].%s", lightId, name);
	GLint loc = glGetUniformLocation(shader->program, s);
	glUniform1i(loc, v);
}

static void setLightUniform(const PipelineShader *shader, int lightId, const char *name, float v)
{
	char s[128];
	sprintf(s, "lights[%d].%s", lightId, name);
	GLint loc = glGetUniformLocation(shader->program, s);
	glUniform1f(loc, v);
}

static void setLightUniform4f(const PipelineShader *shader, int lightId, const char *name, const float *v)
{
	char s[128];
	sprintf(s, "lights[%d].%s", lightId, name);
	GLint loc = glGetUniformLocation(shader->program, s);
	glUniform4fv(loc, 1, v);
}

void setN2Uniforms(const PolyParam *pp, const PipelineShader *shader)
{
	glUniformMatrix4fv(shader->mvMat, 1, GL_FALSE, &pp->mvMatrix[0]);
	glUniformMatrix4fv(shader->projMat, 1, GL_FALSE, &pp->projMatrix[0]);
	glUniform1f(shader->glossCoef0, pp->glossCoef0);
	N2LightModel *const lightModel = pp->lightModel;
	if (lightModel != nullptr)
	{
		glUniform1i(shader->ambientMaterial, lightModel->ambientMaterial);
		glUniform4fv(shader->ambientBase, 1, lightModel->ambientBase);
		glUniform4fv(shader->ambientOffset, 1, lightModel->ambientOffset);
		glUniform1i(shader->useBaseOver, lightModel->useBaseOver);
		glUniform1i(shader->lightCount, lightModel->lightCount);
		for (int i = 0; i < lightModel->lightCount; i++)
		{
			const N2Light& light = lightModel->lights[i];
			setLightUniform(shader, i, "parallel", light.parallel);

			setLightUniform4f(shader, i, "color", light.color);
			setLightUniform4f(shader, i, "direction", light.direction);
			setLightUniform4f(shader, i, "position", light.position);

			setLightUniform(shader, i, "diffuse", light.diffuse);
			setLightUniform(shader, i, "specular", light.specular);
			setLightUniform(shader, i, "routing", light.routing);
			setLightUniform(shader, i, "dmode", light.dmode);
			setLightUniform(shader, i, "smode", light.smode);
			setLightUniform(shader, i, "distAttnMode", light.distAttnMode);

			setLightUniform(shader, i, "attnDistA", light.attnDistA);
			setLightUniform(shader, i, "attnDistB", light.attnDistB);
			setLightUniform(shader, i, "attnAngleA", light.attnAngleA);
			setLightUniform(shader, i, "attnAngleB", light.attnAngleB);
		}
	}
	else
	{
		float white[] { 1.f, 1.f, 1.f, 1.f };
		float black[4]{};
		glUniform1i(shader->ambientMaterial, 0);
		glUniform4fv(shader->ambientBase, 1, white);
		glUniform4fv(shader->ambientOffset, 1, black);
		glUniform1i(shader->useBaseOver, 0);
		glUniform1i(shader->lightCount, 0);
	}
	glUniform1i(shader->envMapping, pp->envMapping);

	glEnable(GL_CLIP_DISTANCE0);
}
