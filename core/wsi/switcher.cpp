/*
    Created on: Oct 18, 2019

	Copyright 2019 flyinghead

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
#ifndef LIBRETRO
#include "context.h"
#include "cfg/option.h"

#include "gl_context.h"
#include "rend/dx9/dxcontext.h"
#include "rend/dx11/dx11context.h"
#ifdef USE_VULKAN
#include "rend/vulkan/vulkan_context.h"

VulkanContext theVulkanContext;
#endif
#ifdef USE_METAL
#include "rend/metal/metal_context.h"

MetalContext theMetalContext;
#endif
GraphicsContext *GraphicsContext::instance;

void initRenderApi(void *window, void *display)
{
#ifdef USE_VULKAN
	if (isVulkan(config::RendererType))
	{
		theVulkanContext.setWindow(window, display);
		if (theVulkanContext.init())
			return;
		// Fall back to OpenGL
		WARN_LOG(RENDERER, "Vulkan init failed. Falling back to OpenGL.");
		config::RendererType = RenderType::OpenGL;
	}
#endif
#ifdef USE_DX11
	bool dx11Failed = false;
	if (config::RendererType == RenderType::DirectX11 || config::RendererType == RenderType::DirectX11_OIT)
	{
		theDX11Context.setWindow(window, display);
		if (theDX11Context.init())
			return;
		dx11Failed = true;
		WARN_LOG(RENDERER, "DirectX 11 init failed. Falling back to DirectX 9.");
		config::RendererType = RenderType::DirectX9;
	}
#endif
#ifdef USE_DX9
	if (config::RendererType == RenderType::DirectX9)
	{
		theDXContext.setWindow(window, display);
		if (theDXContext.init())
			return;
		// Fall back to OpenGL
		WARN_LOG(RENDERER, "DirectX 9 init failed. Falling back to OpenGL.");
		config::RendererType = RenderType::OpenGL;
	}
#endif
#ifdef USE_METAL
	if (isMetal(config::RendererType))
	{
		theMetalContext.setWindow(window, display);

		if (theMetalContext.init())
			return;
		// Fall back to OpenGL
		WARN_LOG(RENDERER, "Metal init failed. Falling back to OpenGL.");
		config::RendererType = RenderType::OpenGL;
	}
#endif
#ifdef USE_OPENGL
	if (!isOpenGL(config::RendererType))
		config::RendererType = RenderType::OpenGL;
	theGLContext.setWindow(window, display);
	if (theGLContext.init())
		return;
#endif
#ifdef USE_DX11
	if (!dx11Failed)
	{
		// Try dx11 as a last resort if it hasn't been tried before
		WARN_LOG(RENDERER, "OpenGL init failed. Trying DirectX 11.");
		config::RendererType = RenderType::DirectX11;
		theDX11Context.setWindow(window, display);
		if (theDX11Context.init())
			return;
	}
#endif
	die("Cannot initialize the graphics API");
}

void termRenderApi()
{
	if (GraphicsContext::Instance() != nullptr)
		GraphicsContext::Instance()->term();
	verify(GraphicsContext::Instance() == nullptr);
}

void switchRenderApi()
{
	void *window = nullptr;
	void *display = nullptr;
	if (GraphicsContext::Instance() != nullptr)
		GraphicsContext::Instance()->getWindow(&window,  &display);
	termRenderApi();
	initRenderApi(window, display);
}
#endif
