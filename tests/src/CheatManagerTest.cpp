#include "gtest/gtest.h"
#include "types.h"
#include "cfg/ini.h"
#include "cfg/cfg.h"
#include "cheats.h"
#include "emulator.h"
#include "hw/sh4/sh4_mem.h"

class CheatManagerTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!_vmem_reserve())
			die("_vmem_reserve failed");
		emu.init();
	}
};

TEST_F(CheatManagerTest, TestLoad)
{
	FILE *fp = fopen("test.cht", "w");
	const char *s = R"(
cheat0_address = "1234"
cheat0_address_bit_position = "0"
cheat0_big_endian = "false"
cheat0_cheat_type = "1"
cheat0_code = ""
cheat0_desc = "widescreen"
cheat0_dest_address = "7890"
cheat0_enable = "false"
cheat0_handler = "1"
cheat0_memory_search_size = "5"
cheat0_repeat_add_to_address = "4"
cheat0_repeat_add_to_value = "16"
cheat0_repeat_count = "3"
cheat0_value = "5678"
cheats = "1"
)";
	fputs(s, fp);
	fclose(fp);

	CheatManager mgr;
	mgr.reset("TESTGAME");
	mgr.loadCheatFile("test.cht");
	ASSERT_EQ(1, (int)mgr.cheatCount());
	ASSERT_EQ("widescreen", mgr.cheatDescription(0));
	ASSERT_FALSE(mgr.cheatEnabled(0));
	mgr.enableCheat(0, true);
	ASSERT_TRUE(mgr.cheatEnabled(0));
	mgr.enableCheat(0, false);
	ASSERT_FALSE(mgr.cheatEnabled(0));

	ASSERT_EQ(1234u, mgr.cheats[0].address);
	ASSERT_EQ(5678u, mgr.cheats[0].value);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[0].type);
	ASSERT_EQ(32u, mgr.cheats[0].size);
	ASSERT_EQ(3u, mgr.cheats[0].repeatCount);
	ASSERT_EQ(4u, mgr.cheats[0].repeatAddressIncrement);
	ASSERT_EQ(16u, mgr.cheats[0].repeatValueIncrement);
	ASSERT_EQ(7890u, mgr.cheats[0].destAddress);
}

