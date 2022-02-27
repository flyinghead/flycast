#include "gtest/gtest.h"
#include "types.h"
#include "hw/mem/_vmem.h"
#include "hw/arm7/arm7.h"
#include "hw/aica/aica_if.h"
#include "hw/arm7/arm7_rec.h"
#include "emulator.h"

extern bool Arm7Enabled;

static const u32 N_FLAG = 1 << 31;
static const u32 Z_FLAG = 1 << 30;
static const u32 C_FLAG = 1 << 29;
static const u32 V_FLAG = 1 << 28;
static const u32 NZCV_MASK = N_FLAG | Z_FLAG | C_FLAG | V_FLAG;


namespace aicaarm::recompiler {

extern void (*EntryPoints[])();

class AicaArmTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!_vmem_reserve())
			die("_vmem_reserve failed");
		emu.init();
		dc_reset(true);
		Arm7Enabled = true;
	}

	void PrepareOp(u32 op)
	{
		PrepareOps(1, &op);
	}

	void PrepareOps(int count, u32 *ops)
	{
		arm_Reg[R15_ARM_NEXT].I = 0x1000;
		for (int i = 0; i < count; i++)
			*(u32*)&aica_ram[0x1000 + i * 4] = ops[i];
		*(u32*)&aica_ram[0x1000 + count * 4] = 0xea000000 | ((u32)(-count * 4 - 8) << 2);	// b pc+8-12
		flush();
		compile();
	}

	void RunOp()
	{
		arm_Reg[R15_ARM_NEXT].I = 0x1000;
		arm_Reg[CYCL_CNT].I = 1;
		arm_mainloop(arm_Reg, EntryPoints);
	}
	void ResetNZCV()
	{
		arm_Reg[RN_PSR_FLAGS].I &= ~NZCV_MASK;
	}
};
#define ASSERT_NZCV_EQ(expected) ASSERT_EQ(arm_Reg[RN_PSR_FLAGS].I & NZCV_MASK, (expected));

TEST_F(AicaArmTest, ArithmeticOpsTest)
{
	PrepareOp(0xe0810002);	// add r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xbaad0000;
	arm_Reg[2].I = 0x0000cafe;
	arm_Reg[RN_PSR_FLAGS].I |= NZCV_MASK;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaadcafe);
	ASSERT_NZCV_EQ(NZCV_MASK);

	PrepareOp(0xe0810000);	// add r0, r1, r0
	arm_Reg[0].I = 11;
	arm_Reg[1].I = 22;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 33);

	PrepareOp(0xe0410002);	// sub r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xbaadcafe;
	arm_Reg[2].I = 0x0000cafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaad0000);
	ASSERT_NZCV_EQ(NZCV_MASK);

	PrepareOp(0xe0410000);	// sub r0, r1, r0
	arm_Reg[0].I = 11;
	arm_Reg[1].I = 22;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 11);

	PrepareOp(0xe0910002);	// adds r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x80000000;
	arm_Reg[2].I = 0x80000000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG | V_FLAG);	// Z,C,V

	PrepareOp(0xe0510002);	// subs r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xbaadcafe;
	arm_Reg[2].I = 0x0000cafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaad0000);
	ASSERT_NZCV_EQ(N_FLAG | C_FLAG);	// N,C
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xbaadcafe;
	arm_Reg[2].I = 0xbaadcafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG);	// Z,C
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x80000000;
	arm_Reg[2].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x7fffffff);
	ASSERT_NZCV_EQ(C_FLAG | V_FLAG);	// C,V
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xffffffff;
	arm_Reg[2].I = 0xffffffff;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG);	// Z,C

	PrepareOp(0xe0b10002);	// adcs r0, r1, r2
	arm_Reg[RN_PSR_FLAGS].I &= ~NZCV_MASK;
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 3);
	ASSERT_NZCV_EQ(0);	//
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG; // set C
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 4);
	ASSERT_NZCV_EQ(0);	//

	// from armwrestler
	PrepareOp(0xe0d22002);	// sbcs r2,r2,r2
	ResetNZCV();
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG;	// set C
	arm_Reg[2].I = 0xFFFFFFFF;
	RunOp();
	ASSERT_EQ(arm_Reg[2].I, 0);
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG);

	// from armwrestler
	PrepareOp(0xe2d22000);	// sbcs r2,r2,#0
	ResetNZCV();
	arm_Reg[2].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[2].I, -1);
	ASSERT_NZCV_EQ(N_FLAG);

	PrepareOp(0xe0d10002);	// sbcs r0, r1, r2
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, -2);
	ASSERT_NZCV_EQ(N_FLAG);	// N

	PrepareOp(0xe0710002);	// rsbs r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	ASSERT_NZCV_EQ(C_FLAG);	// C, confirmed by interpreter and online arm sim

	PrepareOp(0xe0f10002);	// rscs r0, r1, r2
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG);	// Z,C
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG; // C set
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	ASSERT_NZCV_EQ(C_FLAG);	// C

	PrepareOp(0xe1500001);	// cmp r0, r1
	ResetNZCV();
	arm_Reg[0].I = 2;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_NZCV_EQ(C_FLAG);	// C

	PrepareOp(0xe1700001);	// cmn r0, r1
	ResetNZCV();
	arm_Reg[0].I = 2;
	arm_Reg[1].I = -1;
	RunOp();
	ASSERT_NZCV_EQ(C_FLAG);	// C
}

