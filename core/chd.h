#pragma once

#include <utility>

#include <libchdr/chd.h>

#include "oslib/storage.h"

namespace CHD
{
	namespace Callbacks
	{
		// Moved out here to avoid being duplicated by the template.
		inline uint64_t fsize(struct chd_core_file *core_file)
		{
			auto file = static_cast<hostfs::File*>(core_file->argp);
			return file->size();
		}

		inline size_t fread(void* buffer, size_t size, size_t count, struct chd_core_file *core_file)
		{
			auto file = static_cast<hostfs::File*>(core_file->argp);
			return file->read(buffer, size, count);
		}

		inline int fclose(struct chd_core_file *core_file)
		{
			auto file = static_cast<hostfs::File*>(core_file->argp);
			delete file;
			delete core_file;
			return 0;
		}

		inline int fseek(struct chd_core_file *core_file, int64_t offset, int whence)
		{
			auto file = static_cast<hostfs::File*>(core_file->argp);
			return file->seek(offset, whence);
		}
	}
}

template<typename... Ts>
inline auto chd_open_file(hostfs::File *file, Ts &&...args)
{
	auto core_file = new struct chd_core_file();

	core_file->argp = file;
	core_file->fsize = CHD::Callbacks::fsize;
	core_file->fread = CHD::Callbacks::fread;
	core_file->fclose = CHD::Callbacks::fclose;
	core_file->fseek = CHD::Callbacks::fseek;

	return chd_open_core_file(core_file, std::forward<Ts>(args)...);
}
