/*
 * Copyright (c) 1999-2004 University of New South Wales
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <elf/elf.h>
#include <elf/elf32.h>
#include <elf/elf64.h>
#include <string.h>
#include <stdio.h>

/* ELF header functions */
int elf_newFile(const void *file, size_t size, elf_t *res)
{
    return elf_newFile_maybe_unsafe(file, size, true, true, res);
}

int elf_newFile_maybe_unsafe(const void *file, size_t size, bool check_pht, bool check_st, elf_t *res)
{
    elf_t new_file = {
        .elfFile = file,
        .elfSize = size
    };

    int status = elf_checkFile(&new_file);
    if (status < 0) {
        return status;
    }

    if (check_pht) {
        status = elf_checkProgramHeaderTable(&new_file);
        if (status < 0) {
            return status;
        }
    }

    if (check_st) {
        status = elf_checkSectionTable(&new_file);
        if (status < 0) {
            return status;
        }
    }

    if (res) {
        *res = new_file;
    }

    return status;
}

int elf_check_magic(const char *file)
{
    if (memcmp(file, ELFMAG, SELFMAG) != 0) {
        return -1;
    }

    return 0;
}

/*
 * Checks that elfFile points to a valid elf file. Returns 0 if the elf
 * file is valid, < 0 if invalid.
 */
int elf_checkFile(elf_t *elfFile)
{
    int res = elf32_checkFile(elfFile);
    if (res == 0) {
        return 0;
    }

    res = elf64_checkFile(elfFile);
    if (res == 0) {
        return 0;
    }

    return -1;
}

int elf_checkProgramHeaderTable(const elf_t *elfFile)
{
    if (elf_isElf32(elfFile)) {
        return elf32_checkProgramHeaderTable(elfFile);
    } else {
        return elf64_checkProgramHeaderTable(elfFile);
    }
}

int elf_checkSectionTable(const elf_t *elfFile)
{
    if (elf_isElf32(elfFile)) {
        return elf32_checkSectionTable(elfFile);
    } else {
        return elf64_checkSectionTable(elfFile);
    }
}

uintptr_t elf_getEntryPoint(const elf_t *elfFile)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getEntryPoint(elfFile);
    } else {
        return elf64_getEntryPoint(elfFile);
    }
}

size_t elf_getNumProgramHeaders(const elf_t *elfFile)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getNumProgramHeaders(elfFile);
    } else {
        return elf64_getNumProgramHeaders(elfFile);
    }
}

size_t elf_getNumSections(const elf_t *elfFile)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getNumSections(elfFile);
    } else {
        return elf64_getNumSections(elfFile);
    }
}

size_t elf_getSectionStringTableIndex(const elf_t *elf)
{
    if (elf_isElf32(elf)) {
        return elf32_getSectionStringTableIndex(elf);
    } else {
        return elf64_getSectionStringTableIndex(elf);
    }
}

const char *elf_getStringTable(const elf_t *elf, size_t string_segment)
{
    const char *string_table = elf_getSection(elf, string_segment);
    if (string_table == NULL) {
        return NULL; /* no such section */
    }

    if (elf_getSectionType(elf, string_segment) != SHT_STRTAB) {
        return NULL; /* not a string table */
    }

    size_t size = elf_getSectionSize(elf, string_segment);
    if (string_table[size - 1] != 0) {
        return NULL; /* string table is not null-terminated */
    }

    return string_table;
}

const char *elf_getSectionStringTable(const elf_t *elf)
{
    size_t index = elf_getSectionStringTableIndex(elf);
    return elf_getStringTable(elf, index);
}


/* Section header functions */
const void *elf_getSection(const elf_t *elf, size_t i)
{
    if (i == 0 || i >= elf_getNumSections(elf)) {
        return NULL; /* no such section */
    }

    size_t section_offset = elf_getSectionOffset(elf, i);
    size_t section_size = elf_getSectionSize(elf, i);
    if (section_size == 0) {
        return NULL; /* section is empty */
    }

    size_t section_end = section_offset + section_size;
    /* possible wraparound - check that section end is not before section start */
    if (section_end > elf->elfSize || section_end < section_offset) {
        return NULL;
    }

    return elf->elfFile + section_offset;
}

