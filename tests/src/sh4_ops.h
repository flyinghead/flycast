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
#include "hw/mem/_vmem.h"

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
		_vmem_WriteMem32(r(14), 0xffccaa88u);
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
		_vmem_WriteMem32(r(8), 0x4433ff11);
		PrepareOp(0x6006 | Rm(8) | Rn(7));		// mov.l @Rm+,Rn
		RunOp();
		ASSERT_EQ(r(7), 0x4433ff11u);
		ASSERT_EQ(r(8), 0x8C001008);
		AssertState();

		ClearRegs();
		r(7) = 0x8C001004;
		_vmem_WriteMem32(r(7), 0x4433ff11);
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
		_vmem_WriteMem32(0x8C001010, 0x50607080);
		r(8) = 0x8C001004;
		PrepareOp(0x5000 | Rm(8) | Rn(7) | Imm4(3));// mov.l @(disp, Rm), Rn
		RunOp();
		ASSERT_EQ(r(7), 0x50607080u);
		AssertState();

		ClearRegs();
		_vmem_WriteMem32(0x8C001010, 0x50607080);
		r(8) = 0x8C001004;
		PrepareOp(0x8500 | Rm(8) | Imm4(6));		// mov.w @(disp, Rm), R0
		RunOp();
		ASSERT_EQ(r(0), 0x7080u);
		AssertState();

		ClearRegs();
		_vmem_WriteMem32(0x8C001010, 0x50607080);
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
		_vmem_WriteMem32(r(11) + r(0), 0x88aaccffu);
		PrepareOp(0x000e | Rm(11) | Rn(12));	// mov.l @(R0, Rm), Rn
		RunOp();
		ASSERT_EQ(r(12), 0x88aaccffu);
		AssertState();

		ClearRegs();
		r(11) = 0x8C000800;
		r(0) = 0x00000800;
		_vmem_WriteMem32(r(11) + r(0), 0x88aaccffu);
		PrepareOp(0x000d | Rm(11) | Rn(12));	// mov.w @(R0, Rm), Rn
		RunOp();
		ASSERT_EQ(r(12), 0xffffccffu);
		AssertState();

		ClearRegs();
		r(11) = 0x8C000800;
		r(0) = 0x00000800;
		_vmem_WriteMem32(r(11) + r(0), 0x88aaccffu);
		PrepareOp(0x000c | Rm(11) | Rn(12));	// mov.b @(R0, Rm), Rn
		RunOp();
		ASSERT_EQ(r(12), 0xffffffffu);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		_vmem_WriteMem32(gbr() + 0x10 * 4, 0x11223344u);
		PrepareOp(0xc600 | Imm8(0x10));		// mov.l @(disp, GBR), R0
		RunOp();
		ASSERT_EQ(r(0), 0x11223344u);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		_vmem_WriteMem32(gbr() + 0x18 * 2, 0x11223344u);
		PrepareOp(0xc500 | Imm8(0x18));		// mov.w @(disp, GBR), R0
		RunOp();
		ASSERT_EQ(r(0), 0x3344u);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		_vmem_WriteMem32(gbr() + 0x17, 0x112233c4u);
		PrepareOp(0xc400 | Imm8(0x17));		// mov.b @(disp, GBR), R0
		RunOp();
		ASSERT_EQ(r(0), 0xffffffc4u);
		AssertState();

		ClearRegs();
		u32 disp = 0x11;
		_vmem_WriteMem32(START_PC + 4 + disp * 4, 0x01020304u);
		PrepareOp(0x9,							// nop
				0xd000 | Rn(6) | Imm8(disp));	// mov.l @(disp, PC), Rn
		RunOp(2);
		ASSERT_EQ(r(6), 0x01020304u);	// uses PC & 0xfffffffc
		AssertState();

		ClearRegs();
		disp = 0x12;
		_vmem_WriteMem32(START_PC + 4 + disp * 2, 0x01020304u);
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
		ASSERT_EQ(_vmem_ReadMem32(0x8C001000), 0xbeeff00du);
		ASSERT_EQ(r(14), 0x8C001000u);
		ASSERT_EQ(r(11), 0xbeeff00du);
		AssertState();

		ClearRegs();
		r(14) = 0x8C001000;
		r(11) = 0xf00dbeef;
		_vmem_WriteMem32(0x8C001000, 0xbaadbaad);
		PrepareOp(0x2001 | Rm(11) | Rn(14));	// mov.w Rm, @Rn
		RunOp();
		ASSERT_EQ(_vmem_ReadMem32(0x8C001000), 0xbaadbeefu);
		ASSERT_EQ(r(14), 0x8C001000u);
		ASSERT_EQ(r(11), 0xf00dbeefu);
		AssertState();

		ClearRegs();
		r(14) = 0x8C001000;
		r(11) = 0xccccccf0;
		_vmem_WriteMem32(0x8C001000, 0xbaadbaad);
		PrepareOp(0x2000 | Rm(11) | Rn(14));	// mov.b Rm, @Rn
		RunOp();
		ASSERT_EQ(_vmem_ReadMem32(0x8C001000), 0xbaadbaf0u);
		ASSERT_EQ(r(14), 0x8C001000u);
		ASSERT_EQ(r(11), 0xccccccf0u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		r(7) = 0xfeedf00d;
		PrepareOp(0x2006 | Rm(7) | Rn(8));		// mov.l Rm, @-Rn
		RunOp();
		ASSERT_EQ(_vmem_ReadMem32(0x8C001000), 0xfeedf00du);
		ASSERT_EQ(r(7), 0xfeedf00du);
		ASSERT_EQ(r(8), 0x8C001000u);
		AssertState();

		ClearRegs();
		r(7) = 0x8C001004;
		PrepareOp(0x2006 | Rm(7) | Rn(7));		// mov.l Rm, @-Rn
		RunOp();
		ASSERT_EQ(_vmem_ReadMem32(0x8C001000), 0x8C001004); // value before decrement is stored
		ASSERT_EQ(r(7), 0x8C001000u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001002;
		r(7) = 0x1234cafe;
		PrepareOp(0x2005 | Rm(7) | Rn(8));		// mov.w Rm, @-Rn
		RunOp();
		ASSERT_EQ(_vmem_ReadMem16(0x8C001000), 0xcafeu);
		ASSERT_EQ(r(7), 0x1234cafeu);
		ASSERT_EQ(r(8), 0x8C001000u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001001;
		r(7) = 0x12345642;
		PrepareOp(0x2004 | Rm(7) | Rn(8));		// mov.b Rm, @-Rn
		RunOp();
		ASSERT_EQ(_vmem_ReadMem8(0x8C001000), 0x42u);
		ASSERT_EQ(r(7), 0x12345642u);
		ASSERT_EQ(r(8), 0x8C001000u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		r(7) = 0x50607080;
		PrepareOp(0x1000 | Rm(7) | Rn(8) | Imm4(3));// mov.l Rm, @(disp, Rn)
		RunOp();
		ASSERT_EQ(_vmem_ReadMem32(0x8C001010), 0x50607080u);
		ASSERT_EQ(r(7), 0x50607080u);
		ASSERT_EQ(r(8), 0x8C001004u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		r(0) = 0x10203040;
		PrepareOp(0x8100 | Rm(8) | Imm4(3));		// mov.w R0, @(disp, Rn)
		RunOp();
		ASSERT_EQ(_vmem_ReadMem16(0x8C00100A), 0x3040u);
		ASSERT_EQ(r(0), 0x10203040u);
		ASSERT_EQ(r(8), 0x8C001004u);
		AssertState();

		ClearRegs();
		r(8) = 0x8C001004;
		r(0) = 0x66666672;
		PrepareOp(0x8000 | Rm(8) | Imm4(3));		// mov.b R0, @(disp, Rn)
		RunOp();
		ASSERT_EQ(_vmem_ReadMem8(0x8C001007), 0x72u);
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
		ASSERT_EQ(_vmem_ReadMem32(0x8C001000), 0x87654321u);
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
		ASSERT_EQ(_vmem_ReadMem32(0x8C001000), 0x87655678u);	// relies on value set in previous test
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
		ASSERT_EQ(_vmem_ReadMem32(0x8C001000), 0x87655699u);	// relies on value set in 2 previous tests
		ASSERT_EQ(r(12), 0x99999999u);
		ASSERT_EQ(r(11), 0x8C000800u);
		ASSERT_EQ(r(0), 0x00000800u);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		r(0) = 0xabcdef01;
		PrepareOp(0xc200 | Imm8(0x10));			// mov.l R0, @(disp, GBR)
		RunOp();
		ASSERT_EQ(_vmem_ReadMem32(0x8C000840), 0xabcdef01u);
		ASSERT_EQ(gbr(), 0x8C000800u);
		ASSERT_EQ(r(0), 0xabcdef01u);
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		r(0) = 0x11117777;
		PrepareOp(0xc100 | Imm8(0x20));			// mov.w R0, @(disp, GBR)
		RunOp();
		ASSERT_EQ(_vmem_ReadMem32(0x8C000840), 0xabcd7777u);	// relies on value set in previous test
		AssertState();

		ClearRegs();
		gbr() = 0x8C000800;
		r(0) = 0x22222266;
		PrepareOp(0xc000 | Imm8(0x40));			// mov.b R0, @(disp, GBR)
		RunOp();
		ASSERT_EQ(_vmem_ReadMem32(0x8C000840), 0xabcd7766u);	// relies on value set in 2 previous tests
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
		_vmem_WriteMem32(r(7), 4);
		r(8) = 0xAC002000;
		_vmem_WriteMem32(r(8), 3);
		PrepareOp(0x000f | Rn(7) | Rm(8));	// mac.l @Rm+, @Rn+
		RunOp();
		ASSERT_EQ(mac(), 12ull);
		ASSERT_EQ(r(7), 0xAC001004u);
		ASSERT_EQ(r(8), 0xAC002004u);

		_vmem_WriteMem32(r(7), -5);
		_vmem_WriteMem32(r(8), 7);
		RunOp();
		ASSERT_EQ(mac(), -23ull);
		ASSERT_EQ(r(7), 0xAC001008u);
		ASSERT_EQ(r(8), 0xAC002008u);
		AssertState();

		ClearRegs();
		r(7) = 0xAC001000;
		_vmem_WriteMem32(r(7), (u16)-7);
		r(8) = 0xAC002000;
		_vmem_WriteMem32(r(8), 3);
		PrepareOp(0x400f | Rn(7) | Rm(8));	// mac.w @Rm+, @Rn+
		RunOp();
		ASSERT_EQ(mac(), -21ull);
		ASSERT_EQ(r(7), 0xAC001002u);
		ASSERT_EQ(r(8), 0xAC002002u);

		_vmem_WriteMem16(r(7), 5);
		_vmem_WriteMem16(r(8), 7);
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
};
