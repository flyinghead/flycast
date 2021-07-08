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
#include "dxcontext.h"
#include "d3d_renderer.h"
#include "rend/gui.h"
#include "rend/osd.h"
#ifdef USE_SDL
#include "sdl/sdl.h"
#endif
#include "hw/pvr/Renderer_if.h"

DXContext theDXContext;
extern int screen_width, screen_height; // FIXME

bool DXContext::Init()
{
#ifdef USE_SDL
	if (!sdl_recreate_window(0))
		return false;
#endif

	pD3D.reset(Direct3DCreate9(D3D_SDK_VERSION));
	if (!pD3D)
		return false;
	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.hDeviceWindow = hWnd;
	d3dpp.Windowed = true;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	d3dpp.EnableAutoDepthStencil = FALSE;						// No need for depth/stencil buffer for the backbuffer
#ifndef TEST_AUTOMATION
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;		// Present with vsync
#else
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;	// Present without vsync, maximum unthrottled framerate
#endif
	if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
			D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &pDevice.get())))
	    return false;
	gui_init();
	overlay.init(pDevice);
	return ImGui_ImplDX9_Init(pDevice.get());
}

void DXContext::Term()
{
	overlay.term();
	ImGui_ImplDX9_Shutdown();
	pDevice.reset();
	pD3D.reset();
}

void DXContext::Present()
{
	HRESULT result = pDevice->Present(NULL, NULL, NULL, NULL);
	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST)
	{
		result = pDevice->TestCooperativeLevel();
		if (result == D3DERR_DEVICENOTRESET)
			resetDevice();
	}
	else if (FAILED(result))
		WARN_LOG(RENDERER, "Present failed %x", result);
}

void DXContext::EndImGuiFrame()
{
	verify((bool)pDevice);
	pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	if (!overlayOnly)
	{
		pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);
		if (renderer != nullptr)
			renderer->RenderLastFrame();
	}
	if (SUCCEEDED(pDevice->BeginScene()))
	{
		if (overlayOnly)
		{
			if (crosshairsNeeded() || config::FloatVMUs)
				overlay.draw(screen_width, screen_height, config::FloatVMUs, true);
		}
		else
		{
			overlay.draw(screen_width, screen_height, true, false);
		}
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		pDevice->EndScene();
	}
}

void DXContext::resize()
{
	if (!pDevice)
		return;
	RECT rect;
	GetClientRect(hWnd, &rect);
	d3dpp.BackBufferWidth = screen_width = rect.right;
	d3dpp.BackBufferHeight = screen_height = rect.bottom;
	if (screen_width == 0 || screen_height == 0)
		// window minimized
		return;
	resetDevice();
}

void DXContext::resetDevice()
{
	if (renderer != nullptr)
		((D3DRenderer *)renderer)->preReset();
	overlay.term();
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = pDevice->Reset(&d3dpp);
    if (hr == D3DERR_INVALIDCALL)
    {
        ERROR_LOG(RENDERER, "DX9 device reset failed");
        return;
    }
    ImGui_ImplDX9_CreateDeviceObjects();
    overlay.init(pDevice);
	if (renderer != nullptr)
		((D3DRenderer *)renderer)->postReset();
}
