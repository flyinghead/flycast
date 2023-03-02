#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

project(libelf C)

add_library(elf EXCLUDE_FROM_ALL src/elf.c src/elf32.c src/elf64.c)
target_include_directories(elf PUBLIC include)
target_link_libraries(elf muslc)
