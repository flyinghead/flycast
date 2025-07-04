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

#if FEAT_SHREC == DYNAREC_JITLESS

class JitlessDynarecTest : public Sh4OpTest {
protected:
	void SetUp() override
	{
		if (!addrspace::reserve())
			die("addrspace::reserve failed");
		emu.init();
		mem_map_default();
		dc_reset(true);
		ctx = &p_sh4rcb->cntx;
		Get_Sh4Recompiler(&this->sh4);
		printf("[JITLESS TEST] Using jitless dynarec\n");
	}
	void PrepareOp(u16 op, u16 op2 = 0, u16 op3 = 0) override
	{
		ctx->pc = START_PC;

		// Always populate three 16-bit words so the dynarec has deterministic padding.
		addrspace::write16(ctx->pc, op);
		addrspace::write16(ctx->pc + 2, op2 != 0 ? op2 : 0x0009); // NOP if unused
		addrspace::write16(ctx->pc + 4, op3 != 0 ? op3 : 0x0009); // NOP if unused
		
		printf("[JITLESS TEST] PrepareOp: PC=0x%08X op=0x%04X op2=0x%04X op3=0x%04X\n", 
		       ctx->pc, op, op2, op3);
	}
	
	void RunOp(int numOp = 1) override
	{
		printf("[JITLESS TEST] RunOp: Starting execution of %d ops, PC=0x%08X\n", numOp, ctx->pc);
		printf("[JITLESS TEST] PRE-RUN: r[15]=0x%08X next_pc=0x%08X\n", ctx->r[15], next_pc);
		
		ctx->pc = START_PC;
		next_pc = START_PC;
		
		for (int i = 0; i < numOp; i++) {
			uint32_t initial_pc = ctx->pc;
			printf("[JITLESS TEST] Step %d: PC=0x%08X next_pc=0x%08X\n", i, ctx->pc, next_pc);
			
			// Check for suspicious addresses before execution
			if (next_pc >= 0x9F000000 && next_pc <= 0x9FFFFFFF) {
				printf("[JITLESS TEST] 🚨 CORRUPTION DETECTED BEFORE STEP: next_pc=0x%08X\n", next_pc);
				FAIL() << "PC corruption detected before step execution";
			}
			
			if (next_pc > 0xC0000000) {
				printf("[JITLESS TEST] 🚨 INVALID PC DETECTED BEFORE STEP: next_pc=0x%08X\n", next_pc);
				FAIL() << "Invalid PC detected before step execution";
			}
			
			sh4.Step();
			
			printf("[JITLESS TEST] Step %d complete: PC=0x%08X next_pc=0x%08X\n", i, ctx->pc, next_pc);
			
			// Check for corruption after execution
			if (next_pc >= 0x9F000000 && next_pc <= 0x9FFFFFFF) {
				printf("[JITLESS TEST] 🚨 CORRUPTION DETECTED AFTER STEP: next_pc=0x%08X\n", next_pc);
				FAIL() << "PC corruption detected after step execution";
			}
			
			if (next_pc > 0xC0000000) {
				printf("[JITLESS TEST] 🚨 INVALID PC DETECTED AFTER STEP: next_pc=0x%08X\n", next_pc);
				FAIL() << "Invalid PC detected after step execution";
			}
			
			// Safety check for reasonable execution
			if (ctx->pc == initial_pc && i < numOp - 1) {
				printf("[JITLESS TEST] WARNING: PC didn't advance (PC=%08X), continuing\n", ctx->pc);
			}
		}
		
		printf("[JITLESS TEST] POST-RUN: r[15]=0x%08X next_pc=0x%08X\n", ctx->r[15], next_pc);
	}
};

TEST_F(JitlessDynarecTest, SimpleMovTest)
{
	printf("[JITLESS TEST] Testing simple mov instruction with jitless dynarec\n");
	
	ClearRegs();
	r(1) = 0x12345678;
	PrepareOp(0x6003 | Rm(1) | Rn(2));	// mov R1, R2
	RunOp();
	ASSERT_EQ(r(2), 0x12345678u);
	ASSERT_EQ(r(1), 0x12345678u);
	AssertState();
}

TEST_F(JitlessDynarecTest, SimpleArithmeticTest)
{
	printf("[JITLESS TEST] Testing simple arithmetic with jitless dynarec\n");
	
	ClearRegs();
	r(1) = 10;
	r(2) = 5;
	PrepareOp(0x300c | Rm(2) | Rn(1));	// add R2, R1
	RunOp();
	ASSERT_EQ(r(1), 15u);
	ASSERT_EQ(r(2), 5u);
	AssertState();
}

TEST_F(JitlessDynarecTest, MultipleOpsTest)
{
	printf("[JITLESS TEST] Testing multiple operations to trigger potential corruption\n");
	
	ClearRegs();
	r(1) = 0x1000;
	r(2) = 0x2000;
	
	// Multiple operations that might trigger corruption
	PrepareOp(0x6003 | Rm(1) | Rn(3),  // mov R1, R3
	          0x300c | Rm(2) | Rn(3),  // add R2, R3  
	          0x0009);                 // nop
	RunOp(3);
	
	ASSERT_EQ(r(3), 0x3000u); // 0x1000 + 0x2000
	AssertState();
}

TEST_F(JitlessDynarecTest, StressTest)
{
	printf("[JITLESS TEST] Running stress test to trigger corruption\n");
	
	// Run many simple operations to see if we can trigger corruption
	for (int i = 0; i < 100; i++) {
		ClearRegs();
		r(1) = i;
		r(2) = i * 2;
		
		PrepareOp(0x300c | Rm(2) | Rn(1),  // add R2, R1
		          0x6003 | Rm(1) | Rn(3),  // mov R1, R3
		          0x0009);                 // nop
		RunOp(3);
		
		ASSERT_EQ(r(1), (u32)(i + i * 2));
		ASSERT_EQ(r(3), (u32)(i + i * 2));
		
		if (i % 10 == 0) {
			printf("[JITLESS TEST] Completed %d iterations without corruption\n", i);
		}
	}
}

#endif // FEAT_SHREC == DYNAREC_JITLESS 