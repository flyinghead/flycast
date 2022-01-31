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
extern const char *GeometryClippingShader;

static const char *gouraudSource = R"(
#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION noperspective
#endif
#define NOPERSPECTIVE noperspective
)";

N2Vertex4Source::N2Vertex4Source(bool gouraud, bool geometryOnly, bool texture) : OpenGl4Source()
{
	addConstant("pp_Gouraud", gouraud);
	addConstant("GEOM_ONLY", geometryOnly);
	addConstant("TWO_VOLUMES", 1);
	addConstant("pp_Texture", (int)texture);

	addSource(gouraudSource);
	if (!geometryOnly)
		addSource(N2ColorShader);
	addSource(N2VertexShader);
}

N2Geometry4Shader::N2Geometry4Shader(bool gouraud, bool geometryOnly) : OpenGl4Source()
{
	addConstant("pp_Gouraud", gouraud);
	addConstant("GEOM_ONLY", geometryOnly);
	addConstant("TWO_VOLUMES", 1);

	addSource(gouraudSource);
	addSource(GeometryClippingShader);
}
