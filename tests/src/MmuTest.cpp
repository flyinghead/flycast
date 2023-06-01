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
#include "hw/mem/addrspace.h"
#include "emulator.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_core.h"

class MmuTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		if (!addrspace::reserve())
			die("addrspace::reserve failed");
		emu.init();
		dc_reset(true);
		config::ForceWindowsCE = true;
		CCN_MMUCR.AT = 1;
		MMU_reset();
	}
};

TEST_F(MmuTest, TestUntranslated)
{
	u32 pa;
	// P1
	MmuError err = mmu_data_translation<MMU_TT_DREAD>(0x80000000, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x80000000u, pa);
	err = mmu_instruction_translation(0x80000002, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x80000002u, pa);

	// P2
	err = mmu_data_translation<MMU_TT_DWRITE>(0xA0001234, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0xA0001234u, pa);

	// P4
	err = mmu_data_translation<MMU_TT_DREAD>(0xFF0000CC, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0xFF0000CCu, pa);

	// 7C000000 to 7FFFFFFF in P0/U0 not translated
	err = mmu_data_translation<MMU_TT_DREAD>(0x7D000088, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x7D000088u, pa);

	// SQ write
	UTLB[0].Address.VPN = 0xE2000000 >> 10;
	UTLB[0].Data.SZ0 = 1;
	UTLB[0].Data.V = 1;
	UTLB[0].Data.PR = 3;
	UTLB[0].Data.D = 1;
	UTLB_Sync(0);
	err = mmu_data_translation<MMU_TT_DWRITE>(0xE2000004, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0xE2000004, pa);
}

TEST_F(MmuTest, TestTranslated)
{
	u32 pa;
	// U0
	UTLB[0].Address.VPN = 0x02000000 >> 10;
	UTLB[0].Data.SZ0 = 1;
	UTLB[0].Data.V = 1;
	UTLB[0].Data.PR = 3;
	UTLB[0].Data.D = 1;
	UTLB[0].Data.PPN = 0x0C000000 >> 10;
	UTLB_Sync(0);
	MmuError err = mmu_data_translation<MMU_TT_DREAD>(0x02000044, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000044u, pa);

	err = mmu_data_translation<MMU_TT_DWRITE>(0x02000045, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000045u, pa);

	err = mmu_instruction_translation(0x02000046, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000046u, pa);

	// ASID match
	UTLB[0].Address.ASID = 13;
	CCN_PTEH.ASID = 13;
	UTLB_Sync(0);
	err = mmu_data_translation<MMU_TT_DWRITE>(0x02000222, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000222u, pa);
	err = mmu_instruction_translation(0x02000232, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000232u, pa);

	// Shared entry
	UTLB[0].Data.SH = 1;
	CCN_PTEH.ASID = 14;
	UTLB_Sync(0);
	err = mmu_data_translation<MMU_TT_DWRITE>(0x02000222, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0x0C000222u, pa);

	// 1C000000-1FFFFFF mapped to P4
	UTLB[0].Data.PPN = 0x1C000000 >> 10;
	UTLB_Sync(0);
	err = mmu_data_translation<MMU_TT_DWRITE>(0x02000222, pa);
	ASSERT_EQ(MmuError::NONE, err);
	ASSERT_EQ(0xFC000222u, pa);
}

TEST_F(MmuTest, TestMiss)
{
	u32 pa;
	UTLB[0].Address.VPN = 0x02000000 >> 10;
	UTLB[0].Data.SZ0 = 1;
	UTLB[0].Data.V = 1;
	UTLB[0].Data.PR = 3;
	UTLB[0].Data.D = 1;
	UTLB[0].Data.PPN = 0x0C000000 >> 10;
	UTLB_Sync(0);
	// no match
	MmuError err = mmu_data_translation<MMU_TT_DREAD>(0x02100044, pa);
	ASSERT_EQ(MmuError::TLB_MISS, err);

#ifndef FAST_MMU
	// entry not valid
	UTLB[0].Data.V = 0;
	UTLB_Sync(0);
	err = mmu_data_translation<MMU_TT_DREAD>(0x02000044, pa);
	ASSERT_EQ(MmuError::TLB_MISS, err);
#endif

	// asid mismatch
	UTLB[0].Data.V = 1;
	UTLB[0].Address.ASID = 13;
	CCN_PTEH.ASID = 14;
	UTLB_Sync(0);
	err = mmu_data_translation<MMU_TT_DREAD>(0x02000044, pa);
	ASSERT_EQ(MmuError::TLB_MISS, err);
}

