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
#include "dx11context.h"
#ifndef LIBRETRO
#include "rend/osd.h"
#ifdef USE_SDL
#include "sdl/sdl.h"
#endif
#include "hw/pvr/Renderer_if.h"
#include "emulator.h"
#include "dx11_driver.h"
#include "imgui/backends/imgui_impl_dx11.h"
#ifdef TARGET_UWP
#include <windows.h>
#include <gamingdeviceinformation.h>
#endif

#include "nowide/stackstring.hpp"

DX11Context theDX11Context;

bool DX11Context::init(bool keepCurrentWindow)
{
	NOTICE_LOG(RENDERER, "DX11 Context initializing");
	GraphicsContext::instance = this;
#ifdef USE_SDL
	if (!keepCurrentWindow && !sdl_recreate_window(0))
		return false;
#endif
#ifdef TARGET_UWP
	GAMING_DEVICE_MODEL_INFORMATION info {};
	GetGamingDeviceModelInformation(&info);
	if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT && info.deviceId != GAMING_DEVICE_DEVICE_ID_NONE)
	{
		Windows::Graphics::Display::Core::HdmiDisplayInformation^ dispInfo = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
		Windows::Graphics::Display::Core::HdmiDisplayMode^ displayMode = dispInfo->GetCurrentDisplayMode();
		NOTICE_LOG(RENDERER, "HDMI resolution: %d x %d", displayMode->ResolutionWidthInRawPixels, displayMode->ResolutionHeightInRawPixels);
		settings.display.width = displayMode->ResolutionWidthInRawPixels;
		settings.display.height = displayMode->ResolutionHeightInRawPixels;
		settings.display.uiScale = settings.display.width / 1920.0f * 1.4f;
	}
#endif

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	HRESULT hr;
	hr = D3D11CreateDevice(
	    nullptr, // Specify nullptr to use the default adapter.
	    D3D_DRIVER_TYPE_HARDWARE,
	    nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT, // | D3D11_CREATE_DEVICE_DEBUG,
	    featureLevels,
	    ARRAYSIZE(featureLevels),
	    D3D11_SDK_VERSION,
	    &pDevice.get(),
	    &featureLevel,
	    &pDeviceContext.get());
	if (FAILED(hr)) {
		WARN_LOG(RENDERER, "D3D11 device creation failed: %x", hr);
		return false;
	}

	ComPtr<IDXGIDevice2> dxgiDevice;
	pDevice.as(dxgiDevice);

	ComPtr<IDXGIAdapter> dxgiAdapter;
	dxgiDevice->GetAdapter(&dxgiAdapter.get());
	DXGI_ADAPTER_DESC desc;
	dxgiAdapter->GetDesc(&desc);
	nowide::stackstring wdesc;
	wdesc.convert(desc.Description);
	adapterDesc = wdesc.get();
	adapterVersion = std::to_string(desc.Revision);
	vendorId = desc.VendorId;

	ComPtr<IDXGIFactory1> dxgiFactory;
	dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), (void **)&dxgiFactory.get());

	ComPtr<IDXGIFactory2> dxgiFactory2;
	dxgiFactory.as(dxgiFactory2);

	if (dxgiFactory2)
	{
		// DX 11.1
		DXGI_SWAP_CHAIN_DESC1 desc{};
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.SampleDesc.Count = 1;
		desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

#ifdef TARGET_UWP
		desc.Width = settings.display.width;
		desc.Height = settings.display.height;
		hr = dxgiFactory2->CreateSwapChainForCoreWindow(pDevice, (IUnknown *)window, &desc, nullptr, &swapchain1.get());
#else
		hr = dxgiFactory2->CreateSwapChainForHwnd(pDevice, (HWND)window, &desc, nullptr, nullptr, &swapchain1.get());
#endif
		if (SUCCEEDED(hr))
			swapchain1.as(swapchain);
	}
	else
	{
		// DX 11.0
		swapchain1.reset();
#ifdef TARGET_UWP
		return false;
#endif
		DXGI_SWAP_CHAIN_DESC desc{};
		desc.BufferCount = 2;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.OutputWindow = (HWND)window;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.BufferCount = 2;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		hr = dxgiFactory->CreateSwapChain(pDevice, &desc, &swapchain.get());
	}
	if (FAILED(hr)) {
		WARN_LOG(RENDERER, "D3D11 swap chain creation failed: %x", hr);
		pDevice.reset();
		return false;
	}