TEST_F(AicaArmTest, LogicOpsTest)
{
	PrepareOp(0xe1a00001);	// mov r0, r1
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xbaadcafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaadcafe);

	PrepareOp(0xe3c100ff);	// bic r0, r1, 0xff
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xffff;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xff00);

	PrepareOp(0xe0010002);	// and r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xff0f;
	arm_Reg[2].I = 0xf0f0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xf000);

	PrepareOp(0xe0010000);	// and r0, r1, r0
	arm_Reg[0].I = 0xf0f0f0f0;
	arm_Reg[1].I = 0xffffffff;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xf0f0f0f0);

	PrepareOp(0xe1a00251);	// asr r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xfffffff8;
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xfffffffe);

	PrepareOp(0xe1a00241); // asr r0, r1, 4
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x0ffffff0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x00ffffff);

	PrepareOp(0xe0210002);	// eor r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0xffff0000;
	arm_Reg[2].I = 0xf0f0f0f0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x0f0ff0f0);

	PrepareOp(0xe0210000);	// eor r0, r1, r0
	arm_Reg[0].I = 0xf0f0f0f0;
	arm_Reg[1].I = 0xffffffff;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x0f0f0f0f);

	PrepareOp(0xe1810000);	// orr r0, r1, r0
	arm_Reg[0].I = 0xf0f0f0f0;
	arm_Reg[1].I = 0x0f0f0f0f;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xffffffff);

	PrepareOp(0xe1a00211);	// lsl r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 8;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x100);

	PrepareOp(0xe1a00231);	// lsr r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x100;
	arm_Reg[2].I = 4;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x10);

	PrepareOp(0xe1a00271);	// ror r0, r1, r2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x12345678;
	arm_Reg[2].I = 16;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x56781234);

	PrepareOp(0xe1300001);	// teq r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_NZCV_EQ(0);
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG | V_FLAG;	// set C,V
	arm_Reg[0].I = 1;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG | V_FLAG);	// Z,C,V

	PrepareOp(0xe1100001);	// tst r0, r1
	ResetNZCV();
	arm_Reg[0].I = 1;
	arm_Reg[1].I = 2;
	RunOp();
	ASSERT_NZCV_EQ(0x40000000);	// Z
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG | V_FLAG;	// set C,V
	arm_Reg[0].I = 3;
	arm_Reg[1].I = 2;
	RunOp();
	ASSERT_NZCV_EQ(C_FLAG | V_FLAG);	// C,V
}

