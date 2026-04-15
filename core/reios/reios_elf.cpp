#include "reios.h"

extern "C" {
#include <elf/elf.h>
}

#include "hw/sh4/sh4_mem.h"

bool reios_loadElf(const std::string& elf) {

	FILE* f = nowide::fopen(elf.c_str(), "rb");
	if (!f)
		return false;

	std::fseek(f, 0, SEEK_END);
	size_t size = std::ftell(f);

	if (size == 0 || size > 16_MB) {
		std::fclose(f);
		return false;
	}

	void* elfF = malloc(size);

	std::fseek(f, 0, SEEK_SET);
	size_t nread = std::fread(elfF, 1, size, f);
	std::fclose(f);

	elf_t elfFile;
	if (nread != size || elf_newFile(elfF, nread, &elfFile) != 0)
	{
		free(elfF);
		return false;
	}

	Elf32_Ehdr const *header = (const Elf32_Ehdr *)elfFile.elfFile;
	if (header->e_machine != EM_SH)
		WARN_LOG(REIOS, "Elf file is not for Hitachi SH: machine %d", header->e_machine);

	for (size_t i = 0; i < elf_getNumProgramHeaders(&elfFile); i++)
	{
		uint32_t type = elf_getProgramHeaderType(&elfFile, i);
		if (type != PT_LOAD) {
			DEBUG_LOG(REIOS, "Ignoring section %d type %d", (int)i, type);
			continue;
		}
		// Load/initialize that section
		uint64_t dest = elf_getProgramHeaderVaddr(&elfFile, i);
		size_t len = elf_getProgramHeaderFileSize(&elfFile, i);
		void *src = (u8 *)(elfFile.elfFile) + elf_getProgramHeaderOffset(&elfFile, i);
		size_t memsize = elf_getProgramHeaderMemorySize(&elfFile, i);
		if (memsize < len) {
			WARN_LOG(REIOS, "Invalid memory size for section %d: %lx", (int)i, (long)memsize);
			continue;
		}
		if (memsize == 0)
			continue;
		u8* ptr = GetMemPtr(dest, memsize);
		if (ptr == nullptr)
		{
			WARN_LOG(REIOS, "Invalid load address or size for section %d: %08lx size %lx", (int)i, (long)dest, (long)memsize);
			continue;
		}
		DEBUG_LOG(REIOS, "Loading section %d to %08lx - %08lx", (int)i, (long)dest, (long)(dest + memsize - 1));
		memcpy(ptr, src, len);
		memset(ptr + len, 0, memsize - len);
	}
	free(elfF);

	return true;
}
