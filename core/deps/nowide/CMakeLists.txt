cmake_minimum_required(VERSION 2.8.5)
if(NOT CMAKE_VERSION VERSION_LESS "3.0")
    cmake_policy(SET CMP0048 OLD)
endif()
if(NOT CMAKE_VERSION VERSION_LESS "3.1")
    cmake_policy(SET CMP0054 OLD)
endif()
if(NOT CMAKE_VERSION VERSION_LESS "3.3")
    cmake_policy(SET CMP0063 NEW)
    set(HONOR_VISIBILITY TRUE)
else()
    set(HONOR_VISIBILITY FALSE)
endif()

#
# Project Definition
#

project(nowide CXX)
set(NOWIDE_TARGET_NAME nowide)
set(TARGET_VER 0.0.0_neph)
set(TARGET_ABI_VER 0)
set(TARGET_OUTPUT_NAME "${NOWIDE_TARGET_NAME}")

#
# CMake Modules
#

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/contrib/CMake/modules")
include(CheckCXXCompilerFlag)
include(CMakeDependentOption)
include(GNUInstallDirs)
find_package(Doxygen)
find_package(HHC)
find_package(LATEX)
find_package(Perl)

#
# Include Files
#

include_directories("${PROJECT_SOURCE_DIR}/include")

#
# Project Options
#

option(NOWIDE_BUILD_SHARED "Build Shared Library" ON)
option(NOWIDE_BUILD_STATIC "Build Static Library" ON)
cmake_dependent_option(NOWIDE_BUILD_TESTS "Build NoWide Tests" ON "NOT CMAKE_TOOLCHAIN_FILE" OFF)
cmake_dependent_option(NOWIDE_BUILD_DOC_HTML "Build NoWide Documentation - HTML" ON "DOXYGEN_FOUND" OFF)
cmake_dependent_option(NOWIDE_BUILD_DOC_CHM "Build NoWide Documentation - CHM" ON "HHC_FOUND AND NOWIDE_BUILD_DOC_HTML" OFF)
cmake_dependent_option(NOWIDE_BUILD_DOC_TEX "Build NoWide Documentation - LaTeX" ON "DOXYGEN_FOUND AND LATEX_FOUND" OFF)

set(NOWIDE_HEADERS
	"${PROJECT_SOURCE_DIR}/include/nowide/config.hpp"
	"${PROJECT_SOURCE_DIR}/include/nowide/convert.hpp"
	"${PROJECT_SOURCE_DIR}/include/nowide/encoding_errors.hpp"
	"${PROJECT_SOURCE_DIR}/include/nowide/encoding_utf.hpp"
	"${PROJECT_SOURCE_DIR}/include/nowide/iostream.hpp"
	"${PROJECT_SOURCE_DIR}/include/nowide/scoped_ptr.hpp"
	"${PROJECT_SOURCE_DIR}/include/nowide/utf.hpp")

set(NOWIDE_SOURCE
	"${PROJECT_SOURCE_DIR}/src/iostream.cpp")

#
# Require C++11 Standard
#

if(NOT MSVC)
	if(NOT CMAKE_VERSION VERSION_LESS "3.1")
		set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
		set(CMAKE_CXX_STANDARD "11")
	else()
		check_cxx_compiler_flag("-std=gnu++11" _COMPILER_HAS_STD_GNUPP11)
		if(_COMPILER_HAS_STD_GNUPP11)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
		else()
			check_cxx_compiler_flag("-std=c++11" _COMPILER_HAS_STD_GNUPP0X)
			if(_COMPILER_HAS_STD_GNUPP0X)
				set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x")
			else()
				check_cxx_compiler_flag("-std=c++11" _COMPILER_HAS_STD_CPP11)
				if(_COMPILER_HAS_STD_CPP11)
					set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
				endif()
			endif()
		endif()
	endif()
endif()

#
# MSVC Runtime DLL
#

cmake_dependent_option(NOWIDE_USE_MSVC_RUNTIME "Use MSVC Runtime Library DLL" ON MSVC OFF)
if(MSVC AND NOT NOWIDE_USE_MSVC_RUNTIME)
	foreach(flag CMAKE_C_FLAGS
			CMAKE_C_FLAGS_DEBUG
			CMAKE_C_FLAGS_RELEASE
			CMAKE_C_FLAGS_MINSIZEREL
			CMAKE_C_FLAGS_RELWITHDEBINFO)
		string(REGEX REPLACE "/MDd" "/MTd" ${flag} "${${flag}}")
		string(REGEX REPLACE "/MD" "/MT" ${flag} "${${flag}}")
	endforeach()
