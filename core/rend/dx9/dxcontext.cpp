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
#include "rend/gui.h"
#include "sdl/sdl.h"

DXContext theDXContext;
extern int screen_width, screen_height; // FIXME

bool DXContext::Init()
{
	if (!sdl_recreate_window(0))
		return false;

	pD3D.reset(Direct3DCreate9(D3D_SDK_VERSION));
	if (!pD3D)
		return false;
	NOTICE_LOG(RENDERER, "Direct3D object created");
	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;	// FIXME
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;			// TODO D3DFMT_D24FS8 if supported?
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
	//d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
	if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, sdl_get_native_hwnd(),
			D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &pDevice.get())))
	    return false;
	gui_init();
	return ImGui_ImplDX9_Init(pDevice.get());
}

void DXContext::Term()
{
	ImGui_ImplDX9_Shutdown();
	pDevice.reset();
	pD3D.reset();
}

void DXContext::Present()
{
	HRESULT result = pDevice->Present(NULL, NULL, NULL, NULL);
	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && pDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		resetDevice();
}

void DXContext::EndImGuiFrame()
{
	verify((bool)pDevice);
	pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	if (!overlay)
	{
		pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);
		if (renderer != nullptr)
			renderer->RenderLastFrame();
	}
	if (SUCCEEDED(pDevice->BeginScene()))
	{
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		pDevice->EndScene();
	}
}

void DXContext::resize()
{
	if (!pDevice)
		return;
	RECT rect;
	GetClientRect(sdl_get_native_hwnd(), &rect);
	d3dpp.BackBufferWidth = screen_width = rect.right;
	d3dpp.BackBufferHeight = screen_height = rect.bottom;
	resetDevice();
}