TEST_F(AicaArmTest, Operand2ImmTest)
{
	PrepareOp(0xe2810003);	// add r0, r1, #3
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 4);

	PrepareOp(0xe2a10003);	// adc r0, r1, #3
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 5);

	PrepareOp(0xe2410003);	// sub r0, r1, #3
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 7;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 4);

	PrepareOp(0xe2610003);	// rsb r0, r1, #3
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 2);

	PrepareOp(0xe2c10003);	// sbc r0, r1, #3
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 10;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 6);

	PrepareOp(0xe2e10010);	// rsc r0, r1, #16
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 6;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 9);

	PrepareOp(0xe3500010);	// cmp r0, #16
	ResetNZCV();
	arm_Reg[0].I = 16;
	RunOp();
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG);

	PrepareOp(0xe3700001);	// cmn r0, #1
	ResetNZCV();
	arm_Reg[0].I = 0xffffffff;
	RunOp();
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG);

	PrepareOp(0xe2010001);	// and r0, r1, #1
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 3;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);

	PrepareOp(0xe3810001);	// orr r0, r1, #1
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 3);

	PrepareOp(0xe2210001);	// eor r0, r1, #1
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 3;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 2);

	PrepareOp(0xe3c10001);	// bic r0, r1, #1
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 3;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 2);

	PrepareOp(0xe3100001);	// tst r0, #1
	ResetNZCV();
	arm_Reg[0].I = 2;
	RunOp();
	ASSERT_NZCV_EQ(Z_FLAG);
	ResetNZCV();
	arm_Reg[0].I = 1;
	RunOp();
	ASSERT_NZCV_EQ(0);

	PrepareOp(0xe3300001);	// teq r0, #1
	ResetNZCV();
	arm_Reg[0].I = 1;
	RunOp();
	ASSERT_NZCV_EQ(Z_FLAG);
	ResetNZCV();
	arm_Reg[0].I = 2;
	RunOp();
	ASSERT_NZCV_EQ(0);

	PrepareOp(0xe2522001);	// subs r2, #1
	ResetNZCV();
	arm_Reg[2].I = 1;
	RunOp();
	ASSERT_NZCV_EQ(Z_FLAG | C_FLAG);
	ResetNZCV();
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_NZCV_EQ(C_FLAG);
	ResetNZCV();
	arm_Reg[2].I = 0;
	RunOp();
	ASSERT_NZCV_EQ(N_FLAG);
}

TEST_F(AicaArmTest, Operand2ShiftImmTest)
{
	PrepareOp(0xe0810202);	// add r0, r1, r2, LSL #4
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x21);

	PrepareOp(0xe0810142); // add r0, r1, r2, ASR #2
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = -8;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, -1);

	PrepareOp(0xe08100a2); // add r0, r1, r2, LSR #1
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x80000000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x40000001);

	PrepareOp(0xe0810862); // add r0, r1, r2, ROR #16
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x56771234;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x12345678);

	PrepareOp(0xe0810062);	// add r0, r1, r2, RRX
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x22222221;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x11111111);
	ASSERT_NZCV_EQ(0);	//

	PrepareOp(0xe1910062);	// orrs r0, r1, r2, RRX
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG;	// set C
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x22222221;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x91111111);
	ASSERT_NZCV_EQ(N_FLAG | C_FLAG); // N,C

	// When an Operand2 constant is used with the instructions MOVS, MVNS, ANDS, ORRS, ORNS, EORS, BICS, TEQ or TST, the carry flag is updated to bit[31] of the constant,
	// if the constant is greater than 255 and can be produced by shifting an 8-bit value.
	PrepareOp(0xe3b00102);	// movs r0, #0x80000000
	ResetNZCV();
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x80000000);
	ASSERT_NZCV_EQ(N_FLAG | C_FLAG); // N,C

	PrepareOp(0xe3f00000);	// mvns r0, #0
	arm_Reg[RN_PSR_FLAGS].I &= ~NZCV_MASK;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xffffffff);
	ASSERT_NZCV_EQ(N_FLAG); // N
}

TEST_F(AicaArmTest, Operand2RegShiftTest)
{
	PrepareOp(0xe1810312);	// orr r0, r1, r2, LSL r3
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x10;
	arm_Reg[3].I = 8;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x1001);

	PrepareOp(0xe1810352);	// orr r0, r1, r2, ASR r3
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x80000000;
	arm_Reg[3].I = 30;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xffffffff);

	PrepareOp(0xe1810332);	// orr r0, r1, r2, LSR r3
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x80000000;
	arm_Reg[3].I = 30;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 3);

	PrepareOp(0xe1810372);	// orr r0, r1, r2, ROR r3
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x43208765;
	arm_Reg[3].I = 16;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x87654321);

	PrepareOp(0xe1b0431f);	// movs r4, r15, lsl r3
	ResetNZCV();
	arm_Reg[3].I = 0;
	arm_Reg[4].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[4].I, 0x1000 + 12);
	ASSERT_NZCV_EQ(0);

	PrepareOp(0xe1b0400f);	// movs r4, r15, lsl #0
	ResetNZCV();
	arm_Reg[3].I = 0;
	arm_Reg[4].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[4].I, 0x1000 + 8);
	ASSERT_NZCV_EQ(0);
}