endif()

#
# Compiler Flags
#

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
else()
	check_cxx_compiler_flag("-fno-strict-aliasing" _COMPILER_HAS_NO_STRICT_ALIASING)
	check_cxx_compiler_flag("-fvisibility=hidden" _COMPILER_HAS_VISIBILITY_HIDDEN)
	check_cxx_compiler_flag("-Wall" _COMPILER_HAS_WARN_ALL)
	check_cxx_compiler_flag("-Wextra" _COMPILER_HAS_WARN_EXTRA)
	check_cxx_compiler_flag("-Wno-unused-parameter" _COMPILER_HAS_NO_WARN_UNUSED_PARAMETER)
	if(_COMPILER_HAS_NO_STRICT_ALIASING)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
	endif()
	if(_COMPILER_HAS_WARN_ALL)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
	endif()
	if(_COMPILER_HAS_WARN_EXTRA)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
	endif()
	if(_COMPILER_HAS_NO_WARN_UNUSED_PARAMETER)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
	endif()
endif()

#
# Linker Flags
#

if(NOT MSVC)
	set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})

	set(CMAKE_REQUIRED_FLAGS "-Wl,--no-undefined")
	check_cxx_compiler_flag("" _LINKER_HAS_NO_UNDEFINED)
	if (_LINKER_HAS_NO_UNDEFINED)
		set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined ${CMAKE_SHARED_LINKER_FLAGS}")
	endif()

	if (MINGW)
		set(CMAKE_REQUIRED_FLAGS "-Wl,--nxcompat")
		check_cxx_compiler_flag("" _LINKER_HAS_DEP)
		if (_LINKER_HAS_DEP)
			set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--nxcompat ${CMAKE_SHARED_LINKER_FLAGS}")
		endif()
		set(CMAKE_REQUIRED_FLAGS "-Wl,--dynamicbase")
		check_cxx_compiler_flag("" _LINKER_HAS_ASLR)
		if (_LINKER_HAS_ASLR)
			set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--dynamicbase ${CMAKE_SHARED_LINKER_FLAGS}")
		endif()
		set(CMAKE_REQUIRED_FLAGS "-Wl,--high-entropy-va")
		check_cxx_compiler_flag("" _LINKER_HAS_64ASLR)
		if (_LINKER_HAS_64ASLR)
			set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--high-entropy-va ${CMAKE_SHARED_LINKER_FLAGS}")
		endif()
	endif()
	
	set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
endif()

#
# Build Targets
#

set(NOWIDE_TARGET_BUILT "")
set(NOWIDE_TARGET_TYPES "")

if(NOWIDE_BUILD_SHARED)
	list(APPEND NOWIDE_TARGET_TYPES "shared")
	add_library("${NOWIDE_TARGET_NAME}_shared" SHARED ${NOWIDE_SOURCE} ${NOWIDE_HEADERS})
	if(WIN32 OR ANDROID OR MSYS)
		set_target_properties("${NOWIDE_TARGET_NAME}_shared" PROPERTIES
			OUTPUT_NAME "${TARGET_OUTPUT_NAME}${TARGET_ABI_VER}")
	else()
		set_target_properties("${NOWIDE_TARGET_NAME}_shared" PROPERTIES
			OUTPUT_NAME "${TARGET_OUTPUT_NAME}")
	endif()
	if(NOT ANDROID)
		set_target_properties("${NOWIDE_TARGET_NAME}_shared" PROPERTIES
			VERSION "${TARGET_VER}"
			SOVERSION "${TARGET_ABI_VER}")
	endif()
	set_target_properties("${NOWIDE_TARGET_NAME}_shared" PROPERTIES
		COMPILE_DEFINITIONS "NOWIDE_DLL")
endif()

