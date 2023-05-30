/*
	Copyright 2021 flyinghead

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
#pragma once
#include <set>
#define GTEST_DONT_DEFINE_ASSERT_EQ 1
#include "gtest/gtest.h"
#include "types.h"
#include "hw/sh4/sh4_if.h"
#include "hw/mem/addrspace.h"
#include "hw/sh4/sh4_core.h"
#undef r
#undef fr
#undef sr
#undef mac
#undef gbr
#undef fpscr
#undef fpul

constexpr u32 REG_MAGIC = 0xbaadf00d;

#define ASSERT_EQ(val1, val2) GTEST_ASSERT_EQ(val1, val2)

#define ASSERT_CLEAN_REG(regNum) { ASSERT_EQ(r(regNum), REG_MAGIC); }

class Sh4OpTest : public ::testing::Test {
protected:
	virtual void PrepareOp(u16 op, u16 op2 = 0, u16 op3 = 0) = 0;
	virtual void RunOp(int numOp = 1) = 0;

	void ClearRegs() {
		for (int i = 0; i < 16; i++)
			r(i) = REG_MAGIC;
		for (int i = 0; i < 32; i++)
			*(u32 *)&ctx->xffr[i] = REG_MAGIC;
		sh4_sr_SetFull(0x700000F0);
		mac() = 0;
		gbr() = REG_MAGIC;
		checkedRegs.clear();
	}
	void AssertState() {
		for (int i = 0; i < 16; i++)
			if (checkedRegs.count(&ctx->r[i]) == 0)
				ASSERT_CLEAN_REG(i);
		if (checkedRegs.count(&ctx->gbr) == 0)
		{
			ASSERT_EQ(ctx->gbr, REG_MAGIC);
		}
		for (int i = 0; i < 32; i++)
			if (checkedRegs.count((u32 *)&ctx->xffr[i]) == 0)
			{
				ASSERT_EQ(*(u32 *)&ctx->xffr[i], REG_MAGIC);
			}
	}
	static u16 Rm(int r) { return r << 4; }
	static u16 Rn(int r) { return r << 8; }
	static u16 Imm8(int i) { return (u8)i; }
	static u16 Imm4(int i) { return i & 0xf; }

	u32& r(u32 regNum) { checkedRegs.insert(&ctx->r[regNum]); return ctx->r[regNum]; }
	u32& gbr() { checkedRegs.insert(&ctx->gbr); return ctx->gbr; }
	u64& mac() { return ctx->mac.full; }
	u32& mach() { return ctx->mac.l; }
	u32& macl() { return ctx->mac.h; }
	sr_t& sr() { return ctx->sr; }
	f32& fr(int regNum) { checkedRegs.insert((u32 *)&ctx->xffr[regNum + 16]); return ctx->xffr[regNum + 16]; }
	double getDr(int regNum) {
		checkedRegs.insert((u32 *)&ctx->xffr[regNum * 2 + 16]);
		checkedRegs.insert((u32 *)&ctx->xffr[regNum * 2 + 1 + 16]);
		return GetDR(regNum);
	}
	void setDr(int regNum, double d) {
		checkedRegs.insert((u32 *)&ctx->xffr[regNum * 2 + 16]);
		checkedRegs.insert((u32 *)&ctx->xffr[regNum * 2 + 1 + 16]);
		SetDR(regNum, d);
	}

	Sh4Context *ctx;
	sh4_if sh4;
	std::set<u32 *> checkedRegs;
	static constexpr u32 START_PC = 0xAC000000;

	void MovRmRnTest()
	{
		for (int src = 0; src < 16; src++)
		{
			for (int dst = 0; dst < 16; dst++)
			{
				ClearRegs();
				u32 v = (dst << 28) | src;
				r(src) = v;
				PrepareOp(0x6003 | Rm(src) | Rn(dst));	// mov Rm, Rn
				RunOp();
				ASSERT_EQ(r(dst), v);
				ASSERT_EQ(r(src), v);
				AssertState();
			}
		}
	}
	void MovImmRnTest()
	{
		ClearRegs();
		int v = 42;
		PrepareOp(0xe000 | Rn(1) | Imm8(v));	// mov #imm, Rn
		RunOp();
		ASSERT_EQ(r(1), (u32)v);
		AssertState();

		ClearRegs();
		v = -1;
		PrepareOp(0xe000 | Rn(12) | Imm8(v));	// mov #imm, Rn
		RunOp();
		ASSERT_EQ(r(12), 0xffffffff);
		AssertState();
	}
	void MovMiscTest()
	{
		ClearRegs();
		u32 disp = 0x10;
		PrepareOp(0xc700 | Imm8(disp));	// mova @(disp,PC),R0
		RunOp();
		ASSERT_EQ(r(0), START_PC + 4 + disp * 4);
		AssertState();

		ClearRegs();
		disp = 0x11;
		PrepareOp(0x9,						// nop
				0xc700 | Imm8(disp));		// mova @(disp,PC),R0
		RunOp(2);
		ASSERT_EQ(r(0), START_PC + 4 + disp * 4);	// uses PC & 0xfffffffc
		AssertState();

		ClearRegs();
		sr().T = 1;
		PrepareOp(0x0029 | Rn(1));			// movt Rn
		RunOp();
		ASSERT_EQ(r(1), 1u);
		AssertState();
		sr().T = 0;
		RunOp();
		ASSERT_EQ(r(1), 0u);

		ClearRegs();
		r(3) = 0x12345678;
		PrepareOp(0x6009 | Rn(4) | Rm(3));			// swap.w Rm, Rn
		RunOp();
		ASSERT_EQ(r(4), 0x56781234u);
		ASSERT_EQ(r(3), 0x12345678u);
		AssertState();

		ClearRegs();
		r(3) = 0xaabbccdd;
		PrepareOp(0x6008 | Rn(4) | Rm(3));			// swap.b Rm, Rn
		RunOp();
		ASSERT_EQ(r(4), 0xaabbddccu);
		ASSERT_EQ(r(3), 0xaabbccddu);
		AssertState();

		ClearRegs();
		r(3) = 0xaabbccdd;
		r(4) = 0xeeff0011;
		PrepareOp(0x200d | Rn(4) | Rm(3));			// xtrct Rm, Rn
		RunOp();
		ASSERT_EQ(r(4), 0xccddeeffu);
		ASSERT_EQ(r(3), 0xaabbccddu);
		AssertState();
	}

	void LoadTest()
	{
		ClearRegs();
		r(14) = 0x8C001000;
		addrspace::write32(r(14), 0xffccaa88u);
		PrepareOp(0x6002 | Rm(14) | Rn(11));	// mov.l @Rm,Rn
		RunOp();
		ASSERT_EQ(r(11), 0xffccaa88u);
		AssertState();

		ClearRegs();
		r(14) = 0x8C001000;
		PrepareOp(0x6001 | Rm(14) | Rn(11));	// mov.w @Rm,Rn
		RunOp();
		ASSERT_EQ(r(11), 0xffffaa88u);
		AssertState();

		ClearRegs();
		r(14) = 0x8C001000;
		PrepareOp(0x6000 | Rm(14) | Rn(11));	// mov.b @Rm,Rn
		RunOp();
		ASSERT_EQ(r(11), 0xffffff88u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		addrspace::write32(r(8), 0x4433ff11);
		PrepareOp(0x6006 | Rm(8) | Rn(7));		// mov.l @Rm+,Rn
		RunOp();
		ASSERT_EQ(r(7), 0x4433ff11u);
		ASSERT_EQ(r(8), 0x8C001008);
		AssertState();

		ClearRegs();
		r(7) = 0x8C001004;
		addrspace::write32(r(7), 0x4433ff11);
		PrepareOp(0x6006 | Rm(7) | Rn(7));		// mov.l @Rm+,Rn
		RunOp();
		ASSERT_EQ(r(7), 0x4433ff11u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		PrepareOp(0x6005 | Rm(8) | Rn(7));		// mov.w @Rm+,Rn
		RunOp();
		ASSERT_EQ(r(7), 0xffffff11u);
		ASSERT_EQ(r(8), 0x8C001006);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		PrepareOp(0x6004 | Rm(8) | Rn(7));		// mov.b @Rm+,Rn
		RunOp();
		ASSERT_EQ(r(7), 0x11u);
		ASSERT_EQ(r(8), 0x8C001005);
		AssertState();

		ClearRegs();
		addrspace::write32(0x8C001010, 0x50607080);
		r(8) = 0x8C001004;
		PrepareOp(0x5000 | Rm(8) | Rn(7) | Imm4(3));// mov.l @(disp, Rm), Rn
		RunOp();
		ASSERT_EQ(r(7), 0x50607080u);
		AssertState();

		ClearRegs();
		addrspace::write32(0x8C001010, 0x50607080);
		r(8) = 0x8C001004;
		PrepareOp(0x8500 | Rm(8) | Imm4(6));		// mov.w @(disp, Rm), R0
		RunOp();
		ASSERT_EQ(r(0), 0x7080u);
		AssertState();

		ClearRegs();
		addrspace::write32(0x8C001010, 0x50607080);
		r(8) = 0x8C001004;
		PrepareOp(0x8400 | Rm(8) | Imm4(12));		// mov.b @(disp, Rm), R0
		RunOp();
		ASSERT_EQ(r(0), 0xffffff80u);
		AssertState();
	}
	void LoadTest2()
	{
		ClearRegs();
		r(11) = 0x8C000800;
		r(0) = 0x00000800;
		addrspace::write32(r(11) + r(0), 0x88aaccffu);
		PrepareOp(0x000e | Rm(11) | Rn(12));	// mov.l @(R0, Rm), Rn
		RunOp();
		ASSERT_EQ(r(12), 0x88aaccffu);
		AssertState();

		ClearRegs();
		r(11) = 0x8C000800;
		r(0) = 0x00000800;
		addrspace::write32(r(11) + r(0), 0x88aaccffu);
		PrepareOp(0x000d | Rm(11) | Rn(12));	// mov.w @(R0, Rm), Rn
		RunOp();
		ASSERT_EQ(r(12), 0xffffccffu);
		AssertState();

		ClearRegs();
		r(11) = 0x8C000800;
		r(0) = 0x00000800;
		addrspace::write32(r(11) + r(0), 0x88aaccffu);
		PrepareOp(0x000c | Rm(11) | Rn(12));	// mov.b @(R0, Rm), Rn
		RunOp();
		ASSERT_EQ(r(12), 0xffffffffu);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		addrspace::write32(gbr() + 0x10 * 4, 0x11223344u);
		PrepareOp(0xc600 | Imm8(0x10));		// mov.l @(disp, GBR), R0
		RunOp();
		ASSERT_EQ(r(0), 0x11223344u);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		addrspace::write32(gbr() + 0x18 * 2, 0x11223344u);
		PrepareOp(0xc500 | Imm8(0x18));		// mov.w @(disp, GBR), R0
		RunOp();
		ASSERT_EQ(r(0), 0x3344u);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		addrspace::write32(gbr() + 0x17, 0x112233c4u);
		PrepareOp(0xc400 | Imm8(0x17));		// mov.b @(disp, GBR), R0
		RunOp();
		ASSERT_EQ(r(0), 0xffffffc4u);
		AssertState();

		ClearRegs();
		u32 disp = 0x11;
		addrspace::write32(START_PC + 4 + disp * 4, 0x01020304u);
		PrepareOp(0x9,							// nop
				0xd000 | Rn(6) | Imm8(disp));	// mov.l @(disp, PC), Rn
		RunOp(2);
		ASSERT_EQ(r(6), 0x01020304u);	// uses PC & 0xfffffffc
		AssertState();

		ClearRegs();
		disp = 0x12;
		addrspace::write32(START_PC + 4 + disp * 2, 0x01020304u);
		PrepareOp(0x9000 | Rn(5) | Imm8(disp));	// mov.w @(disp, PC), Rn
		RunOp();
		ASSERT_EQ(r(5), 0x0304u);
		AssertState();
	}

	void StoreTest()
	{
		ClearRegs();
		r(14) = 0x8C001000;
		r(11) = 0xbeeff00d;
		PrepareOp(0x2002 | Rm(11) | Rn(14));	// mov.l Rm, @Rn
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001000), 0xbeeff00du);
		ASSERT_EQ(r(14), 0x8C001000u);
		ASSERT_EQ(r(11), 0xbeeff00du);
		AssertState();

		ClearRegs();
		r(14) = 0x8C001000;
		r(11) = 0xf00dbeef;
		addrspace::write32(0x8C001000, 0xbaadbaad);
		PrepareOp(0x2001 | Rm(11) | Rn(14));	// mov.w Rm, @Rn
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001000), 0xbaadbeefu);
		ASSERT_EQ(r(14), 0x8C001000u);
		ASSERT_EQ(r(11), 0xf00dbeefu);
		AssertState();

		ClearRegs();
		r(14) = 0x8C001000;
		r(11) = 0xccccccf0;
		addrspace::write32(0x8C001000, 0xbaadbaad);
		PrepareOp(0x2000 | Rm(11) | Rn(14));	// mov.b Rm, @Rn
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001000), 0xbaadbaf0u);
		ASSERT_EQ(r(14), 0x8C001000u);
		ASSERT_EQ(r(11), 0xccccccf0u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		r(7) = 0xfeedf00d;
		PrepareOp(0x2006 | Rm(7) | Rn(8));		// mov.l Rm, @-Rn
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001000), 0xfeedf00du);
		ASSERT_EQ(r(7), 0xfeedf00du);
		ASSERT_EQ(r(8), 0x8C001000u);
		AssertState();

		ClearRegs();
		r(7) = 0x8C001004;
		PrepareOp(0x2006 | Rm(7) | Rn(7));		// mov.l Rm, @-Rn
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001000), 0x8C001004); // value before decrement is stored
		ASSERT_EQ(r(7), 0x8C001000u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001002;
		r(7) = 0x1234cafe;
		PrepareOp(0x2005 | Rm(7) | Rn(8));		// mov.w Rm, @-Rn
		RunOp();
		ASSERT_EQ(addrspace::read16(0x8C001000), 0xcafeu);
		ASSERT_EQ(r(7), 0x1234cafeu);
		ASSERT_EQ(r(8), 0x8C001000u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001001;
		r(7) = 0x12345642;
		PrepareOp(0x2004 | Rm(7) | Rn(8));		// mov.b Rm, @-Rn
		RunOp();
		ASSERT_EQ(addrspace::read8(0x8C001000), 0x42u);
		ASSERT_EQ(r(7), 0x12345642u);
		ASSERT_EQ(r(8), 0x8C001000u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		r(7) = 0x50607080;
		PrepareOp(0x1000 | Rm(7) | Rn(8) | Imm4(3));// mov.l Rm, @(disp, Rn)
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001010), 0x50607080u);
		ASSERT_EQ(r(7), 0x50607080u);
		ASSERT_EQ(r(8), 0x8C001004u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		r(0) = 0x10203040;
		PrepareOp(0x8100 | Rm(8) | Imm4(3));		// mov.w R0, @(disp, Rn)
		RunOp();
		ASSERT_EQ(addrspace::read16(0x8C00100A), 0x3040u);
		ASSERT_EQ(r(0), 0x10203040u);
		ASSERT_EQ(r(8), 0x8C001004u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		r(0) = 0x66666672;
		PrepareOp(0x8000 | Rm(8) | Imm4(3));		// mov.b R0, @(disp, Rn)
		RunOp();
		ASSERT_EQ(addrspace::read8(0x8C001007), 0x72u);
		ASSERT_EQ(r(0), 0x66666672u);
		ASSERT_EQ(r(8), 0x8C001004u);
		AssertState();
	}
	void StoreTest2()
	{
		ClearRegs();
		r(11) = 0x8C000800;
		r(0) = 0x00000800;
		r(12) = 0x87654321;
		PrepareOp(0x0006 | Rm(12) | Rn(11));	// mov.l Rm, @(R0, Rn)
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001000), 0x87654321u);
		ASSERT_EQ(r(12), 0x87654321u);
		ASSERT_EQ(r(11), 0x8C000800u);
		ASSERT_EQ(r(0), 0x00000800u);
		AssertState();

		ClearRegs();
		r(11) = 0x8C000800;
		r(0) = 0x00000800;
		r(12) = 0x12345678;
		PrepareOp(0x0005 | Rm(12) | Rn(11));	// mov.w Rm, @(R0, Rn)
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001000), 0x87655678u);	// relies on value set in previous test
		ASSERT_EQ(r(12), 0x12345678u);
		ASSERT_EQ(r(11), 0x8C000800u);
		ASSERT_EQ(r(0), 0x00000800u);
		AssertState();

		ClearRegs();
		r(11) = 0x8C000800;
		r(0) = 0x00000800;
		r(12) = 0x99999999;
		PrepareOp(0x0004 | Rm(12) | Rn(11));	// mov.b Rm, @(R0, Rn)
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C001000), 0x87655699u);	// relies on value set in 2 previous tests
		ASSERT_EQ(r(12), 0x99999999u);
		ASSERT_EQ(r(11), 0x8C000800u);
		ASSERT_EQ(r(0), 0x00000800u);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		r(0) = 0xabcdef01;
		PrepareOp(0xc200 | Imm8(0x10));			// mov.l R0, @(disp, GBR)
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C000840), 0xabcdef01u);
		ASSERT_EQ(gbr(), 0x8C000800u);
		ASSERT_EQ(r(0), 0xabcdef01u);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		r(0) = 0x11117777;
		PrepareOp(0xc100 | Imm8(0x20));			// mov.w R0, @(disp, GBR)
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C000840), 0xabcd7777u);	// relies on value set in previous test
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		r(0) = 0x22222266;
		PrepareOp(0xc000 | Imm8(0x40));			// mov.b R0, @(disp, GBR)
		RunOp();
		ASSERT_EQ(addrspace::read32(0x8C000840), 0xabcd7766u);	// relies on value set in 2 previous tests
		AssertState();
	}

	void ArithmeticTest()
	{
		ClearRegs();
		r(0) = 0x111111;
		r(1) = 0x222222;
		PrepareOp(0x300c | Rm(0) | Rn(1));	// add Rm, Rn
		RunOp();
		ASSERT_EQ(r(0), 0x111111u);
		ASSERT_EQ(r(1), 0x333333u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(2) = 0x11111;
		PrepareOp(0x7000 | Rn(2) | Imm8(-1));// add #imm, Rn
		RunOp();
		ASSERT_EQ(r(2), 0x11110u);
		AssertState();

		ClearRegs();
		r(2) = 0;
		r(3) = 0;
		PrepareOp(0x300e | Rn(2) | Rm(3));	// addc Rm, Rn
		RunOp();
		ASSERT_EQ(r(2), 0u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		sr().T = 1;
		r(2) = 0;
		r(3) = 0;
		RunOp();
		ASSERT_EQ(r(2), 1u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		sr().T = 0;
		r(2) = 0xffffffff;
		r(3) = 1;
		RunOp();
		ASSERT_EQ(r(2), 0u);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		sr().T = 0;
		r(2) = 0xfffffffe;
		r(3) = 1;
		RunOp();
		ASSERT_EQ(r(2), 0xffffffffu);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		sr().T = 1;
		r(2) = 0xfffffffe;
		r(3) = 1;
		RunOp();
		ASSERT_EQ(r(2), 0u);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(2) = 0;
		r(3) = 0;
		PrepareOp(0x300f | Rn(2) | Rm(3));	// addv Rm, Rn
		RunOp();
		ASSERT_EQ(r(2), 0u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(2) = 1;
		r(3) = 0x7FFFFFFE;
		RunOp();
		ASSERT_EQ(r(2), 0x7FFFFFFFu);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(2) = 2;
		r(3) = 0x7FFFFFFE;
		RunOp();
		ASSERT_EQ(r(2), 0x80000000u);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(2) = -1;
		r(3) = 0x80000001;
		RunOp();
		ASSERT_EQ(r(2), 0x80000000u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(2) = -2;
		r(3) = 0x80000001;
		RunOp();
		ASSERT_EQ(r(2), 0x7FFFFFFFu);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(0) = 0x111111;
		r(1) = 0x333333;
		PrepareOp(0x3008 | Rm(0) | Rn(1));	// sub Rm, Rn
		RunOp();
		ASSERT_EQ(r(1), 0x222222u);
		ASSERT_EQ(r(0), 0x111111u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(0) = 0x111111;
		r(1) = 0x333333;
		PrepareOp(0x300A | Rm(0) | Rn(1));	// subc Rm, Rn
		RunOp();
		ASSERT_EQ(r(1), 0x222222u);
		ASSERT_EQ(r(0), 0x111111u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		sr().T = 0;
		r(0) = 1;
		r(1) = 0;
		RunOp();
		ASSERT_EQ(r(1), 0xffffffffu);
		ASSERT_EQ(r(0), 1u);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		sr().T = 1;
		r(0) = 0;
		r(1) = 0;
		RunOp();
		ASSERT_EQ(r(1), 0xffffffffu);
		ASSERT_EQ(r(0), 0u);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		sr().T = 1;
		r(0) = 0;
		r(1) = 1;
		RunOp();
		ASSERT_EQ(r(1), 0u);
		ASSERT_EQ(r(0), 0u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(2) = 0;
		r(3) = 0;
		PrepareOp(0x300b | Rn(2) | Rm(3));	// subv Rm, Rn
		RunOp();
		ASSERT_EQ(r(2), 0u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(2) = 0x80000001;
		r(3) = 2;
		RunOp();
		ASSERT_EQ(r(2), 0x7fffffffu);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(2) = 0x7FFFFFFE;
		r(3) = 0xFFFFFFFE;
		RunOp();
		ASSERT_EQ(r(2), 0x80000000u);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(2) = 0x7FFFFFFE;
		r(3) = 0xFFFFFFFF;
		RunOp();
		ASSERT_EQ(r(2), 0x7FFFFFFFu);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 0;
		PrepareOp(0x600b | Rn(15) | Rm(13));	// neg Rm, Rn
		RunOp();
		ASSERT_EQ(r(15), 0u);
		ASSERT_EQ(r(13), 0u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 1;
		RunOp();
		ASSERT_EQ(r(15), 0xffffffff);
		ASSERT_EQ(r(13), 1u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 0;
		PrepareOp(0x600a | Rn(15) | Rm(13));	// negc Rm, Rn
		RunOp();
		ASSERT_EQ(r(15), 0u);
		ASSERT_EQ(r(13), 0u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		sr().T = 1;
		r(13) = 0xffffffff;
		RunOp();
		ASSERT_EQ(r(15), 0u);
		ASSERT_EQ(r(13), 0xffffffffu);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		sr().T = 0;
		r(13) = 0xffffffff;
		RunOp();
		ASSERT_EQ(r(15), 1u);
		ASSERT_EQ(r(13), 0xffffffffu);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(13) = 1;
		PrepareOp(0x600a | Rn(13) | Rm(13));	// negc Rm, Rn
		RunOp();
		ASSERT_EQ(r(13), 0xffffffffu);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		sr().T = 1;
		r(13) = 0;
		RunOp();
		ASSERT_EQ(r(13), 0xffffffffu);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(13) = 1;
		PrepareOp(0x600f | Rn(12) | Rm(13));	// exts.w Rm, Rn
		RunOp();
		ASSERT_EQ(r(12), 1u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 0xffff;
		RunOp();
		ASSERT_EQ(r(12), -1u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 1;
		PrepareOp(0x600d | Rn(12) | Rm(13));	// extu.w Rm, Rn
		RunOp();
		ASSERT_EQ(r(12), 1u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 0xffff;
		RunOp();
		ASSERT_EQ(r(12), 0xffffu);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 1;
		PrepareOp(0x600e | Rn(12) | Rm(13));	// exts.b Rm, Rn
		RunOp();
		ASSERT_EQ(r(12), 1u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 0x80;
		RunOp();
		ASSERT_EQ(r(12), 0xffffff80u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 1;
		PrepareOp(0x600c | Rn(12) | Rm(13));	// extu.b Rm, Rn
		RunOp();
		ASSERT_EQ(r(12), 1u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(13) = 0xff;
		RunOp();
		ASSERT_EQ(r(12), 0xffu);
		ASSERT_EQ(sr().T, 0u);
		AssertState();
	}
	void MulDivTest()
	{
		ClearRegs();
		r(2) = 0;
		r(3) = 0;
		PrepareOp(0x2007 | Rn(2) | Rm(3));	// div0s Rm, Rn
		RunOp();
		ASSERT_EQ(sr().Q, 0u);
		ASSERT_EQ(sr().M, 0u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(2) = 0x80000000;
		r(3) = 0;
		RunOp();
		ASSERT_EQ(sr().Q, 1u);
		ASSERT_EQ(sr().M, 0u);
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(2) = 0x80000000;
		r(3) = 0xffffffff;
		RunOp();
		ASSERT_EQ(sr().Q, 1u);
		ASSERT_EQ(sr().M, 1u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		sr().Q = sr().M = sr().T = 1;
		PrepareOp(0x0019);					// div0u
		RunOp();
		ASSERT_EQ(sr().Q, 0u);
		ASSERT_EQ(sr().M, 0u);
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		// TODO div1 :P

		ClearRegs();
		r(7) = 1000000000;
		r(8) = -1000000000;
		PrepareOp(0x300d | Rn(7) | Rm(8));	// dmuls.l Rm, Rn
		RunOp();
		ASSERT_EQ(mac(), (u64)((s64)1000000000 * -1000000000));
		AssertState();

		ClearRegs();
		r(7) = 0x12345678;
		r(8) = 0x10000000;
		PrepareOp(0x3005 | Rn(7) | Rm(8));	// dmulu.l Rm, Rn
		RunOp();
		ASSERT_EQ(mac(), 0x123456780000000ull);
		AssertState();

		ClearRegs();
		r(7) = 0x12345678;
		r(8) = 0x10000000;
		PrepareOp(0x0007 | Rn(7) | Rm(8));	// mul.l Rm, Rn
		RunOp();
		ASSERT_EQ(mac(), 0x80000000ull);
		AssertState();

		ClearRegs();
		r(7) = 0x5678;
		r(8) = 0x8000;
		PrepareOp(0x200e | Rn(7) | Rm(8));	// mulu.l Rm, Rn
		RunOp();
		ASSERT_EQ(mac(), 0x2B3C0000ull);
		AssertState();

		ClearRegs();
		r(7) = -5678;
		r(8) = -1000;
		PrepareOp(0x200f | Rn(7) | Rm(8));	// muls.l Rm, Rn
		RunOp();
		ASSERT_EQ(mac(), 5678000ull);
		AssertState();

		ClearRegs();
		r(7) = 0xAC001000;
		addrspace::write32(r(7), 4);
		r(8) = 0xAC002000;
		addrspace::write32(r(8), 3);
		PrepareOp(0x000f | Rn(7) | Rm(8));	// mac.l @Rm+, @Rn+
		RunOp();
		ASSERT_EQ(mac(), 12ull);
		ASSERT_EQ(r(7), 0xAC001004u);
		ASSERT_EQ(r(8), 0xAC002004u);

		addrspace::write32(r(7), -5);
		addrspace::write32(r(8), 7);
		RunOp();
		ASSERT_EQ(mac(), -23ull);
		ASSERT_EQ(r(7), 0xAC001008u);
		ASSERT_EQ(r(8), 0xAC002008u);
		AssertState();

		ClearRegs();
		r(7) = 0xAC001000;
		addrspace::write32(r(7), (u16)-7);
		r(8) = 0xAC002000;
		addrspace::write32(r(8), 3);
		PrepareOp(0x400f | Rn(7) | Rm(8));	// mac.w @Rm+, @Rn+
		RunOp();
		ASSERT_EQ(mac(), -21ull);
		ASSERT_EQ(r(7), 0xAC001002u);
		ASSERT_EQ(r(8), 0xAC002002u);

		addrspace::write16(r(7), 5);
		addrspace::write16(r(8), 7);
		RunOp();
		ASSERT_EQ(mac(), 14ull);
		ASSERT_EQ(r(7), 0xAC001004u);
		ASSERT_EQ(r(8), 0xAC002004u);
		AssertState();
	}
	void CmpTest()
	{
		ClearRegs();
		r(0) = 0;
		PrepareOp(0x8800 | Imm8(3));	// cmp/eq #imm, R0
		RunOp();
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		r(0) = 3;
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(0) = -1;
		PrepareOp(0x8800 | Imm8(-1));	// cmp/eq #imm, R0
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		AssertState();

		ClearRegs();
		r(2) = 0;
		r(3) = 0;
		PrepareOp(0x3000 | Rn(2) | Rm(3));	// cmp/eq Rm, Rn
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		ASSERT_EQ(r(2), 0u);
		ASSERT_EQ(r(3), 0u);
		AssertState();

		ClearRegs();
		r(2) = 0;
		r(3) = 1;
		RunOp();
		ASSERT_EQ(sr().T, 0u);
		ASSERT_EQ(r(2), 0u);
		ASSERT_EQ(r(3), 1u);
		AssertState();

		ClearRegs();
		r(2) = 0;
		r(3) = 0;
		PrepareOp(0x3002 | Rn(2) | Rm(3));	// cmp/hs Rm, Rn
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		ASSERT_EQ(r(2), 0u);
		ASSERT_EQ(r(3), 0u);
		AssertState();

		ClearRegs();
		r(2) = 0x80000000;
		r(3) = 1;
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		ASSERT_EQ(r(2), 0x80000000u);
		ASSERT_EQ(r(3), 1u);
		AssertState();

		// TBC
	}

	void StatusRegTest()
	{
		ClearRegs();
		sr().S = 1;
		PrepareOp(0x0048);	// clrs
		RunOp();
		ASSERT_EQ(sr().S, 0u);

		PrepareOp(0x0058);	// sets
		RunOp();
		ASSERT_EQ(sr().S, 1u);

		sr().T = 1;
		PrepareOp(0x0008);	// clrt
		RunOp();
		ASSERT_EQ(sr().T, 0u);

		PrepareOp(0x0018);	// sett
		RunOp();
		ASSERT_EQ(sr().T, 1u);
	}

	void FloatingPointTest()
	{
		ctx->fpscr.PR = 0;

		ClearRegs();
		PrepareOp(0xF08D);	// fldi0 fr0
		RunOp();
		ASSERT_EQ(fr(0), 0.f);
		AssertState();

		ClearRegs();
		PrepareOp(0xF39D);	// fldi1 fr3
		RunOp();
		ASSERT_EQ(fr(3), 1.f);
		AssertState();

		ClearRegs();
		fr(2) = 48.f;
		PrepareOp(0xF21D);	// flds fr2, fpul
		RunOp();
		ASSERT_EQ(*(float *)&ctx->fpul, 48.f);
		AssertState();

		ClearRegs();
		*(float *)&ctx->fpul = 844.f;
		PrepareOp(0xF60D);	// fsts fpul, fr6
		RunOp();
		ASSERT_EQ(fr(6), 844.f);
		AssertState();

		ClearRegs();
		fr(10) = -128.f;
		PrepareOp(0xFA5D);	// fabs fr10
		RunOp();
		ASSERT_EQ(fr(10), 128.f);
		AssertState();

		ClearRegs();
		fr(11) = 64.f;
		PrepareOp(0xFB4D);	// fneg fr11
		RunOp();
		ASSERT_EQ(fr(11), -64.f);
		AssertState();

		// FADD
		ClearRegs();
		fr(12) = 13.f;
		fr(13) = 12.f;
		PrepareOp(0xFCD0);	// fadd fr13, fr12
		RunOp();
		ASSERT_EQ(fr(12), 25.f);
		ASSERT_EQ(fr(13), 12.f);
		AssertState();
		// special cases
		// +inf + norm = +inf
		fr(12) = std::numeric_limits<float>::infinity();
		fr(13) = -10.f;
		RunOp();
		ASSERT_EQ(fr(12), std::numeric_limits<float>::infinity());
		// norm + -inf = -inf
		fr(12) = 2.f;
		fr(13) = -std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(fr(12), -std::numeric_limits<float>::infinity());
		// NaN + norm = NaN
		fr(12) = 2.f;
		fr(13) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		GTEST_ASSERT_NE(fr(12), fr(12));

		// FSUB
		ClearRegs();
		fr(14) = 10.f;
		fr(15) = 11.f;
		PrepareOp(0xFEF1);	// fsub fr15, fr14
		RunOp();
		ASSERT_EQ(fr(14), -1.f);
		ASSERT_EQ(fr(15), 11.f);
		AssertState();
		// special cases
		// +inf - norm = +inf
		fr(14) = std::numeric_limits<float>::infinity();
		fr(15) = 10.f;
		RunOp();
		ASSERT_EQ(fr(14), std::numeric_limits<float>::infinity());
		// -inf - norm = -inf
		fr(14) = -std::numeric_limits<float>::infinity();
		fr(15) = -1.f;
		RunOp();
		ASSERT_EQ(fr(14), -std::numeric_limits<float>::infinity());
		// norm - NaN = NaN
		fr(14) = 2.f;
		fr(15) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		GTEST_ASSERT_NE(fr(14), fr(14));

		// FMUL
		ClearRegs();
		fr(0) = 4.f;
		fr(2) = 8.f;
		PrepareOp(0xF022);	// fmul fr2, fr0
		RunOp();
		ASSERT_EQ(fr(0), 32.f);
		ASSERT_EQ(fr(2), 8.f);
		AssertState();
		// special cases
		// +inf * norm = +inf
		fr(0) = std::numeric_limits<float>::infinity();
		fr(2) = 50.f;
		RunOp();
		ASSERT_EQ(fr(0), std::numeric_limits<float>::infinity());
		// -inf * norm = -inf
		fr(0) = -std::numeric_limits<float>::infinity();
		fr(2) = 100.f;
		RunOp();
		ASSERT_EQ(fr(0), -std::numeric_limits<float>::infinity());
		// norm * NaN = NaN
		fr(0) = 2.f;
		fr(2) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		GTEST_ASSERT_NE(fr(0), fr(0));

		// FDIV
		ClearRegs();
		fr(1) = 8.f;
		fr(3) = -2.f;
		PrepareOp(0xF133);	// fdiv fr3, fr1
		RunOp();
		ASSERT_EQ(fr(1), -4.f);
		ASSERT_EQ(fr(3), -2.f);
		AssertState();
		// special cases
		// +inf / -norm = -inf
		fr(1) = std::numeric_limits<float>::infinity();
		fr(3) = -50.f;
		RunOp();
		ASSERT_EQ(fr(1), -std::numeric_limits<float>::infinity());
		// -inf / norm = -inf
		fr(1) = -std::numeric_limits<float>::infinity();
		fr(3) = 100.f;
		RunOp();
		ASSERT_EQ(fr(1), -std::numeric_limits<float>::infinity());
		// norm / NaN = NaN
		fr(1) = 2.f;
		fr(3) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		GTEST_ASSERT_NE(fr(1), fr(1));

		// FMAC
		ClearRegs();
		fr(0) = 3.f;
		fr(4) = 5.f;
		fr(5) = -7.f;
		PrepareOp(0xF45E);	// fmac fr0, fr5, fr4
		RunOp();
		ASSERT_EQ(fr(0), 3.f);
		ASSERT_EQ(fr(4), -16.f);
		ASSERT_EQ(fr(5), -7.f);
		AssertState();
		// special cases
		// +inf + norm * norm = +inf
		fr(4) = std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(fr(4), std::numeric_limits<float>::infinity());
		// norm + +inf * -inf = -inf
		fr(0) = std::numeric_limits<float>::infinity();
		fr(4) = 1.f;
		fr(5) = -std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(fr(4), -std::numeric_limits<float>::infinity());
		// norm + norm * NaN = NaN
		fr(0) = 2.f;
		fr(4) = 1.f;
		fr(5) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		GTEST_ASSERT_NE(fr(4), fr(4));

		// FSQRT
		ClearRegs();
		fr(11) = 64.f;
		PrepareOp(0xFB6D);	// fsqrt fr11
		RunOp();
		ASSERT_EQ(fr(11), 8.f);
		AssertState();
		// special cases
		// sqrt(+inf) = +inf
		fr(11) = std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(fr(11), std::numeric_limits<float>::infinity());
		// sqrt(NaN) = NaN
		fr(11) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		GTEST_ASSERT_NE(fr(11), fr(11));
		// sqrt(0) = 0
		fr(11) = 0.f;
		RunOp();
		ASSERT_EQ(fr(11), 0.f);

		// FCMP/EQ
		ClearRegs();
		fr(4) = 45.f;
		fr(7) = 45.f;
		PrepareOp(0xF474);	// fcmp/eq fr7, fr4
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		AssertState();
		fr(4) = 46.f;
		RunOp();
		ASSERT_EQ(sr().T, 0u);
		AssertState();
		// special cases
		// +inf == +inf
		fr(4) = std::numeric_limits<float>::infinity();
		fr(7) = std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		// -inf == -inf
		fr(4) = -std::numeric_limits<float>::infinity();
		fr(7) = -std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		// NaN != NaN
		fr(4) = std::numeric_limits<float>::quiet_NaN();
		fr(7) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		ASSERT_EQ(sr().T, 0u);

		// FCMP/GT
		ClearRegs();
		fr(8) = 23.f;
		fr(9) = 22.f;
		PrepareOp(0xF895);	// fcmp/gt fr9, fr8
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		AssertState();
		fr(9) = 100.f;
		RunOp();
		ASSERT_EQ(sr().T, 0u);
		AssertState();
		// special cases
		// +inf > norm
		fr(8) = std::numeric_limits<float>::infinity();
		fr(9) = 77.f;
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		// -inf < +inf
		fr(8) = -std::numeric_limits<float>::infinity();
		fr(9) = std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(sr().T, 0u);
		// norm > -inf
		fr(8) = -27.f;
		fr(9) = -std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		// !(Nan > NaN)
		fr(8) = std::numeric_limits<float>::quiet_NaN();
		fr(9) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		ASSERT_EQ(sr().T, 0u);

		ClearRegs();
		ctx->fpul = 222;
		PrepareOp(0xF62D);	// float fpul, fr6
		RunOp();
		ASSERT_EQ(fr(6), 222.f);
		AssertState();

		// FTRC
		ClearRegs();
		fr(1) = 100.f;
		PrepareOp(0xF13D);	// ftrc fr1, fpul
		RunOp();
		ASSERT_EQ(ctx->fpul, 100u);
		AssertState();
		// special cases
		// 2147483520.f -> 2147483520
		fr(1) = 2147483520.f;
		RunOp();
		ASSERT_EQ(ctx->fpul, 2147483520u);
		// >2147483520.f -> 0x7fffffff;
		fr(1) = 2147483648.f;
		RunOp();
		ASSERT_EQ(ctx->fpul, 0x7fffffffu);
		// -2147483648 -> -2147483648
		fr(1) = -2147483648.f;
		RunOp();
		ASSERT_EQ(ctx->fpul, (u32)-2147483648);
		// <-2147483648 -> 0x80000000
		fr(1) = -2147483904.f;
		RunOp();
		ASSERT_EQ(ctx->fpul, 0x80000000u);
		// +inf -> 0x7fffffff
		fr(1) = std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(ctx->fpul, 0x7fffffffu);
		// -inf -> 0x80000000
		fr(1) = -std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(ctx->fpul, 0x80000000u);
		// NaN -> 0x80000000
		fr(1) = -std::numeric_limits<float>::quiet_NaN();
		RunOp();
		ASSERT_EQ(ctx->fpul, 0x80000000u);

		// FSRRA
		ClearRegs();
		fr(5) = 16.f;
		PrepareOp(0xF57D);	// fsrra fr5
		RunOp();
		ASSERT_EQ(fr(5), 0.25f);
		AssertState();
		// special cases
		// 1/sqrt(+inf) -> 0
		fr(5) = std::numeric_limits<float>::infinity();
		RunOp();
		ASSERT_EQ(fr(5), 0);
		// 1/sqrt(NaN) -> NaN
		fr(5) = std::numeric_limits<float>::quiet_NaN();
		RunOp();
		GTEST_ASSERT_NE(fr(5), fr(5));

		// FSCA
		ClearRegs();
		ctx->fpul = 0x8000;	// pi
		PrepareOp(0xF6FD);	// fsca fpul, dr3
		RunOp();
		ASSERT_EQ(fr(6), 0.f);	// sin
		ASSERT_EQ(fr(7), -1.f);	// cos
		AssertState();
	}

	void DoubleFloatingPointTest()
	{
		ctx->fpscr.PR = 1;
		ctx->fpscr.SZ = 0;

		ClearRegs();
		setDr(5, -128.0);
		PrepareOp(0xFA5D);	// fabs dr5
		RunOp();
		ASSERT_EQ(getDr(5), 128.0);
		AssertState();

		ClearRegs();
		setDr(4, 64.0);
		PrepareOp(0xF84D);	// fneg dr4
		RunOp();
		ASSERT_EQ(getDr(4), -64.0);
		AssertState();

		ClearRegs();
		setDr(6, 13.0);
		setDr(7, 12.0);
		PrepareOp(0xFCE0);	// fadd dr7, dr6
		RunOp();
		ASSERT_EQ(getDr(6), 25.0);
		ASSERT_EQ(getDr(7), 12.0);
		AssertState();

		ClearRegs();
		setDr(0, 10.0);
		setDr(1, 11.0);
		PrepareOp(0xF021);	// fsub dr1, dr0
		RunOp();
		ASSERT_EQ(getDr(0), -1.0);
		ASSERT_EQ(getDr(1), 11.0);
		AssertState();

		ClearRegs();
		setDr(0, 4.0);
		setDr(2, 8.0);
		PrepareOp(0xF042);	// fmul dr2, dr0
		RunOp();
		ASSERT_EQ(getDr(0), 32.0);
		ASSERT_EQ(getDr(2), 8.0);
		AssertState();

		ClearRegs();
		setDr(1, 8.0);
		setDr(3, -2.0);
		PrepareOp(0xF263);	// fdiv dr3, dr1
		RunOp();
		ASSERT_EQ(getDr(1), -4.0);
		ASSERT_EQ(getDr(3), -2.0);
		AssertState();

		ClearRegs();
		setDr(1, 64.0);
		PrepareOp(0xF26D);	// fsqrt dr1
		RunOp();
		ASSERT_EQ(getDr(1), 8.0);
		AssertState();

		ClearRegs();
		setDr(4, 45.0);
		setDr(7, 45.0);
		PrepareOp(0xF8E4);	// fcmp/eq dr7, dr4
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		AssertState();
		setDr(4, 46.0);
		RunOp();
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		setDr(4, 23.0);
		setDr(7, 22.0);
		PrepareOp(0xF8E5);	// fcmp/gt dr7, dr4
		RunOp();
		ASSERT_EQ(sr().T, 1u);
		AssertState();
		setDr(7, 100.0);
		RunOp();
		ASSERT_EQ(sr().T, 0u);
		AssertState();

		ClearRegs();
		ctx->fpul = 123;
		PrepareOp(0xFC2D);	// float fpul, dr6
		RunOp();
		ASSERT_EQ(getDr(6), 123.0);
		AssertState();

		ClearRegs();
		setDr(1, 100.0);
		PrepareOp(0xF23D);	// ftrc dr1, fpul
		RunOp();
		ASSERT_EQ(ctx->fpul, 100u);
		AssertState();

		ClearRegs();
		*(float *)&ctx->fpul = 0.5f;
		PrepareOp(0xF8AD);	// fcnvsd fpul, dr4
		RunOp();
		ASSERT_EQ(getDr(4), 0.5);
		AssertState();

		ClearRegs();
		setDr(1, 0.25);
		PrepareOp(0xF2BD);	// fcnvds dr1, fpul
		RunOp();
		ASSERT_EQ(*(float *)&ctx->fpul, 0.25f);
		AssertState();
	}
};