#ifndef TARGET_UWP
	// Prevent DXGI from monitoring our message queue for ALT+Enter
	dxgiFactory->MakeWindowAssociation((HWND)window, DXGI_MWA_NO_WINDOW_CHANGES);
#endif
	D3D11_FEATURE_DATA_SHADER_CACHE cacheSupport{};
	if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D11_FEATURE_SHADER_CACHE, &cacheSupport, (UINT)sizeof(cacheSupport))))
	{
		_hasShaderCache = cacheSupport.SupportFlags & D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_DISK_CACHE;
		if (!_hasShaderCache)
			NOTICE_LOG(RENDERER, "No system-provided shader cache");
	}

	imguiDriver = std::unique_ptr<ImGuiDriver>(new DX11Driver(pDevice, pDeviceContext));
	resize();
	shaders.init(pDevice, &D3DCompile);
	overlay.init(pDevice, pDeviceContext, &shaders, &samplers);
	bool success = checkTextureSupport();
	if (!success)
		term();
	return success;
}

void DX11Context::term()
{
	NOTICE_LOG(RENDERER, "DX11 Context terminating");
	GraphicsContext::instance = nullptr;
	overlay.term();
	samplers.term();
	shaders.term();
	imguiDriver.reset();
	renderTargetView.reset();
	swapchain1.reset();
	swapchain.reset();
	if (pDeviceContext)
	{
		pDeviceContext->ClearState();
		pDeviceContext->Flush();
	}
	pDeviceContext.reset();
	pDevice.reset();
	d3dcompiler = nullptr;
	if (d3dcompilerHandle != NULL)
	{
		FreeLibrary(d3dcompilerHandle);
		d3dcompilerHandle = NULL;
	}
}

void DX11Context::Present()
{
	if (!frameRendered)
		return;
	frameRendered = false;
	bool swapOnVSync = !settings.input.fastForwardMode && config::VSync;
	HRESULT hr;
	if (!swapchain)
	{
		hr = DXGI_ERROR_DEVICE_RESET;
	}
	else if (swapOnVSync)
	{
		int swapInterval = std::min(4, std::max(1, (int)(settings.display.refreshRate / 60)));
		hr = swapchain->Present(swapInterval, 0);
	}
	else
	{
		hr = swapchain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
	}
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		WARN_LOG(RENDERER, "Present failed: device removed/reset");
		handleDeviceLost();
	}
	else if (hr != DXGI_ERROR_WAS_STILL_DRAWING && FAILED(hr))
		WARN_LOG(RENDERER, "Present failed %x", hr);
}

void DX11Context::EndImGuiFrame()
{
	if (pDevice && pDeviceContext && renderTargetView)
	{
		if (!overlayOnly)
		{
			pDeviceContext->OMSetRenderTargets(1, &renderTargetView.get(), nullptr);
			const FLOAT black[4] { 0.f, 0.f, 0.f, 1.f };
			pDeviceContext->ClearRenderTargetView(renderTargetView, black);
			if (renderer != nullptr)
				renderer->RenderLastFrame();
		}
		if (overlayOnly)
		{
			if (crosshairsNeeded() || config::FloatVMUs)
				overlay.draw(settings.display.width, settings.display.height, config::FloatVMUs, true);
		}
		else
		{
			overlay.draw(settings.display.width, settings.display.height, true, false);
		}
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	frameRendered = true;
}

void DX11Context::resize()
{
	if (!pDevice)
		return;
	if (swapchain)
	{
		ID3D11RenderTargetView *nullRTV = nullptr;
		pDeviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
		renderTargetView.reset();
#ifdef TARGET_UWP
		HRESULT hr = swapchain->ResizeBuffers(2, settings.display.width, settings.display.height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
#else
		HRESULT hr = swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			handleDeviceLost();
		    return;
		}
#endif
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "ResizeBuffers failed: %x", hr);
			return;
		}

		// Create a render target view
		ComPtr<ID3D11Texture2D> backBuffer;
		hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backBuffer.get());
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "swapChain->GetBuffer() failed: %x", hr);
			return;
		}

		hr = pDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView.get());
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "CreateRenderTargetView failed: %x", hr);
			return;
		}
		pDeviceContext->OMSetRenderTargets(1, &renderTargetView.get(), nullptr);

		if (swapchain1)
		{
			DXGI_SWAP_CHAIN_DESC1 desc;
			swapchain1->GetDesc1(&desc);
#ifndef TARGET_UWP
			settings.display.width = desc.Width;
			settings.display.height = desc.Height;
#endif
			NOTICE_LOG(RENDERER, "Swapchain resized: %d x %d", desc.Width, desc.Height);
		}
		else
		{
			DXGI_SWAP_CHAIN_DESC desc;
			swapchain->GetDesc(&desc);
			settings.display.width = desc.BufferDesc.Width;
			settings.display.height = desc.BufferDesc.Height;
			NOTICE_LOG(RENDERER, "Swapchain resized: %d x %d", desc.BufferDesc.Width, desc.BufferDesc.Height);
		}
	}
	// TODO minimized window
}

