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
#pragma once

#ifdef __OBJC__
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#endif

#include "wsi/context.h"

class MetalContext : public GraphicsContext
{
public:
    MetalContext();
    ~MetalContext() override;

    bool init();
    void term() override;

#ifdef __OBJC__
    id<MTLDevice> GetDevice() const { return device; }
    CAMetalLayer* GetLayer() const { return layer; }
    id<MTLCommandQueue> GetQueue() const { return queue; }
    id<MTLCommandBuffer> commandBuffer = nil;
#endif
    void resize() override;
    void Present();

    std::string getDriverName() override;

    std::string getDriverVersion() override {
        return "";
    }

    bool isAMD() override {
        return false;
    }

    bool hasPerPixel() override {
        return true;
    }

    static MetalContext* Instance() { return contextInstance; }
private:
#ifdef __OBJC__
    NSAutoreleasePool *pool;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = nil;
    CAMetalLayer* layer;
#endif
    static MetalContext* contextInstance;
};