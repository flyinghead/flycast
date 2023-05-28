cmake_minimum_required(VERSION 2.6)
project(sh4asm)
set(sh4asm_VERSION_MAJOR 0)
set(sh4asm_VERSION_MINOR 1)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

enable_testing()

if(UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

set(sh4asm_core_sources
    "sh4asm_core/sh4_opcodes.h"
    "sh4asm_core/sh4asm_bin_emit.h"
    "sh4asm_core/sh4asm_txt_emit.h"
    "sh4asm_core/lexer.c"
    "sh4asm_core/lexer.h"
    "sh4asm_core/parser.c"
    "sh4asm_core/parser.h"
    "sh4asm_core/sh4asm.h"
    "sh4asm_core/sh4asm.c"
    "sh4asm_core/disas.h")

include_directories("${PROJECT_SOURCE_DIR}/sh4asm_core")

add_library(sh4asm_core ${sh4asm_core_sources})

add_executable(sh4asm "sh4asm/main.c")
target_link_libraries(sh4asm sh4asm_core)

add_executable(sh4asm_test "${PROJECT_SOURCE_DIR}/test/sh4asm_test.c")
target_link_libraries(sh4asm_test sh4asm_core)
add_test(NAME sh4asm_test COMMAND ./sh4asm_test)
