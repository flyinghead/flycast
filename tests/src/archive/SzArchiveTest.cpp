#include "gtest/gtest.h"
#include "archive/7zArchive.h"
#include "lzma/7zCrc.h"

#include <memory>
#include <vector>

namespace
{

hostfs::File *openFixture(const char *name)
{
	const std::string path = std::string(FLYCAST_TEST_FILES) + "/test_7z/" + name;
	FILE *file = std::fopen(path.c_str(), "rb");
	return file == nullptr ? nullptr : new hostfs::StdFile(file);
}

}

TEST(SzArchiveTest, ReadsLargeFileInChunks)
{
	SzArchive archive;
	ASSERT_TRUE(archive.Open(openFixture("archive.7z")));
	std::unique_ptr<ArchiveFile> file(archive.OpenFile("payload.bin"));
	ASSERT_NE(nullptr, file);
	ASSERT_EQ(131073u, file->length());

	std::vector<u8> data(file->length());
	ASSERT_EQ(65536u, file->Read(data.data(), 65536));
	ASSERT_EQ(65536u, file->Read(data.data() + 65536, 65536));
	ASSERT_EQ(1u, file->Read(data.data() + 131072, 65536));
	ASSERT_EQ(0u, file->Read(data.data(), 65536));
	EXPECT_EQ(131073u, file->length());
	for (size_t i = 0; i < data.size(); i++)
		ASSERT_EQ(i % 251, data[i]) << "offset " << i;
}

TEST(SzArchiveTest, EnumeratesFilesAndDecodesUtf8Names)
{
	SzArchive archive;
	ASSERT_TRUE(archive.Open(openFixture("archive.7z")));
	bool foundPayload = false;
	bool foundEmpty = false;
	bool foundUnicode = false;
	for (size_t i = 0; i < archive.GetFileCount(); i++)
	{
		if (archive.IsDirectory(i))
		{
			EXPECT_EQ(nullptr, archive.OpenFileByIndex(i));
			continue;
		}
		const std::string name = archive.GetFileName(i);
		std::unique_ptr<ArchiveFile> file(archive.OpenFileByIndex(i));
		ASSERT_NE(nullptr, file) << name;
		EXPECT_EQ(archive.GetFileSize(i), file->length());
		if (name == "payload.bin")
		{
			foundPayload = true;
			EXPECT_EQ(131073u, file->length());
		}
		else if (name == "empty")
		{
			foundEmpty = true;
			EXPECT_EQ(0u, file->length());
			EXPECT_EQ(0u, file->Read(nullptr, 1));
		}
		else if (name == u8"folder/日本語😀.bin")
		{
			foundUnicode = true;
			char data[4];
			ASSERT_EQ(sizeof(data), file->Read(data, sizeof(data)));
			EXPECT_EQ("utf8", std::string(data, sizeof(data)));
		}
	}
	EXPECT_TRUE(foundPayload);
	EXPECT_TRUE(foundEmpty);
	EXPECT_TRUE(foundUnicode);
	EXPECT_EQ(nullptr, archive.OpenFileByIndex(archive.GetFileCount()));
	EXPECT_TRUE(archive.GetFileName(archive.GetFileCount()).empty());
	EXPECT_EQ(0u, archive.GetFileSize(archive.GetFileCount()));
	EXPECT_FALSE(archive.IsDirectory(archive.GetFileCount()));
	EXPECT_EQ(nullptr, archive.OpenFile("missing"));
}

TEST(SzArchiveTest, OpensLongUtf8Names)
{
	SzArchive archive;
	ASSERT_TRUE(archive.Open(openFixture("long-path.7z")));
	bool found = false;
	for (size_t i = 0; i < archive.GetFileCount(); i++)
	{
		const std::string name = archive.GetFileName(i);
		if (archive.IsDirectory(i) || name.find(u8"音声 02.raw") == std::string::npos)
			continue;
		found = true;
		ASSERT_GT(name.size(), 512u);
		std::unique_ptr<ArchiveFile> file(archive.OpenFile(name.c_str()));
		ASSERT_NE(nullptr, file);
		u8 marker[2];
		ASSERT_EQ(sizeof(marker), file->Read(marker, sizeof(marker)));
		EXPECT_EQ(2u, marker[0]);
		EXPECT_EQ(0x11u, marker[1]);
	}
	EXPECT_TRUE(found);
}

TEST(SzArchiveTest, LooksUpDefinedCrc)
{
	SzArchive archive;
	ASSERT_TRUE(archive.Open(openFixture("archive.7z")));
	std::unique_ptr<ArchiveFile> file(archive.OpenFileByCrc(CrcCalc("utf8", 4)));
	ASSERT_NE(nullptr, file);
	char data[4];
	ASSERT_EQ(sizeof(data), file->Read(data, sizeof(data)));
	EXPECT_EQ("utf8", std::string(data, sizeof(data)));
	EXPECT_EQ(nullptr, archive.OpenFileByCrc(0));
}

TEST(SzArchiveTest, IgnoresUndefinedCrc)
{
	SzArchive archive;
	ASSERT_TRUE(archive.Open(openFixture("no-crc.7z")));
	ASSERT_EQ(1u, archive.GetFileCount());
	EXPECT_EQ(nullptr, archive.OpenFileByCrc(CrcCalc("no checksum", 11)));
	std::unique_ptr<ArchiveFile> file(archive.OpenFile("no-crc.bin"));
	ASSERT_NE(nullptr, file);
	char data[11];
	ASSERT_EQ(sizeof(data), file->Read(data, sizeof(data)));
	EXPECT_EQ("no checksum", std::string(data, sizeof(data)));
}

TEST(SzArchiveTest, CanReopenAfterFailure)
{
	SzArchive archive;
	EXPECT_EQ(0u, archive.GetFileCount());
	EXPECT_EQ(nullptr, archive.OpenFileByIndex(0));
	EXPECT_FALSE(archive.Open(nullptr));
	EXPECT_FALSE(archive.Open(openFixture("truncated.7z")));
	EXPECT_EQ(0u, archive.GetFileCount());
	ASSERT_TRUE(archive.Open(openFixture("archive.7z")));
	EXPECT_GT(archive.GetFileCount(), 0u);
	ASSERT_TRUE(archive.Open(openFixture("no-crc.7z")));
	EXPECT_EQ(1u, archive.GetFileCount());
	EXPECT_FALSE(archive.Open(openFixture("truncated.7z")));
	EXPECT_EQ(0u, archive.GetFileCount());
	EXPECT_EQ(nullptr, archive.OpenFileByIndex(0));
}

TEST(SzArchiveTest, PreservesFileLengthsLargerThan32Bits)
{
	if (sizeof(size_t) <= sizeof(u32))
		GTEST_SKIP() << "Requires a 64-bit address space";
	u8 data[1] {};
	const size_t length = static_cast<size_t>((u64(1) << 32) + 7);
	SzArchiveFile file(data, 0, length);
	EXPECT_EQ(length, file.length());
}