TEST_F(MmuTest, TestErrors)
{
#ifndef FAST_MMU
	u32 pa;
	MmuError err;

	// P4 not executable
	err = mmu_instruction_translation(0xFF00008A, pa);
	ASSERT_EQ(MmuError::BADADDR, err);
	err = mmu_instruction_translation(0xE0000004, pa);
	ASSERT_EQ(MmuError::BADADDR, err);
#endif

	// unaligned address
	EXPECT_THROW(mmu_IReadMem16(0xFF00008B), SH4ThrownException);
#ifndef FAST_MMU
	EXPECT_THROW(mmu_ReadMem<u32>(0x80000046), SH4ThrownException);
	EXPECT_THROW(mmu_ReadMem<u64>(0x80000044), SH4ThrownException);
	EXPECT_THROW(mmu_ReadMem<u16>(0x80000045), SH4ThrownException);

	// Protection violation
	p_sh4rcb->cntx.sr.MD = 0;
	UTLB[0].Address.VPN = 0x04000000 >> 10;
	UTLB[0].Data.SZ0 = 1;
	UTLB[0].Data.V = 1;
	UTLB[0].Data.PR = 0;
	UTLB[0].Data.D = 1;
	UTLB[0].Data.PPN = 0x0A000000 >> 10;
	// no access in user mode
	err = mmu_data_translation<MMU_TT_DREAD>(0x04000040, pa);
	ASSERT_EQ(MmuError::PROTECTED, err);
	err = mmu_data_translation<MMU_TT_DWRITE>(0x04000040, pa);
	ASSERT_EQ(MmuError::PROTECTED, err);
	err = mmu_instruction_translation(0x04000042, pa);
	ASSERT_EQ(MmuError::PROTECTED, err);
	// read-only access in priv mode
	p_sh4rcb->cntx.sr.MD = 1;
	err = mmu_data_translation<MMU_TT_DREAD>(0x04000040, pa);
	ASSERT_EQ(MmuError::NONE, err);
	err = mmu_data_translation<MMU_TT_DWRITE>(0x04000040, pa);
	ASSERT_EQ(MmuError::PROTECTED, err);
	// read-only access in user & priv mode
	UTLB[0].Data.PR = 2;
	p_sh4rcb->cntx.sr.MD = 0;
	err = mmu_data_translation<MMU_TT_DWRITE>(0x04000040, pa);
	ASSERT_EQ(MmuError::PROTECTED, err);
	p_sh4rcb->cntx.sr.MD = 1;
	err = mmu_data_translation<MMU_TT_DWRITE>(0x04000040, pa);
	ASSERT_EQ(MmuError::PROTECTED, err);
	UTLB[0].Data.PR = 3;

	// kernel address in user mode
	p_sh4rcb->cntx.sr.MD = 0;
	err = mmu_data_translation<MMU_TT_DWRITE>(0xA4000004, pa);
	ASSERT_EQ(MmuError::BADADDR, err);
	err = mmu_instruction_translation(0xA4000006, pa);
	ASSERT_EQ(MmuError::BADADDR, err);

	// multiple hits
	memset(ITLB, 0, sizeof(ITLB));
	UTLB[1].Address.VPN = 0x04000000 >> 10;
	UTLB[1].Data.SZ1 = 1;
	UTLB[1].Data.V = 1;
	UTLB[1].Data.PR = 3;
	UTLB[1].Data.D = 1;
	UTLB[1].Data.PPN = 0x0C000000 >> 10;
	err = mmu_data_translation<MMU_TT_DREAD>(0x04000040, pa);
	ASSERT_EQ(MmuError::TLB_MHIT, err);
	err = mmu_instruction_translation(0x04000042, pa);
	ASSERT_EQ(MmuError::TLB_MHIT, err);
	UTLB[1].Data.V = 0;

	// first write
	UTLB[0].Data.D = 0;
	err = mmu_data_translation<MMU_TT_DWRITE>(0x04000224, pa);
	ASSERT_EQ(MmuError::FIRSTWRITE, err);
#endif
}
