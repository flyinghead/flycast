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
#include "metal_apple.h"
#include "metal.h"

#include <vector>

class MetalCommandPool : public MetalFlightManager
{
public:
	void Init(size_t chainSize = 3);
	void Term();
	void BeginFrame();
	void EndFrame();
	void EndFrameAndWait();
	// Block until the GPU has finished the command buffer committed by the
	// most recent EndFrame() (i.e. the one signalled into events[index]).
	// Unlike EndFrameAndWait() this does NOT call EndFrame() itself — the
	// caller must have already ended the frame. Used by the threaded-render
	// screen path to guarantee GPU completion before signalling RENDER_DONE.
	void WaitForLastFrame();
	id<MTLCommandBuffer> Allocate();

	int GetIndex() const {
		return index;
	}

	void addToFlight(MetalDeletable *object) override {
		inFlightObjects[index].emplace_back(object);
	}

private:
	// A frame slot whose bounded GPU-completion wait timed out. Its deletables
	// must NOT be destroyed yet: their destructors recycle host buffers the
	// still-executing command buffer may read — GPU-side undefined behaviour
	// on top of the stall that caused the timeout. They are held here, keyed
	// on the command buffer, and freed once MTLCommandBuffer.status reports
	// Completed/Error.
	struct QuarantinedFrame
	{
		id<MTLCommandBuffer> buffer;
		std::vector<std::unique_ptr<MetalDeletable>> objects;
	};
	static bool gpuDone(id<MTLCommandBuffer> buffer);
	void quarantineSlot(int slot, const char *where);
	void sweepQuarantine();
	// Poll `buffer` until gpuDone() or timeoutMs elapsed; true if done.
	static bool waitGpuDone(id<MTLCommandBuffer> buffer, uint32_t timeoutMs);

	int index = 0;
	std::vector<id<MTLCommandBuffer>> inFlightBuffers;
	std::vector<id<MTLSharedEvent>> events;
	size_t chainSize;
	std::vector<std::vector<std::unique_ptr<MetalDeletable>>> inFlightObjects;
	std::vector<QuarantinedFrame> quarantine;
	bool frameStarted = false;
	id<MTLCommandQueue> queue = nil;
	id<MTLDevice> device = nil;
};