TEST_F(CheatManagerTest, TestGameShark)
{
	CheatManager mgr;
	mgr.reset("TESTGS1");
	mgr.addGameSharkCheat("cheat1", "00123456 deadc0d3");
	ASSERT_EQ("cheat1", mgr.cheats[0].description);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[0].type);
	ASSERT_EQ(0x123456u, mgr.cheats[0].address);
	ASSERT_EQ(0xdeadc0d3u, mgr.cheats[0].value);
	ASSERT_EQ(8u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);

	mgr.reset("TESTGS2");
	mgr.addGameSharkCheat("cheat2", " 01222222\ndeadc0d3 ");
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[0].type);
	ASSERT_EQ(0x222222u, mgr.cheats[0].address);
	ASSERT_EQ(0xdeadc0d3u, mgr.cheats[0].value);
	ASSERT_EQ(16u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);

	mgr.reset("TESTGS3");
	mgr.addGameSharkCheat("cheat3", "\n02333333 \t baadf00d\n");
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[0].type);
	ASSERT_EQ(0x333333u, mgr.cheats[0].address);
	ASSERT_EQ(0xbaadf00du, mgr.cheats[0].value);
	ASSERT_EQ(32u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);

	mgr.reset("TESTGS4");
	mgr.addGameSharkCheat("cheat4", "\n03010042 \t 0c444444\n");
	ASSERT_EQ(Cheat::Type::increase, mgr.cheats[0].type);
	ASSERT_EQ(0x444444u, mgr.cheats[0].address);
	ASSERT_EQ(0x42u, mgr.cheats[0].value);
	ASSERT_EQ(8u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);

	mgr.reset("TESTGS5");
	mgr.addGameSharkCheat("cheat5", "\n03041984 \t 0c555555\n");
	ASSERT_EQ(Cheat::Type::decrease, mgr.cheats[0].type);
	ASSERT_EQ(0x555555u, mgr.cheats[0].address);
	ASSERT_EQ(0x1984u, mgr.cheats[0].value);
	ASSERT_EQ(16u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);

	mgr.reset("TESTGS6");
	mgr.addGameSharkCheat("cheat6", "03000003 0c666666 11111111 22222222 33333333");
	ASSERT_EQ(3u, mgr.cheats.size());
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[0].type);
	ASSERT_EQ(0x666666u, mgr.cheats[0].address);
	ASSERT_EQ(0x11111111u, mgr.cheats[0].value);
	ASSERT_EQ(32u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[1].type);
	ASSERT_EQ(0x66666au, mgr.cheats[1].address);
	ASSERT_EQ(0x22222222u, mgr.cheats[1].value);
	ASSERT_EQ(32u, mgr.cheats[1].size);
	ASSERT_EQ(1u, mgr.cheats[1].repeatCount);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[2].type);
	ASSERT_EQ(0x66666eu, mgr.cheats[2].address);
	ASSERT_EQ(0x33333333u, mgr.cheats[2].value);
	ASSERT_EQ(32u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);

	mgr.reset("TESTGS7");
	mgr.addGameSharkCheat("cheat7", "04aaaaaa 00020002 11111111");
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[0].type);
	ASSERT_EQ(0xaaaaaau, mgr.cheats[0].address);
	ASSERT_EQ(0x11111111u, mgr.cheats[0].value);
	ASSERT_EQ(32u, mgr.cheats[0].size);
	ASSERT_EQ(2u, mgr.cheats[0].repeatCount);
	ASSERT_EQ(2u, mgr.cheats[0].repeatAddressIncrement);
	ASSERT_EQ(0u, mgr.cheats[0].repeatValueIncrement);

	mgr.reset("TESTGS8");
	mgr.addGameSharkCheat("cheat8", "05bbbbbb 8ccccccc 00000010");
	ASSERT_EQ(Cheat::Type::copy, mgr.cheats[0].type);
	ASSERT_EQ(0xbbbbbbu, mgr.cheats[0].address);
	ASSERT_EQ(8u, mgr.cheats[0].size);
	ASSERT_EQ(0x10u, mgr.cheats[0].repeatCount);
	ASSERT_EQ(0xccccccu, mgr.cheats[0].destAddress);

	mgr.reset("TESTGS9");
	mgr.addGameSharkCheat("cheat9", "0d123456 0000ffff");
	ASSERT_EQ(Cheat::Type::runNextIfEq, mgr.cheats[0].type);
	ASSERT_EQ(0x123456u, mgr.cheats[0].address);
	ASSERT_EQ(0xffffu, mgr.cheats[0].value);
	ASSERT_EQ(16u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);

	mgr.reset("TESTGS10");
	mgr.addGameSharkCheat("cheat10", "0d654321" "00031984");
	ASSERT_EQ(Cheat::Type::runNextIfGt, mgr.cheats[0].type);
	ASSERT_EQ(0x654321u, mgr.cheats[0].address);
	ASSERT_EQ(0x1984u, mgr.cheats[0].value);
	ASSERT_EQ(16u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);

	mgr.reset("TESTGS11");
	mgr.addGameSharkCheat("cheat11", "0e021111 00123456 02222222 22222222 01111111 00001111");
	ASSERT_EQ(Cheat::Type::runNextIfEq, mgr.cheats[0].type);
	ASSERT_EQ(0x123456u, mgr.cheats[0].address);
	ASSERT_EQ(0x1111u, mgr.cheats[0].value);
	ASSERT_EQ(16u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[1].type);
	ASSERT_EQ(0x222222u, mgr.cheats[1].address);
	ASSERT_EQ(0x22222222u, mgr.cheats[1].value);
	ASSERT_EQ(32u, mgr.cheats[1].size);
	ASSERT_EQ(1u, mgr.cheats[1].repeatCount);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[2].type);
	ASSERT_EQ(0x111111u, mgr.cheats[2].address);
	ASSERT_EQ(0x1111u, mgr.cheats[2].value);
	ASSERT_EQ(16u, mgr.cheats[2].size);
	ASSERT_EQ(1u, mgr.cheats[2].repeatCount);
}


