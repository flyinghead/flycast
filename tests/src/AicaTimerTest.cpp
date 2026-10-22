#include "types.h"
#include "hw/mem/addrspace.h"
#include "hw/aica/aica.h"
#include "hw/aica/aica_mem.h"
#include "emulator.h"
#include "gtest/gtest.h"

namespace aica {

class AicaTimerTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		if (!addrspace::reserve())
			die("addrspace::reserve failed");
		emu.init();
		emu.dc_reset(true);
	}

	static u32 count() {
		return aica_reg[TIMER_B];
	}

	static void reload(u16 value) {
		writeTimerAndIntReg<u16>(TIMER_B, value);
	}

	static void step(u32 samples) {
		timers[1].StepTimer(samples);
	}
};

// The driver reloads twice in its interrupt handler and a sample elapses between the two writes:
// the second write must not set the count back.
TEST_F(AicaTimerTest, RepeatedReloadKeepsCount)
{
	reload(212);
	step(1);
	ASSERT_EQ(213u, count());
	reload(212);
	ASSERT_EQ(213u, count());
	// 44 samples per period, the repeated reload notwithstanding
	SCIPD->full = 0;
	step(42);
	ASSERT_EQ(0u, SCIPD->TimerB);
	step(1);
	ASSERT_EQ(1u, SCIPD->TimerB);
	ASSERT_EQ(0u, count());
}

TEST_F(AicaTimerTest, LaterReloadRestartsCount)
{
	reload(212);
	step(2);
	ASSERT_EQ(214u, count());
	reload(212);
	ASSERT_EQ(212u, count());
}

TEST_F(AicaTimerTest, ReloadAfterOverflowRestartsCount)
{
	reload(255);
	step(1);
	ASSERT_EQ(0u, count());
	reload(255);
	ASSERT_EQ(255u, count());
}

TEST_F(AicaTimerTest, DifferentReloadRestartsCount)
{
	reload(212);
	step(1);
	reload(200);
	ASSERT_EQ(200u, count());
}

}
