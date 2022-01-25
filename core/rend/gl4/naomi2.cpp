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

N2Vertex4Source::N2Vertex4Source(bool gouraud, bool geometryOnly) : OpenGl4Source()
{
	addConstant("pp_Gouraud", gouraud);
	addConstant("GEOM_ONLY", geometryOnly);
	addConstant("TWO_VOLUMES", 1);

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

static void setLightUniform(const gl4PipelineShader *shader, int lightId, const char *name, int v)
{
	char s[128];
	sprintf(s, "lights[%d].%s", lightId, name);
	GLint loc = glGetUniformLocation(shader->program, s);
	glUniform1i(loc, v);
}

static void setLightUniform(const gl4PipelineShader *shader, int lightId, const char *name, float v)
{
	char s[128];
	sprintf(s, "lights[%d].%s", lightId, name);
	GLint loc = glGetUniformLocation(shader->program, s);
	glUniform1f(loc, v);
}

static void setLightUniform4f(const gl4PipelineShader *shader, int lightId, const char *name, const float *v)
{
	char s[128];
	sprintf(s, "lights[%d].%s", lightId, name);
	GLint loc = glGetUniformLocation(shader->program, s);
	glUniform4fv(loc, 1, v);
}

void setN2Uniforms(const PolyParam *pp, const gl4PipelineShader *shader)
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
