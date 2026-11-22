#include "gtest/gtest.h"
#include "imgread/common.h"
#include "imgread/isofs.h"

class IsoFsTest : public ::testing::Test {
};

TEST_F(IsoFsTest, VolumeId)
{
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_cues/isofs/isofs.cue");
	ASSERT_NE(nullptr, disk);
	IsoFs isofs(disk);
	ASSERT_EQ("GANRYU", isofs.getVolumeId());
	delete disk;
}

TEST_F(IsoFsTest, NoVolumeDescriptor)
{
	Disc *disk = OpenDisc(FLYCAST_TEST_FILES "/test_cues/subcode/subcode.cue");
	ASSERT_NE(nullptr, disk);
	IsoFs isofs(disk);
	ASSERT_EQ("", isofs.getVolumeId());
	delete disk;
}
