/*
 * Copyright (c) 1999-2004 University of New South Wales
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>
#include <elf/elf.h>

/* ELF header functions */
int elf64_checkFile(elf_t *elf);

int elf64_checkProgramHeaderTable(const elf_t *elf);

int elf64_checkSectionTable(const elf_t *elf);

static inline bool elf_isElf64(const elf_t *elf)
{
    return elf->elfClass == ELFCLASS64;
}

static inline Elf64_Ehdr elf64_getHeader(const elf_t *elf)
{
    return *(Elf64_Ehdr *) elf->elfFile;
}

static inline uintptr_t elf64_getEntryPoint(const elf_t *file)
{
    return elf64_getHeader(file).e_entry;
}

static inline const Elf64_Phdr *elf64_getProgramHeaderTable(const elf_t *file)
{
    return file->elfFile + elf64_getHeader(file).e_phoff;
}

static inline const Elf64_Shdr *elf64_getSectionTable(const elf_t *file)
{
    return file->elfFile + elf64_getHeader(file).e_shoff;
}

static inline size_t elf64_getNumProgramHeaders(const elf_t *file)
{
    return elf64_getHeader(file).e_phnum;
}

static inline size_t elf64_getNumSections(const elf_t *elf)
{
    return elf64_getHeader(elf).e_shnum;
}

static inline size_t elf64_getSectionStringTableIndex(const elf_t *elf)
{
    return elf64_getHeader(elf).e_shstrndx;
}


/* Section header functions */
static inline size_t elf64_getSectionNameOffset(const elf_t *elf, size_t s)
{
    return elf64_getSectionTable(elf)[s].sh_name;
}

static inline uint32_t elf64_getSectionType(const elf_t *file, size_t s)
{
    return elf64_getSectionTable(file)[s].sh_type;
}

static inline size_t elf64_getSectionFlags(const elf_t *file, size_t s)
{
    return elf64_getSectionTable(file)[s].sh_flags;
}

static inline uintptr_t elf64_getSectionAddr(const elf_t *elf, size_t i)
{
    return elf64_getSectionTable(elf)[i].sh_addr;
}

static inline size_t elf64_getSectionOffset(const elf_t *elf, size_t i)
{
    return elf64_getSectionTable(elf)[i].sh_offset;
}

static inline size_t elf64_getSectionSize(const elf_t *elf, size_t i)
{
    return elf64_getSectionTable(elf)[i].sh_size;
}

static inline uint32_t elf64_getSectionLink(const elf_t *elf, size_t i)
{
    return elf64_getSectionTable(elf)[i].sh_link;
}

static inline uint32_t elf64_getSectionInfo(const elf_t *elf, size_t i)
{
    return elf64_getSectionTable(elf)[i].sh_info;
}

static inline size_t elf64_getSectionAddrAlign(const elf_t *elf, size_t i)
{
    return elf64_getSectionTable(elf)[i].sh_addralign;
}

static inline size_t elf64_getSectionEntrySize(const elf_t *elf, size_t i)
{
    return elf64_getSectionTable(elf)[i].sh_entsize;
}


/* Program header functions */
static inline uint32_t elf64_getProgramHeaderType(const elf_t *file, size_t ph)
{
    return elf64_getProgramHeaderTable(file)[ph].p_type;
}

static inline size_t elf64_getProgramHeaderOffset(const elf_t *file, size_t ph)
{
    return elf64_getProgramHeaderTable(file)[ph].p_offset;
}

static inline uintptr_t elf64_getProgramHeaderVaddr(const elf_t *file, size_t ph)
{
    return elf64_getProgramHeaderTable(file)[ph].p_vaddr;
}

static inline uintptr_t elf64_getProgramHeaderPaddr(const elf_t *file, size_t ph)
{
    return elf64_getProgramHeaderTable(file)[ph].p_paddr;
}

static inline size_t elf64_getProgramHeaderFileSize(const elf_t *file, size_t ph)
{
    return elf64_getProgramHeaderTable(file)[ph].p_filesz;
}

static inline size_t elf64_getProgramHeaderMemorySize(const elf_t *file, size_t ph)
{
    return elf64_getProgramHeaderTable(file)[ph].p_memsz;
}

static inline uint32_t elf64_getProgramHeaderFlags(const elf_t *file, size_t ph)
{
    return elf64_getProgramHeaderTable(file)[ph].p_flags;
}

static inline size_t elf64_getProgramHeaderAlign(const elf_t *file, size_t ph)
{
    return elf64_getProgramHeaderTable(file)[ph].p_align;
}

