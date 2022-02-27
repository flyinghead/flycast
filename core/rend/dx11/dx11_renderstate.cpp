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
#include "dx11_renderstate.h"
#include "dx11context.h"

HRESULT DepthStencilStates::createDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *desc, ID3D11DepthStencilState **state)
{
	return theDX11Context.getDevice()->CreateDepthStencilState(desc, state);
}

HRESULT BlendStates::createBlendState(const D3D11_BLEND_DESC *desc, ID3D11BlendState **state)
{
	return theDX11Context.getDevice()->CreateBlendState(desc, state);
}
