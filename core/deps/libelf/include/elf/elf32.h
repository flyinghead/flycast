/*
 * Copyright (c) 1999-2004 University of New South Wales
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>
#include <elf/elf.h>

/* ELF header functions */
int elf32_checkFile(elf_t *elf);

int elf32_checkProgramHeaderTable(const elf_t *elf);

int elf32_checkSectionTable(const elf_t *elf);

static inline bool elf_isElf32(const elf_t *elf)
{
    return elf->elfClass == ELFCLASS32;
}

static inline Elf32_Ehdr elf32_getHeader(const elf_t *elf)
{
    return *(Elf32_Ehdr *) elf->elfFile;
}

static inline uintptr_t elf32_getEntryPoint(const elf_t *elf)
{
    return elf32_getHeader(elf).e_entry;
}

static inline const Elf32_Phdr *elf32_getProgramHeaderTable(const elf_t *file)
{
    return file->elfFile + elf32_getHeader(file).e_phoff;
}

static inline const Elf32_Shdr *elf32_getSectionTable(const elf_t *elf)
{
    return elf->elfFile + elf32_getHeader(elf).e_shoff;
}

static inline size_t elf32_getNumProgramHeaders(const elf_t *elf)
{
    return elf32_getHeader(elf).e_phnum;
}

static inline size_t elf32_getNumSections(const elf_t *elf)
{
    return elf32_getHeader(elf).e_shnum;
}

static inline size_t elf32_getSectionStringTableIndex(const elf_t *elf)
{
    return elf32_getHeader(elf).e_shstrndx;
}


/* Section header functions */
static inline size_t elf32_getSectionNameOffset(const elf_t *elf, size_t s)
{
    return elf32_getSectionTable(elf)[s].sh_name;
}

static inline uint32_t elf32_getSectionType(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_type;
}

static inline size_t elf32_getSectionFlags(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_flags;
}

static inline uintptr_t elf32_getSectionAddr(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_addr;
}

static inline size_t elf32_getSectionOffset(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_offset;
}

static inline size_t elf32_getSectionSize(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_size;
}

static inline uint32_t elf32_getSectionLink(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_link;
}

static inline uint32_t elf32_getSectionInfo(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_info;
}

static inline size_t elf32_getSectionAddrAlign(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_addralign;
}

static inline size_t elf32_getSectionEntrySize(const elf_t *elf, size_t i)
{
    return elf32_getSectionTable(elf)[i].sh_entsize;
}


/* Program header functions */
static inline uint32_t elf32_getProgramHeaderType(const elf_t *file, size_t ph)
{
    return elf32_getProgramHeaderTable(file)[ph].p_type;
}

static inline size_t elf32_getProgramHeaderOffset(const elf_t *file, size_t ph)
{
    return elf32_getProgramHeaderTable(file)[ph].p_offset;
}

static inline uintptr_t elf32_getProgramHeaderVaddr(const elf_t *file, size_t ph)
{
    return elf32_getProgramHeaderTable(file)[ph].p_vaddr;
}

static inline uintptr_t elf32_getProgramHeaderPaddr(const elf_t *file, size_t ph)
{
    return elf32_getProgramHeaderTable(file)[ph].p_paddr;
}

static inline size_t elf32_getProgramHeaderFileSize(const elf_t *file, size_t ph)
{
    return elf32_getProgramHeaderTable(file)[ph].p_filesz;
}

static inline size_t elf32_getProgramHeaderMemorySize(const elf_t *file, size_t ph)
{
    return elf32_getProgramHeaderTable(file)[ph].p_memsz;
}

static inline uint32_t elf32_getProgramHeaderFlags(const elf_t *file, size_t ph)
{
    return elf32_getProgramHeaderTable(file)[ph].p_flags;
}

static inline size_t elf32_getProgramHeaderAlign(const elf_t *file, size_t ph)
{
    return elf32_getProgramHeaderTable(file)[ph].p_align;
}

