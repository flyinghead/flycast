/*
	The test images are built by tests/files/test_chds/make_fixtures.py, which holds
	the track layout of each one, writes the .bin and .cue files and runs chdman over
	them. Every sector starts with a marker identifying its track and frame number;
	swapping its byte pairs yields a different value, so a missing or spurious byte
	swap of the audio tracks fails the tests below just like a misplaced track does.
 */
#include "gtest/gtest.h"
#include "imgread/common.h"

class ChdTest : public ::testing::Test {
protected:
	// Checks that the first 8 bytes of sector FAD are the marker of frame `frame` of track `track`
	static void verifySector(Disc *disc, u32 FAD, u8 track, u32 frame)
	{
		u8 sector[2352];
		memset(sector, 0xCC, sizeof(sector));
		ASSERT_EQ(1u, disc->ReadSectors(FAD, 1, sector, 2352)) << "FAD " << FAD;
		const u8 expected[8] { track, 0x11, (u8)(frame & 0xff), 0x22, (u8)(frame >> 8), 0x33, 0x44, 0x55 };
		ASSERT_EQ(0, memcmp(sector, expected, sizeof(expected)))
				<< "FAD " << FAD << " reads track " << (int)sector[0] << " frame "
				<< (sector[4] * 256 + sector[2]) << " instead of track " << (int)track << " frame " << frame;
	}
};

// MIL-CD laid out as Redump dumps them: 3 audio tracks in session 1, whose pregap is part of
// the track data (PGTYPE:V...), and a single data track in session 2 with no pregap of its own.
TEST_F(ChdTest, LoadMilCd)
{
	Disc *disc = OpenDisc(FLYCAST_TEST_FILES "/test_chds/milcd.chd");

	ASSERT_EQ(CdRom_XA, disc->type);
	ASSERT_EQ(4u, disc->tracks.size());

	ASSERT_FALSE(disc->tracks[0].isDataTrack());
	ASSERT_EQ(150u, disc->tracks[0].StartFAD);
	ASSERT_EQ(449u, disc->tracks[0].EndFAD);

	// 183 pregap frames before INDEX 01
	ASSERT_FALSE(disc->tracks[1].isDataTrack());
	ASSERT_EQ(633u, disc->tracks[1].StartFAD);
	ASSERT_EQ(932u, disc->tracks[1].EndFAD);

	// 210 pregap frames before INDEX 01
	ASSERT_FALSE(disc->tracks[2].isDataTrack());
	ASSERT_EQ(1143u, disc->tracks[2].StartFAD);
	ASSERT_EQ(1342u, disc->tracks[2].EndFAD);

	// second session, 11400 frames after the end of the first one
	ASSERT_TRUE(disc->tracks[3].isDataTrack());
	ASSERT_EQ(1343u + 11400, disc->tracks[3].StartFAD);
	ASSERT_EQ(1792u + 11400, disc->tracks[3].EndFAD);

	ASSERT_EQ(2u, disc->sessions.size());
	ASSERT_EQ(1, disc->sessions[0].FirstTrack);
	ASSERT_EQ(150u, disc->sessions[0].StartFAD);
	ASSERT_EQ(4, disc->sessions[1].FirstTrack);
	ASSERT_EQ(12743u, disc->sessions[1].StartFAD);

	ASSERT_EQ(13193u, disc->LeadOut.StartFAD);
	// the disc boots off the last session
	ASSERT_EQ(12743u, disc->GetBaseFAD());

	// the pregaps must not shift the track data
	verifySector(disc, 150, 1, 0);
	verifySector(disc, 449, 1, 299);
	verifySector(disc, 633, 2, 183);
	verifySector(disc, 932, 2, 482);
	verifySector(disc, 1143, 3, 210);
	verifySector(disc, 1342, 3, 409);
	verifySector(disc, 12743, 4, 0);
	verifySector(disc, 13192, 4, 449);

	delete disc;
}

// Same disc but with the pregap of the second session's track stored in the image. The
// session gap must then be shortened by as much, INDEX 01 sits at the very same FAD.
TEST_F(ChdTest, LoadMilCdSessionPregap)
{
	Disc *disc = OpenDisc(FLYCAST_TEST_FILES "/test_chds/milcd_pregap.chd");

	ASSERT_EQ(CdRom_XA, disc->type);
	ASSERT_EQ(2u, disc->tracks.size());

	ASSERT_EQ(150u, disc->tracks[0].StartFAD);
	ASSERT_EQ(449u, disc->tracks[0].EndFAD);

	ASSERT_TRUE(disc->tracks[1].isDataTrack());
	ASSERT_EQ(450u + 11400, disc->tracks[1].StartFAD);
	ASSERT_EQ(749u + 11400, disc->tracks[1].EndFAD);

	ASSERT_EQ(2u, disc->sessions.size());
	ASSERT_EQ(11850u, disc->sessions[1].StartFAD);
	ASSERT_EQ(12150u, disc->LeadOut.StartFAD);
	ASSERT_EQ(11850u, disc->GetBaseFAD());

	verifySector(disc, 150, 1, 0);
	verifySector(disc, 11850, 2, 150);
	verifySector(disc, 12149, 2, 449);

	delete disc;
}

// Pregap declared with a cue PREGAP command: chdman records it as PREGAP:150 with a PGTYPE
// that isn't prefixed with 'V', meaning the sectors aren't stored in the image
TEST_F(ChdTest, LoadMilCdUnstoredPregap)
{
	Disc *disc = OpenDisc(FLYCAST_TEST_FILES "/test_chds/milcd_nogap.chd");

	ASSERT_EQ(CdRom_XA, disc->type);
	ASSERT_EQ(2u, disc->tracks.size());

	ASSERT_EQ(150u, disc->tracks[0].StartFAD);
	ASSERT_EQ(449u, disc->tracks[0].EndFAD);

	// this is the same physical disc as milcd_pregap.chd, only the pregap sectors aren't
	// stored, so INDEX 01 lands on the very same FAD
	ASSERT_TRUE(disc->tracks[1].isDataTrack());
	ASSERT_EQ(11850u, disc->tracks[1].StartFAD);
	ASSERT_EQ(12149u, disc->tracks[1].EndFAD);

	ASSERT_EQ(2u, disc->sessions.size());
	ASSERT_EQ(11850u, disc->sessions[1].StartFAD);
	ASSERT_EQ(12150u, disc->LeadOut.StartFAD);

	verifySector(disc, 150, 1, 0);
	verifySector(disc, 11850, 2, 0);
	verifySector(disc, 12149, 2, 299);

	delete disc;
}

// Single-session audio CD: no session gap must be inserted
TEST_F(ChdTest, LoadAudioCd)
{
	Disc *disc = OpenDisc(FLYCAST_TEST_FILES "/test_chds/audiocd.chd");

	ASSERT_EQ(CdRom, disc->type);
	ASSERT_EQ(3u, disc->tracks.size());
	ASSERT_EQ(1u, disc->sessions.size());

	ASSERT_EQ(150u, disc->tracks[0].StartFAD);
	ASSERT_EQ(299u, disc->tracks[0].EndFAD);
	ASSERT_EQ(300u, disc->tracks[1].StartFAD);
	ASSERT_EQ(449u, disc->tracks[1].EndFAD);
	ASSERT_EQ(450u, disc->tracks[2].StartFAD);
	ASSERT_EQ(599u, disc->tracks[2].EndFAD);
	ASSERT_EQ(600u, disc->LeadOut.StartFAD);

	verifySector(disc, 150, 1, 0);
	verifySector(disc, 300, 2, 0);
	verifySector(disc, 599, 3, 149);

	delete disc;
}