TEST_F(AicaArmTest, CarryTest)
{
	PrepareOp(0xe1910022); // orrs r0, r1, r2, LSR #32
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x80000000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	ASSERT_NZCV_EQ(C_FLAG); // C

	PrepareOp(0xe1910822); // orrs r0, r1, r2, LSR #16
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x80008000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x8001);
	ASSERT_NZCV_EQ(C_FLAG); // C

	PrepareOp(0xe1910042); // orrs r0, r1, r2, ASR #32
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x80000000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xffffffff);
	ASSERT_NZCV_EQ(N_FLAG | C_FLAG); // N,C

	PrepareOp(0xe1910842); // orrs r0, r1, r2, ASR #16
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x00008000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	ASSERT_NZCV_EQ(C_FLAG); // C
	arm_Reg[RN_PSR_FLAGS].I &= ~NZCV_MASK;
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x00004000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	ASSERT_NZCV_EQ(0); //

	PrepareOp(0xe1910802); // orrs r0, r1, r2, LSL #16
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	arm_Reg[2].I = 0x00010001;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x10001);
	ASSERT_NZCV_EQ(C_FLAG); // C
}

TEST_F(AicaArmTest, MemoryTest)
{
	PrepareOp(0xe5910004); // ldr r0, [r1, #4]
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x10000;
	*(u32*)&aica_ram[0x10004] = 0xbaadcafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaadcafe);

	PrepareOp(0xe5010008); // str r0, [r1, #-8]
	arm_Reg[0].I = 0xbaadcafe;
	arm_Reg[1].I = 0x10008;
	*(u32*)&aica_ram[0x10000] = 0;
	RunOp();
	ASSERT_EQ(*(u32*)&aica_ram[0x10000], 0xbaadcafe);

	PrepareOp(0xe5b10004);	// ldr r0, [r1, #4]!
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x10000;
	*(u32*)&aica_ram[0x10004] = 0xbaadcafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaadcafe);
	ASSERT_EQ(arm_Reg[1].I, 0x10004);

	PrepareOp(0xe4110004);	// ldr r0, [r1], #-4
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x10004;
	*(u32*)&aica_ram[0x10004] = 0xbaadcafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaadcafe);
	ASSERT_EQ(arm_Reg[1].I, 0x10000);

	PrepareOp(0xe4900004);	// ldr r0, [r0], #4
	arm_Reg[0].I = 0x10004;
	*(u32*)&aica_ram[0x10004] = 0xbaadcafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaadcafe);

	PrepareOp(0xe5b00004);	// ldr r0, [r0, #4]!
	arm_Reg[0].I = 0x10000;
	*(u32*)&aica_ram[0x10000] = 0xbaadcafe;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0xbaadcafe);

	PrepareOp(0xe51f0004);	// ldr r0, [r15, #-4]
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);

	PrepareOp(0xe79000a4);	// ldr r0, [r0, r4, LSR #1]
	arm_Reg[0].I = 0x1003;
	arm_Reg[4].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);

	PrepareOp(0xe7900084);	// ldr r0, [r0, r4, LSL #1]
	arm_Reg[0].I = 0x1002;
	arm_Reg[4].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);

	PrepareOp(0xe7900024);	// ldr r0, [r0, r4, LSR #32]
	arm_Reg[0].I = 0x1004;
	arm_Reg[4].I = 0x12345678;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[4].I, 0x12345678);

	PrepareOp(0xe7900044);	// ldr r0, [r0, r4, ASR #32]
	arm_Reg[0].I = 0x1005;
	arm_Reg[4].I = 0x80000000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[4].I, 0x80000000);

	PrepareOp(0xe7920002);	// ldr r0,[r2,r2]
	arm_Reg[0].I = 5;
	arm_Reg[2].I = 0x1004 / 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[2].I, 0x1004 / 2);

	PrepareOp(0xe7920063);	// ldr r0,[r2,r3, rrx]
	ResetNZCV();
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG;	// set C
	arm_Reg[0].I = 5;
	arm_Reg[2].I = 0x1006;
	arm_Reg[3].I = -4;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[2].I, 0x1006);
	ASSERT_EQ(arm_Reg[3].I, -4);

	// unaligned read
	PrepareOp(0xe7900002);	// ldr r0,[r0,r2]
	arm_Reg[0].I = 0x1004;
	arm_Reg[2].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, (*(u32*)&aica_ram[0x1004]) >> 16 | (*(u32*)&aica_ram[0x1004]) << 16);
	ASSERT_EQ(arm_Reg[2].I, 2);

	PrepareOp(0xe69000a4);	// ldr r0,[r0],r4, lsr #1
	arm_Reg[0].I = 0x1004;
	arm_Reg[4].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[4].I, 2);

	PrepareOp(0xe6920104);	// ldr r0,[r2],r4, lsl #2
	arm_Reg[0].I = 0;
	arm_Reg[2].I = 0x1004;
	arm_Reg[4].I = 2;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[2].I, 0x1004 + 8);
	ASSERT_EQ(arm_Reg[4].I, 2);

	PrepareOp(0xe6920000);	// ldr r0,[r2],r0
	arm_Reg[0].I = 123;
	arm_Reg[2].I = 0x1004;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[2].I, 0x1004 + 123);

	PrepareOp(0xe6920043);	// ldr r0,[r2],r3, asr #32
	arm_Reg[0].I = 0;
	arm_Reg[2].I = 0x1004;
	arm_Reg[3].I = 0xc0000000;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[2].I, 0x1004 - 1);
	ASSERT_EQ(arm_Reg[3].I, 0xc0000000);

	PrepareOp(0xe6920063);	// ldr r0,[r2],r3, rrx
	ResetNZCV();
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG;	// set C
	arm_Reg[0].I = 5;
	arm_Reg[2].I = 0x1004;
	arm_Reg[3].I = 0xfffffffc;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, *(u32*)&aica_ram[0x1004]);
	ASSERT_EQ(arm_Reg[2].I, 0x1002);
	ASSERT_NZCV_EQ(C_FLAG); // C
	ASSERT_EQ(arm_Reg[3].I, 0xfffffffc);

	// unaligned read
	PrepareOp(0xe6900002);	// ldr r0,[r0],r2
	arm_Reg[0].I = 0x1006;
	arm_Reg[2].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, (*(u32*)&aica_ram[0x1004]) >> 16 | (*(u32*)&aica_ram[0x1004]) << 16);
	ASSERT_EQ(arm_Reg[2].I, 1);

	// conditional with write-back, false condition
	PrepareOp(0x04910004);	// ldreq r0, [r1], #4
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 0x1004;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	ASSERT_EQ(arm_Reg[1].I, 0x1004);

}

