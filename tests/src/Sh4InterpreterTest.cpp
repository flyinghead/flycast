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
#include "hw/sh4/ir/sh4_ir_interpreter.h"

class Sh4InterpreterTest : public Sh4OpTest {
protected:
	void SetUp() override
	{
		if (!addrspace::reserve())
			die("addrspace::reserve failed");
		emu.init();
		mem_map_default();
		dc_reset(true);
		ctx = &p_sh4rcb->cntx;
		#ifdef ENABLE_SH4_CACHED_IR
			::sh4::ir::Get_Sh4Interpreter(&this->sh4);
		#else
			::Get_Sh4Interpreter(&this->sh4);
		#endif
	}
	void PrepareOp(u16 op, u16 op2 = 0, u16 op3 = 0) override
	{
		ctx->pc = START_PC;

		// Always populate three 16-bit words so the interpreter has deterministic padding.
		addrspace::write16(ctx->pc, op);
		addrspace::write16(ctx->pc + 2, op2 != 0 ? op2 : 0x0009); // NOP if unused
		addrspace::write16(ctx->pc + 4, op3 != 0 ? op3 : 0x0009); // NOP if unused
	}
		void RunOp(int numOp = 1) override
	{
		ctx->pc = START_PC;
		for (int i = 0; i < numOp; i++) {
			uint32_t initial_pc = ctx->pc;

			// For self-modifying code scenarios, we may need to retry
			// if cache invalidation occurs
			int retries = 0;
			#ifdef ENABLE_SH4_CACHED_IR
			const int max_retries = 2;
			#else
			const int max_retries = 0;
			#endif

			do {
				sh4.Step();
				retries++;

				// If we're still at the initial PC after stepping, retry
				if (ctx->pc == initial_pc && retries < max_retries) {
					printf("[RunOp] PC didn't advance (PC=%08X), retrying Step() for cache invalidation (attempt %d)\n", ctx->pc, retries);
					continue;
				}
				break;
			} while (retries < max_retries);
		}
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
	// Add debug output after ArithmeticTest to check register and context

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
TEST_F(Sh4InterpreterTest, FloatingPointTest)
{
	Sh4OpTest::FloatingPointTest();
}
TEST_F(Sh4InterpreterTest, DoubleFloatingPointTest)
{
	Sh4OpTest::DoubleFloatingPointTest();
}