if(NOWIDE_BUILD_STATIC)
	list(APPEND NOWIDE_TARGET_TYPES "static")
	add_library("${NOWIDE_TARGET_NAME}_static" STATIC ${NOWIDE_SOURCE} ${NOWIDE_HEADERS})
	if(MSVC)
		set_target_properties("${NOWIDE_TARGET_NAME}_static" PROPERTIES
			OUTPUT_NAME "${TARGET_OUTPUT_NAME}${TARGET_ABI_VER}_static")
	else()
		set_target_properties("${NOWIDE_TARGET_NAME}_static" PROPERTIES
			OUTPUT_NAME "${TARGET_OUTPUT_NAME}${TARGET_ABI_VER}")
	endif()
endif()

foreach(NOWIDE_TARGET_TYPE ${NOWIDE_TARGET_TYPES})
	list(APPEND NOWIDE_TARGET_BUILT "${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE}")
	if(NOT MSYS)
		target_link_libraries("${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE}" ${CMAKE_DL_LIBS})
	endif()
	if(HONOR_VISILIBITY)
		set_target_properties("${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE}" PROPERTIES
			C_VISIBILITY_PRESET hidden)
	elseif(_COMPILER_HAS_VISIBILITY_HIDDEN)
		set_target_properties("${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE}" PROPERTIES
			COMPILE_FLAGS "-fvisibility=hidden")
	endif()
	set_target_properties("${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE}" PROPERTIES
		POSITION_INDEPENDENT_CODE ON)
endforeach()

#
# Documentation
#

if(NOWIDE_BUILD_DOC_HTML)
	set(NOWIDE_DOXYGEN_HTML YES)
else()
	set(NOWIDE_DOXYGEN_HTML NO)
endif()

if(NOWIDE_BUILD_DOC_CHM)
	set(NOWIDE_DOXYGEN_HTMLHELP YES)
else()
	set(NOWIDE_DOXYGEN_HTMLHELP NO)
endif()

if(NOWIDE_BUILD_DOC_TEX)
	set(NOWIDE_DOXYGEN_LATEX YES)
else()
	set(NOWIDE_DOXYGEN_LATEX NO)
endif()

if(NOWIDE_BUILD_DOC_HTML AND NOT LATEX_FOUND)
	set(NOWIDE_DOXYGEN_MATHJAX YES)
else()
	set(NOWIDE_DOXYGEN_MATHJAX NO)
endif()

if(DOXYGEN_DOT_FOUND)
	set(NOWIDE_DOXYGEN_DOT YES)
else()
	set(NOWIDE_DOXYGEN_DOT NO)
endif()

