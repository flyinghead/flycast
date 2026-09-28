#include "gtest/gtest.h"
#include "stdclass.h"
#include "oslib/storage.h"
#include "imgread/common.h"
#include <cstdio>
#include <vector>

#define TRACK_FILE FLYCAST_TEST_FILES "/test_cues/d/cs (0,1).bin"
// md5 of the content of TRACK_FILE, the only non-empty track of test_cues/d/cs.cue
static const std::vector<u8> TRACK_MD5 {
	0xd7, 0x8c, 0x60, 0x0a, 0x1b, 0x41, 0xa0, 0x2d, 0x26, 0xfa, 0x78, 0x07, 0x37, 0x3f, 0xba, 0xa1
};

class Md5SumTest : public ::testing::Test {
};

TEST_F(Md5SumTest, HostfsFileHashesContent)
{
	hostfs::File *file = hostfs::storage().openFile(TRACK_FILE, "rb");
	ASSERT_NE(nullptr, file);
	// the whole file is hashed regardless of the current position
	file->seek(1000, SEEK_SET);
	std::vector<u8> digest = MD5Sum().add(file).getDigest();
	delete file;
	ASSERT_EQ(TRACK_MD5, digest);
}

TEST_F(Md5SumTest, HostfsFileMatchesStdFile)
{
	std::FILE *stdFile = std::fopen(TRACK_FILE, "rb");
	ASSERT_NE(nullptr, stdFile);
	std::vector<u8> expected = MD5Sum().add(stdFile).getDigest();
	std::fclose(stdFile);

	hostfs::File *file = hostfs::storage().openFile(TRACK_FILE, "rb");
	ASSERT_NE(nullptr, file);
	std::vector<u8> digest = MD5Sum().add(file).getDigest();
	delete file;
	ASSERT_EQ(expected, digest);
}

// The disc digest is what GGPO peers compare: it must depend on the track data only.
TEST_F(Md5SumTest, CueDiscDigest)
{
	std::vector<u8> digest;
	Disc *disc = OpenDisc(FLYCAST_TEST_FILES "/test_cues/d/cs.cue", &digest);
	ASSERT_NE(nullptr, disc);
	delete disc;
	ASSERT_EQ(TRACK_MD5, digest);
}
