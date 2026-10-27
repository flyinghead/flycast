// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_presentation_timing.h"
#include <cstdio>
#include <cstdint>
#include <limits>
#include <deque>
#include <vector>

// Independent producer/callback events exercise the bounded three-image queue.
// In particular, the producer can win the race at a display boundary. Deadlines
// that look regular in isolation must not evict still-valid queued contents.
static unsigned queueEvictions(double fieldHz, int gameInterval, double displayHz,
		double phase, bool previousExtraRefresh)
{
	struct Event { double at; bool producer; unsigned content; };
	std::vector<Event> events;
	const double sourceInterval = gameInterval / fieldHz, displayInterval = 1 / displayHz;
	for (unsigned n = 0; n < 600; ++n)
		events.push_back({n * sourceInterval + (n % 5) * .0004, true, n});
	for (unsigned n = 0; n * displayInterval < 600 * sourceInterval + 1; ++n)
		events.push_back({phase + n * displayInterval + (n % 3) * .0002, false, 0});
	std::stable_sort(events.begin(),events.end(),[](const Event& a,const Event& b) { return a.at < b.at; });
	metal::PresentationTimeline timeline;
	std::deque<double> queue;
	unsigned evicted = 0, presented = 0;
	for (const auto& event : events) {
		if (event.producer) {
			const double buffer = previousExtraRefresh ? 2 / fieldHz + displayInterval
				: metal::presentationBufferDuration(1 / fieldHz);
			const double target = timeline.videoFrame(100 + event.at,
				10 + event.content * sourceInterval, buffer, displayInterval);
			if (queue.size() == 3) { queue.pop_front(); ++evicted; }
			queue.push_back(target);
		} else if (!queue.empty() && queue.front() <= 100 + event.at + displayInterval) {
			queue.pop_front(); ++presented;
		}
	}
	if (presented + evicted != 600 || !queue.empty()) return 600;
	return evicted;
}