if(NOWIDE_BUILD_DOC_HTML OR NOWIDE_BUILD_DOC_TEX)
	configure_file("Doxyfile.in" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" @ONLY)
	add_custom_target(doc "${DOXYGEN_EXECUTABLE}" Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			COMMENT "Generating API Documentation" VERBATIM)
endif()

#
# Tests
#

if(NOWIDE_BUILD_TESTS)
	enable_testing()
	foreach(NOWIDE_TARGET_TYPE ${NOWIDE_TARGET_TYPES})
		add_executable(test_${NOWIDE_TARGET_TYPE}_convert test/test_convert.cpp)
		target_link_libraries(test_${NOWIDE_TARGET_TYPE}_convert ${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE})
		add_test(test_${NOWIDE_TARGET_TYPE}_convert test_${NOWIDE_TARGET_TYPE}_convert)
		if("${NOWIDE_TARGET_TYPE}" STREQUAL "shared")
			set_target_properties(test_${NOWIDE_TARGET_TYPE}_convert PROPERTIES COMPILE_DEFINITIONS "NOWIDE_DLL")
		endif()
		
		add_executable(test_${NOWIDE_TARGET_TYPE}_stdio test/test_stdio.cpp)
		target_link_libraries(test_${NOWIDE_TARGET_TYPE}_stdio ${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE})
		add_test(test_${NOWIDE_TARGET_TYPE}_stdio test_${NOWIDE_TARGET_TYPE}_stdio)
		if("${NOWIDE_TARGET_TYPE}" STREQUAL "shared")
			set_target_properties(test_${NOWIDE_TARGET_TYPE}_stdio PROPERTIES COMPILE_DEFINITIONS "NOWIDE_DLL")
		endif()

		if(NOT APPLE)
			# OSX fstream is bugged?
			add_executable(test_${NOWIDE_TARGET_TYPE}_fstream test/test_fstream.cpp)
			target_link_libraries(test_${NOWIDE_TARGET_TYPE}_fstream ${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE})
			add_test(test_${NOWIDE_TARGET_TYPE}_fstream test_${NOWIDE_TARGET_TYPE}_fstream)
			if("${NOWIDE_TARGET_TYPE}" STREQUAL "shared")
				set_target_properties(test_${NOWIDE_TARGET_TYPE}_fstream PROPERTIES COMPILE_DEFINITIONS "NOWIDE_DLL")
			endif()
		endif()
		
		add_executable(test_${NOWIDE_TARGET_TYPE}_iostream test/test_iostream.cpp)
		target_link_libraries(test_${NOWIDE_TARGET_TYPE}_iostream ${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE})
		add_test(test_${NOWIDE_TARGET_TYPE}_iostream test_${NOWIDE_TARGET_TYPE}_iostream)
		if("${NOWIDE_TARGET_TYPE}" STREQUAL "shared")
			set_target_properties(test_${NOWIDE_TARGET_TYPE}_iostream PROPERTIES COMPILE_DEFINITIONS "NOWIDE_DLL")
		endif()
		
		add_executable(test_${NOWIDE_TARGET_TYPE}_env_proto test/test_env.cpp)
		target_link_libraries(test_${NOWIDE_TARGET_TYPE}_env_proto ${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE})
		add_test(test_${NOWIDE_TARGET_TYPE}_env_proto test_${NOWIDE_TARGET_TYPE}_env_proto)
		if("${NOWIDE_TARGET_TYPE}" STREQUAL "shared")
			set_target_properties(test_${NOWIDE_TARGET_TYPE}_env_proto PROPERTIES COMPILE_DEFINITIONS "NOWIDE_DLL")
		endif()
		
		if(WIN32 OR MSYS)
			add_executable(test_${NOWIDE_TARGET_TYPE}_env_win test/test_env.cpp)
			target_link_libraries(test_${NOWIDE_TARGET_TYPE}_env_win ${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE})
			add_test(test_${NOWIDE_TARGET_TYPE}_env_win test_${NOWIDE_TARGET_TYPE}_env_win)
			if("${NOWIDE_TARGET_TYPE}" STREQUAL "shared")
				set_target_properties(test_${NOWIDE_TARGET_TYPE}_env_win PROPERTIES
						COMPILE_DEFINITIONS "NOWIDE_DLL;NOWIDE_USE_WINDOWS_H")
			else()
				set_target_properties(test_${NOWIDE_TARGET_TYPE}_env_win PROPERTIES
						COMPILE_DEFINITIONS "NOWIDE_USE_WINDOWS_H")
			endif()
		endif()
		
		if(NOT MSYS)
			# This test fails on MSYS2 MinTTY
			add_executable(test_${NOWIDE_TARGET_TYPE}_system test/test_system.cpp)
			target_link_libraries(test_${NOWIDE_TARGET_TYPE}_system ${NOWIDE_TARGET_NAME}_${NOWIDE_TARGET_TYPE})
			add_test(test_${NOWIDE_TARGET_TYPE}_system_n test_${NOWIDE_TARGET_TYPE}_system "-n")
			add_test(test_${NOWIDE_TARGET_TYPE}_system_w test_${NOWIDE_TARGET_TYPE}_system "-w")
			if("${NOWIDE_TARGET_TYPE}" STREQUAL "shared")
				set_target_properties(test_${NOWIDE_TARGET_TYPE}_system PROPERTIES COMPILE_DEFINITIONS "NOWIDE_DLL")
			endif()
		endif()
	endforeach()
endif()

#
# Install
#

install(TARGETS ${NOWIDE_TARGET_BUILT}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/nowide" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(MSVC)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Debug/${TARGET_OUTPUT_NAME}${TARGET_ABI_VER}.pdb" DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Debug)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/${TARGET_OUTPUT_NAME}${TARGET_ABI_VER}.pdb" DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS RelWithDebInfo)
endif()
install(FILES "${PROJECT_SOURCE_DIR}/COPYING" DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES "${PROJECT_SOURCE_DIR}/README.md" DESTINATION ${CMAKE_INSTALL_DOCDIR})
if(NOWIDE_BUILD_DOC_HTML)
	install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html" DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()
if(NOWIDE_BUILD_DOC_TEX)
	install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/latex" DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()
if(NOWIDE_BUILD_DOC_CHM)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${NOWIDE_TARGET_NAME}.chm" DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()
