#include "common.h"
#include "archive/7zArchive.h"
#include "oslib/i18n.h"
#include "stdclass.h"

#include <array>
#include <limits>
#include <map>
#include <random>

namespace {

// Archive paths never become host paths, including the paths in GDI/CUE files.
std::string memberPath(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	if ((!path.empty() && path.front() == '/') || path.find(':') != std::string::npos)
		throw FlycastException(i18n::Ts("Invalid path in 7z archive"));
	std::vector<std::string> components;
	size_t start = 0;
	while (start < path.size())
	{
		size_t end = path.find('/', start);
		if (end == std::string::npos)
			end = path.size();
		std::string part = path.substr(start, end - start);
		if (part == "..") {
			if (components.empty())
				throw FlycastException(i18n::Ts("Invalid path in 7z archive"));
			components.pop_back();
		}
		else if (!part.empty() && part != ".")
			components.push_back(part);
		start = end + 1;
	}
	std::string result;
	for (const std::string& part : components) {
		if (!result.empty())
			result += '/';
		result += part;
	}
	return result;
}

// Use Flycast's writable directory: the system temporary directory isn't writable
// on every platform. Only generated names are used, with exclusive creation.
struct TemporaryFile
{
	std::string path;
	FILE *writer = nullptr;

	TemporaryFile()
	{
		std::random_device random;
		for (int attempt = 0; attempt < 16; attempt++)
		{
			path = get_writable_data_path(".flycast-7z-")
					+ std::to_string(random()) + "-" + std::to_string(random())
					+ "-" + std::to_string(random()) + ".tmp";
			writer = nowide::fopen(path.c_str(), "wbx");
			if (writer != nullptr)
				return;
			if (errno != EEXIST)
				break;
		}
		throw FlycastException(i18n::Ts("Cannot create temporary disc file"));
	}

	~TemporaryFile()
	{
		if (writer != nullptr)
			std::fclose(writer);
		nowide::remove(path.c_str());
	}

	void finish()
	{
		int result = std::fclose(writer);
		writer = nullptr;
		if (result != 0)
			throw FlycastException(i18n::Ts("Cannot write temporary disc file"));
	}
};

// Readers have separate cursors. The backing file is removed only after its last
// reader closes, including when parsing fails or two discs are open at once.
class CachedFile : public hostfs::File
{
	std::shared_ptr<TemporaryFile> backing;
	hostfs::StdFile file; // destroyed before backing (required on Windows)

public:
	CachedFile(std::shared_ptr<TemporaryFile> backing, FILE *file)
		: backing(std::move(backing)), file(file) {}

	size_t read(void *buffer, size_t size, size_t count) override { return file.read(buffer, size, count); }
	size_t write(const void *, size_t, size_t) override { return 0; }
	s64 tell() override { return file.tell(); }
	int seek(s64 offset, int whence) override { return file.seek(offset, whence); }
	char *gets(char *str, int count) override { return file.gets(str, count); }
	s64 size() override { return file.size(); }
	int eof() override { return file.eof(); }
	int error() override { return file.error(); }
};

class ArchiveStorage : public hostfs::Storage
{
	struct Member
	{
		size_t index;
		size_t size;
		std::shared_ptr<TemporaryFile> cached;
	};

	SzArchive archive;
	std::map<std::string, Member> members;
	std::string discName;

public:
	explicit ArchiveStorage(const std::string& path)
	{
		hostfs::File *file = hostfs::storage().openFile(path, "rb");
		if (file == nullptr || !archive.Open(file))
			throw FlycastException(i18n::Ts("Cannot open 7z archive"));
		for (size_t i = 0; i < archive.GetFileCount(); i++)
		{
			if (archive.IsDirectory(i))
				continue;
			std::string name = memberPath(archive.GetFileName(i));
			u64 size = archive.GetFileSize(i);
			if (name.empty() || size > std::numeric_limits<size_t>::max()
					|| size > (u64)std::numeric_limits<s64>::max()
					|| !members.emplace(name, Member { i, (size_t)size, nullptr }).second)
				throw FlycastException(i18n::Ts("Invalid file in 7z archive"));
			std::string extension = get_file_extension(name);
			if (extension == "gdi" || extension == "cue" || extension == "cdi" || extension == "chd")
			{
				if (!discName.empty())
					throw FlycastException(i18n::Ts("7z archive contains multiple disc images"));
				discName = name;
			}
		}
		if (discName.empty())
			throw FlycastException(i18n::Ts("No supported disc image in 7z archive"));
	}

	const std::string& getDiscName() const { return discName; }
	bool isKnownPath(const std::string&) override { return true; }

	std::vector<hostfs::FileInfo> listContent(const std::string&) override
	{
		std::vector<hostfs::FileInfo> result;
		for (const auto& entry : members)
			result.emplace_back(entry.first, entry.first, false, entry.second.size);
		return result;
	}

	hostfs::File *openFile(const std::string& path, const std::string& mode) override
	{
		if (mode != "rb")
			return nullptr;
		auto it = members.find(memberPath(path));
		if (it == members.end())
			return nullptr;
		Member& member = it->second;
		if (!member.cached)
		{
			std::unique_ptr<ArchiveFile> input(archive.OpenFileByIndex(member.index));
			if (!input || input->length() != member.size)
				throw FlycastException(i18n::Ts("Cannot extract disc file from 7z archive"));
			auto output = std::make_shared<TemporaryFile>();
			std::array<u8, 64 * 1024> buffer;
			size_t remaining = member.size;
			while (remaining != 0)
			{
				u32 count = (u32)std::min(remaining, buffer.size());
				if (input->Read(buffer.data(), count) != count)
					throw FlycastException(i18n::Ts("Cannot extract disc file from 7z archive"));
				if (std::fwrite(buffer.data(), 1, count, output->writer) != count)
					throw FlycastException(i18n::Ts("Cannot write temporary disc file"));
				remaining -= count;
			}
			output->finish();
			member.cached = std::move(output);
		}
		FILE *file = nowide::fopen(member.cached->path.c_str(), "rb");
		if (file == nullptr)
			throw FlycastException(i18n::Ts("Cannot open temporary disc file"));
		return new CachedFile(member.cached, file);
	}

	std::string getParentPath(const std::string& path) override
	{
		std::string name = memberPath(path);
		size_t slash = name.find_last_of('/');
		return slash == std::string::npos ? "" : name.substr(0, slash);
	}

	std::string getSubPath(const std::string& reference, const std::string& subpath) override
	{
		// Check absolute paths before joining; relative '..' may stay inside the archive.
		if (!subpath.empty() && (subpath.front() == '/' || subpath.front() == '\\'))
			throw FlycastException(i18n::Ts("Invalid path in 7z archive"));
		return memberPath(reference.empty() ? subpath : reference + '/' + subpath);
	}

	hostfs::FileInfo getFileInfo(const std::string& path) override
	{
		std::string name = memberPath(path);
		auto it = members.find(name);
		if (it == members.end())
			throw hostfs::StorageException(i18n::Ts("File not found in 7z archive"));
		return { name.substr(name.find_last_of('/') + 1), name, false, it->second.size };
	}

	bool exists(const std::string& path) override
	{
		return members.count(memberPath(path)) != 0;
	}
};

} // namespace

bool is7zDisc(const std::string& path)
{
	try {
		ArchiveStorage storage(path);
		return true;
	} catch (const std::exception&) {
		return false;
	}
}

Disc *sz_parse(const std::string& path, std::vector<u8> *digest)
{
	ArchiveStorage storage(path);
	return OpenDisc(storage.getDiscName(), digest, storage);
}