int main()
{
	struct Case { double hz; int game; bool gui, vsync, fast; double seconds; };
	// Ordinary frames inherit emulation cadence; no presentation duration
	// imposes a second cap, including after an interval-2 game opens the GUI.
	const Case cases[] = {
		{60, 1, false, true, false, 0}, {60, 2, false, true, false, 0},
		{120, 1, false, true, false, 0}, {120, 2, false, true, false, 0},
		{90, 1, false, true, false, 0}, {144, 1, false, true, false, 0},
		{90, 2, false, true, false, 0},
		{144, 2, false, true, false, 0}, {240, 1, false, true, false, 0},
		{50, 1, false, true, false, 0}, {59.94, 1, false, true, false, 0},
		{120, 2, true, true, false, 0}, {144, 2, true, true, false, 0},
		{120, 2, false, false, false, 0}, {120, 2, false, true, true, 0},
		{120, 2, true, true, true, 0}, {120, 0, false, true, false, 0},
		{120, 9, false, true, false, 0}, {0, 1, false, true, false, 0},
		{-1, 1, false, true, false, 0},
		{std::numeric_limits<double>::quiet_NaN(), 1, false, true, false, 0},
		{std::numeric_limits<double>::infinity(), 1, false, true, false, 0},
	};
	for (const auto& c : cases) {
		const double actual = metal::minimumFrameDuration(c.hz, c.game, c.gui, c.vsync, c.fast);
		if (std::abs(actual - c.seconds) > 1e-12) {
			std::fprintf(stderr, "FAIL: %.3f Hz, interval %d, GUI %d, VSync %d, fast %d: %.9f != %.9f\n", c.hz, c.game, c.gui, c.vsync, c.fast, actual, c.seconds);
			return 1;
		}
	}
	struct DuplicateCase { double hz; int game; bool gui, vsync, fast, duplicate; int copies; double seconds; };
	const DuplicateCase duplicateCases[] = {
		{120,1,false,true,false,true,2,1.0/120}, {120,2,false,true,false,true,4,1.0/120},
		{60,2,false,true,false,true,2,1.0/60}, {90,2,false,true,false,true,3,1.0/90},
		{144,2,false,true,false,true,4,1.0/144}, {240,2,false,true,false,true,8,1.0/240},
		{120,2,true,true,false,true,1,0}, {120,2,false,false,false,true,1,0},
		{120,2,false,true,true,true,1,0}, {120,2,false,true,false,false,1,0},
		{0,2,false,true,false,true,2,1.0/60},
		{std::numeric_limits<double>::infinity(),1,false,true,false,true,1,1.0/60},
	};
	for (const auto& c : duplicateCases) {
		if (metal::presentationCount(c.hz, c.game, c.gui, c.vsync, c.fast, c.duplicate) != c.copies
				|| std::abs(metal::minimumFrameDuration(c.hz, c.game, c.gui, c.vsync, c.fast, c.duplicate) - c.seconds) > 1e-12) {
			std::fprintf(stderr, "FAIL: duplicate policy %.3f Hz, interval %d\n", c.hz, c.game); return 1;
		}
	}
	// Independent integer-rational oracle: cumulative outputs must match the
	// display refreshes covered by the source fields, with less than one extra
	// refresh carried to the next source frame, even when intervals alternate.
	unsigned cadenceFrames=0;
	for(unsigned numerator:{50u,60u,60000u})for(unsigned display:{50u,60u,90u,100u,120u,144u,240u})
	for(unsigned mode:{1u,2u,3u}) {
		const unsigned denominator=numerator==60000?1001:1;
		metal::DuplicateCadence cadence;uint64_t accumulated=0,observed=0;
		for(unsigned n=0;n<10000;++n) {
			const unsigned interval=mode==3?1+n%2:mode;
			accumulated+=std::max(uint64_t(numerator),uint64_t(display)*interval*denominator);
			observed+=cadence.count(display,double(denominator)/numerator,interval,false,true,false,true);
			if(observed!=accumulated/numerator){std::fprintf(stderr,"FAIL: duplicate cadence field %u/%u display %u interval %u frame %u\n",numerator,denominator,display,interval,n);return 1;}
			++cadenceFrames;
		}
		for(unsigned disabled=0;disabled<4;++disabled){
			if(cadence.count(display,double(denominator)/numerator,2,disabled==0,disabled!=1,disabled==2,disabled!=3)!=1){std::fputs("FAIL: duplicate bypass mode\n",stderr);return 1;}
			metal::DuplicateCadence fresh;
			if(cadence.count(display,double(denominator)/numerator,2,false,true,false,true)!=fresh.count(display,double(denominator)/numerator,2,false,true,false,true)){std::fputs("FAIL: duplicate mode retained old phase\n",stderr);return 1;}
		}
	}
	{
		metal::DuplicateCadence cadence;
		for(const auto input:std::vector<std::pair<double,double>>{{120,1./50},{144,1./60},{120,1001./60000},{100,1./50}}){
			metal::DuplicateCadence fresh;
			for(unsigned n=0;n<19;++n)
				if(cadence.count(input.first,input.second,1,false,true,false,true)!=fresh.count(input.first,input.second,1,false,true,false,true)){
					std::fputs("FAIL: duplicate phase survived refresh/video-mode change\n",stderr);return 1;
				}
		}
		for(double field:{0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
			if(metal::presentationCount(120,2,false,true,false,true,field)!=4){std::fputs("FAIL: invalid video field fallback\n",stderr);return 1;}
	}
	// Rendering arrival jitter must not enter the source video timeline.
	for (double hz : {50.0, 59.94, 60.0}) {
		metal::PresentationTimeline clock;
		metal::PresentationTimeline displayClock;
		double source = 10, target = 0, lastSource = 0;
		for (int n = 0; n < 10000; ++n) {
			const double arrival = 100 + source - 10 + (n % 5 - 2) * 0.002;
			const double next = clock.videoFrame(arrival, source, 2 / hz + 1.0 / 120);
			// The display can request a composition well before it appears.
			// Its lead must shift presentation, without shrinking the producer's
			// jitter buffer or changing the emulated video cadence.
			const double lead = n < 5000 ? 5.0 / 120 : 3.0 / 60;
			const double display = displayClock.videoFrame(arrival, source, 2 / hz + 1.0 / 120, lead);
			if (std::abs(display - next - lead) > 1e-9) {
				std::fprintf(stderr, "FAIL: display lead changed the source clock at %.2f Hz\n", hz); return 1;
			}
			if (n && std::abs((next - target) - (source - lastSource)) > 1e-9) {
				std::fprintf(stderr, "FAIL: video clock drift at %.2f Hz, frame %d\n", hz, n); return 1;
			}
			target = next; lastSource = source;
			// Arbitrary game cadence, including alternating intervals.
			source += (1 + n % 3) / hz;
		}
		if (clock.videoFrame(1000, source, .05) < 1000
				|| clock.videoFrame(1001, 0, .05) < 1001
				|| clock.videoFrame(1002, std::numeric_limits<double>::quiet_NaN(), .05) != 0) {
			std::fprintf(stderr, "FAIL: pause/reset/invalid clock handling\n"); return 1;
		}
	}
	unsigned previousEvictions = 0;
	for (double fieldHz : {50.0, 59.94, 60.0})
		for (int interval : {1,2})
			for (double displayHz : {60.0,120.0})
				for (unsigned phase = 0; phase < 16; ++phase) {
					if (queueEvictions(fieldHz,interval,displayHz,phase / displayHz / 16,false)) {
						std::fprintf(stderr,"FAIL: valid contents evicted at field %.2f interval %d display %.0f phase %u\n",
							fieldHz,interval,displayHz,phase); return 1;
					}
					previousEvictions += queueEvictions(fieldHz,interval,displayHz,phase / displayHz / 16,true);
				}
	if (!previousEvictions) { std::fprintf(stderr,"FAIL: queue regression did not detect the previous overlong horizon\n"); return 1; }
	std::printf("PASS: 34 presentation policies, %u integer-oracle duplicate frames, 60000 PAL/NTSC deadlines and 192 producer/display queue schedules; previous horizon evicted %u valid contents\n",cadenceFrames,previousEvictions);
	return 0;
}
