// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace metal {
// Keep one video field of producer buffering and headroom in the three-image
// queue for arrival/callback jitter. The display's advance request is added
// separately by PresentationTimeline. Two fields plus another refresh can fill
// all slots before the first 60-Hz image is due. Even two fields alone can race
// the callback when 59.94-Hz contents meet a 60-Hz display.
inline double presentationBufferDuration(double videoFieldDuration)
{
	return videoFieldDuration;
}
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
// Explicit repeats follow the emulated video field, including PAL/NTSC and
// interval-2 games. Carry fractional refreshes across source frames instead of
// rounding each field before applying the game's interval.
class DuplicateCadence {
public:
	void reset(){fraction=0;refresh=field=0;}
	int count(double refreshRate,double videoFieldDuration,int gameInterval,
		bool guiOpen,bool vsync,bool fastForward,bool duplicateFrames)
	{
		if(!duplicateFrames || guiOpen || !vsync || fastForward){reset();return 1;}
		if(!std::isfinite(refreshRate) || refreshRate<=0)refreshRate=60;
		if(!std::isfinite(videoFieldDuration) || videoFieldDuration<=0)videoFieldDuration=1./60;
		if(refresh!=refreshRate || field!=videoFieldDuration){reset();refresh=refreshRate;field=videoFieldDuration;}
		const double maximum=double(std::numeric_limits<int>::max());
		double duration=refreshRate*videoFieldDuration*std::clamp(gameInterval,1,2);
		if(!std::isfinite(duration))duration=maximum;
		// Duplication never discards a source frame on a slower display.
		const double total=fraction+std::clamp(duration,1.0,maximum);
		const int copies=int(std::min(maximum,std::floor(total+1e-9)));
		fraction=std::max(0.0,total-copies);
		if(fraction>=1)fraction=0; // Saturated integer range, not a valid display ratio.
		return copies;
	}
private:
	double fraction=0,refresh=0,field=0;
};
// First-frame query for callers which do not own a continuing source stream.
inline int presentationCount(double refreshRate, int gameInterval, bool guiOpen, bool vsync, bool fastForward,
	bool duplicateFrames,double videoFieldDuration=1./60)
{
	DuplicateCadence cadence;
	return cadence.count(refreshRate,videoFieldDuration,gameInterval,guiOpen,vsync,fastForward,duplicateFrames);
}
inline double minimumFrameDuration(double refreshRate, int /*gameInterval*/, bool guiOpen, bool vsync, bool fastForward, bool duplicateFrames = false)
{
	// Ordinary presents and GUI frames have no second duration limit.
	// Reserve timed presentation for explicitly requested copies.
	if (!duplicateFrames || guiOpen || !vsync || fastForward) return 0;
	if (!std::isfinite(refreshRate) || refreshRate <= 0) refreshRate = 60;
	return 1 / refreshRate;
}
}
