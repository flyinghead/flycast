# Copyright 2019 - 2020 Alexander Grund
# Distributed under the Nowide Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

# Builds the libraries for Nowide.Nowide
#
# Options:
# NOWIDE_INSTALL
# NOWIDE_WERROR
# BUILD_TESTING
#
# Created target: nowide::nowide
#
# When not using CMake to link against the shared library on windows define -DNOWIDE_DYN_LINK

cmake_minimum_required(VERSION 3.9)
# Version number starts at 10 to avoid conflicts with Nowide version
set(_version 11.3.0)
project(nowide VERSION ${_version} LANGUAGES CXX)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  # Make sure all binarys (especially exe/dll) are in one directory for tests to work
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

include(NowideAddWarnings)
include(CheckCXXSourceCompiles)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(CTest)
  set(def_INSTALL ON)
  set(def_WERROR ON)
else()
  set(def_INSTALL OFF)
  set(def_WERROR OFF)
endif()

  option(NOWIDE_INSTALL "Install library" "${def_INSTALL}")
option(NOWIDE_WERROR "Treat warnings as errors" "${def_WERROR}")


file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/check_lfs_support.cpp lfsSource)
check_cxx_source_compiles("${lfsSource}" NOWIDE_HAS_LFS)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/check_attribute_init_priority.cpp attributeInitPrioritySource)
check_cxx_source_compiles("${attributeInitPrioritySource}" NOWIDE_HAS_INIT_PRIORITY)

# Using glob here is ok as it is only for headers
file(GLOB_RECURSE headers include/*.hpp)
add_library(nowide src/console_buffer.cpp src/cstdio.cpp src/cstdlib.cpp src/filebuf.cpp src/iostream.cpp src/stat.cpp ${headers})
add_library(nowide::nowide ALIAS nowide)
set_target_properties(nowide PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    EXPORT_NAME nowide
)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(nowide PUBLIC NOWIDE_DYN_LINK)
endif()
if(NOT NOWIDE_HAS_LFS)
  target_compile_definitions(nowide PRIVATE NOWIDE_NO_LFS)
endif()
if(NOWIDE_HAS_INIT_PRIORITY)
  target_compile_definitions(nowide PRIVATE NOWIDE_HAS_INIT_PRIORITY)
endif()
target_compile_definitions(nowide PUBLIC NOWIDE_NO_LIB)
target_include_directories(nowide PUBLIC include)
nowide_add_warnings(nowide pedantic ${NOWIDE_WERROR})
target_compile_features(nowide PUBLIC cxx_std_11)


if(BUILD_TESTING)
  add_subdirectory(test)
endif()

if(NOWIDE_INSTALL)
  include(InstallTargets)
  install_targets(TARGETS nowide NAMESPACE nowide)
endif()
