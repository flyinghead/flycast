/*
	Several AICA chips (the Sega Hikaru has two sound boards).

	Each chip has its own registers, wave RAM, channels, DSP, timers, RTC and ARM7.
	The recompilers address the ARM7 registers and the DSP state at fixed places, so the
	state of the chip being run or accessed is switched in with selectChip(). Chip 0 is the
	only chip on the Dreamcast, Naomi and Atomiswave, and its state stays where it always was.
	The other chips run the ARM7 recompiler with their own entry table and the DSP interpreter.
 */
#pragma once
#include "types.h"

namespace aica
{

constexpr int MAX_CHIPS = 2;

// Number of emulated chips. Extra chips get their own wave RAM of ARAM_SIZE bytes.
void setChipCount(int count);
// Schedule the next update of the chips again (after loading a state)
void rescheduleUpdate();
int chipCount();

int activeChip();
void selectChip(int chip);

// The wave RAM of a chip, whichever chip is active
u8 *chipRam(int chip);

// Selects a chip for the lifetime of the scope
class ChipScope
{
public:
	explicit ChipScope(int chip) : previous(activeChip()) {
		selectChip(chip);
	}
	~ChipScope() {
		selectChip(previous);
	}
	ChipScope(const ChipScope&) = delete;
	ChipScope& operator=(const ChipScope&) = delete;

private:
	int previous;
};

// Chips after the first one: their mixed output of the last sample, and the samples fed to the
// external inputs (EXTS) of chip 0 instead of the CD audio (Hikaru: the optional board feeds the
// main board).
void getChipOutput(int chip, s32& left, s32& right);
void setExternalSource(int chip);	// chip whose output is fed to chip 0's EXTS, -1 for CD audio

}
