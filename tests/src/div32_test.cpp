#include "gtest/gtest.h"
#include "types.h"
#include "hw/mem/_vmem.h"
#include "emulator.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/shil.h"
#define SHIL_MODE 2
#include "hw/sh4/dyna/shil_canonical.h"

static void div1(u32& r1, u32 r2)
{
	const u8 old_q = sr.Q;
	sr.Q = (u8)((0x80000000 & r1) != 0);

	r1 <<= 1;
	r1 |= sr.T;

	const u32 old_rn = r1;

	if (old_q == 0)
	{
		if (sr.M == 0)
		{
			r1 -= r2;
			bool tmp1 = r1 > old_rn;
			sr.Q = sr.Q ^ tmp1;
		}
		else
		{
			r1 += r2;
			bool tmp1 = r1 < old_rn;
			sr.Q = !sr.Q ^ tmp1;
		}
	}
	else
	{
		if (sr.M == 0)
		{
			r1 += r2;
			bool tmp1 = r1 < old_rn;
			sr.Q = sr.Q ^ tmp1;
		}
		else
		{
			r1 -= r2;
			bool tmp1 = r1 > old_rn;
			sr.Q = !sr.Q ^ tmp1;
		}
	}
	sr.T = (sr.Q == sr.M);
}

static void div32s_slow(u32& r1, u32 r2, u32& r3)
{
	sr.Q = r3 >> 31;
	sr.M = r2 >> 31;
	sr.T = sr.Q ^ sr.M;
	for (int i = 0; i < 32; i++)
	{
		u64 rv = shil_opcl_rocl::f1::impl(r1, sr.T);
		r1 = (u32)rv;
		sr.T = rv >> 32;

		div1(r3, r2);
	}
}

static void div32s_fast(u32& r1, u32 r2, u32& r3)
{
	sr.T = (r3 ^ r2) & 0x80000000;
	u64 rv = shil_opcl_div32s::f1::impl(r1, r2, r3);
	r1 = (u32)rv;
	r3 = rv >> 32;
	sr.T |= r1 & 1;
	r1 = (s32)r1 >> 1;
	r3 = shil_opcl_div32p2::f1::impl(r3, r2, sr.T);
	sr.T &= 1;
}

static void div32u_fast(u32& r1, u32 r2, u32& r3)
{
	u64 rv = shil_opcl_div32u::f1::impl(r1, r2, r3);
	r1 = (u32)rv;
	r3 = rv >> 32;
	sr.T = r1 & 1;
	r1 = r1 >> 1;
	r3 = shil_opcl_div32p2::f1::impl(r3, r2, sr.T);
}

static void div32u_slow(u32& r1, u32 r2, u32& r3)
{
	sr.Q = 0;
	sr.M = 0;
	sr.T = 0;

	for (int i = 0; i < 32; i++)
	{
		u64 rv = shil_opcl_rocl::f1::impl(r1, sr.T);
		r1 = (u32)rv;
		sr.T = rv >> 32;

		div1(r3, r2);
	}
}

class Div32Test : public ::testing::Test {
protected:
	void SetUp() override {
		if (!_vmem_reserve())
			die("_vmem_reserve failed");
		emu.init();
		dc_reset(true);
	}

	void div32s(u32 n1, u32 n2, u32 n3)
	{
		const long long int dividend = (long long)n3 << 32 | n1;
		//printf("%lld / %d = ", dividend, n2);
		int r1s = n1;
		int r2 = n2;
		int r3s = n3;
		int r1f = r1s;
		int r3f = r3s;
		div32s_slow((u32&)r1s, r2, (u32&)r3s);
		div32s_fast((u32&)r1f, r2, (u32&)r3f);
		//printf("%d %% %d\n", (r1s << 1) | sr.T, r3s);
		ASSERT_EQ(r1s, r1f);
		ASSERT_EQ(r3s, r3f);
	}

	void div32u(u32 n1, u32 n2, u32 n3)
	{
		const long long int dividend = (long long)n3 << 32 | n1;
		//printf("%lld / %d = ", dividend, n2);
		int r1s = n1;
		int r2 = n2;
		int r3s = n3;
		int r1f = r1s;
		int r3f = r3s;
		div32u_slow((u32&)r1s, r2, (u32&)r3s);
		div32u_fast((u32&)r1f, r2, (u32&)r3f);
		//printf("%d %% %d\n", (r1s << 1) | sr.T, r3s);
		ASSERT_EQ(r3s, r3f);
	}
};

TEST_F(Div32Test, Div32sTest)
{
	div32s(0, 1, 0);
	div32s(1, 1, 0);
	div32s(2, 1, 0);
	div32s(4, 2, 0);
	div32s(5, 2, 0);

	div32s(1000, 100, 0);
	div32s(1001, 100, 0);
	div32s(1099, 100, 0);
	div32s(1100, 100, 0);

	div32s(37, 5, 0);
	div32s(-37, 5, -1);
	div32s(-37, -5, -1);
	div32s(37, -5, 0);

	div32s(42, 5, 0);
	div32s(42, -5, 0);
	div32s(-42, 5, -1);
	div32s(-42, -5, -1);

	div32s(5, 7, 0);
	div32s(5, -7, 0);
	div32s(-5, 7, -1);
	div32s(-5, -7, -1);

	div32s(-1846, -1643, -1);
	div32s(-496138, -1042, -1);
	div32s(-416263, -1037, -1);
	div32s(-270831, -13276, -1);
	div32s(-3338802, -7266, -1);
	div32s(-3106, -354865, -1);
	div32s(-4446, -4095, -1);

	div32s(-64, -8, -1);
	div32s(-72, -8, -1);

	div32s(217781009, -45, 0);
	div32s(1858552, -8, 0);
	div32s(64, -8, 0);
	div32s(-64, 8, -1);
	div32s(-9415081, 130765, -1);
	div32s(-3639715, 78, -1);
	div32s(-11361399, 107183, -1);
}

TEST_F(Div32Test, Div32uTest)
{
	div32u(0, 1, 0);
	div32u(1, 1, 0);
	div32u(2, 1, 0);
	div32u(4, 2, 0);
	div32u(5, 2, 0);

	div32u(1000, 100, 0);
	div32u(1001, 100, 0);
	div32u(1099, 100, 0);
	div32u(1100, 100, 0);

	div32u(1964671145, 123383161, 0);
	div32u(1867228769, 653467280, 0);
	div32u(1523687288, 32181601, 0);

	div32u(3499805483u, 1401792939, 29611);

	div32u(0x80000000u, 0x80000000u, 0);
}

