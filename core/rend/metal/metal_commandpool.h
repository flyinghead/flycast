/*
    Copyright 2025 flyinghead

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
#include "metal.h"

#include <Metal/Metal.h>
#include <vector>

class MetalCommandPool : public MetalFlightManager
{
public:
    void Init(size_t chainSize = 3);
    void Term();
    void BeginFrame();
    void EndFrame();
    void EndFrameAndWait();
    id<MTLCommandBuffer> Allocate();

    int GetIndex() const {
        return index;
    }

    void addToFlight(MetalDeletable *object) override {
        inFlightObjects[index].emplace_back(object);
    }

private:
    int index = 0;
    std::vector<id<MTLCommandBuffer>> inFlightBuffers;
    std::vector<id<MTLSharedEvent>> events;
    size_t chainSize;
    std::vector<std::vector<std::unique_ptr<MetalDeletable>>> inFlightObjects;
    bool frameStarted = false;
    id<MTLCommandQueue> queue = nil;
    id<MTLDevice> device = nil;
};
