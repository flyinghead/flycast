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
#include "metal_context.h"
#include "log/Log.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace {
std::atomic<uint64_t> s_gpuWaitTimeouts{0};
std::atomic<uint64_t> s_phantomEvents{0};
// Frame slots parked in the quarantine (see QuarantinedFrame) because their
// GPU work had not finished when the slot was recycled; and the objects that
// had to be leaked at Term() because the GPU never finished with them.
std::atomic<uint64_t> s_quarantinedFrames{0};
std::atomic<uint64_t> s_leakedAtTerm{0};

void logGpuWaitTimeout(const char *where)
{
	const uint64_t n = ++s_gpuWaitTimeouts;
	if (n == 1 || (n % 60) == 0)
		WARN_LOG(RENDERER, "GPU-completion wait timed out (200ms) at %s, count=%llu",
			where, (unsigned long long)n);
}

// The Metal context doesn't start before macOS 12 (see MetalContext::init)
bool waitForEvent(id<MTLSharedEvent> event, uint64_t timeoutMs)
{
	if (@available(macOS 12.0, iOS 15.0, tvOS 15.0, *))
		return [event waitUntilSignaledValue:1 timeoutMS:timeoutMs];
	return true;
}

void logPhantomEvent()
{
	const uint64_t n = ++s_phantomEvents;
	if (n == 1 || (n % 60) == 0)
		DEBUG_LOG(RENDERER, "EndFrame: no command buffer this frame, count=%llu",
			(unsigned long long)n);
}
}

