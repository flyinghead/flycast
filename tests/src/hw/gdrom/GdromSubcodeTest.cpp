#include "gtest/gtest.h"
#include "imgread/common.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/gdrom/gdromv3.h"

extern Disc *disc;

class GdromSubcodeTest : public ::testing::Test {
protected:
	void load(const char *path)
	{
		disc = OpenDisc(path);
		ASSERT_NE(nullptr, disc);
		cdda.status = cdda_t::NoInfo;
	}

	void TearDown() override
	{
		delete disc;
		disc = nullptr;
	}

	u8 reply[100] {};
};

TEST_F(GdromSubcodeTest, MediaCatalogNumber)
{
	load(FLYCAST_TEST_FILES "/test_cues/subcode/subcode.cue");

	ASSERT_EQ(0x18u, gd_get_subcode(2, 150, reply));
	ASSERT_EQ(0x15, reply[1]);	// No audio status
	ASSERT_EQ(2, reply[4]);		// Format code
	ASSERT_EQ(0x80, reply[8]);	// MCVal
	ASSERT_EQ(0, memcmp(&reply[9], "0731453345927", 13));
}

TEST_F(GdromSubcodeTest, NoMediaCatalogNumber)
{
	load(FLYCAST_TEST_FILES "/test_cues/e/cs.cue");

	ASSERT_EQ(0x18u, gd_get_subcode(2, 150, reply));
	ASSERT_EQ(0, reply[8]);		// MCVal
	ASSERT_EQ(0, memcmp(&reply[9], "0000000000000", 13));
}

TEST_F(GdromSubcodeTest, Isrc)
{
	load(FLYCAST_TEST_FILES "/test_cues/subcode/subcode.cue");

	// Audio track with ISRC USPR39640267
	ASSERT_EQ(0x18u, gd_get_subcode(3, 150, reply));
	ASSERT_EQ(0x15, reply[1]);	// No audio status
	ASSERT_EQ(3, reply[4]);		// Format code
	ASSERT_EQ(0x80, reply[8]);	// TCVal
	const u8 isrc[12] = {
		0x25, 0x23, 0x20, 0x22, 0x03,				// U S P R 3: 6-bit subcode Q characters
		0x39, 0x36, 0x34, 0x30, 0x32, 0x36, 0x37	// 9 6 4 0 2 6 7
	};
	ASSERT_EQ(0, memcmp(&reply[9], isrc, sizeof(isrc)));
}

TEST_F(GdromSubcodeTest, NoIsrc)
{
	load(FLYCAST_TEST_FILES "/test_cues/subcode/subcode.cue");

	// Data track without ISRC
	ASSERT_EQ(0x18u, gd_get_subcode(3, 154, reply));
	ASSERT_EQ(0, reply[8]);		// TCVal
	const u8 isrc[12] = { 0, 0, 0, 0, 0, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30 };
	ASSERT_EQ(0, memcmp(&reply[9], isrc, sizeof(isrc)));
}