TEST_F(AicaArmTest, PcRelativeTest)
{
	PrepareOp(0xe38f0010);	// orr r0, r15, #16
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x1008 + 16);

	PrepareOp(0xe180011f); // orr r0, r15, LSL r1
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0x100C << 1);

	PrepareOp(0xe28f5c7d); // add r5, r15, #32000
	arm_Reg[5].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[5].I, 32000 + 0x1008);
}

TEST_F(AicaArmTest, ConditionalTest)
{
	PrepareOp(0x01a00001);	// moveq r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= Z_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);

	PrepareOp(0x11a00001);	// movne r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[RN_PSR_FLAGS].I |= Z_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);

	PrepareOp(0x21a00001);	// movcs r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);

	PrepareOp(0x31a00001);	// movcc r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);

	PrepareOp(0x41a00001);	// movmi r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= N_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);

	PrepareOp(0x51a00001);	// movpl r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[RN_PSR_FLAGS].I |= N_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);

	PrepareOp(0x61a00001);	// movvs r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= V_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);

	PrepareOp(0x71a00001);	// movvc r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[RN_PSR_FLAGS].I |= V_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);

	PrepareOp(0x81a00001);	// movhi r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[RN_PSR_FLAGS].I |= Z_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);

	PrepareOp(0x91a00001);	// movls r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[RN_PSR_FLAGS].I |= C_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I = Z_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);

	PrepareOp(0xa1a00001);	// movge r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[RN_PSR_FLAGS].I |= N_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= V_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);

	PrepareOp(0xb1a00001);	// movlt r0, r1
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= N_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[RN_PSR_FLAGS].I |= V_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);

	PrepareOp(0xc1a00001);	// movgt r0, r1
	// Z==0 && N==V
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	ResetNZCV();
	arm_Reg[RN_PSR_FLAGS].I |= Z_FLAG;
	arm_Reg[0].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	ResetNZCV();
	arm_Reg[RN_PSR_FLAGS].I |= V_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= N_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);

	PrepareOp(0xd1a00001);	// movle r0, r1
	// Z==1 || N!=V
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[1].I = 1;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
	arm_Reg[RN_PSR_FLAGS].I |= Z_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	ResetNZCV();
	arm_Reg[0].I = 0;
	arm_Reg[RN_PSR_FLAGS].I |= V_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 1);
	arm_Reg[0].I = 0;
	arm_Reg[RN_PSR_FLAGS].I |= N_FLAG;
	RunOp();
	ASSERT_EQ(arm_Reg[0].I, 0);
}

