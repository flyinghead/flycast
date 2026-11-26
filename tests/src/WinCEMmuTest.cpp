/*
	Copyright 2023 flyinghead

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
#include "gtest/gtest.h"
#include "types.h"
#include "emulator.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_mem.h"

class WinCEMmuTest : public ::testing::Test {
protected:
	void SetUp() override
	{
        emu.init(DC_PLATFORM_DREAMCAST);
        settings.content.windowsCE = true;
		CCN_MMUCR.AT = 1;
		mmu::enabled = true;
		mmu::reset();
	}

	void TearDown() override {
		emu.term();
	}
};

TEST_F(WinCEMmuTest, TestUntranslated)
{
	u32 pa;
	// P1
	MmuError err = mmu::translateData<MMU_TT_DREAD>(0x80000000, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x80000000u, pa);
	err = mmu::translateInstruction(0x80000002, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x80000002u, pa);

	// P2
	err = mmu::translateData<MMU_TT_DWRITE>(0xA0001234, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0xA0001234u, pa);

	// P4
	err = mmu::translateData<MMU_TT_DREAD>(0xFF0000CC, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0xFF0000CCu, pa);

	// 7C000000 to 7FFFFFFF in P0/U0 not translated
	err = mmu::translateData<MMU_TT_DREAD>(0x7D000088, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x7D000088u, pa);

	// SQ write
	UTLB[0].Address.VPN = 0xE2000000 >> 10;
	UTLB[0].Data.SZ0 = 1;
	UTLB[0].Data.V = 1;
	UTLB[0].Data.PR = 3;
	UTLB[0].Data.D = 1;
	mmu::UTLBSync(0);
	err = mmu::translateData<MMU_TT_DWRITE>(0xE2000004, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0xE2000004, pa);
}

TEST_F(WinCEMmuTest, TestTranslated)
{
	u32 pa;
	// U0
	UTLB[0].Address.VPN = 0x02000000 >> 10;
	UTLB[0].Data.SZ0 = 1;
	UTLB[0].Data.V = 1;
	UTLB[0].Data.PR = 3;
	UTLB[0].Data.D = 1;
	UTLB[0].Data.PPN = 0x0C000000 >> 10;
	mmu::UTLBSync(0);
	MmuError err = mmu::translateData<MMU_TT_DREAD>(0x02000044, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000044u, pa);

	err = mmu::translateData<MMU_TT_DWRITE>(0x02000045, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000045u, pa);

	err = mmu::translateInstruction(0x02000046, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000046u, pa);

	// ASID match
	UTLB[0].Address.ASID = 13;
	CCN_PTEH.ASID = 13;
	mmu::UTLBSync(0);
	err = mmu::translateData<MMU_TT_DWRITE>(0x02000222, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000222u, pa);
	err = mmu::translateInstruction(0x02000232, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000232u, pa);

	// Shared entry
	UTLB[0].Data.SH = 1;
	CCN_PTEH.ASID = 14;
	mmu::UTLBSync(0);
	err = mmu::translateData<MMU_TT_DWRITE>(0x02000222, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000222u, pa);

	// 1C000000-1FFFFFF mapped to P4
	UTLB[0].Data.PPN = 0x1C000000 >> 10;
	mmu::UTLBSync(0);
	err = mmu::translateData<MMU_TT_DWRITE>(0x02000222, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0xFC000222u, pa);
}

TEST_F(WinCEMmuTest, TestMiss)
{
	u32 pa;
	UTLB[0].Address.VPN = 0x02000000 >> 10;
	UTLB[0].Data.SZ0 = 1;
	UTLB[0].Data.V = 1;
	UTLB[0].Data.PR = 3;
	UTLB[0].Data.D = 1;
	UTLB[0].Data.PPN = 0x0C000000 >> 10;
	mmu::UTLBSync(0);
	// no match
	MmuError err = mmu::translateData<MMU_TT_DREAD>(0x02100044, pa);
	ASSERT_EQ(MmuError::TLB_MISS, err);

	// asid mismatch
	UTLB[0].Data.V = 1;
	UTLB[0].Address.ASID = 13;
	CCN_PTEH.ASID = 14;
	mmu::UTLBSync(0);
	err = mmu::translateData<MMU_TT_DREAD>(0x02000044, pa);
	ASSERT_EQ(MmuError::TLB_MISS, err);
}

TEST_F(WinCEMmuTest, TestErrors)
{
	// unaligned address
	EXPECT_THROW(IReadMem16(0xFF00008B), SH4ThrownException);
}
