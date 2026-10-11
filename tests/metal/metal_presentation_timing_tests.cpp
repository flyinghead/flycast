// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal/metal_presentation_timing.h"
#include <cstdio>
#include <limits>

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
		{60,2,false,true,false,true,2,1.0/60}, {90,2,false,true,false,true,2,1.0/90},
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
	std::printf("PASS: 34 presentation policies and 60000 PAL/NTSC video deadlines (jitter, variable cadence, display lead, pause/reset)\n");
	return 0;
}