TEST_F(AicaArmTest, JumpTest)
{
	PrepareOp(0xea00003e);	// b +248
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0x1100);

	PrepareOp(0xeb00003e);	// bl +248
	arm_Reg[14].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0x1100);
	ASSERT_EQ(arm_Reg[14].I, 0x1004);

	PrepareOp(0xe1a0f000);	// mov pc, r0
	arm_Reg[0].I = 0x100;
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0x100);

	PrepareOp(0xc1a0f000);	// movgt pc, r0
	ResetNZCV();
	arm_Reg[RN_PSR_FLAGS].I |= N_FLAG;
	arm_Reg[0].I = 0x100;
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0x1004);
	ResetNZCV();
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0x100);

	PrepareOp(0xe590f000); // ldr r15, [r0]
	arm_Reg[0].I = 0x10000;
	*(u32*)&aica_ram[0x10000] = 0xbaadcafc;
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0xbaadcafc);

	PrepareOp(0x1b00003e);	// blne +248
	ResetNZCV();
	arm_Reg[14].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0x1100);
	ASSERT_EQ(arm_Reg[14].I, 0x1004);
	ResetNZCV();
	arm_Reg[RN_PSR_FLAGS].I |= Z_FLAG;
	arm_Reg[14].I = 0;
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0x1004);
	ASSERT_EQ(arm_Reg[14].I, 0);
}

TEST_F(AicaArmTest, LdmStmTest)
{
	PrepareOp(0xe8bd8000);	// ldm sp!, {pc}
	arm_Reg[13].I = 0x1100;
	*(u32*)&aica_ram[0x1100] = 0x1234;
	RunOp();
	ASSERT_EQ(arm_Reg[R15_ARM_NEXT].I, 0x1234);

	PrepareOp(0xe92d8000);	// stmdb sp!, {pc}
	arm_Reg[13].I = 0x1104;
	RunOp();
	ASSERT_EQ(arm_Reg[13].I, 0x1100);
	ASSERT_EQ(*(u32*)&aica_ram[0x1100], 0x1000 + 12);
}

TEST_F(AicaArmTest, RegAllocTest)
{
	u32 ops[] = {
			0xe3a00000,	// mov r0, #0
			0xe3a01001,	// mov r1, #1
			0xe3a02002,	// mov r2, #2
			0xe3a03003,	// mov r3, #3
			0xe3a04004,	// mov r4, #4
			0xe3a05005,	// mov r5, #5
			0xe3a06006,	// mov r6, #6
			0xe0800001,	// add r0, r0, r1
			0xe0811002,	// add r1, r1, r2
			0xe0822003,	// add r2, r2, r3
			0xe0833004,	// add r3, r3, r4
			0xe0844005,	// add r4, r4, r5
			0xe0855006,	// add r5, r5, r6
			0xe0866000,	// add r6, r6, r0
	};
	PrepareOps(ARRAY_SIZE(ops), ops);
	for (int i = 0; i < 15; i++)
		arm_Reg[i].I = 0;

	RunOp();

	ASSERT_EQ(arm_Reg[0].I, 1);
	ASSERT_EQ(arm_Reg[1].I, 3);
	ASSERT_EQ(arm_Reg[2].I, 5);
	ASSERT_EQ(arm_Reg[3].I, 7);
	ASSERT_EQ(arm_Reg[4].I, 9);
	ASSERT_EQ(arm_Reg[5].I, 11);
	ASSERT_EQ(arm_Reg[6].I, 7);
}

TEST_F(AicaArmTest, ConditionRegAllocTest)
{
	u32 ops1[] = {
			0x03a0004d,	// moveq r0, #77
			0xe1a01000	// mov r1, r0
	};
	PrepareOps(ARRAY_SIZE(ops1), ops1);
	arm_Reg[0].I = 22;
	arm_Reg[1].I = 22;
	ResetNZCV();

	RunOp();

	ASSERT_EQ(arm_Reg[0].I, 22);
	ASSERT_EQ(arm_Reg[1].I, 22);

	u32 ops2[] = {
			0x01a01000,	// moveq r1, r0
			0xe1a02000	// mov r2, r0
	};
	PrepareOps(ARRAY_SIZE(ops2), ops2);
	arm_Reg[0].I = 22;
	arm_Reg[1].I = 0;
	arm_Reg[2].I = 0;
	ResetNZCV();

	RunOp();

	ASSERT_EQ(arm_Reg[0].I, 22);
	ASSERT_EQ(arm_Reg[1].I, 0);
	ASSERT_EQ(arm_Reg[2].I, 22);
}
}
