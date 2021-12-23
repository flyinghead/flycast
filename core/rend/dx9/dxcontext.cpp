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
#include "emulator.h"
#include "dx9_driver.h"

DXContext theDXContext;

bool DXContext::init(bool keepCurrentWindow)
{
	GraphicsContext::instance = this;
#ifdef USE_SDL
	if (!keepCurrentWindow && !sdl_recreate_window(0))
		return false;
#endif

	pD3D.reset(Direct3DCreate9(D3D_SDK_VERSION));
	if (!pD3D)
		return false;
	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.hDeviceWindow = (HWND)window;
	d3dpp.Windowed = true;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	d3dpp.EnableAutoDepthStencil = FALSE;						// No need for depth/stencil buffer for the backbuffer
	swapOnVSync = !settings.input.fastForwardMode && config::VSync;
	if (swapOnVSync)
	{
		switch ((int)(settings.display.refreshRate / 60))
		{
		case 0:
		case 1:
			d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
			break;
		case 2:
			d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
			break;
		case 3:
			d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_THREE;
			break;
		case 4:
		default:
			d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_FOUR;
			break;
		}
	}
	else
		d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	// TODO should be 0 in windowed mode
	//d3dpp.FullScreen_RefreshRateInHz = swapOnVSync ? 60 : 0;
	if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)window,
			D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &pDevice.get())))
	    return false;
	imguiDriver = std::unique_ptr<ImGuiDriver>(new DX9Driver());
	gui_init();
	overlay.init(pDevice);
	return ImGui_ImplDX9_Init(pDevice.get());
}

void DXContext::term()
{
	GraphicsContext::instance = nullptr;
	overlay.term();
	imguiDriver.reset();
	ImGui_ImplDX9_Shutdown();
	gui_term();
	pDevice.reset();
	pD3D.reset();
}

void DXContext::Present()
{
	if (!frameRendered)
		return;
	frameRendered = false;
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
	else
	{
		if (swapOnVSync != (!settings.input.fastForwardMode && config::VSync))
		{
			DEBUG_LOG(RENDERER, "Switch vsync %d", !swapOnVSync);
			if (renderer != nullptr)
			{
				renderer->Term();
				delete renderer;
			}
			term();
			init(true);
			if (renderer != nullptr)
			{
				renderer = new D3DRenderer();
				renderer->Init();
				rend_resize_renderer();
			}
		}
	}
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
				overlay.draw(settings.display.width, settings.display.height, config::FloatVMUs, true);
		}
		else
		{
			overlay.draw(settings.display.width, settings.display.height, true, false);
		}
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		pDevice->EndScene();
	}
	frameRendered = true;
}

void DXContext::resize()
{
	if (!pDevice)
		return;
	RECT rect;
	GetClientRect((HWND)window, &rect);
	d3dpp.BackBufferWidth = settings.display.width = rect.right;
	d3dpp.BackBufferHeight = settings.display.height = rect.bottom;
	if (settings.display.width == 0 || settings.display.height == 0)
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
