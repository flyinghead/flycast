// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace metal {
// Keep a common host-time deadline across submissions. GPU/CPU delays must
// not be added to every subsequent frame or duplicate presentation.
class PresentationTimeline {
public:
	void reset() { source = 0; host = 0; previous = -1; }
	// Source time is captured at the emulated video swap that produced the
	// image, retaining sub-field precision. No assumed 30/60 Hz game rate and no accumulated host jitter.
	double videoFrame(double now, double videoTime, double latency, double displayLead = 0) {
		if (!std::isfinite(now) || !std::isfinite(videoTime) || !std::isfinite(latency)
				|| !std::isfinite(displayLead) || displayLead < 0 || latency <= 0) { reset(); return 0; }
		double target = host + videoTime - source;
		// Audio/emulation can initially run ahead in a burst. Deadlines
		// beyond the composition horizon evict images before they are due.
		// Rebase that phase instead of chasing an ever-future mailbox.
		if (previous < 0 || videoTime < previous || videoTime - previous > 1
				|| now - target > 0.1 || target - now > latency + 1e-6) {
			source = videoTime; host = now + latency; target = host;
		}
		previous = videoTime;
		// Producer buffering and the display's advance request are different
		// intervals. Only producer buffering bounds the pending image queue.
		return target + displayLead;
	}
private:
	double source = 0, host = 0, previous = -1;
};
// Match Vulkan's explicit duplicate presents. Ordinary frames follow the
// emulator's render cadence and the layer's VSync without an extra rate cap.
inline int presentationCount(double refreshRate, int gameInterval, bool guiOpen, bool vsync, bool fastForward, bool duplicateFrames)
{
	if (!duplicateFrames || guiOpen || !vsync || fastForward) return 1;
	if (!std::isfinite(refreshRate) || refreshRate <= 0) refreshRate = 60;
	// Vulkan rounds the display multiple before applying the game's interval.
	const double count = std::floor(std::max(1.0, refreshRate / 60)) * std::clamp(gameInterval, 1, 2);
	return int(std::min(count, double(std::numeric_limits<int>::max())));
}
inline double minimumFrameDuration(double refreshRate, int /*gameInterval*/, bool guiOpen, bool vsync, bool fastForward, bool duplicateFrames = false)
{
	// Vulkan does not impose a second duration limit on ordinary presents or
	// GUI frames. Reserve timed presentation for explicitly requested copies.
	if (!duplicateFrames || guiOpen || !vsync || fastForward) return 0;
	if (!std::isfinite(refreshRate) || refreshRate <= 0) refreshRate = 60;
	return 1 / refreshRate;
}
}