TEST_F(CheatManagerTest, TestGameSharkError)
{
	CheatManager mgr;
	EXPECT_THROW(mgr.addGameSharkCheat("error", "00"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", "001234560000000"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", "00 123456 00000000"), FlycastException);

	EXPECT_THROW(mgr.addGameSharkCheat("error", "00123456 "), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 01123456 "), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 02123456 "), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 03000002 00123456 00000001"), FlycastException);

	EXPECT_THROW(mgr.addGameSharkCheat("error", " 03013456"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 03023456"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 03033456"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 03043456"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 03050000 8c123456"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 03060000 8c123456"), FlycastException);

	EXPECT_THROW(mgr.addGameSharkCheat("error", " 04654321 00030004"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 05654321 8c111111"), FlycastException);

	EXPECT_THROW(mgr.addGameSharkCheat("error", " 0d0FFFFF"), FlycastException);
	EXPECT_THROW(mgr.addGameSharkCheat("error", " 0e0FFFFF"), FlycastException);
}

TEST_F(CheatManagerTest, TestSave)
{
	CheatManager mgr;
	mgr.reset("TESTSAVE");
	mgr.addGameSharkCheat("cheat1", "00010000 000000d3");
	mgr.addGameSharkCheat("cheat2", "01020000 0000c0d3");
	mgr.addGameSharkCheat("cheat3", "02030000 deadc0d3");
	mgr.addGameSharkCheat("cheat4", "03000004 8c040000 00000001 00000002 00000003 00000004");
	mgr.addGameSharkCheat("cheat5", "030100ff 8c050000");
	mgr.addGameSharkCheat("cheat6", "0304ffff 8c060000");

	mgr.loadCheatFile(cfgLoadStr("cheats", "TESTSAVE", ""));
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[0].type);
	ASSERT_EQ(0x010000u, mgr.cheats[0].address);
	ASSERT_EQ(0xd3u, mgr.cheats[0].value);
	ASSERT_EQ(8u, mgr.cheats[0].size);
	ASSERT_EQ(1u, mgr.cheats[0].repeatCount);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[1].type);
	ASSERT_EQ(0x020000u, mgr.cheats[1].address);
	ASSERT_EQ(0xc0d3u, mgr.cheats[1].value);
	ASSERT_EQ(16u, mgr.cheats[1].size);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[2].type);
	ASSERT_EQ(0x030000u, mgr.cheats[2].address);
	ASSERT_EQ(0xdeadc0d3u, mgr.cheats[2].value);
	ASSERT_EQ(32u, mgr.cheats[2].size);

	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[3].type);
	ASSERT_EQ(0x040000u, mgr.cheats[3].address);
	ASSERT_EQ(1u, mgr.cheats[3].value);
	ASSERT_EQ(32u, mgr.cheats[3].size);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[4].type);
	ASSERT_EQ(0x040004u, mgr.cheats[4].address);
	ASSERT_EQ(2u, mgr.cheats[4].value);
	ASSERT_EQ(32u, mgr.cheats[4].size);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[5].type);
	ASSERT_EQ(0x040008u, mgr.cheats[5].address);
	ASSERT_EQ(3u, mgr.cheats[5].value);
	ASSERT_EQ(32u, mgr.cheats[5].size);
	ASSERT_EQ(Cheat::Type::setValue, mgr.cheats[6].type);
	ASSERT_EQ(0x04000cu, mgr.cheats[6].address);
	ASSERT_EQ(4u, mgr.cheats[6].value);
	ASSERT_EQ(32u, mgr.cheats[6].size);

	ASSERT_EQ(Cheat::Type::increase, mgr.cheats[7].type);
	ASSERT_EQ(0x050000u, mgr.cheats[7].address);
	ASSERT_EQ(0xffu, mgr.cheats[7].value);
	ASSERT_EQ(8u, mgr.cheats[7].size);
	ASSERT_EQ(Cheat::Type::decrease, mgr.cheats[8].type);
	ASSERT_EQ(0x060000u, mgr.cheats[8].address);
	ASSERT_EQ(0xffffu, mgr.cheats[8].value);
	ASSERT_EQ(16u, mgr.cheats[8].size);
}

TEST_F(CheatManagerTest, TestSubBytePatch)
{
	FILE *fp = fopen("test.cht", "w");
	const char *s = R"(
cheat0_address = "0x10000"
cheat0_address_bit_position = "0xF0"
cheat0_big_endian = "false"
cheat0_cheat_type = "1"
cheat0_desc = "patch 4 MSB bits"
cheat0_handler = "1"
cheat0_memory_search_size = "2"
cheat0_repeat_count = "1"
cheat0_value = "0xCF"
cheat1_address = "0x10001"
cheat1_address_bit_position = "0x03"
cheat1_big_endian = "false"
cheat1_cheat_type = "1"
cheat1_desc = "patch 2 LSB bits"
cheat1_handler = "1"
cheat1_memory_search_size = "1"
cheat1_repeat_count = "1"
cheat1_value = "0xFE"
cheats = "2"
)";
	fputs(s, fp);
	fclose(fp);

	CheatManager mgr;
	mgr.reset("TESTSUB8");
	mgr.loadCheatFile("test.cht");
	mem_map_default();
	dc_reset(true);

	mgr.enableCheat(0, true);
	WriteMem8_nommu(0x8c010000, 0xFA);
	mgr.apply();
	ASSERT_EQ(0xCA, ReadMem8_nommu(0x8c010000));
	WriteMem8_nommu(0x8c010000, 0);
	mgr.apply();
	ASSERT_EQ(0xC0, ReadMem8_nommu(0x8c010000));

	mgr.enableCheat(1, true);
	WriteMem8_nommu(0x8c010001, 0xC8);
	mgr.apply();
	ASSERT_EQ(0xCA, ReadMem8_nommu(0x8c010001));
	WriteMem8_nommu(0x8c010001, 0);
	mgr.apply();
	ASSERT_EQ(2, ReadMem8_nommu(0x8c010001));

}