void MetalCommandPool::Init(size_t chainSize)
{
	this->chainSize = chainSize;
	device = MetalContext::Instance()->GetDevice();

	queue = [device newCommandQueue];

	DEBUG_LOG(RENDERER, "MetalCommandPool::Init chainSize=%zu", chainSize);
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

bool MetalCommandPool::gpuDone(id<MTLCommandBuffer> buffer)
{
	// Completed OR Error: either way the GPU is finished with the resources.
	// An errored buffer never signals its shared event, which is why the
	// event alone is not a safe completion test (Term() used to hang on it).
	return buffer == nil || buffer.status >= MTLCommandBufferStatusCompleted;
}

bool MetalCommandPool::waitGpuDone(id<MTLCommandBuffer> buffer, uint32_t timeoutMs)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (!gpuDone(buffer))
	{
		if (std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
	return true;
}

void MetalCommandPool::quarantineSlot(int slot, const char *where)
{
	if (inFlightBuffers[slot] == nil || inFlightObjects[slot].empty())
	{
		// Nothing on the GPU, or nothing whose destruction could hurt it.
		inFlightObjects[slot].clear();
		return;
	}
	const uint64_t n = ++s_quarantinedFrames;
	if (n == 1 || (n % 60) == 0)
		NOTICE_LOG(RENDERER, "GPU still busy at %s — %zu deletables quarantined until the command buffer completes (count=%llu, held=%zu)",
			where, inFlightObjects[slot].size(), (unsigned long long)n, quarantine.size() + 1);
	quarantine.push_back({inFlightBuffers[slot], std::move(inFlightObjects[slot])});
	inFlightObjects[slot].clear();
}

void MetalCommandPool::sweepQuarantine()
{
	// Non-blocking: free whatever the GPU has finished with since last frame.
	// Runs on the render thread (the pool's owner), so the deletables'
	// destructors (BufferHolder pushes back into the drawer's pool) stay on
	// the thread they expect.
	for (size_t i = 0; i < quarantine.size();)
	{
		if (gpuDone(quarantine[i].buffer))
			quarantine.erase(quarantine.begin() + i);
		else
			++i;
	}
}

void MetalCommandPool::Term()
{
	// Bounded, and keyed on the command buffers rather than the events: an
	// errored buffer never signals, and the old UINT64_MAX event wait ran on
	// the frontend's main thread from unload. If the GPU still hasn't finished
	// after the bound, the deletables are LEAKED on purpose — a few textures
	// per wedged GPU is better than recycling buffers it may still read.
	constexpr uint32_t kTermWaitMs = 2000;
	for (size_t i = 0; i < inFlightBuffers.size(); i++)
	{
		if (!waitGpuDone(inFlightBuffers[i], kTermWaitMs))
			quarantineSlot((int)i, "Term");
	}
	inFlightObjects.clear();
	inFlightBuffers.clear();
	events.clear();
	for (QuarantinedFrame& q : quarantine)
	{
		if (waitGpuDone(q.buffer, kTermWaitMs))
			continue;
		s_leakedAtTerm += q.objects.size();
		WARN_LOG(RENDERER, "MetalCommandPool::Term: GPU never finished a command buffer (status=%ld) — leaking %zu deletables (total leaked=%llu)",
			(long)q.buffer.status, q.objects.size(), (unsigned long long)s_leakedAtTerm.load());
		for (auto& obj : q.objects)
			(void)obj.release();
	}
	quarantine.clear();
}

void MetalCommandPool::BeginFrame()
{
	if (frameStarted)
		return;
	frameStarted = true;
	index = (index + 1) % chainSize;
	// Bounded wait (same latent deadlock class as WaitForLastFrame): a missed
	// GPU-completion signal on the buffer we're about to recycle must not hang
	// the render+SH4 threads forever. Healthy frames complete in <17ms so the
	// 200ms timeout never fires; worst case is a dropped frame, not a hang.
	sweepQuarantine();
	if (events[index] != nil)
		if (!waitForEvent(events[index], 200))
			logGpuWaitTimeout("BeginFrame");
	// The timeout is only a stall if the buffer is genuinely still running;
	// an errored buffer never signals but IS finished (gpuDone). Only a
	// still-running buffer's deletables go to quarantine.
	if (!gpuDone(inFlightBuffers[index]))
		quarantineSlot(index, "BeginFrame");
	inFlightBuffers[index] = nil;
	inFlightObjects[index].clear();
}

void MetalCommandPool::EndFrame()
{
	if (!frameStarted)
		return;
	frameStarted = false;
	// No command buffer allocated this frame: nothing could ever signal the
	// event we'd create below, so the next WaitForLastFrame/BeginFrame on this
	// index would sit out the full 200ms bound. There is also nothing for a
	// later wait to depend on: no GPU work was submitted for this slot.
	// Leave the slot's event nil so the bounded waits skip it.
	if (inFlightBuffers[index] == nil)
	{
		logPhantomEvent();
		events[index] = nil;
		return;
	}
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
	// Same bound as WaitForLastFrame/BeginFrame. This was the last UINT64_MAX
	// wait on the frame path (render-to-texture, metal_drawer EndFrameAndWait
	// callers): a missed signal here hung the render thread — and through
	// renderEnd.Wait(), the SH4 thread — permanently, with no log. Healthy
	// frames signal in <17ms; on a miss we log, drop the wait and continue.
	// Term() keeps its unbounded wait — at teardown a hang is preferable to
	// freeing objects the GPU may still touch.
	bool signaled = true;
	for (size_t i = 0; i < events.size(); i++)
	{
		if (events[i] != nil && !waitForEvent(events[i], 200))
		{
			logGpuWaitTimeout("EndFrameAndWait");
			if ((int)i == index)
				signaled = false;
		}
	}
	// Callers read the frame back (getBytes) right after this; on a timeout
	// that readback is torn — a dropped frame, unchanged. What must not
	// happen is freeing this slot's deletables under a running buffer.
	// The event is signaled once the GPU has executed every command encoded
	// before it, so a signaled slot is safe to free even though the command
	// buffer status only switches to Completed a little later.
	if (!signaled && !gpuDone(inFlightBuffers[index]))
		quarantineSlot(index, "EndFrameAndWait");
	else
		inFlightObjects[index].clear();
}

void MetalCommandPool::WaitForLastFrame()
{
	// EndFrame() assigns events[index] and encodeSignalEvent:value:1 on the
	// just-committed command buffer, then BeginFrame() bumps index on the
	// next frame. So between EndFrame() and the next BeginFrame(),
	// events[index] is exactly the event the most recent frame's GPU work
	// signals on completion. Wait on that single event only.
	//
	// Deadlock-free: the signal is encoded by the GPU on a committed command
	// buffer (no CPU-thread dependency), the event is non-nil here because
	// EndFrame() just created it.
	//
	// Bounded wait: a never-arriving GPU-completion signal must not wedge the
	// render thread (and through renderEnd.Wait(), the SH4 thread) forever.
	// Healthy frames complete in <17ms, so the 200ms timeout never fires in
	// normal operation; worst case on a missed signal is a dropped frame
	// instead of a permanent hang.
	if (events[index] != nil)
	{
		if (!waitForEvent(events[index], 200))
			logGpuWaitTimeout("WaitForLastFrame");
	}
}