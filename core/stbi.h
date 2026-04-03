#pragma once

#include <utility>

#include <stb_image.h>

#include "oslib/storage.h"

namespace STBI
{
	// Moved out here to avoid being duplicated by the template.
	inline const stbi_io_callbacks callbacks = {
		[](void *user, char *data, int size) -> int
		{
			auto file = static_cast<hostfs::File*>(user);
			return file->read(data, 1, size);
		},
		[](void *user, int n)
		{
			auto file = static_cast<hostfs::File*>(user);
			file->seek(n, SEEK_CUR);
		},
		[](void *user) -> int
		{
			auto file = static_cast<hostfs::File*>(user);
			return file->eof();
		}
	};
}

template <typename... Ts>
inline auto stbi_load_from_file(hostfs::File *file, Ts &&...args)
{
	return stbi_load_from_callbacks(&STBI::callbacks, file, std::forward<Ts>(args)...);
}
