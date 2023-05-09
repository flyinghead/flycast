# Copyright 2019 - 2021 Alexander Grund
# Distributed under the Nowide Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wsuggest-override _NOWIDE_SUGGEST_OVERRIDE_SUPPORTED)

add_library(nowide_file_test_helpers STATIC file_test_helpers.cpp)
target_link_libraries(nowide_file_test_helpers PRIVATE PRIVATE nowide::nowide)
target_compile_definitions(nowide_file_test_helpers PRIVATE)

if(NOT TARGET tests)
  add_custom_target(tests)
endif()
  

# In some environments this test (part) may fail, so allow to disable it
option(NOWIDE_DISABLE_CIN_TEST "Disable integration test using console input" OFF)

function(nowide_add_test name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "COMPILE_ONLY" "SRC" "LIBRARIES;DEFINITIONS;ARGS")
  if(NOT ARG_SRC)
    set(ARG_SRC ${name}.cpp)
  endif()
  set(name ${PROJECT_NAME}-${name})
  
  add_executable(${name} ${ARG_SRC})
  add_dependencies(tests ${name})
  target_link_libraries(${name} PRIVATE nowide::nowide ${ARG_LIBRARIES})
  nowide_add_warnings(${name} pedantic ${NOWIDE_WERROR})
  if(_NOWIDE_SUGGEST_OVERRIDE_SUPPORTED)
    target_compile_options(${name} PRIVATE -Wsuggest-override -Wno-error=suggest-override)
  endif()
  target_compile_definitions(${name} PRIVATE ${ARG_DEFINITIONS})
  if(NOT ARG_COMPILE_ONLY)
    add_test(NAME ${name} COMMAND ${name} ${ARG_ARGS})
  endif()
endfunction()

nowide_add_test(test_codecvt)
nowide_add_test(test_convert)
nowide_add_test(test_env)
nowide_add_test(test_env_win SRC test_env.cpp DEFINITIONS NOWIDE_TEST_INCLUDE_WINDOWS)
nowide_add_test(test_filebuf LIBRARIES nowide_file_test_helpers)
nowide_add_test(test_ifstream LIBRARIES nowide_file_test_helpers)
nowide_add_test(test_ofstream LIBRARIES nowide_file_test_helpers)
nowide_add_test(test_fstream LIBRARIES nowide_file_test_helpers)
nowide_add_test(test_fstream_special LIBRARIES nowide_file_test_helpers)
nowide_add_test(test_iostream LIBRARIES nowide_file_test_helpers)
if(NOWIDE_DISABLE_CIN_TEST)
    target_compile_definitions(${PROJECT_NAME}-test_iostream PRIVATE NOWIDE_DISABLE_CIN_TEST)
endif()
nowide_add_test(test_iostream_interactive COMPILE_ONLY SRC test_iostream.cpp DEFINITIONS NOWIDE_TEST_INTERACTIVE LIBRARIES nowide_file_test_helpers)
nowide_add_test(test_stackstring)
nowide_add_test(test_stat)
nowide_add_test(test_stdio)
nowide_add_test(test_system_n SRC test_system.cpp DEFINITIONS NOWIDE_TEST_USE_NARROW=1)
if(WIN32)
  nowide_add_test(test_system_w SRC test_system.cpp DEFINITIONS NOWIDE_TEST_USE_NARROW=0)
  nowide_add_test(test_system_use_windows_h SRC test_system.cpp DEFINITIONS NOWIDE_TEST_USE_NARROW=0 BOOST_USE_WINDOWS_H)
  nowide_add_test(test_system_use_windows_h_lean SRC test_system.cpp DEFINITIONS NOWIDE_TEST_USE_NARROW=0 BOOST_USE_WINDOWS_H WIN32_LEAN_AND_MEAN)
else()
  foreach(test test_filebuf test_ifstream test_ofstream test_fstream test_fstream_special)
    nowide_add_test(${test}_internal SRC ${test}.cpp DEFINITIONS NOWIDE_USE_FILEBUF_REPLACEMENT=1 LIBRARIES nowide_file_test_helpers)
  endforeach()
endif()
nowide_add_test(test_traits)

# Test that passthrough writes everything from stdin to stdout
# Needs to be done with CMake as the test driver to write any input to stdin and check output
add_test(
  NAME ${PROJECT_NAME}-test_iostream_passthrough
  COMMAND ${CMAKE_COMMAND}
    -D TEST_BINARY=$<TARGET_FILE:${PROJECT_NAME}-test_iostream>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test_iostream_passthrough.cmake
)

nowide_add_test(benchmark_fstream COMPILE_ONLY DEFINITIONS NOWIDE_USE_FILEBUF_REPLACEMENT=1)
