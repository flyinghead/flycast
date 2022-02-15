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

class Naomi2Helper
{
public:
	void init(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext> deviceContext);

	void term()
	{
		polyConstantsBuffer.reset();
		lightConstantsBuffer.reset();
		deviceContext.reset();
	}

	void setConstants(const PolyParam& pp, u32 polyNumber);
	void setConstants(const float *mvMatrix, const float *projMatrix);

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