const void *elf_getSectionNamed(const elf_t *elfFile, const char *str, size_t *id)
{
    size_t numSections = elf_getNumSections(elfFile);
    for (size_t i = 0; i < numSections; i++) {
        if (strcmp(str, elf_getSectionName(elfFile, i)) == 0) {
            if (id != NULL) {
                *id = i;
            }
            return elf_getSection(elfFile, i);
        }
    }
    return NULL;
}

const char *elf_getSectionName(const elf_t *elf, size_t i)
{
    size_t str_table_idx = elf_getSectionStringTableIndex(elf);
    const char *str_table = elf_getStringTable(elf, str_table_idx);
    size_t offset = elf_getSectionNameOffset(elf, i);
    size_t size = elf_getSectionSize(elf, str_table_idx);

    if (str_table == NULL || offset > size) {
        return "<corrupted>";
    }

    return str_table + offset;
}

size_t elf_getSectionNameOffset(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionNameOffset(elfFile, i);
    } else {
        return elf64_getSectionNameOffset(elfFile, i);
    }
}

uint32_t elf_getSectionType(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionType(elfFile, i);
    } else {
        return elf64_getSectionType(elfFile, i);
    }
}

size_t elf_getSectionFlags(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionFlags(elfFile, i);
    } else {
        return elf64_getSectionFlags(elfFile, i);
    }
}

uintptr_t elf_getSectionAddr(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionAddr(elfFile, i);
    } else {
        return elf64_getSectionAddr(elfFile, i);
    }
}

size_t elf_getSectionOffset(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionOffset(elfFile, i);
    } else {
        return elf64_getSectionOffset(elfFile, i);
    }
}

size_t elf_getSectionSize(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionSize(elfFile, i);
    } else {
        return elf64_getSectionSize(elfFile, i);
    }
}

uint32_t elf_getSectionLink(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionLink(elfFile, i);
    } else {
        return elf64_getSectionLink(elfFile, i);
    }
}

uint32_t elf_getSectionInfo(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionInfo(elfFile, i);
    } else {
        return elf64_getSectionInfo(elfFile, i);
    }
}

size_t elf_getSectionAddrAlign(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionAddrAlign(elfFile, i);
    } else {
        return elf64_getSectionAddrAlign(elfFile, i);
    }
}

size_t elf_getSectionEntrySize(const elf_t *elfFile, size_t i)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getSectionEntrySize(elfFile, i);
    } else {
        return elf64_getSectionEntrySize(elfFile, i);
    }
}


/* Program headers function */
const void *elf_getProgramSegment(const elf_t *elf, size_t ph)
{
    size_t offset = elf_getProgramHeaderOffset(elf, ph);
    size_t file_size = elf_getProgramHeaderFileSize(elf, ph);
    size_t segment_end = offset + file_size;
    /* possible wraparound - check that segment end is not before segment start */
    if (elf->elfSize < segment_end || segment_end < offset) {
        return NULL;
    }

    return elf->elfFile + offset;
}

uint32_t elf_getProgramHeaderType(const elf_t *elfFile, size_t ph)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getProgramHeaderType(elfFile, ph);
    } else {
        return elf64_getProgramHeaderType(elfFile, ph);
    }
}

size_t elf_getProgramHeaderOffset(const elf_t *elfFile, size_t ph)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getProgramHeaderOffset(elfFile, ph);
    } else {
        return elf64_getProgramHeaderOffset(elfFile, ph);
    }
}

uintptr_t elf_getProgramHeaderVaddr(const elf_t *elfFile, size_t ph)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getProgramHeaderVaddr(elfFile, ph);
    } else {
        return elf64_getProgramHeaderVaddr(elfFile, ph);
    }
}

