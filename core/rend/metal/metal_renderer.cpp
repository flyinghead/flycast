/*
    Copyright 2024 flyinghead

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

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "metal_renderer.h"
#include "hw/pvr/ta.h"

bool MetalRenderer::Init()
{
    NOTICE_LOG(RENDERER, "Metal renderer initializing");

    shaders = MetalShaders();
    samplers = MetalSamplers();

    frameRendered = false;

    return true;
}

void MetalRenderer::Term() {
    shaders.term();
    samplers.term();
}

void MetalRenderer::Process(TA_context *ctx) {

}

bool MetalRenderer::Render() {
    return true;
}

void MetalRenderer::RenderFramebuffer(const FramebufferInfo &info) {

}

Renderer* rend_Metal()
{
    return new MetalRenderer();
}
