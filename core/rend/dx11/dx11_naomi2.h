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
#include <windows.h>
#include <d3d11.h>
#include "windows/comptr.h"
#include "hw/pvr/ta_ctx.h"

extern const char * const DX11N2VertexShader;
extern const char * const DX11N2ColorShader;

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

class Naomi2Helper
{
public:
	void init(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext> deviceContext)
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

	void term()
	{
		polyConstantsBuffer.reset();
		lightConstantsBuffer.reset();
		deviceContext.reset();
	}

	void setConstants(const PolyParam& pp, u32 polyNumber)
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

	void setConstants(const float *mvMatrix, const float *projMatrix)
	{
		N2PolyConstants polyConstants;
		memcpy(polyConstants.mvMat, mvMatrix, sizeof(polyConstants.mvMat));
		memcpy(polyConstants.projMat, projMatrix, sizeof(polyConstants.projMat));
		setConstBuffer(polyConstantsBuffer, polyConstants);
		deviceContext->VSSetConstantBuffers(1, 1, &polyConstantsBuffer.get());
	}

	void resetCache() {
		lastModel = (N2LightModel *)1;
	}

private:
	template<typename T>
	void setConstBuffer(const ComPtr<ID3D11Buffer>& buffer, const T& data)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, &data, sizeof(T));
		deviceContext->Unmap(buffer, 0);
	}

	ComPtr<ID3D11DeviceContext> deviceContext;
	ComPtr<ID3D11Buffer> polyConstantsBuffer;
	ComPtr<ID3D11Buffer> lightConstantsBuffer;
	const N2LightModel *lastModel;
};
