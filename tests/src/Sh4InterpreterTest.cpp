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
#include "sh4_ops.h"
#include "emulator.h"
#include "hw/sh4/sh4_mem.h"

class Sh4InterpreterTest : public Sh4OpTest {
protected:
	void SetUp() override {
		if (!_vmem_reserve())
			die("_vmem_reserve failed");
		emu.init();
		mem_map_default();
		dc_reset(true);
		ctx = &p_sh4rcb->cntx;
		Get_Sh4Interpreter(&sh4);
	}
	void PrepareOp(u16 op, u16 op2 = 0, u16 op3 = 0) override
	{
		ctx->pc = START_PC;
		_vmem_WriteMem16(ctx->pc, op);
		if (op2 != 0)
			_vmem_WriteMem16(ctx->pc + 2, op2);
		if (op3 != 0)
			_vmem_WriteMem16(ctx->pc + 4, op3);
	}
	void RunOp(int numOp = 1) override
	{
		ctx->pc = START_PC;
		for (int i = 0; i < numOp; i++)
			sh4.Step();
	}
};

TEST_F(Sh4InterpreterTest, MovRmRnTest)
{
	Sh4OpTest::MovRmRnTest();
}
TEST_F(Sh4InterpreterTest, MovImmRnTest)
{
	Sh4OpTest::MovImmRnTest();
}
TEST_F(Sh4InterpreterTest, MovMiscTest)
{
	Sh4OpTest::MovMiscTest();
}
TEST_F(Sh4InterpreterTest, LoadTest)
{
	Sh4OpTest::LoadTest();
}
TEST_F(Sh4InterpreterTest, LoadTest2)
{
	Sh4OpTest::LoadTest2();
}
TEST_F(Sh4InterpreterTest, StoreTest)
{
	Sh4OpTest::StoreTest();
}
TEST_F(Sh4InterpreterTest, StoreTest2)
{
	Sh4OpTest::StoreTest2();
}
TEST_F(Sh4InterpreterTest, ArithmeticTest)
{
	Sh4OpTest::ArithmeticTest();
}
TEST_F(Sh4InterpreterTest, MulDivTest)
{
	Sh4OpTest::MulDivTest();
}
TEST_F(Sh4InterpreterTest, CmpTest)
{
	Sh4OpTest::CmpTest();
}
TEST_F(Sh4InterpreterTest, StatusRegTest)
{
	Sh4OpTest::StatusRegTest();
}
