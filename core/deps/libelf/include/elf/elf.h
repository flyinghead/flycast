/*
 * Copyright (c) 1999-2004 University of New South Wales
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
  Authors: Luke Deller, Ben Leslie
  Created: 24/Sep/1999
*/

/**
\file

\brief Generic ELF library

The ELF library is designed to make the task of parsing and getting information
out of an ELF file easier.

It provides function to obtain the various different fields in the ELF header, and
the program and segment information.

Also importantly, it provides a function elf_loadFile which will load a given
ELF file into memory.

*/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <elf.h>

struct elf {
    void const *elfFile;
    size_t elfSize;
    unsigned char elfClass; /* 32-bit or 64-bit */
};
typedef struct elf elf_t;

enum elf_addr_type {
    VIRTUAL,
    PHYSICAL
};
typedef enum elf_addr_type elf_addr_type_t;

/* ELF header functions */
/**
 * Initialises an elf_t structure and checks that the ELF file is valid.
 * This function must be called to validate the ELF before any other function.
 * Otherwise, attempting to call other functions with an invalid ELF file may
 * result in undefined behaviour.
 *
 * @param file ELF file to use
 * @param size Size of the ELF file
 * @param res elf_t to initialise
 *
 * \return 0 on success, otherwise < 0
 */
int elf_newFile(const void *file, size_t size, elf_t *res);

/**
 * Initialises and elf_t structure and checks that the ELF file is valid.
 * The validity of a potential ELF file can be determined by the arguments
 * check_pht and check_st.
 * If both check_pht and check_st are true, this function is equivalent to
 * elf_newFile.
 * Calling other functions with an invalid ELF file may result in undefined
 * behaviour.
 *
 * @param file ELF file to use
 * @param size Size of the ELF file
 * @param check_pht Whether to check the ELF program header table is valid
 * @param check_st Whether to check the ELF section table is valid
 * @param res elf_t to initialise
 *
 * \return 0 on success, otherwise < 0
 */
int elf_newFile_maybe_unsafe(const void *file, size_t size, bool check_pht, bool check_st, elf_t *res);

/**
 * Checks that file starts with the ELF magic number.
 * File must be at least 4 bytes (SELFMAG).
 *
 * @param file to check
 *
 * \return 0 on success, otherwise < 0
 */
int elf_check_magic(const char *file);

/**
 * Checks that elfFile points to an ELF file with a valid ELF header.
 *
 * @param elfFile Potential ELF file to check
 *
 * \return 0 on success, otherwise < 0
 */
int elf_checkFile(elf_t *elfFile);

/**
 * Checks that elfFile points to an ELF file with a valid program header table.
 *
 * @param elfFile Potential ELF file to check
 *
 * \return 0 on success, otherwise < 0
 */
int elf_checkProgramHeaderTable(const elf_t *elfFile);

/**
 * Checks that elfFile points to an ELF file with a valid section table.
 *
 * @param elfFile Potential ELF file to check
 *
 * \return 0 on success, otherwise < 0
 */
int elf_checkSectionTable(const elf_t *elfFile);

/**
 * Find the entry point of an ELF file.
 *
 * @param elfFile Pointer to a valid ELF structure
 *
 * \return The entry point address.
 */
uintptr_t elf_getEntryPoint(const elf_t *elfFile);

/**
 * Determine number of program headers in an ELF file.
 *
 * @param elfFile Pointer to a valid ELF structure.
 *
 * \return Number of program headers in the ELF file.
 */
size_t elf_getNumProgramHeaders(const elf_t *elfFile);

/**
 * Determine number of sections in an ELF file.
 *
 * @param elfFile Pointer to a valid ELF structure.
 *
 * \return Number of sections in the ELF file.
 */
size_t elf_getNumSections(const elf_t *elfFile);

/**
 * Get the index of the section header string table of an ELF file.
 *
 * @param elf Pointer to a valid ELF structure.
 *
 * \return The index of the section header string table.
 */
size_t elf_getSectionStringTableIndex(const elf_t *elf);

/**
 * Get a string table section of an ELF file.
 *
 * @param elfFile Pointer to a valid ELF structure.
 * @param string_section The section number of the string table.
 *
 * \return The string table, or NULL if the section is not a string table.
 */
const char *elf_getStringTable(const elf_t *elfFile, size_t string_segment);

/**
 * Get the string table for section header names.
 *
 * @param elfFile Pointer to a valid ELF structure.
 *
 * \return The string table, or NULL if there is no table.
 */
const char *elf_getSectionStringTable(const elf_t *elfFile);


/* Section header functions */
/**
 * Get a section of an ELF file.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i The section number
 *
 * \return The section, or NULL if there is no section.
 */
const void *elf_getSection(const elf_t *elfFile, size_t i);

/**
 * Get the section of an ELF file with a given name.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param str Name of the section
 * @param i Pointer to store the section number
 *
 * \return The section, or NULL if there is no section.
 */
const void *elf_getSectionNamed(const elf_t *elfFile, const char *str, size_t *i);

