#include "gtest/gtest.h"
#include "imgread/common.h"
#include <locale>

class GdiTest : public ::testing::Test {
};

static void verifyDisk(Disc *disk)
{
	ASSERT_EQ(GdRom, disk->type);
	ASSERT_EQ(3, disk->tracks.size());

	ASSERT_TRUE(disk->tracks[0].isDataTrack());
	ASSERT_EQ(150, disk->tracks[0].StartFAD);

	ASSERT_FALSE(disk->tracks[1].isDataTrack());
	ASSERT_EQ(600, disk->tracks[1].StartFAD);

	ASSERT_TRUE(disk->tracks[2].isDataTrack());
	ASSERT_EQ(45150, disk->tracks[2].StartFAD);
}

TEST_F(GdiTest, LoadA)
{
	// uses backslash instead of slash
#ifndef _WIN32
	ASSERT_THROW(OpenDisc(FLYCAST_TEST_FILES "/test_gdis/a/cs.gdi"),
			FlycastException);
#else
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_gdis/a/cs.gdi");
	verifyDisk(disk);
	delete disk;
#endif
}

TEST_F(GdiTest, LoadB)
{
	std::locale::global(std::locale("fr_FR.UTF-8"));
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_gdis/b/cs.gdi");
	verifyDisk(disk);
	delete disk;
}

TEST_F(GdiTest, LoadC)
{
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_gdis/c/cs.gdi");
	verifyDisk(disk);
	delete disk;
}

TEST_F(GdiTest, LoadD)
{
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_gdis/d/cs.gdi");
	verifyDisk(disk);
	delete disk;
}

