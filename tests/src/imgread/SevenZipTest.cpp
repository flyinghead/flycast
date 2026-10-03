#include "gtest/gtest.h"
#include "imgread/common.h"
#include "stdclass.h"

#include <chrono>
#include <filesystem>
#include <memory>

class SevenZipTest : public ::testing::Test
{
protected:
	std::filesystem::path dataDir;
	std::string previousDataDir;

	void SetUp() override
	{
		previousDataDir = get_writable_data_path("");
		auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		dataDir = std::filesystem::temp_directory_path() / ("flycast-7z-test-" + std::to_string(stamp));
		ASSERT_TRUE(std::filesystem::create_directory(dataDir));
		set_user_data_dir(dataDir.string());
	}

	void TearDown() override
	{
		EXPECT_TRUE(std::filesystem::is_empty(dataDir)) << "Temporary archive files were not removed";
		set_user_data_dir(previousDataDir.empty() ? "." : previousDataDir);
		std::filesystem::remove_all(dataDir);
	}

	static std::string fixture(const char *name)
	{
		return std::string(FLYCAST_TEST_FILES "/test_7z/") + name;
	}

	static void verifySector(Disc *disc, u32 fad, u8 track, u8 frame)
	{
		u8 sector[2352] {};
		ASSERT_EQ(1u, disc->ReadSectors(fad, 1, sector, sizeof(sector))) << "FAD " << fad;
		const u8 expected[] { track, 0x11, frame, 0x22, 0, 0x33, 0x44, 0x55 };
		EXPECT_EQ(0, memcmp(sector, expected, sizeof(expected))) << "FAD " << fad;
	}

	static void verifyGdi(Disc *disc)
	{
		ASSERT_EQ(GdRom, disc->type);
		ASSERT_EQ(3u, disc->tracks.size());
		EXPECT_EQ(150u, disc->tracks[0].StartFAD);
		EXPECT_EQ(600u, disc->tracks[1].StartFAD);
		EXPECT_EQ(45150u, disc->tracks[2].StartFAD);
		// Read tracks and sectors out of order to exercise seeking.
		verifySector(disc, 45153, 3, 3);
		verifySector(disc, 45189, 3, 39); // beyond the first 64 KiB extraction chunk
		verifySector(disc, 601, 2, 1);
		verifySector(disc, 152, 1, 2);
		verifySector(disc, 45150, 3, 0);
		verifySector(disc, 150, 1, 0);
	}
};

TEST_F(SevenZipTest, GdiSolidAndNonSolidWithNestedUnicodePaths)
{
	for (const char *name : { "gdi-solid.7z", "gdi-nonsolid.7z", "long-path.7z" })
	{
		SCOPED_TRACE(name);
		ASSERT_TRUE(is7zDisc(fixture(name)));
		std::unique_ptr<Disc> disc(OpenDisc(fixture(name)));
		verifyGdi(disc.get());
	}
}

TEST_F(SevenZipTest, CueTracksShareMemberWithIndependentHandles)
{
	std::unique_ptr<Disc> disc(OpenDisc(fixture("cue-shared.7z")));
	ASSERT_EQ(3u, disc->tracks.size());
	EXPECT_EQ(150u, disc->tracks[0].StartFAD);
	EXPECT_EQ(154u, disc->tracks[1].StartFAD);
	EXPECT_EQ(158u, disc->tracks[2].StartFAD);
	verifySector(disc.get(), 161, 3, 3);
	verifySector(disc.get(), 154, 2, 0);
	verifySector(disc.get(), 152, 1, 2);
	verifySector(disc.get(), 159, 3, 1);
	// Closing one track must not close the other handles to its member.
	disc->tracks[0].Destroy();
	verifySector(disc.get(), 155, 2, 1);
	verifySector(disc.get(), 160, 3, 2);
}

TEST_F(SevenZipTest, ChdInsideArchive)
{
	std::unique_ptr<Disc> disc(OpenDisc(fixture("chd.7z")));
	ASSERT_EQ(CdRom, disc->type);
	ASSERT_EQ(3u, disc->tracks.size());
	verifySector(disc.get(), 150, 1, 0);
	verifySector(disc.get(), 300, 2, 0);
	verifySector(disc.get(), 599, 3, 149);
}

