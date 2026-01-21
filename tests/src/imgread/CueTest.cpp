#include "gtest/gtest.h"
#include "imgread/common.h"
#include <locale>

class CueTest : public ::testing::Test {
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

TEST_F(CueTest, LoadD)
{
	std::locale::global(std::locale("fr_FR.UTF-8"));
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_cues/d/cs.cue");
	verifyDisk(disk);
	delete disk;
}

TEST_F(CueTest, LoadE)
{
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_cues/e/cs.cue");

	ASSERT_EQ(CdRom_XA, disk->type);
	ASSERT_EQ(2, disk->tracks.size());

	ASSERT_FALSE(disk->tracks[0].isDataTrack());
	ASSERT_EQ(150, disk->tracks[0].StartFAD);

	ASSERT_TRUE(disk->tracks[1].isDataTrack());
	ASSERT_EQ(11550, disk->tracks[1].StartFAD);
	delete disk;
}

TEST_F(CueTest, LoadAudio)
{
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_cues/d/audio.cue");

	ASSERT_EQ(CdRom, disk->type);
	ASSERT_EQ(3, disk->tracks.size());

	ASSERT_FALSE(disk->tracks[0].isDataTrack());
	ASSERT_EQ(150, disk->tracks[0].StartFAD);

	ASSERT_FALSE(disk->tracks[1].isDataTrack());
	ASSERT_EQ(225, disk->tracks[1].StartFAD);

	ASSERT_FALSE(disk->tracks[2].isDataTrack());
	ASSERT_EQ(300, disk->tracks[2].StartFAD);
	delete disk;
}
