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

#include "metal_commandpool.h"
#import "metal_context.h"

void MetalCommandPool::Init(size_t chainSize)
{
    this->chainSize = chainSize;
    device = MetalContext::Instance()->GetDevice();

    queue = [device newCommandQueue];

    ERROR_LOG(RENDERER, "MetalCommandPool::Init chainSize=%d", chainSize);
    if (events.size() > chainSize)
    {
        events.resize(chainSize);
    }
    else
    {
        while (events.size() < chainSize)
        {
            events.push_back(nil);
        }
    }

    inFlightBuffers.resize(chainSize);
    inFlightObjects.resize(chainSize);
}

void MetalCommandPool::Term()
{
    for (id<MTLSharedEvent> event : events)
    {
        if (event != nil)
            [event waitUntilSignaledValue:1 timeoutMS:UINT64_MAX];
    }
    inFlightObjects.clear();
    inFlightBuffers.clear();
    events.clear();
}

void MetalCommandPool::BeginFrame()
{
    if (frameStarted)
        return;
    frameStarted = true;
    index = (index + 1) % chainSize;
    if (events[index] != nil)
        [events[index] waitUntilSignaledValue:1 timeoutMS:UINT64_MAX];
    inFlightBuffers[index] = nil;
    inFlightObjects[index].clear();
}

void MetalCommandPool::EndFrame()
{
    if (!frameStarted)
        return;
    frameStarted = false;
    events[index] = [device newSharedEvent];
    [inFlightBuffers[index] encodeSignalEvent:events[index] value:1];
    [inFlightBuffers[index] commit];
}

id<MTLCommandBuffer> MetalCommandPool::Allocate()
{
    verify(frameStarted);
    if (inFlightBuffers[index] == nil)
        inFlightBuffers[index] = [queue commandBuffer];
    return inFlightBuffers[index];
}

void MetalCommandPool::EndFrameAndWait()
{
    EndFrame();
    for (id<MTLSharedEvent> event : events)
    {
        if (event != nil)
            [event waitUntilSignaledValue:1 timeoutMS:UINT64_MAX];
    }
    inFlightObjects[index].clear();
}