TEST_F(SevenZipTest, CdiWithUppercaseExtensions)
{
	ASSERT_TRUE(is7zDisc(fixture("cdi.7Z")));
	std::unique_ptr<Disc> disc(OpenDisc(fixture("cdi.7Z")));
	ASSERT_EQ(2u, disc->tracks.size());
	EXPECT_TRUE(disc->tracks[0].isDataTrack());
	EXPECT_FALSE(disc->tracks[1].isDataTrack());
	EXPECT_EQ(150u, disc->tracks[0].StartFAD);
	EXPECT_EQ(154u, disc->tracks[1].StartFAD);
	verifySector(disc.get(), 157, 2, 3);
	verifySector(disc.get(), 151, 1, 1);
	verifySector(disc.get(), 154, 2, 0);
}

TEST_F(SevenZipTest, ReopenAndKeepTwoDiscsAlive)
{
	for (int i = 0; i < 3; ++i)
	{
		std::unique_ptr<Disc> first(OpenDisc(fixture("gdi-solid.7z")));
		std::unique_ptr<Disc> second(OpenDisc(fixture("gdi-solid.7z")));
		verifyGdi(first.get());
		verifyGdi(second.get());
		first.reset();
		verifyGdi(second.get());
	}
}

TEST_F(SevenZipTest, ChdDigestMatchesUncompressedImage)
{
	std::vector<u8> archiveDigest;
	std::vector<u8> rawDigest;
	std::unique_ptr<Disc> archive(OpenDisc(fixture("chd.7z"), &archiveDigest));
	std::unique_ptr<Disc> raw(OpenDisc(FLYCAST_TEST_FILES "/test_chds/audiocd.chd", &rawDigest));
	ASSERT_FALSE(archiveDigest.empty());
	EXPECT_EQ(archiveDigest, rawDigest);
	verifySector(archive.get(), 599, 3, 149);
}

TEST_F(SevenZipTest, RejectInvalidArchivesAndImages)
{
	for (const char *name : { "does-not-exist.7z", "truncated.7z", "corrupt-data.7z",
			"no-disc.7z", "multiple-discs.7z", "mixed-descriptors.7z",
			"missing-track.7z", "invalid-descriptor.7z", "traversal-track.7z", "traversal-entry.7z" })
	{
		SCOPED_TRACE(name);
		EXPECT_THROW(std::unique_ptr<Disc> disc(OpenDisc(fixture(name))), FlycastException);
		EXPECT_TRUE(std::filesystem::is_empty(dataDir));
	}
}

TEST_F(SevenZipTest, MetadataProbeDoesNotExtractMembers)
{
	for (const char *name : { "gdi-solid.7z", "cue-shared.7z", "chd.7z" })
		EXPECT_TRUE(is7zDisc(fixture(name))) << name;
	for (const char *name : { "does-not-exist.7z", "truncated.7z", "no-disc.7z",
			"multiple-discs.7z", "mixed-descriptors.7z", "traversal-entry.7z" })
		EXPECT_FALSE(is7zDisc(fixture(name))) << name;
	EXPECT_TRUE(std::filesystem::is_empty(dataDir));
}

TEST_F(SevenZipTest, PreserveArcadePlatformDetection)
{
	EXPECT_EQ(DC_PLATFORM_DREAMCAST, getGamePlatform("synthetic-dreamcast-game.7z"));
	EXPECT_EQ(DC_PLATFORM_DREAMCAST, getGamePlatform("synthetic-dreamcast-game.7Z"));
	EXPECT_EQ(DC_PLATFORM_NAOMI, getGamePlatform("ikaruga.7z"));
	EXPECT_EQ(DC_PLATFORM_ATOMISWAVE, getGamePlatform("claychal.7z"));
	EXPECT_EQ(DC_PLATFORM_NAOMI2, getGamePlatform("vf4.7z"));
	EXPECT_EQ(DC_PLATFORM_NAOMI, getGamePlatform("unknown-arcade-game.zip"));
}
