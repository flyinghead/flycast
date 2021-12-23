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
#include "types.h"
#include <d3d11.h>
#include "windows/comptr.h"
#include <array>
#include <unordered_map>

const D3D11_COMPARISON_FUNC Zfunction[]
{
	D3D11_COMPARISON_NEVER,				//0	Never
	D3D11_COMPARISON_LESS,				//1	Less
	D3D11_COMPARISON_EQUAL,				//2	Equal
	D3D11_COMPARISON_LESS_EQUAL,		//3	Less Or Equal
	D3D11_COMPARISON_GREATER,			//4	Greater
	D3D11_COMPARISON_NOT_EQUAL,			//5	Not Equal
	D3D11_COMPARISON_GREATER_EQUAL,		//6	Greater Or Equal
	D3D11_COMPARISON_ALWAYS,			//7	Always
};

const D3D11_BLEND DestBlend[]
{
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_SRC_COLOR,
	D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_INV_BLEND_FACTOR
};

const D3D11_BLEND SrcBlend[]
{
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_BLEND_FACTOR
};

const D3D11_BLEND DestBlendAlpha[]
{
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_INV_BLEND_FACTOR
};

const D3D11_BLEND SrcBlendAlpha[]
{
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_BLEND_FACTOR
};

class DepthStencilStates
{
public:
	enum ModifierVolumeMode { Xor, Or, Inclusion, Exclusion, Final, Count };

	ComPtr<ID3D11DepthStencilState> getState(bool depth, bool depthWrite, int depthFunc, bool stencil)
	{
		int hash = (depthFunc << 3) | (int)depth | ((int)depthWrite << 1) | ((int)stencil << 2);
		auto& state = states[hash];
		if (!state)
		{
			D3D11_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = depth;
			desc.DepthWriteMask = depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = Zfunction[depthFunc];
			desc.StencilEnable = stencil;
			desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
			desc.BackFace = desc.FrontFace;
			desc.StencilWriteMask = 0xFF;
			createDepthStencilState(&desc, &state.get());
		}
		return state;
	}

	ComPtr<ID3D11DepthStencilState> getMVState(ModifierVolumeMode mode)
	{
		auto& state = mvStates[mode];
		if (!state)
		{
			D3D11_DEPTH_STENCIL_DESC desc{};
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = D3D11_COMPARISON_GREATER;
			desc.StencilEnable = true;
			desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
			switch (mode)
			{
			case Xor:
				desc.DepthEnable = true;
				desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
				desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INVERT;
				desc.StencilWriteMask = 2;
				break;
			case Or:
				desc.DepthEnable = true;
				desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
				desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
				desc.StencilWriteMask = 2;
				break;
			case Inclusion:
				desc.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
				desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
				desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
				desc.StencilReadMask = 3;
				desc.StencilWriteMask = 3;
				break;
			case Exclusion:
				desc.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
				desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
				desc.StencilReadMask = 3;
				desc.StencilWriteMask = 3;
				break;
			case Final:
				desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
				desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
				desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
				desc.StencilReadMask = 0x81;
				desc.StencilWriteMask = 3;
				break;
			default:
				break;
			}
			desc.BackFace = desc.FrontFace;
			createDepthStencilState(&desc, &state.get());
		}
		return state;
	}

	void term()
	{
		states.clear();
		for (auto& state : mvStates)
			state.reset();
	}

private:
	HRESULT createDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *desc, ID3D11DepthStencilState **state);

	std::unordered_map<int, ComPtr<ID3D11DepthStencilState>> states;
	std::array<ComPtr<ID3D11DepthStencilState>, ModifierVolumeMode::Count> mvStates;
};

class BlendStates
{
public:
	ComPtr<ID3D11BlendState> getState(bool enable, int srcBlend = 0, int destBlend = 0, bool disableWrite = false)
	{
		int hash = (int)enable | (srcBlend << 1) | (destBlend << 5) | ((int)disableWrite << 9);
		auto& state = states[hash];
		if (!state)
		{
			D3D11_BLEND_DESC desc{};
			desc.RenderTarget[0].RenderTargetWriteMask = disableWrite ? 0 : D3D11_COLOR_WRITE_ENABLE_ALL;
			desc.RenderTarget[0].BlendEnable = enable;
			desc.RenderTarget[0].SrcBlend = SrcBlend[srcBlend];
			desc.RenderTarget[0].DestBlend = DestBlend[destBlend];
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha =  SrcBlendAlpha[srcBlend];
			desc.RenderTarget[0].DestBlendAlpha = DestBlendAlpha[destBlend];
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			createBlendState(&desc, &state.get());
		}
		return state;
	}

	void term() {
		states.clear();
	}

private:
	HRESULT createBlendState(const D3D11_BLEND_DESC *, ID3D11BlendState **state);

	std::unordered_map<int, ComPtr<ID3D11BlendState>> states;
};
