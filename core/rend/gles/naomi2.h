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
#pragma once
#include "gles.h"

// FIXME GLES
#ifndef GL_CLIP_DISTANCE0
#define GL_CLIP_DISTANCE0 0x3000
#endif

class N2VertexSource : public OpenGlSource
{
public:
	N2VertexSource(bool gouraud, bool geometryOnly = false);
};

class N2GeometryShader : public OpenGlSource
{
public:
	N2GeometryShader(bool gouraud, bool geometryOnly = false);
};

template<typename ShaderType>
void resetN2UniformCache(ShaderType *shader)
{
	shader->lastMvMat = nullptr;
	shader->lastProjMat = nullptr;
	shader->lastLightModel = nullptr;
}

template<typename ShaderType>
void initN2Uniforms(ShaderType *shader)
{
	shader->mvMat = glGetUniformLocation(shader->program, "mvMat");
	shader->normalMat = glGetUniformLocation(shader->program, "normalMat");
	shader->projMat = glGetUniformLocation(shader->program, "projMat");
	shader->glossCoef0 = glGetUniformLocation(shader->program, "glossCoef0");
	shader->envMapping = glGetUniformLocation(shader->program, "envMapping");
	shader->bumpMapping = glGetUniformLocation(shader->program, "bumpMapping");
	// Lights
	shader->lightCount = glGetUniformLocation(shader->program, "lightCount");
	shader->ambientBase = glGetUniformLocation(shader->program, "ambientBase");
	shader->ambientOffset = glGetUniformLocation(shader->program, "ambientOffset");
	shader->ambientMaterial = glGetUniformLocation(shader->program, "ambientMaterial");
	shader->useBaseOver = glGetUniformLocation(shader->program, "useBaseOver");
	for (u32 i = 0; i < ARRAY_SIZE(shader->lights); i++)
	{
		char str[128];
		sprintf(str, "lights[%d].color", i);
		shader->lights[i].color = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].direction", i);
		shader->lights[i].direction = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].position", i);
		shader->lights[i].position = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].parallel", i);
		shader->lights[i].parallel = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].diffuse", i);
		shader->lights[i].diffuse = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].specular", i);
		shader->lights[i].specular = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].routing", i);
		shader->lights[i].routing = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].dmode", i);
		shader->lights[i].dmode = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].smode", i);
		shader->lights[i].smode = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].distAttnMode", i);
		shader->lights[i].distAttnMode = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].attnDistA", i);
		shader->lights[i].attnDistA = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].attnDistB", i);
		shader->lights[i].attnDistB = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].attnAngleA", i);
		shader->lights[i].attnAngleA = glGetUniformLocation(shader->program, str);
		sprintf(str, "lights[%d].attnAngleB", i);
		shader->lights[i].attnAngleB = glGetUniformLocation(shader->program, str);
	}
	resetN2UniformCache(shader);
}

template<typename ShaderType>
void setN2Uniforms(const PolyParam *pp, ShaderType *shader)
{
	if (pp->mvMatrix != shader->lastMvMat)
	{
		shader->lastMvMat = pp->mvMatrix;
		glUniformMatrix4fv(shader->mvMat, 1, GL_FALSE, pp->mvMatrix);
	}
	if (pp->normalMatrix != shader->lastNormalMat)
	{
		shader->lastNormalMat = pp->normalMatrix;
		glUniformMatrix4fv(shader->normalMat, 1, GL_FALSE, pp->normalMatrix);
	}
	if (pp->projMatrix != shader->lastProjMat)
	{
		shader->lastProjMat = pp->projMatrix;
		glUniformMatrix4fv(shader->projMat, 1, GL_FALSE, pp->projMatrix);
	}
	glUniform1f(shader->glossCoef0, pp->glossCoef0);

	N2LightModel *const lightModel = pp->lightModel;
	if (lightModel != shader->lastLightModel)
	{
		shader->lastLightModel = lightModel;
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
				glUniform1i(shader->lights[i].parallel, light.parallel);

				glUniform4fv(shader->lights[i].color, 1, light.color);
				glUniform4fv(shader->lights[i].direction, 1, light.direction);
				glUniform4fv(shader->lights[i].position, 1, light.position);

				glUniform1i(shader->lights[i].diffuse, light.diffuse);
				glUniform1i(shader->lights[i].specular, light.specular);
				glUniform1i(shader->lights[i].routing, light.routing);
				glUniform1i(shader->lights[i].dmode, light.dmode);
				glUniform1i(shader->lights[i].smode, light.smode);
				glUniform1i(shader->lights[i].distAttnMode, light.distAttnMode);

				glUniform1f(shader->lights[i].attnDistA, light.attnDistA);
				glUniform1f(shader->lights[i].attnDistB, light.attnDistB);
				glUniform1f(shader->lights[i].attnAngleA, light.attnAngleA);
				glUniform1f(shader->lights[i].attnAngleB, light.attnAngleB);
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
	}
	glUniform1i(shader->envMapping, pp->envMapping);
	glUniform1i(shader->bumpMapping, pp->pcw.Texture == 1 && pp->tcw.PixelFmt == PixelBumpMap);

	glEnable(GL_CLIP_DISTANCE0);
	glEnable(GL_CLIP_DISTANCE0 + 1);
}