/**
 * Return the name of a given section.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The name of a given section.
 */
const char *elf_getSectionName(const elf_t *elfFile, size_t i);

/**
 * Return the offset to the name of a given section in the section header
 * string table.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The offset to the name of a given section in the section header
 * string table.
 */
size_t elf_getSectionNameOffset(const elf_t *elfFile, size_t i);

/**
 * Return the type of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The type of a given section.
 */
uint32_t elf_getSectionType(const elf_t *elfFile, size_t i);

/**
 * Return the flags of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The flags of a given section.
 */
size_t elf_getSectionFlags(const elf_t *elfFile, size_t i);

/**
 * Return the address of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The address of a given section.
 */
uintptr_t elf_getSectionAddr(const elf_t *elfFile, size_t i);

/**
 * Return the offset of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The offset of a given section.
 */
size_t elf_getSectionOffset(const elf_t *elfFile, size_t i);

/**
 * Return the size of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The size of a given section.
 */
size_t elf_getSectionSize(const elf_t *elfFile, size_t i);

/**
 * Return the related section index of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The related section index of a given section.
 */
uint32_t elf_getSectionLink(const elf_t *elfFile, size_t i);

/**
 * Return extra information of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return Extra information of a given section.
 */
uint32_t elf_getSectionInfo(const elf_t *elfFile, size_t i);

/**
 * Return the alignment of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The alignment of a given section.
 */
size_t elf_getSectionAddrAlign(const elf_t *elfFile, size_t i);

/**
 * Return the entry size of a given section
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param i Index of the section
 *
 * \return The entry size of a given section.
 */
size_t elf_getSectionEntrySize(const elf_t *elfFile, size_t i);


/* Program header functions */

/**
 * Return the segment data for a given program header.
 *
 * @param elf Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return Pointer to the segment data
 */
const void *elf_getProgramSegment(const elf_t *elf, size_t ph);

/**
 * Return the type for a given program header.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return The type of a given program header.
 */
uint32_t elf_getProgramHeaderType(const elf_t *elfFile, size_t ph);

/**
 * Return the segment offset for a given program header.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return The offset of this program header from the start of the file.
 */
size_t elf_getProgramHeaderOffset(const elf_t *elfFile, size_t ph);

/**
 * Return the base virtual address of given program header.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return The memory size of the specified program header.
 */
uintptr_t elf_getProgramHeaderVaddr(const elf_t *elfFile, size_t ph);

/**
 * Return the base physical address of given program header.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return The memory size of the specified program header.
 */
uintptr_t elf_getProgramHeaderPaddr(const elf_t *elfFile, size_t ph);

/**
 * Return the file size of a given program header.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return The file size of the specified program header.
 */
size_t elf_getProgramHeaderFileSize(const elf_t *elfFile, size_t ph);

/**
 * Return the memory size of a given program header.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return The memory size of the specified program header.
 */
size_t elf_getProgramHeaderMemorySize(const elf_t *elfFile, size_t ph);

/**
 * Return the flags for a given program header.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return The flags of a given program header.
 */
uint32_t elf_getProgramHeaderFlags(const elf_t *elfFile, size_t ph);

/**
 * Return the alignment for a given program header.
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param ph Index of the program header
 *
 * \return The alignment of the given program header.
 */
size_t elf_getProgramHeaderAlign(const elf_t *elfFile, size_t ph);


/* Utility functions */

/**
 * Determine the memory bounds of an ELF file
 *
 * @param elfFile Pointer to a valid ELF structure
 * @param addr_type If PHYSICAL return bounds of physical memory, otherwise
 *                  return bounds of virtual memory
 * @param min Pointer to return value of the minimum
 * @param max Pointer to return value of the maximum
 *
 * \return true on success. false on failure, if for example, it is an invalid ELF file
 */
int elf_getMemoryBounds(const elf_t *elfFile, elf_addr_type_t addr_type, uintptr_t *min, uintptr_t *max);

/**
 *
 * \return true if the address in in this program header
 */
int elf_vaddrInProgramHeader(const elf_t *elfFile, size_t ph, uintptr_t vaddr);

/**
 * Return the physical translation of a physical address, with respect
 * to a given program header
 *
 */
uintptr_t elf_vtopProgramHeader(const elf_t *elfFile, size_t ph, uintptr_t vaddr);

/**
 * Load an ELF file into memory
 *
 * @param elfFile Pointer to a valid ELF file
 * @param addr_type If PHYSICAL load using the physical address, otherwise using the
 *                  virtual addresses
 *
 * \return true on success, false on failure.
 *
 * The function assumes that the ELF file is loaded in memory at some
 * address different to the target address at which it will be loaded.
 * It also assumes direct access to the source and destination address, i.e:
 * Memory must be able to be loaded with a simple memcpy.
 *
 * Obviously this also means that if we are loading a 64bit ELF on a 32bit
 * platform, we assume that any memory addresses are within the first 4GB.
 *
 */
int elf_loadFile(const elf_t *elfFile, elf_addr_type_t addr_type);