void DX11Context::handleDeviceLost()
{
	if (pDevice)
	{
		HRESULT hr = pDevice->GetDeviceRemovedReason();
		WARN_LOG(RENDERER, "Device removed reason: %x", hr);
	}
	rend_term_renderer();
	term();
	if (init(true))
	{
		rend_init_renderer();
	}
	else
	{
		Renderer* rend_norend();
		renderer = rend_norend();
		renderer->Init();
	}
}

const pD3DCompile DX11Context::getCompiler()
{
	if (d3dcompiler == nullptr)
	{
#ifndef TARGET_UWP
		d3dcompilerHandle = LoadLibraryA("d3dcompiler_47.dll");
		if (d3dcompilerHandle == NULL)
			d3dcompilerHandle = LoadLibraryA("d3dcompiler_46.dll");
		if (d3dcompilerHandle == NULL)
		{
			WARN_LOG(RENDERER, "Neither d3dcompiler_47.dll or d3dcompiler_46.dll can be loaded");
			return D3DCompile;
		}
		d3dcompiler = (pD3DCompile)GetProcAddress(d3dcompilerHandle, "D3DCompile");
#endif
		if (d3dcompiler == nullptr)
			d3dcompiler = D3DCompile;
	}
	return d3dcompiler;
}
#endif // !LIBRETRO

bool DX11Context::checkTextureSupport()
{
	const DXGI_FORMAT formats[] = { DXGI_FORMAT_B5G5R5A1_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_A8_UNORM };
	const char * const fmtNames[] = { "B5G5R5A1", "B4G4R4A4", "B5G6R5", "B8G8R8A8", "A8" };
	const TextureType dcTexTypes[] = { TextureType::_5551, TextureType::_4444, TextureType::_565, TextureType::_8888, TextureType::_8 };
	UINT support;
	for (std::size_t i = 0; i < std::size(formats); i++)
	{
		supportedTexFormats[(int)dcTexTypes[i]] = false;
		pDevice->CheckFormatSupport(formats[i], &support);
		if ((support & (D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)) != (D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE))
		{
			if (formats[i] == DXGI_FORMAT_B8G8R8A8_UNORM)
			{
				// Can't do much without this format
				ERROR_LOG(RENDERER, "Fatal: Format %s not supported", fmtNames[i]);
				return false;
			}
			WARN_LOG(RENDERER, "Format %s not supported", fmtNames[i]);
		}
		else
		{
			if ((support & D3D11_FORMAT_SUPPORT_MIP) == 0)
				WARN_LOG(RENDERER, "Format %s doesn't support mipmaps", fmtNames[i]);
			else if ((support & (D3D11_FORMAT_SUPPORT_MIP_AUTOGEN | D3D11_FORMAT_SUPPORT_RENDER_TARGET)) != (D3D11_FORMAT_SUPPORT_MIP_AUTOGEN | D3D11_FORMAT_SUPPORT_RENDER_TARGET))
				WARN_LOG(RENDERER, "Format %s doesn't support mipmap autogen", fmtNames[i]);
			else
				supportedTexFormats[(int)dcTexTypes[i]] = true;
		}
	}

	return true;
}
