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
#include "gl4naomi2.h"

extern const char *N2VertexShader;
extern const char *N2ColorShader;

static const char *gouraudSource = R"(
#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION
#endif
)";

N2Vertex4Source::N2Vertex4Source(const gl4PipelineShader* shader) : OpenGl4Source()
{
	addConstant("OIT_RENDER");
	addConstant("DIV_POS_Z", false);
	if (shader == nullptr)
	{
		addConstant("POSITION_ONLY", 1);
		addConstant("pp_TwoVolumes", 0);
		addConstant("pp_Gouraud", 0);
		addConstant("pp_Texture", 0);
		addConstant("LIGHT_ON", 0);
	}
	else
	{
		addConstant("POSITION_ONLY", 0);
		addConstant("pp_TwoVolumes", shader->pp_TwoVolumes || shader->pp_BumpMap);
		addConstant("pp_Gouraud", shader->pp_Gouraud);
		addConstant("pp_Texture", shader->pp_Texture);
		addConstant("LIGHT_ON", shader->pass != Pass::Depth);
	}

	addSource(gouraudSource);
	if (shader != nullptr && shader->pass != Pass::Depth)
		addSource(N2ColorShader);
	addSource(N2VertexShader);
}
