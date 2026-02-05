#include "gtest/gtest.h"
#include "types.h"
#include "hw/sh4/modules/modules.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_sched.h"

class TimerTest : public ::testing::Test {
	void SetUp() override {
		tmu.init();
	}
	void TearDown() override {
		tmu.term();
	}

protected:
	void tick(int ticks) {
		Sh4cntx.sh4_sched_next -= ticks;
		sh4_sched_tick(ticks);
	}
};

TEST_F(TimerTest, init)
{
	ASSERT_EQ(0xffffffff, tmu.read<u32>(TMU_TCNT0_addr));
	ASSERT_EQ(0xffffffff, tmu.read<u32>(TMU_TCNT1_addr));
	ASSERT_EQ(0xffffffff, tmu.read<u32>(TMU_TCNT2_addr));

	ASSERT_EQ(0xffffffff, tmu.read<u32>(TMU_TCOR0_addr));
	ASSERT_EQ(0xffffffff, tmu.read<u32>(TMU_TCOR1_addr));
	ASSERT_EQ(0xffffffff, tmu.read<u32>(TMU_TCOR2_addr));

	ASSERT_EQ(0, tmu.read<u16>(TMU_TCR0_addr));
	ASSERT_EQ(0, tmu.read<u16>(TMU_TCR1_addr));
	ASSERT_EQ(0, tmu.read<u16>(TMU_TCR2_addr));

	ASSERT_EQ(0, tmu.read<u8>(TMU_TSTR_addr));
}

TEST_F(TimerTest, precision)
{
	tmu.write<u32>(TMU_TCOR0_addr, 1000);
	tmu.write<u32>(TMU_TCNT0_addr, 1000);
	tmu.write<u8>(TMU_TSTR_addr, 1);

	ASSERT_EQ(1000, tmu.read<u32>(TMU_TCOR0_addr));
	ASSERT_EQ(1000, tmu.read<u32>(TMU_TCNT0_addr));
	ASSERT_EQ(1, tmu.read<u8>(TMU_TSTR_addr));
	ASSERT_EQ(0, tmu.read<u16>(TMU_TCR0_addr));

	tick(999 << 4);
	ASSERT_EQ(1, tmu.read<u32>(TMU_TCNT0_addr));
	ASSERT_EQ(0, tmu.read<u16>(TMU_TCR0_addr));

	tick(2 << 4);
	 // underflow flag
	ASSERT_EQ(0x0100, tmu.read<u16>(TMU_TCR0_addr));
	// counter reloaded with TCOR
	ASSERT_EQ(999, tmu.read<u32>(TMU_TCNT0_addr));

	Sh4cntx.cycle_counter -= 1 << 4;
	// current cycle counter taken into account for increased precision
	ASSERT_EQ(998, tmu.read<u32>(TMU_TCNT0_addr));
}

TEST_F(TimerTest, smallTCOR)
{
	for (int i = 0; i < 16; i++)
		tick(0x7fffffff);
	tmu.write<u32>(TMU_TCOR0_addr, 2);
	tmu.write<u32>(TMU_TCNT0_addr, 2);
	tmu.write<u8>(TMU_TSTR_addr, 1);

	tick(5 << 4);
	 // underflow flag
	ASSERT_EQ(0x0100, tmu.read<u16>(TMU_TCR0_addr));
	tmu.write<u16>(TMU_TCR0_addr, 0);
	ASSERT_EQ(0, tmu.read<u32>(TMU_TCNT0_addr));

	tick(1 << 4);
	 // underflow flag
	ASSERT_EQ(0x0100, tmu.read<u16>(TMU_TCR0_addr));
	ASSERT_EQ(1, tmu.read<u32>(TMU_TCNT0_addr));
}

TEST_F(TimerTest, underflow)
{
	tmu.write<u32>(TMU_TCOR0_addr, 100);
	tmu.write<u32>(TMU_TCNT0_addr, 100);
	tmu.write<u8>(TMU_TSTR_addr, 1);

	tick(100 << 4);
	// no underflow yet
	ASSERT_EQ(0, tmu.read<u16>(TMU_TCR0_addr));
	ASSERT_EQ(0, tmu.read<u32>(TMU_TCNT0_addr));

	Sh4cntx.cycle_counter -= 1 << 4;
	// this shouldn't affect the timer
	tmu.write<u8>(TMU_TSTR_addr, tmu.read<u8>(TMU_TSTR_addr));
	// now read to reload the timer
	ASSERT_EQ(99, tmu.read<u32>(TMU_TCNT0_addr));
	// underflow set
	ASSERT_EQ(0x100, tmu.read<u16>(TMU_TCR0_addr));
}

