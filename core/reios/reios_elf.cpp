#include "reios.h"

extern "C" {
#include <elf/elf.h>
}

#include "hw/sh4/sh4_mem.h"

// True if a PT_LOAD segment already copied [addr, addr + size) in. Tested
// against file size, not memory size: the gap between them is only zero-filled.
static bool isLoadedBySegment(const elf_t *elfFile, uint64_t addr, size_t size)
{
	for (size_t i = 0; i < elf_getNumProgramHeaders(elfFile); i++)
	{
		if (elf_getProgramHeaderType(elfFile, i) != PT_LOAD)
			continue;
		uint64_t start = elf_getProgramHeaderVaddr(elfFile, i);
		uint64_t end = start + elf_getProgramHeaderFileSize(elfFile, i);
		if (addr >= start && addr + size <= end)
			return true;
	}
	return false;
}

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
	if (elfF == nullptr) {
		std::fclose(f);
		return false;
	}

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

	unsigned loaded = 0;

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
		loaded++;
	}

	// Sections added after linking (objcopy --add-section) have no program
	// header. Load those the loop above missed, after it so its zero-fill
	// doesn't clobber them.
	for (size_t i = 0; i < elf_getNumSections(&elfFile); i++)
	{
		if ((elf_getSectionFlags(&elfFile, i) & SHF_ALLOC) == 0
				|| elf_getSectionType(&elfFile, i) == SHT_NOBITS)
			continue;
		uint64_t dest = elf_getSectionAddr(&elfFile, i);
		size_t len = elf_getSectionSize(&elfFile, i);
		if (dest == 0 || len == 0 || isLoadedBySegment(&elfFile, dest, len))
			continue;
		size_t offset = elf_getSectionOffset(&elfFile, i);
		if (offset + len > nread) {
			WARN_LOG(REIOS, "Section %s extends past end of file", elf_getSectionName(&elfFile, i));
			continue;
		}
		u8* ptr = GetMemPtr(dest, len);
		if (ptr == nullptr)
		{
			WARN_LOG(REIOS, "Invalid load address or size for section %s: %08lx size %lx",
					elf_getSectionName(&elfFile, i), (long)dest, (long)len);
			continue;
		}
		DEBUG_LOG(REIOS, "Loading section %s to %08lx - %08lx",
				elf_getSectionName(&elfFile, i), (long)dest, (long)(dest + len - 1));
		memcpy(ptr, (u8 *)(elfFile.elfFile) + offset, len);
		loaded++;
	}

	free(elfF);

	if (loaded == 0) {
		WARN_LOG(REIOS, "Elf file has nothing to load");
		return false;
	}

	return true;
}