uintptr_t elf_getProgramHeaderPaddr(const elf_t *elfFile, size_t ph)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getProgramHeaderPaddr(elfFile, ph);
    } else {
        return elf64_getProgramHeaderPaddr(elfFile, ph);
    }
}

size_t elf_getProgramHeaderFileSize(const elf_t *elfFile, size_t ph)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getProgramHeaderFileSize(elfFile, ph);
    } else {
        return elf64_getProgramHeaderFileSize(elfFile, ph);
    }
}

size_t elf_getProgramHeaderMemorySize(const elf_t *elfFile, size_t ph)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getProgramHeaderMemorySize(elfFile, ph);
    } else {
        return elf64_getProgramHeaderMemorySize(elfFile, ph);
    }
}

uint32_t elf_getProgramHeaderFlags(const elf_t *elfFile, size_t ph)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getProgramHeaderFlags(elfFile, ph);
    } else {
        return elf64_getProgramHeaderFlags(elfFile, ph);
    }
}

size_t elf_getProgramHeaderAlign(const elf_t *elfFile, size_t ph)
{
    if (elf_isElf32(elfFile)) {
        return elf32_getProgramHeaderAlign(elfFile, ph);
    } else {
        return elf64_getProgramHeaderAlign(elfFile, ph);
    }
}


/* Utility functions */
int elf_getMemoryBounds(const elf_t *elfFile, elf_addr_type_t addr_type, uintptr_t *min, uintptr_t *max)
{
    uintptr_t mem_min = UINTPTR_MAX;
    uintptr_t mem_max = 0;
    size_t i;

    for (i = 0; i < elf_getNumProgramHeaders(elfFile); i++) {
        uintptr_t sect_min, sect_max;

        if (elf_getProgramHeaderMemorySize(elfFile, i) == 0) {
            continue;
        }

        if (addr_type == PHYSICAL) {
            sect_min = elf_getProgramHeaderPaddr(elfFile, i);
        } else {
            sect_min = elf_getProgramHeaderVaddr(elfFile, i);
        }

        sect_max = sect_min + elf_getProgramHeaderMemorySize(elfFile, i);

        if (sect_max > mem_max) {
            mem_max = sect_max;
        }
        if (sect_min < mem_min) {
            mem_min = sect_min;
        }
    }
    *min = mem_min;
    *max = mem_max;

    return 1;
}

int elf_vaddrInProgramHeader(const elf_t *elfFile, size_t ph, uintptr_t vaddr)
{
    uintptr_t min = elf_getProgramHeaderVaddr(elfFile, ph);
    uintptr_t max = min + elf_getProgramHeaderMemorySize(elfFile, ph);
    if (vaddr >= min && vaddr < max) {
        return 1;
    } else {
        return 0;
    }
}

uintptr_t elf_vtopProgramHeader(const elf_t *elfFile, size_t ph, uintptr_t vaddr)
{
    uintptr_t ph_phys = elf_getProgramHeaderPaddr(elfFile, ph);
    uintptr_t ph_virt = elf_getProgramHeaderVaddr(elfFile, ph);
    uintptr_t paddr;

    paddr = vaddr - ph_virt + ph_phys;

    return paddr;
}

int elf_loadFile(const elf_t *elf, elf_addr_type_t addr_type)
{
    size_t i;

    for (i = 0; i < elf_getNumProgramHeaders(elf); i++) {
        /* Load that section */
        uintptr_t dest, src;
        size_t len;
        if (addr_type == PHYSICAL) {
            dest = elf_getProgramHeaderPaddr(elf, i);
        } else {
            dest = elf_getProgramHeaderVaddr(elf, i);
        }
        len = elf_getProgramHeaderFileSize(elf, i);
        src = (uintptr_t) elf->elfFile + elf_getProgramHeaderOffset(elf, i);
        memcpy((void *) dest, (void *) src, len);
        dest += len;
        memset((void *) dest, 0, elf_getProgramHeaderMemorySize(elf, i) - len);
    }

    return 1;
}
