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
#pragma once

// GLSL code for poly params handling, used by Open GL and Vulkan per-pixel renderers

#define OIT_POLY_PARAM " \n\
struct Pixel { \n\
	uint color; \n\
	float depth; \n\
	uint seq_num; \n\
	uint next; \n\
}; \n\
#define EOL 0xFFFFFFFFu \n\
\n\
#define ZERO				0 \n\
#define ONE					1 \n\
#define OTHER_COLOR			2 \n\
#define INVERSE_OTHER_COLOR	3 \n\
#define SRC_ALPHA			4 \n\
#define INVERSE_SRC_ALPHA	5 \n\
#define DST_ALPHA			6 \n\
#define INVERSE_DST_ALPHA	7 \n\
 \n\
void setFragDepth(float z) \n\
{ \n\
	float w = 100000.0 * z; \n\
	gl_FragDepth = log2(1.0 + w) / 34.0; \n\
} \n\
\n\
struct PolyParam { \n\
	int tsp_isp_pcw; \n\
	int tsp1; \n\
}; \n\
 \n\
#define GET_TSP_FOR_AREA int tsp = area1 ? pp.tsp1 : pp.tsp_isp_pcw; \n\
 \n\
int getSrcBlendFunc(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return (tsp >> 29) & 7; \n\
} \n\
 \n\
int getDstBlendFunc(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return (tsp >> 26) & 7; \n\
} \n\
 \n\
bool getSrcSelect(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return ((tsp >> 25) & 1) != 0; \n\
} \n\
 \n\
bool getDstSelect(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return ((tsp >> 24) & 1) != 0; \n\
} \n\
 \n\
int getFogControl(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return (tsp >> 22) & 3; \n\
} \n\
 \n\
bool getUseAlpha(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return ((tsp >> 20) & 1) != 0; \n\
} \n\
 \n\
bool getIgnoreTexAlpha(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return ((tsp >> 19) & 1) != 0; \n\
} \n\
 \n\
int getShadingInstruction(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return (tsp >> 6) & 3; \n\
} \n\
 \n\
int getDepthFunc(const PolyParam pp) \n\
{ \n\
	return (pp.tsp_isp_pcw >> 13) & 7; \n\
} \n\
 \n\
bool getDepthMask(const PolyParam pp) \n\
{ \n\
	return ((pp.tsp_isp_pcw >> 10) & 1) != 1; \n\
} \n\
 \n\
bool getShadowEnable(const PolyParam pp) \n\
{ \n\
	return (pp.tsp_isp_pcw & 1) != 0; \n\
} \n\
 \n\
uint getPolyNumber(const Pixel pixel) \n\
{ \n\
	return pixel.seq_num & 0x3FFFFFFFu; \n\
} \n\
 \n\
#define SHADOW_STENCIL 0x40000000u \n\
#define SHADOW_ACC	   0x80000000u \n\
 \n\
bool isShadowed(const Pixel pixel) \n\
{ \n\
	return (pixel.seq_num & SHADOW_ACC) == SHADOW_ACC; \n\
} \n\
 \n\
bool isTwoVolumes(const PolyParam pp) \n\
{ \n\
	return pp.tsp1 != -1; \n\
} \n\
 \n\
uint packColors(vec4 v) \n\
{ \n\
	return (uint(round(v.r * 255.0)) << 24) | (uint(round(v.g * 255.0)) << 16) | (uint(round(v.b * 255.0)) << 8) | uint(round(v.a * 255.0)); \n\
} \n\
 \n\
vec4 unpackColors(uint u) \n\
{ \n\
	return vec4(float((u >> 24) & 255u) / 255.0, float((u >> 16) & 255u) / 255.0, float((u >> 8) & 255u) / 255.0, float(u & 255u) / 255.0); \n\
} \n\
"
