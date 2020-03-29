#include "reios.h"

#include "deps/libelf/elf.h"

#include "hw/sh4/sh4_mem.h"

bool reios_loadElf(const std::string& elf) {

	FILE* f = fopen(elf.c_str(), "rb");
	if (!f) {
		return false;
	}
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);

	if (size > 16 * 1024 * 1024) {
		fclose(f);
		return false;
	}

	void* elfFile = malloc(size);
	memset(elfFile, 0, size);

	fseek(f, 0, SEEK_SET);
	size_t nread = fread(elfFile, 1, size, f);
	fclose(f);

	if (nread != size || elf_checkFile(elfFile) != 0)
	{
		free(elfFile);
		return false;
	}

	bool phys = false;
	for (int i = 0; i < elf_getNumProgramHeaders(elfFile); i++)
	{
		// Load that section
		uint64_t dest;
		if (phys)
			dest = elf_getProgramHeaderPaddr(elfFile, i);
		else
			dest = elf_getProgramHeaderVaddr(elfFile, i);
		size_t len = elf_getProgramHeaderFileSize(elfFile, i);
		void *src = (u8 *)elfFile + elf_getProgramHeaderOffset(elfFile, i);
		u8* ptr = GetMemPtr(dest, len);
		if (ptr == NULL)
		{
			WARN_LOG(REIOS, "Invalid load address for section %d: %08lx", i, dest);
			continue;
		}
		DEBUG_LOG(REIOS, "Loading section %d to %08lx - %08lx", i, dest, dest + len - 1);
		memcpy(ptr, src, len);
		ptr += len;
		memset(ptr, 0, elf_getProgramHeaderMemorySize(elfFile, i) - len);
	}
	free(elfFile);

	return true;
}
