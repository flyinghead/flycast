if(APPLE AND NOT LIBRETRO)
	# Requirement from SDL
	# Minimum version for $<LINK_LIBRARY:feature,library-list>
	cmake_minimum_required(VERSION 3.24)
else()
	cmake_minimum_required(VERSION 3.22.1)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
	set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "Compiler launcher for C.")
	set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "Compiler launcher for CXX.")
endif()

set(SENTRY_UPLOAD_URL "" CACHE STRING "Sentry upload URL")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shell/cmake")

# Set WINDOWS_STORE flag for UWP builds
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	set(WINDOWS_STORE TRUE)
endif()

if(APPLE)
	if(CMAKE_SYSTEM_NAME STREQUAL iOS)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS deployment version")
		set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
	elseif(CMAKE_SYSTEM_NAME STREQUAL tvOS)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum tvOS deployment version")
		set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
	else()
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")
		set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
	endif()
	set(ZLIB_LIBRARY "-lz" CACHE STRING "Use generic linker flag for Xcode to support multiple SDKs")
endif()

if(LIBRETRO)
	project(flycast_libretro)
else()
	project(flycast)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_SYSTEM_NAME MATCHES "NetBSD|Haiku" OR NINTENDO_SWITCH)
	set(CMAKE_CXX_EXTENSIONS ON)
else()
	set(CMAKE_CXX_EXTENSIONS OFF)
endif()

include(DetectArchitecture)
if(NOT DEFINED ARCHITECTURE)
	message(FATAL_ERROR "Unsupported architecture encountered. Ending CMake generation.")
endif()
message(STATUS "Target architecture: ${ARCHITECTURE}")

set(USE_HOST_SDL_DEFAULT OFF)
if(NOT APPLE AND (NOT UNIX OR CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|OpenBSD|NetBSD|Haiku"))
	set(USE_HOST_SDL_DEFAULT ON)
endif()

option(ENABLE_CTEST "Enables unit tests" OFF)
option(ENABLE_OPROFILE "Enable OProfile" OFF)
option(TEST_AUTOMATION "Enable test automation" OFF)
option(ENABLE_LOG "Enable full logging" OFF)
option(ASAN "Enable address sanitizer" OFF)
option(USE_GLES "Use GLES[3] API" OFF)
option(USE_GLES2 "Use GLES2 API" OFF)
option(USE_HOST_GLSLANG "Use host glslang" OFF)
option(USE_HOST_LIBZIP "Use host libzip" ON)
option(USE_HOST_SDL "Use host SDL library" ${USE_HOST_SDL_DEFAULT})
option(USE_HOST_LIBCHDR "Use host libchdr" OFF)
option(USE_OPENMP "Use OpenMP if available" ON)
option(USE_VULKAN "Build with Vulkan support" ON)
option(USE_DX9 "Build with Direct3D 9 support" ON)
option(USE_DX11 "Build with Direct3D 11 support" ON)
option(LIBRETRO "Build libretro core" OFF)
option(USE_OPENGL "Use OpenGL API" ON)
option(USE_VIDEOCORE "RPI: use the legacy Broadcom GLES libraries" OFF)
option(USE_ALSA "Build with ALSA support" ON)
option(USE_LIBAO "Build with AO support" ON)
option(USE_OSS "Build with OSS support" OFF)
option(USE_PULSEAUDIO "Build with PulseAudio support" ON)
option(USE_BREAKPAD "Build and link with breakpad library" ON)
option(USE_LUA "Build with Lua support" ON)
option(ENABLE_GDB_SERVER "Build with GDB debugging support" OFF)
option(ENABLE_DC_PROFILER "Build with support for target machine (SH4) profiler" OFF)
option(ENABLE_FC_PROFILER "Build with support for host app (Flycast) profiler" OFF)
option(USE_DISCORD "Use Discord Presence API" OFF)
option(USE_LIBCDIO "Use libcdio for CDROM access" OFF)

if(IOS AND NOT LIBRETRO)
	set(USE_VULKAN OFF CACHE BOOL "Force vulkan off" FORCE)
endif()

include(GNUInstallDirs)
include(CMakeRC)

if(ENABLE_CTEST)
	include(CTest)
	add_compile_definitions(FLYCAST_TEST_FILES="${CMAKE_CURRENT_SOURCE_DIR}/tests/files")
endif()

if(IOS AND NOT LIBRETRO)
	set(CMAKE_Swift_LANGUAGE_VERSION 5.0)
	enable_language(Swift)
endif()

find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --tags --always
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
else()
	set(GIT_VERSION "v0.0.0-0-g000000000")
	set(GIT_HASH "000000000")
endif()

if(WINDOWS_STORE)
	string(REGEX REPLACE "[Vv]" "" MS_VERSION ${GIT_VERSION})
	string(REPLACE "-" "." MS_VERSION ${MS_VERSION})
	string(REGEX REPLACE "\.g[0-9a-f]+" "" MS_VERSION ${MS_VERSION})
	string(REGEX MATCHALL "[0-9]+" VERSION_PARTS ${MS_VERSION})
	list(LENGTH VERSION_PARTS VERSION_PARTS_LENGTH)
	if(VERSION_PARTS_LENGTH EQUAL 2)
		string(APPEND MS_VERSION ".0.0")
	elseif(VERSION_PARTS_LENGTH EQUAL 3)
		string(APPEND MS_VERSION ".0")
	endif()
endif()

string(TIMESTAMP BUILD_TIMESTAMP UTC)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/core/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/core/version.h" @ONLY)

if(NINTENDO_SWITCH)
	set(USE_VULKAN OFF)
	enable_language(ASM)

	if(LIBRETRO)
		add_library(${PROJECT_NAME} STATIC core/emulator.cpp)
		target_compile_definitions(${PROJECT_NAME} PRIVATE LIBRETRO HAVE_LIBNX HAVE_OPENGL HAVE_OIT)
		find_path(GLEXT_H_INCLUDE_DIR NAMES GL/glext.h REQUIRED)
		target_include_directories(${PROJECT_NAME} PRIVATE ${GLEXT_H_INCLUDE_DIR})
		set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "flycast_libretro_libnx")
		set(CMAKE_STATIC_LIBRARY_PREFIX "")
	else()
		add_executable(${PROJECT_NAME} core/emulator.cpp)
	endif()
	if(USE_GLES)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES)
	endif()
	# asio
	target_compile_definitions(${PROJECT_NAME} PRIVATE
		ASIO_DISABLE_LOCAL_SOCKETS
		ASIO_DISABLE_SERIAL_PORT
		ESHUTDOWN=110
		SA_RESTART=0
		SA_NOCLDWAIT=0
		IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS)

elseif(LIBRETRO)
	add_library(${PROJECT_NAME} SHARED core/emulator.cpp)
	if(APPLE)
		if(NOT IOS)
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,error")
		endif()
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/shell/libretro/libretro.osx.def")
	else()
		if(NOT CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
		endif()
	endif()
	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "flycast_libretro"
	  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
	  XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	target_compile_definitions(${PROJECT_NAME} PRIVATE LIBRETRO)
	if(APPLE)
		find_library(FOUNDATION Foundation)
		target_link_libraries(${PROJECT_NAME} PRIVATE ${FOUNDATION})
	endif()
	if(ANDROID OR USE_GLES)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3 HAVE_OPENGLES HAVE_OPENGLES3 HAVE_OIT)
    elseif(IOS)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3 HAVE_OPENGLES HAVE_OPENGLES3)
		find_library(OPENGLES OpenGLES)
		find_library(GLKIT GLKit)
		target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGLES} ${GLKIT})
	elseif(USE_GLES2)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES2 HAVE_OPENGLES HAVE_OPENGLES2)
		if(USE_VIDEOCORE)
			target_compile_definitions(${PROJECT_NAME} PRIVATE TARGET_NO_STENCIL)
			target_link_libraries(${PROJECT_NAME} PRIVATE "-lbrcmGLESv2")
			target_link_directories(${PROJECT_NAME} PRIVATE "/opt/vc/lib")
		else()
			target_link_libraries(${PROJECT_NAME} PRIVATE "-lGLESv2")
		endif()
	elseif(USE_OPENGL)
		if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|NetBSD")
			find_package(OpenGL REQUIRED)
			if(CMAKE_VERSION VERSION_LESS 3.29.0)
				target_include_directories(${PROJECT_NAME} PRIVATE ${OPENGL_INCLUDE_DIR})
			else()
				target_include_directories(${PROJECT_NAME} PRIVATE ${OPENGL_INCLUDE_DIRS})
			endif()
		endif()
		target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENGL)
		if(APPLE)
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-framework,OpenGL")
		else()
			target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OIT)
		endif()
	endif()
elseif(ANDROID)
	add_library(${PROJECT_NAME} SHARED core/emulator.cpp)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_RELEASE} -O3")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE} -O3")
	target_compile_options(${PROJECT_NAME} PRIVATE -fno-stack-protector)
	set(CMAKE_ANDROID_STL_TYPE "c++_static")
elseif(WIN32)
	if(BUILD_TESTING)
		add_executable(${PROJECT_NAME} core/emulator.cpp)
	else()
		add_executable(${PROJECT_NAME} WIN32 core/emulator.cpp)
	endif()
	if(CMAKE_GENERATOR MATCHES "Visual Studio")
		if(NOT CMAKE_VS_GLOBALS MATCHES "(^|;)UseMultiToolTask=")
			list(APPEND CMAKE_VS_GLOBALS UseMultiToolTask=true)
		endif()
		if(NOT CMAKE_VS_GLOBALS MATCHES "(^|;)EnforceProcessCountAcrossBuilds=")
			list(APPEND CMAKE_VS_GLOBALS EnforceProcessCountAcrossBuilds=true)
		endif()
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
	endif()
elseif(APPLE)
	add_executable(${PROJECT_NAME} MACOSX_BUNDLE core/emulator.cpp)
else()
	add_executable(${PROJECT_NAME} core/emulator.cpp)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
		CMAKE_CXX_STANDARD 20
		CMAKE_CXX_STANDARD_REQUIRED ON)

if(WINDOWS_STORE)
	set(USE_OPENGL OFF)
	set(USE_VULKAN OFF)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /GR /GS-)
	if(WINDOWS_STORE)
		target_compile_options(${PROJECT_NAME} PRIVATE /ZW)
	endif()
else()
	target_compile_options(${PROJECT_NAME} PRIVATE
		$<$<COMPILE_LANGUAGE:CXX>:-fno-strict-aliasing>
		$<$<COMPILE_LANGUAGE:CXX>:-Wall>)
endif()

if(APPLE AND NOT IOS)
	set(TARGET_MAC ON)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
		$<$<BOOL:${APPLE}>:GL_SILENCE_DEPRECATION>
		$<$<BOOL:${ENABLE_LOG}>:DEBUGFAST>
		$<$<BOOL:${IOS}>:TARGET_IPHONE>
		$<$<BOOL:${TARGET_MAC}>:TARGET_MAC>
		$<$<BOOL:${IOS}>:GLES>
		$<$<BOOL:${IOS}>:GLES3>
		$<$<BOOL:${IOS}>:GLES_SILENCE_DEPRECATION>
		$<$<BOOL:${MSVC}>:_CRT_NONSTDC_NO_WARNINGS>
		$<$<BOOL:${MSVC}>:_CRT_SECURE_NO_WARNINGS>
		$<$<BOOL:${MSVC}>:_WINSOCK_DEPRECATED_NO_WARNING>
		$<$<BOOL:${MSVC}>:NOMINMAX>
		$<$<BOOL:${TEST_AUTOMATION}>:TEST_AUTOMATION>
		$<$<BOOL:${WINDOWS_STORE}>:WINDOWS_STORE>
		$<$<BOOL:${WINDOWS_STORE}>:TARGET_UWP>
		$<$<BOOL:${WINDOWS_STORE}>:NOCRYPT>
		$<$<BOOL:${WINDOWS_STORE}>:_WIN32_WINNT=0x0A00>
		$<$<OR:$<BOOL:${MINGW}>,$<BOOL:${MSVC}>>:_USE_MATH_DEFINES>)

if(UNIX AND NOT ANDROID AND NOT APPLE)
	execute_process(COMMAND getconf PAGESIZE OUTPUT_VARIABLE PAGE_SIZE OUTPUT_STRIP_TRAILING_WHITESPACE)
	target_compile_definitions(${PROJECT_NAME} PRIVATE PAGE_SIZE=${PAGE_SIZE})
endif()

if(NOT "${SENTRY_UPLOAD_URL}" STREQUAL "")
	target_compile_definitions(${PROJECT_NAME} PRIVATE SENTRY_UPLOAD="${SENTRY_UPLOAD_URL}")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE core core/deps core/deps/stb core/deps/json core/deps/asio/asio/include)
if(LIBRETRO)
	target_include_directories(${PROJECT_NAME} PRIVATE shell/libretro)
endif()
if(NINTENDO_SWITCH)
	target_include_directories(${PROJECT_NAME} PRIVATE shell/switch)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--no-execute-only")
endif()

if(USE_BREAKPAD AND NOT LIBRETRO)
	find_program(SH_EXECUTABLE sh)
	if((ANDROID AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows") OR (MINGW AND SH_EXECUTABLE) OR (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
		add_subdirectory(core/deps/breakpad)
		if(WIN32)
			target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--build-id")
		endif()
		target_link_libraries(${PROJECT_NAME} PUBLIC breakpad_client)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_BREAKPAD)
	elseif(APPLE AND NOT IOS)
		target_sources(${PROJECT_NAME} PRIVATE
			core/deps/breakpad/src/client/minidump_file_writer-inl.h
			core/deps/breakpad/src/client/minidump_file_writer.cc
			core/deps/breakpad/src/client/minidump_file_writer.h
			core/deps/breakpad/src/client/mac/handler/breakpad_nlist_64.cc
			core/deps/breakpad/src/client/mac/handler/breakpad_nlist_64.h
			core/deps/breakpad/src/client/mac/handler/dynamic_images.cc
			core/deps/breakpad/src/client/mac/handler/dynamic_images.h
			core/deps/breakpad/src/client/mac/handler/exception_handler.cc
			core/deps/breakpad/src/client/mac/handler/exception_handler.h
			core/deps/breakpad/src/client/mac/handler/minidump_generator.cc
			core/deps/breakpad/src/client/mac/handler/minidump_generator.h
			core/deps/breakpad/src/client/mac/handler/protected_memory_allocator.cc
			core/deps/breakpad/src/client/mac/handler/protected_memory_allocator.h
			core/deps/breakpad/src/client/mac/crash_generation/ConfigFile.h
			core/deps/breakpad/src/client/mac/crash_generation/ConfigFile.mm
			core/deps/breakpad/src/client/mac/crash_generation/client_info.h
			core/deps/breakpad/src/client/mac/crash_generation/crash_generation_client.h
			core/deps/breakpad/src/client/mac/crash_generation/crash_generation_client.cc
			core/deps/breakpad/src/common/convert_UTF.cc
			core/deps/breakpad/src/common/convert_UTF.h
			core/deps/breakpad/src/common/md5.cc
			core/deps/breakpad/src/common/string_conversion.cc
			core/deps/breakpad/src/common/string_conversion.h
			core/deps/breakpad/src/common/mac/file_id.cc
			core/deps/breakpad/src/common/mac/file_id.h
			core/deps/breakpad/src/common/mac/MachIPC.mm
			core/deps/breakpad/src/common/mac/MachIPC.h
			core/deps/breakpad/src/common/mac/bootstrap_compat.cc
			core/deps/breakpad/src/common/mac/bootstrap_compat.h
			core/deps/breakpad/src/common/mac/macho_id.cc
			core/deps/breakpad/src/common/mac/macho_id.h
			core/deps/breakpad/src/common/mac/macho_utilities.cc
			core/deps/breakpad/src/common/mac/macho_utilities.h
			core/deps/breakpad/src/common/mac/macho_walker.cc
			core/deps/breakpad/src/common/mac/macho_walker.h
			core/deps/breakpad/src/common/mac/string_utilities.cc
			core/deps/breakpad/src/common/mac/string_utilities.h
		)
		target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/deps/breakpad/src)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_BREAKPAD)
		add_custom_target(dump_syms COMMAND xcodebuild -project ${CMAKE_CURRENT_SOURCE_DIR}/core/deps/breakpad/src/tools/mac/dump_syms/dump_syms.xcodeproj -target dump_syms -configuration Release CONFIGURATION_BUILD_DIR=${CMAKE_CURRENT_SOURCE_DIR}/build/breakpad)
		ADD_DEPENDENCIES(${PROJECT_NAME} dump_syms)

		if(${CMAKE_GENERATOR} MATCHES "^Xcode.*")
			add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND sleep 20
				COMMAND mkdir -p ../symbols
				COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/breakpad/dump_syms
					-a x86_64
					-g ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Flycast.app.dSYM
					$<TARGET_FILE:flycast> > ../symbols/Flycast-x64.sym
				COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/breakpad/dump_syms
					-a arm64
					-g ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Flycast.app.dSYM
					$<TARGET_FILE:flycast> > ../symbols/Flycast-arm64.sym
			)
		else()
			add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND sleep 20
				COMMAND mkdir -p ../symbols
				COMMAND dsymutil $<TARGET_FILE:flycast>
					-o ${CMAKE_CURRENT_BINARY_DIR}/Flycast.app.dSYM
				COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/breakpad/dump_syms
					-a x86_64
					-g ${CMAKE_CURRENT_BINARY_DIR}/Flycast.app.dSYM
					$<TARGET_FILE:flycast> > ../symbols/Flycast-x64.sym
				COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/breakpad/dump_syms
					-a arm64
					-g ${CMAKE_CURRENT_BINARY_DIR}/Flycast.app.dSYM
					$<TARGET_FILE:flycast> > ../symbols/Flycast-arm64.sym
			)
		endif()
	endif()
endif()

if(USE_OPENMP)
	if(APPLE AND IS_DIRECTORY "/opt/homebrew/opt/libomp")
		list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/libomp")
		message(STATUS "Found and using Homebrew libomp hint.")
	endif()
	find_package(OpenMP)
	if(OpenMP_CXX_FOUND)
		if(MINGW)
			target_link_libraries(${PROJECT_NAME} PRIVATE -lpthread)
			target_link_options(${PROJECT_NAME} PRIVATE -fopenmp -static)
			target_compile_options(${PROJECT_NAME} PRIVATE -fopenmp)
		elseif(ANDROID)
			# Reference: https://android.googlesource.com/platform/ndk/+/refs/heads/master/tests/device/openmp/CMakeLists.txt
			set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_OPTIONS -fopenmp)
			target_link_libraries(${PROJECT_NAME} PRIVATE -fopenmp -static-openmp)
		else()
			target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
		endif()
	endif()
endif()

option(BUILD_SHARED_LIBS "Build shared library" OFF)
set(XXHASH_BUILD_XXHSUM OFF CACHE BOOL "Build the xxhsum binary")
add_subdirectory(core/deps/xxHash/cmake_unofficial)
target_link_libraries(${PROJECT_NAME} PRIVATE xxHash::xxhash)

option(BUILD_SHARED_LIBS "Build shared library" OFF)
add_subdirectory(core/deps/glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)

if(USE_VULKAN)
	if(USE_HOST_GLSLANG)
		find_package(glslang REQUIRED)
	else()
		option(BUILD_EXTERNAL "Build external dependencies in /External" OFF)
		option(ENABLE_SPVREMAPPER "Enables building of SPVRemapper" OFF)
		option(ENABLE_GLSLANG_BINARIES "Builds glslang and spirv-remap" OFF)
		option(ENABLE_HLSL "Enables HLSL input support" OFF)
		option(ENABLE_OPT "Enables spirv-opt capability if present" OFF)
		option(ENABLE_PCH "Enables Precompiled header" OFF)
		add_subdirectory(core/deps/glslang EXCLUDE_FROM_ALL)
	endif()
	target_link_libraries(${PROJECT_NAME} PRIVATE glslang::glslang-default-resource-limits glslang::SPIRV)
endif()

if(NOT LIBRETRO)
	if(USE_OSS)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OSS)
	endif()

	if(USE_ALSA)
		find_package(ALSA)
		if(ALSA_FOUND AND NOT ANDROID)
			target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ALSA)
			target_include_directories(${PROJECT_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
			target_link_libraries(${PROJECT_NAME} PRIVATE ${ALSA_LIBRARIES})
		endif()
	endif()

	if(MINGW)
		target_link_libraries(${PROJECT_NAME} PRIVATE "-static-libgcc -static-libstdc++")
	endif()

	if(NOT ANDROID AND NOT IOS)
		if (NOT NINTENDO_SWITCH)
			# DreamLink enabled for non-mobile Linux, MacOS, and Windows
			target_compile_definitions(${PROJECT_NAME} PRIVATE USE_DREAMLINK_DEVICES=1)
			set(USE_DREAMLINK_DEVICES ON) # Must be set before adding core/sdl

			if (WINDOWS_STORE)
				# Use winrt implementation of the DreamPicoPort-API
				option(DREAMPICOPORT_WITH_LIBUSB "Use libusb to access DreamPicoPort-API" OFF)
			elseif (CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|Haiku")
				# A version of libusb is installed in FreeBSD
				find_package(PkgConfig REQUIRED)
   	 			pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

				set(DREAMPICOPORT_LIBUSB_LIBRARIES "${LIBUSB_LIBRARIES}" CACHE STRING "Name of the libusb library to link against")
				set(DREAMPICOPORT_LIBUSB_INCLUDE_DIRS "${LIBUSB_INCLUDE_DIRS}" CACHE STRING "Path to libusb include directory, when applicable")
			else()
				# libusb
				add_subdirectory(core/deps/DreamPicoPort-API/ext/libusb-cmake)
			endif()

			# DreamPicoPort-API
			option(DREAMPICOPORT_ADD_LIBUSB "Add internal libusb library" OFF) # Already included above, when applicable
			add_subdirectory(core/deps/DreamPicoPort-API)
			target_link_libraries(${PROJECT_NAME} PRIVATE dream_pico_port_api)
		endif()

		if(USE_HOST_SDL)
			find_package(SDL2 2.0.9)
		endif()
		if(NOT SDL2_FOUND)
			set(SDL_TEST_ENABLED_BY_DEFAULT OFF)
			add_subdirectory(core/deps/SDL EXCLUDE_FROM_ALL)
			set(SDL2_FOUND 1)
		endif()

		# SDL2::SDL2main may or may not be available. It is e.g. required by Windows GUI applications
		if(TARGET SDL2::SDL2main AND NOT BUILD_TESTING)
			# It has an implicit dependency on SDL2 functions, so it MUST be added before SDL2::SDL2 (or SDL2::SDL2-static)
			target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main)
		endif()

		if((APPLE OR WIN32) AND TARGET SDL2::SDL2-static)
			target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static)
		elseif(TARGET SDL2::SDL2)
			target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
		else()
			target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
			target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})
		endif()

		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SDL USE_SDL_AUDIO)
		add_subdirectory(core/sdl)

		if((UNIX AND NOT APPLE) OR NINTENDO_SWITCH)
			find_package(CURL REQUIRED)
			target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
		endif()
	endif()

	find_package(ZLIB)
	if(TARGET ZLIB::ZLIB AND NOT ANDROID AND (NOT WIN32 OR WINDOWS_STORE))
		set(WITH_SYSTEM_ZLIB ON CACHE BOOL "Use system provided zlib library")
		target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
	endif()

	if(USE_LUA)
		find_package(Lua 5.2)
		if(NOT APPLE AND LUA_FOUND)
			target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LUA)
			target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR} core/deps/luabridge/Source)
			list(TRANSFORM LUA_LIBRARIES REPLACE "\.dll" "")
			target_link_libraries(${PROJECT_NAME} PRIVATE ${LUA_LIBRARIES})
		endif()
	endif()
endif()

find_package(PkgConfig)

if(NOT WITH_SYSTEM_ZLIB)
    option(ZLIB_BUILD_EXAMPLES "Enable Zlib Examples" OFF)
    add_subdirectory("core/deps/libchdr/deps/zlib-1.3.1" EXCLUDE_FROM_ALL)
	set_target_properties(zlibstatic PROPERTIES POSITION_INDEPENDENT_CODE ON)
	target_link_libraries(${PROJECT_NAME} PRIVATE zlibstatic)
	# help libzip find the package
	set(ZLIB_RELATIVE_PATH "core/deps/libchdr/deps/zlib-1.3.1")
	set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${ZLIB_RELATIVE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${ZLIB_RELATIVE_PATH}")
endif()

if(PKG_CONFIG_FOUND AND USE_HOST_LIBCHDR)
	pkg_check_modules(LIBCHDR IMPORTED_TARGET libchdr)
	target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBCHDR)
else()
	option(ZSTD_BUILD_SHARED "BUILD SHARED LIBRARIES" OFF)
	option(ZSTD_BUILD_PROGRAMS "BUILD PROGRAMS" OFF)
	option(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" OFF)
	add_subdirectory(core/deps/libchdr/deps/zstd-1.5.6/build/cmake EXCLUDE_FROM_ALL)
	target_link_libraries(${PROJECT_NAME} PRIVATE libzstd_static)

	option(WITH_SYSTEM_ZSTD "Use system provided zstd library" ON)
	add_subdirectory(core/deps/libchdr EXCLUDE_FROM_ALL)
	target_link_libraries(${PROJECT_NAME} PRIVATE chdr-static)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/libchdr/include)
endif()

if(PKG_CONFIG_FOUND AND NOT ANDROID AND NOT APPLE AND NOT LIBRETRO)
	if(USE_LIBAO)
		pkg_check_modules(AO IMPORTED_TARGET ao)
		if(AO_FOUND)
			target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LIBAO)
			target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::AO)
		endif()
	endif()

	if(NOT SDL2_FOUND)
		pkg_check_modules(LIBEVDEV IMPORTED_TARGET libevdev)
		if(LIBEVDEV_FOUND)
			target_compile_definitions(${PROJECT_NAME} PRIVATE USE_EVDEV)
			target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBEVDEV)

			pkg_check_modules(LIBUDEV IMPORTED_TARGET libudev)
			if(LIBUDEV_FOUND)
				target_compile_definitions(${PROJECT_NAME} PRIVATE USE_UDEV)
				target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBUDEV)
			endif()
		endif()
	endif()

	if(USE_PULSEAUDIO)
		pkg_check_modules(LIBPULSE IMPORTED_TARGET libpulse)
		if(LIBPULSE_FOUND)
			target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PULSEAUDIO)
			target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBPULSE)
		endif()
	endif()

	if(USE_HOST_LIBZIP)
		pkg_check_modules(LIBZIP IMPORTED_TARGET libzip)
		if(LIBZIP_FOUND)
			target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBZIP)
		endif()
	endif()

	if(ENABLE_OPROFILE)
		target_compile_definitions(${PROJECT_NAME} PRIVATE DYNA_OPROF)
		target_link_libraries(${PROJECT_NAME} PRIVATE opagent)
	endif()

	find_package(MiniUPnPc)
	if(MINIUPNP_FOUND)
		target_include_directories(${PROJECT_NAME} PRIVATE ${MINIUPNP_INCLUDE_DIRS})
		target_link_libraries(${PROJECT_NAME} PRIVATE ${MINIUPNP_LIBRARIES})
	endif()
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	add_definitions(
		-DFLYCAST_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/"
		-DFLYCAST_SYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME}/"
	)

	if(USE_GLES2)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES2)
		if(USE_VIDEOCORE)
			target_compile_definitions(${PROJECT_NAME} PRIVATE TARGET_VIDEOCORE USE_OMX)
			target_link_libraries(${PROJECT_NAME} PRIVATE "-lbrcmGLESv2")
			target_link_directories(${PROJECT_NAME} PRIVATE "/opt/vc/lib")
		endif()
	elseif(USE_GLES)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)
	endif()

	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND "x86_64" IN_LIST ARCHITECTURE AND NOT LIBRETRO)
		set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE False)
		if(${CMAKE_VERSION} VERSION_LESS "3.14.0")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
		endif()
	endif()

	if(NOT SDL2_FOUND AND NOT LIBRETRO)
		find_package(X11 REQUIRED)
		if(X11_FOUND)
			target_compile_definitions(${PROJECT_NAME} PRIVATE SUPPORT_X11)
			target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
			target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
		endif()
	endif()

	find_library(LIBRT rt)
	if(LIBRT)
		target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRT})
	endif()

	target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

if(ASAN)
	if(ANDROID)
		target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
		set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS -fsanitize=address)
	else()
		target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -static-libasan)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -static-libasan")
	endif()
endif()

if(ANDROID AND "arm" IN_LIST ARCHITECTURE)
	enable_language(ASM)
	option(LIBUNWIND_ENABLE_SHARED "Build libunwind as a shared library." OFF)
	add_subdirectory(core/deps/libunwind/libunwind)
	target_link_libraries(${PROJECT_NAME} PRIVATE unwind_static)
endif()

if(ANDROID AND NOT LIBRETRO)
	set(BUILD_SHARED_LIBS OFF)
	add_subdirectory(core/deps/oboe)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OBOE)
	target_link_libraries(${PROJECT_NAME} PRIVATE oboe)
endif()

target_sources(${PROJECT_NAME} PRIVATE
		core/deps/chdpsr/cdipsr.cpp
		core/deps/chdpsr/cdipsr.h)

add_subdirectory(core/deps/nowide EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PRIVATE nowide::nowide)

if(NOT MINIUPNP_FOUND)
	if(NINTENDO_SWITCH)
		target_compile_definitions(${PROJECT_NAME} PRIVATE FEAT_NO_MINIUPNPC)
	else()
		option(UPNPC_BUILD_SHARED "Build shared library" OFF)
		option(UPNPC_BUILD_TESTS "Build test executables" OFF)
		option(UPNPC_BUILD_SAMPLE "Build sample executables" OFF)
		option(UPNPC_NO_INSTALL "Disable installation" ON)
		add_subdirectory(core/deps/miniupnpc)
		if(WINDOWS_STORE)
			get_target_property(miniupnpc-private-defs miniupnpc-private INTERFACE_COMPILE_DEFINITIONS)
			list(REMOVE_ITEM miniupnpc-private-defs "_WIN32_WINNT=0x0501")
			set_property(TARGET miniupnpc-private PROPERTY INTERFACE_COMPILE_DEFINITIONS ${miniupnpc-private-defs})
		endif()
		target_link_libraries(${PROJECT_NAME} PRIVATE miniupnpc::miniupnpc)
	endif()
endif()

if(NOT LIBZIP_FOUND OR NINTENDO_SWITCH)
	option(ENABLE_COMMONCRYPTO "Enable use of CommonCrypto" OFF)
	option(ENABLE_GNUTLS "Enable use of GnuTLS" OFF)
	option(ENABLE_MBEDTLS "Enable use of mbed TLS" OFF)
	option(ENABLE_OPENSSL "Enable use of OpenSSL" OFF)
	option(ENABLE_WINDOWS_CRYPTO "Enable use of Windows cryptography libraries" OFF)
	option(ENABLE_BZIP2 "Enable use of BZip2" OFF)
	option(ENABLE_LZMA "Enable use of LZMA" OFF)
	option(ENABLE_ZSTD "Enable use of Zstandard" ON)
	option(BUILD_TOOLS "Build tools in the src directory (zipcmp, zipmerge, ziptool)" OFF)
	option(BUILD_REGRESS "Build regression tests" OFF)
	option(BUILD_EXAMPLES "Build examples" OFF)
	option(BUILD_DOC "Build documentation" OFF)
	option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
	option(LIBZIP_DO_INSTALL "Install libzip and the related files" OFF)
	add_subdirectory(core/deps/libzip EXCLUDE_FROM_ALL)
	target_link_libraries(${PROJECT_NAME} PRIVATE libzip::zip)
endif()

if(USE_LIBCDIO)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(CDIO IMPORTED_TARGET libcdio)
		if(CDIO_FOUND)
			target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LIBCDIO)
			if(MINGW)
				# Force static link
				target_link_libraries(${PROJECT_NAME} PRIVATE "-l:libcdio.a -l:libiconv.a -lwinmm")
			else()
				target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::CDIO)
			endif()
		endif()
	endif()
	if(NOT CDIO_FOUND)
		find_package(libcdio)
		if(TARGET libcdio::libcdio)
			target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LIBCDIO)
			target_link_libraries(${PROJECT_NAME} PRIVATE libcdio::libcdio)
		endif()
	endif()
endif()

if(WIN32)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/dirent)
endif()

if(NOT LIBRETRO AND NOT NINTENDO_SWITCH)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ICE)
	# libjuice
	option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
	option(NO_SERVER "Disable server support" ON)
	option(NO_TESTS "Disable tests build" ON)
	add_subdirectory(core/deps/libjuice EXCLUDE_FROM_ALL)
	target_link_libraries(${PROJECT_NAME} PRIVATE LibJuice::LibJuiceStatic)

	# websocket++
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/websocketpp)
	target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)
	if (MSVC)
		target_compile_definitions(${PROJECT_NAME} PRIVATE
			_WEBSOCKETPP_CPP11_FUNCTIONAL_
			_WEBSOCKETPP_CPP11_SYSTEM_ERROR_
			_WEBSOCKETPP_CPP11_RANDOM_DEVICE_
			_WEBSOCKETPP_CPP11_MEMORY_
			_WEBSOCKETPP_CPP11_TYPE_TRAITS_)
	elseif(MINGW)
		target_compile_definitions(${PROJECT_NAME} PRIVATE _WEBSOCKETPP_CPP11_THREAD_)
	else()
		target_compile_definitions(${PROJECT_NAME} PRIVATE _WEBSOCKETPP_CPP11_STL_)
	endif()
endif()

set(TINYGETTEXT_WO_ICONV ON)
add_subdirectory(core/deps/tinygettext EXCLUDE_FROM_ALL)
target_include_directories(${PROJECT_NAME} PRIVATE core/deps/tinygettext/include)
target_link_libraries(${PROJECT_NAME} PRIVATE tinygettext::tinygettext)

target_include_directories(${PROJECT_NAME} PRIVATE core/deps/picotcp/include core/deps/picotcp/modules)
target_sources(${PROJECT_NAME} PRIVATE
		core/deps/picotcp/include/arch/pico_arm9.h
		core/deps/picotcp/include/arch/pico_atsamd21j18.h
		core/deps/picotcp/include/arch/pico_avr.h
		core/deps/picotcp/include/arch/pico_cortex_m.h
		core/deps/picotcp/include/arch/pico_dos.h
		core/deps/picotcp/include/arch/pico_esp8266.h
		core/deps/picotcp/include/arch/pico_generic_gcc.h
		core/deps/picotcp/include/arch/pico_linux.h
		core/deps/picotcp/include/arch/pico_mbed.h
		core/deps/picotcp/include/arch/pico_msp430.h
		core/deps/picotcp/include/arch/pico_msvc.h
		core/deps/picotcp/include/arch/pico_none.h
		core/deps/picotcp/include/arch/pico_pic24.h
		core/deps/picotcp/include/arch/pico_pic32.h
		core/deps/picotcp/include/arch/pico_posix.h
		core/deps/picotcp/include/heap.h
		core/deps/picotcp/include/pico_addressing.h
		core/deps/picotcp/include/pico_config.h
		core/deps/picotcp/include/pico_constants.h
		core/deps/picotcp/include/pico_defines.h
		core/deps/picotcp/include/pico_defines_msvc.h
		core/deps/picotcp/include/pico_device.h
		core/deps/picotcp/include/pico_eth.h
		core/deps/picotcp/include/pico_frame.h
		core/deps/picotcp/include/pico_md5.h
		core/deps/picotcp/include/pico_module_eth.h
		core/deps/picotcp/include/pico_protocol.h
		core/deps/picotcp/include/pico_queue.h
		core/deps/picotcp/include/pico_socket.h
		core/deps/picotcp/include/pico_socket_multicast.h
		core/deps/picotcp/include/pico_stack.h
		core/deps/picotcp/include/pico_tree.h
		core/deps/picotcp/modules/pico_arp.c
		core/deps/picotcp/modules/pico_dev_ppp.c
		core/deps/picotcp/modules/pico_dhcp_common.c
		core/deps/picotcp/modules/pico_dhcp_common.h
		core/deps/picotcp/modules/pico_dhcp_server.c
		core/deps/picotcp/modules/pico_dhcp_server.h
		core/deps/picotcp/modules/pico_dns_client.c
		core/deps/picotcp/modules/pico_dns_common.c
		core/deps/picotcp/modules/pico_ethernet.c
		core/deps/picotcp/modules/pico_fragments.c
		core/deps/picotcp/modules/pico_icmp4.c
		core/deps/picotcp/modules/pico_ipv4.c
		core/deps/picotcp/modules/pico_socket_tcp.c
		core/deps/picotcp/modules/pico_socket_udp.c
		core/deps/picotcp/modules/pico_strings.c
		core/deps/picotcp/modules/pico_tcp.c
		core/deps/picotcp/modules/pico_udp.c
		core/deps/picotcp/stack/pico_device.c
		core/deps/picotcp/stack/pico_frame.c
		core/deps/picotcp/stack/pico_md5.c
		core/deps/picotcp/stack/pico_protocol.c
		core/deps/picotcp/stack/pico_socket.c
		core/deps/picotcp/stack/pico_socket_multicast.c
		core/deps/picotcp/stack/pico_stack.c
		core/deps/picotcp/stack/pico_tree.c)

target_compile_definitions(${PROJECT_NAME} PRIVATE _7ZIP_ST)
target_sources(${PROJECT_NAME} PRIVATE core/deps/lzma/7zArcIn.c core/deps/lzma/7zBuf.c core/deps/lzma/7zCrc.c core/deps/lzma/7zCrcOpt.c core/deps/lzma/7zDec.c core/deps/lzma/7zFile.c core/deps/lzma/7zStream.c core/deps/lzma/Alloc.c core/deps/lzma/Bcj2.c core/deps/lzma/Bra86.c core/deps/lzma/Bra.c core/deps/lzma/BraIA64.c core/deps/lzma/CpuArch.c core/deps/lzma/Delta.c core/deps/lzma/LzFind.c core/deps/lzma/Lzma2Dec.c core/deps/lzma/Lzma86Dec.c core/deps/lzma/Lzma86Enc.c core/deps/lzma/LzmaDec.c core/deps/lzma/LzmaEnc.c core/deps/lzma/LzmaLib.c core/deps/lzma/Sort.c)
add_subdirectory(core/deps/libelf)
target_link_libraries(${PROJECT_NAME} PRIVATE elf)
if(NOT LIBRETRO)
	target_compile_definitions(${PROJECT_NAME} PRIVATE
		IMGUI_DISABLE_DEMO_WINDOWS
		IMGUI_DISABLE_DEBUG_TOOLS
		IMGUI_DEFINE_MATH_OPERATORS)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/imgui core/deps/imgui/backends)
	target_sources(${PROJECT_NAME} PRIVATE
		core/deps/imgui/imgui.cpp
		core/deps/imgui/imgui_demo.cpp
		core/deps/imgui/imgui_draw.cpp
		core/deps/imgui/imgui_stdlib.cpp
		core/deps/imgui/imgui_tables.cpp
		core/deps/imgui/imgui_widgets.cpp)

	if(ENABLE_FC_PROFILER)
		target_include_directories(${PROJECT_NAME} PRIVATE core/deps/implot)
		target_sources(${PROJECT_NAME} PRIVATE
			core/deps/implot/implot.h
			core/deps/implot/implot.cpp
			core/deps/implot/implot_internal.h
			core/deps/implot/implot_items.cpp)
	endif()
endif()
target_sources(${PROJECT_NAME} PRIVATE core/deps/xbrz/xbrz.cpp)
target_sources(${PROJECT_NAME} PRIVATE core/deps/md5/md5.cpp)

if(USE_DISCORD AND NOT LIBRETRO)
	option(BUILD_EXAMPLES "Build example apps" OFF)
	add_subdirectory(core/deps/discord-rpc EXCLUDE_FROM_ALL)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/discord-rpc/include)
	target_link_libraries(${PROJECT_NAME} PRIVATE discord-rpc)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_DISCORD)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE flycast::res)

if(LIBRETRO)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/libretro-common/include)
	target_sources(${PROJECT_NAME} PRIVATE
		core/deps/libretro-common/memmap/memalign.c
		core/deps/libretro-common/file/file_path.c
		core/deps/libretro-common/vfs/vfs_implementation.c
		core/deps/libretro-common/encodings/encoding_utf.c
		core/deps/libretro-common/compat/compat_strl.c
		core/deps/libretro-common/compat/fopen_utf8.c
		core/deps/libretro-common/compat/compat_strcasestr.c
		core/deps/libretro-common/file/retro_dirent.c
		core/deps/libretro-common/string/stdstring.c)
	if(USE_OPENGL)
		target_sources(${PROJECT_NAME} PRIVATE
			core/deps/libretro-common/glsm/glsm.c
			core/deps/libretro-common/glsym/rglgen.c)
		if(ANDROID OR IOS OR USE_GLES)
			target_sources(${PROJECT_NAME} PRIVATE core/deps/libretro-common/glsym/glsym_es3.c)
		elseif(USE_GLES2)
			target_sources(${PROJECT_NAME} PRIVATE core/deps/libretro-common/glsym/glsym_es2.c)
		else()
			target_sources(${PROJECT_NAME} PRIVATE core/deps/libretro-common/glsym/glsym_gl.c)
		endif()
	endif()
	target_sources(${PROJECT_NAME} PRIVATE
		shell/libretro/audiostream.cpp
		shell/libretro/keyboard_map.h
		shell/libretro/libretro_core_option_defines.h
		shell/libretro/libretro_core_options_intl.h
		shell/libretro/libretro_core_options.h
		shell/libretro/libretro.cpp
		shell/libretro/LogManager.cpp
		shell/libretro/LogManager.h
		shell/libretro/option.cpp
		shell/libretro/oslib.cpp
		shell/libretro/vmu_xhair.cpp)
	if(APPLE)
		target_sources(${PROJECT_NAME} PRIVATE shell/libretro/oslib_apple.mm)
	endif()
	if(WIN32)
		target_link_libraries(${PROJECT_NAME} PRIVATE mswsock)
	endif()
endif()

add_subdirectory(core/archive)
add_subdirectory(core/cfg)
add_subdirectory(core/hw)
add_subdirectory(core/imgread)
add_subdirectory(core/input)
add_subdirectory(core/log)
add_subdirectory(core/network)
add_subdirectory(core/oslib)
add_subdirectory(core/audio)
add_subdirectory(core/reios)
add_subdirectory(core/achievements)
add_subdirectory(core/ui)
add_subdirectory(core/lua)
add_subdirectory(core/profiler)
add_subdirectory(core/wsi)

include(resources/resources.cmake)

if(WIN32)
	target_sources(${PROJECT_NAME} PRIVATE
			core/windows/comptr.h
			core/windows/fault_handler.cpp
			core/windows/unwind_info.cpp
			core/windows/win_vmem.cpp)
else()
	target_sources(${PROJECT_NAME} PRIVATE
			core/linux/common.cpp
			core/linux/context.cpp
			core/linux/posix_vmem.cpp
			core/linux/unwind_info.cpp)
	if(NINTENDO_SWITCH)
		add_subdirectory(shell/switch)
	endif()
endif()

if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
			core/linux-dist/evdev.cpp
			core/linux-dist/evdev.h
			core/linux-dist/icon.h
			core/linux-dist/x11.cpp
			core/linux-dist/x11.h
			core/linux-dist/x11_keyboard.h)
endif()

if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
			core/deps/rcheevos/src/rc_client_raintegration.c
			core/deps/rcheevos/src/rc_client.c
			core/deps/rcheevos/src/rc_compat.c
			core/deps/rcheevos/src/rc_util.c
			core/deps/rcheevos/src/rc_version.c
			core/deps/rcheevos/src/rapi/rc_api_common.c
			core/deps/rcheevos/src/rapi/rc_api_editor.c
			core/deps/rcheevos/src/rapi/rc_api_info.c
			core/deps/rcheevos/src/rapi/rc_api_runtime.c
			core/deps/rcheevos/src/rapi/rc_api_user.c
			core/deps/rcheevos/src/rcheevos/alloc.c
			core/deps/rcheevos/src/rcheevos/condition.c
			core/deps/rcheevos/src/rcheevos/condset.c
			core/deps/rcheevos/src/rcheevos/consoleinfo.c
			core/deps/rcheevos/src/rcheevos/format.c
			core/deps/rcheevos/src/rcheevos/lboard.c
			core/deps/rcheevos/src/rcheevos/memref.c
			core/deps/rcheevos/src/rcheevos/operand.c
			core/deps/rcheevos/src/rcheevos/rc_validate.c
			core/deps/rcheevos/src/rcheevos/richpresence.c
			core/deps/rcheevos/src/rcheevos/runtime_progress.c
			core/deps/rcheevos/src/rcheevos/runtime.c
			core/deps/rcheevos/src/rcheevos/trigger.c
			core/deps/rcheevos/src/rcheevos/value.c
			core/deps/rcheevos/src/rhash/aes.c
			core/deps/rcheevos/src/rhash/cdreader.c
			core/deps/rcheevos/src/rhash/hash.c
			core/deps/rcheevos/src/rhash/md5.c
			core/deps/rcheevos/src/rurl/url.c)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/rcheevos/include)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_RACHIEVEMENTS RC_DISABLE_LUA)
endif()

if(USE_OPENGL)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OPENGL)

	target_sources(${PROJECT_NAME} PRIVATE
			core/rend/gles/glcache.h
			core/rend/gles/gldraw.cpp
			core/rend/gles/gles.cpp
			core/rend/gles/gles.h
			core/rend/gles/gltex.cpp
			core/rend/gles/quad.cpp
			core/rend/gles/postprocess.cpp
			core/rend/gles/postprocess.h
			core/rend/gles/naomi2.cpp
			core/rend/gles/naomi2.h)

	if(NOT LIBRETRO)
		target_sources(${PROJECT_NAME} PRIVATE
				core/deps/imgui/backends/imgui_impl_opengl3.cpp
				core/deps/imgui/backends/imgui_impl_opengl3.h
				core/rend/gles/opengl_driver.cpp
				core/rend/gles/opengl_driver.h)
	endif()

	if(NOT (APPLE OR USE_GLES2))
		target_sources(${PROJECT_NAME} PRIVATE
				core/rend/gl4/abuffer.cpp
				core/rend/gl4/gl4.h
				core/rend/gl4/gldraw.cpp
				core/rend/gl4/gles.cpp
				core/rend/gl4/gl4naomi2.cpp
				core/rend/gl4/gl4naomi2.h)
	endif()
endif()

target_sources(${PROJECT_NAME} PRIVATE
		core/build.h
		core/cheats.cpp
		core/cheats.h
		core/emulator.h
		core/nullDC.cpp
		core/serialize.cpp
		core/serialize.h
		core/stdclass.cpp
		core/stdclass.h
		core/types.h
		core/debug/gdb_server.h)

target_sources(${PROJECT_NAME} PRIVATE
		core/rend/CustomTexture.cpp
		core/rend/CustomTexture.h
		core/rend/osd.cpp
		core/rend/osd.h
		core/rend/sorter.cpp
		core/rend/sorter.h
		core/rend/tileclip.h
		core/rend/TexCache.cpp
		core/rend/TexCache.h
		core/rend/texconv.cpp
		core/rend/texconv.h
		core/rend/norend/norend.cpp)

if(USE_VULKAN)
	target_compile_definitions(${PROJECT_NAME} PUBLIC VK_ENABLE_BETA_EXTENSIONS VK_NO_PROTOTYPES)
	if(ANDROID)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
	elseif(X11_FOUND)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_XLIB_KHR)
	elseif(WIN32)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_WIN32_KHR)
	elseif(APPLE)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_METAL_EXT)
	endif()

	add_subdirectory(core/deps/Vulkan-Headers)
	target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers)

	add_subdirectory(core/deps/VulkanMemoryAllocator)
	target_compile_options(VulkanMemoryAllocator INTERFACE $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:AppleClang,Clang>>:-Wno-nullability-completeness>)
	target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)

	if(ANDROID AND NOT LIBRETRO AND "arm64" IN_LIST ARCHITECTURE)
		add_subdirectory(core/deps/libadrenotools)
		target_link_libraries(${PROJECT_NAME} PRIVATE adrenotools)
	endif()

	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN HAVE_VULKAN)
	target_sources(${PROJECT_NAME} PRIVATE
			core/rend/vulkan/oit/oit_buffer.h
			core/rend/vulkan/oit/oit_drawer.cpp
			core/rend/vulkan/oit/oit_drawer.h
			core/rend/vulkan/oit/oit_pipeline.cpp
			core/rend/vulkan/oit/oit_pipeline.h
			core/rend/vulkan/oit/oit_renderer.cpp
			core/rend/vulkan/oit/oit_renderpass.cpp
			core/rend/vulkan/oit/oit_renderpass.h
			core/rend/vulkan/oit/oit_shaders.cpp
			core/rend/vulkan/oit/oit_shaders.h
			core/rend/vulkan/adreno.cpp
			core/rend/vulkan/buffer.cpp
			core/rend/vulkan/buffer.h
			core/rend/vulkan/commandpool.cpp
			core/rend/vulkan/commandpool.h
			core/rend/vulkan/compiler.cpp
			core/rend/vulkan/compiler.h
			core/rend/vulkan/drawer.cpp
			core/rend/vulkan/drawer.h
			core/rend/vulkan/pipeline.cpp
			core/rend/vulkan/pipeline.h
			core/rend/vulkan/quad.cpp
			core/rend/vulkan/quad.h
			core/rend/vulkan/shaders.cpp
			core/rend/vulkan/shaders.h
			core/rend/vulkan/texture.cpp
			core/rend/vulkan/texture.h
			core/rend/vulkan/utils.h
			core/rend/vulkan/vmallocator.cpp
			core/rend/vulkan/vmallocator.h
			core/rend/vulkan/overlay.cpp
			core/rend/vulkan/overlay.h
			core/rend/vulkan/vulkan_context.h
			core/rend/vulkan/vulkan.h
			core/rend/vulkan/vulkan_renderer.cpp)
	if(LIBRETRO)
		target_sources(${PROJECT_NAME} PRIVATE
				core/rend/vulkan/vk_context_lr.cpp
				core/rend/vulkan/vk_context_lr.h)
	else()
		target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
		target_sources(${PROJECT_NAME} PRIVATE
				core/rend/vulkan/vulkan_driver.h
				core/rend/vulkan/vulkan_context.cpp
				core/deps/imgui/backends/imgui_impl_vulkan.cpp
				core/deps/imgui/backends/imgui_impl_vulkan.h)
		if(CMAKE_SIZEOF_VOID_P EQUAL 4)
			target_compile_definitions(${PROJECT_NAME} PRIVATE ImTextureID=ImU64)
		endif()
	endif()
endif()

if(WIN32 AND USE_DX9 AND NOT LIBRETRO AND NOT WINDOWS_STORE AND ("x86" IN_LIST ARCHITECTURE OR "x86_64" IN_LIST ARCHITECTURE))
	set(REND_DX9_FILES
		core/rend/dx9/d3d_overlay.h
		core/rend/dx9/d3d_overlay.cpp
		core/rend/dx9/d3d_renderer.h
		core/rend/dx9/d3d_renderer.cpp
		core/rend/dx9/d3d_shaders.h
		core/rend/dx9/d3d_shaders.cpp
		core/rend/dx9/d3d_texture.h
		core/rend/dx9/d3d_texture.cpp
		core/rend/dx9/dx9_driver.h
		core/rend/dx9/dxcontext.cpp
		core/rend/dx9/dxcontext.h
		core/deps/imgui/backends/imgui_impl_dx9.h
		core/deps/imgui/backends/imgui_impl_dx9.cpp)
	target_sources(${PROJECT_NAME} PRIVATE ${REND_DX9_FILES})

	if(NOT MINGW)
		set_source_files_properties(${REND_DX9_FILES} PROPERTIES INCLUDE_DIRECTORIES "$ENV{DXSDK_DIR}/Include")
	endif()
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_DX9)
endif()

if(WIN32 AND USE_DX11)
	if(NOT LIBRETRO)
		target_sources(${PROJECT_NAME} PRIVATE
			core/deps/imgui/backends/imgui_impl_dx11.cpp
			core/deps/imgui/backends/imgui_impl_dx11.h)
	endif()
	target_sources(${PROJECT_NAME} PRIVATE
		core/rend/dx11/dx11_driver.h
		core/rend/dx11/dx11_overlay.cpp
		core/rend/dx11/dx11_overlay.h
		core/rend/dx11/dx11_renderer.cpp
		core/rend/dx11/dx11_renderer.h
		core/rend/dx11/dx11_renderstate.h
		core/rend/dx11/dx11_renderstate.cpp
		core/rend/dx11/dx11_shaders.cpp
		core/rend/dx11/dx11_shaders.h
		core/rend/dx11/dx11_texture.cpp
		core/rend/dx11/dx11_texture.h
		core/rend/dx11/dx11context.cpp
		core/rend/dx11/dx11context.h
		core/rend/dx11/dx11context_lr.cpp
		core/rend/dx11/dx11context_lr.h
		core/rend/dx11/dx11_driver.h
		core/rend/dx11/dx11_naomi2.cpp
		core/rend/dx11/dx11_naomi2.h
		core/rend/dx11/oit/dx11_oitbuffers.h
		core/rend/dx11/oit/dx11_oitrenderer.cpp
		core/rend/dx11/oit/dx11_oitshaders.cpp
		core/rend/dx11/oit/dx11_oitshaders.h)

	target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_D3D11)
	target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 d3dcompiler dxgi)
endif()

if(ENABLE_GDB_SERVER)

	target_sources(${PROJECT_NAME} PRIVATE
		core/debug/gdb_server.cpp)

	target_compile_definitions(${PROJECT_NAME} PRIVATE GDB_SERVER)
endif()

if("arm" IN_LIST ARCHITECTURE AND NOT APPLE)
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/Zc:__cplusplus")
	endif()
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/vixl)
	target_sources(${PROJECT_NAME} PRIVATE
		core/rec-ARM/rec_arm.cpp
		core/deps/vixl/aarch32/location-aarch32.cc
		core/deps/vixl/aarch32/location-aarch32.h
		core/deps/vixl/aarch32/assembler-aarch32.cc
		core/deps/vixl/aarch32/assembler-aarch32.h
		core/deps/vixl/aarch32/instructions-aarch32.cc
		core/deps/vixl/aarch32/instructions-aarch32.h
		core/deps/vixl/aarch32/constants-aarch32.cc
		core/deps/vixl/aarch32/constants-aarch32.h
		core/deps/vixl/aarch32/macro-assembler-aarch32.cc
		core/deps/vixl/aarch32/macro-assembler-aarch32.h
		core/deps/vixl/aarch32/operands-aarch32.cc
		core/deps/vixl/aarch32/operands-aarch32.h
		core/deps/vixl/assembler-base-vixl.h
		core/deps/vixl/code-buffer-vixl.cc
		core/deps/vixl/code-buffer-vixl.h
		core/deps/vixl/code-generation-scopes-vixl.h
		core/deps/vixl/compiler-intrinsics-vixl.cc
		core/deps/vixl/compiler-intrinsics-vixl.h
		core/deps/vixl/cpu-features.cc
		core/deps/vixl/cpu-features.h
		core/deps/vixl/globals-vixl.h
		core/deps/vixl/invalset-vixl.h
		core/deps/vixl/macro-assembler-interface.h
		core/deps/vixl/platform-vixl.h
		core/deps/vixl/pool-manager.h
		core/deps/vixl/pool-manager-impl.h
		core/deps/vixl/utils-vixl.cc
		core/deps/vixl/utils-vixl.h)
endif()
if("arm64" IN_LIST ARCHITECTURE)
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/Zc:__cplusplus")
	endif()
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/vixl)
	target_sources(${PROJECT_NAME} PRIVATE
		core/deps/vixl/aarch64/abi-aarch64.h
		core/deps/vixl/aarch64/assembler-aarch64.cc
		core/deps/vixl/aarch64/assembler-aarch64.h
		core/deps/vixl/aarch64/assembler-sve-aarch64.cc
		core/deps/vixl/aarch64/constants-aarch64.h
		core/deps/vixl/aarch64/instructions-aarch64.cc
		core/deps/vixl/aarch64/instructions-aarch64.h
		core/deps/vixl/aarch64/macro-assembler-aarch64.cc
		core/deps/vixl/aarch64/macro-assembler-aarch64.h
		core/deps/vixl/aarch64/macro-assembler-sve-aarch64.cc
		core/deps/vixl/aarch64/operands-aarch64.cc
		core/deps/vixl/aarch64/operands-aarch64.h
		core/deps/vixl/aarch64/registers-aarch64.cc
		core/deps/vixl/aarch64/registers-aarch64.h
		core/deps/vixl/aarch64/pointer-auth-aarch64.cc
		core/deps/vixl/aarch64/simulator-constants-aarch64.h
		core/deps/vixl/assembler-base-vixl.h
		core/deps/vixl/code-buffer-vixl.cc
		core/deps/vixl/code-buffer-vixl.h
		core/deps/vixl/code-generation-scopes-vixl.h
		core/deps/vixl/compiler-intrinsics-vixl.cc
		core/deps/vixl/compiler-intrinsics-vixl.h
		core/deps/vixl/cpu-features.cc
		core/deps/vixl/cpu-features.h
		core/deps/vixl/globals-vixl.h
		core/deps/vixl/invalset-vixl.h
		core/deps/vixl/macro-assembler-interface.h
		core/deps/vixl/platform-vixl.h
		core/deps/vixl/pool-manager.h
		core/deps/vixl/pool-manager-impl.h
		core/deps/vixl/utils-vixl.cc
		core/deps/vixl/utils-vixl.h)
	target_sources(${PROJECT_NAME} PRIVATE core/rec-ARM64/rec_arm64.cpp core/rec-ARM64/arm64_regalloc.h)
endif()
if("x86" IN_LIST ARCHITECTURE OR "x86_64" IN_LIST ARCHITECTURE)
	add_subdirectory(core/deps/xbyak EXCLUDE_FROM_ALL)
	target_link_libraries(${PROJECT_NAME} PRIVATE xbyak::xbyak)
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		target_sources(${PROJECT_NAME} PRIVATE
			core/rec-x64/xbyak_base.h
			core/rec-x86/rec_x86.h
			core/rec-x86/x86_regalloc.h
			core/rec-x86/rec_x86.cpp
			core/rec-x86/x86_ops.cpp)
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		target_sources(${PROJECT_NAME} PRIVATE
			core/rec-x64/xbyak_base.h
			core/rec-x64/rec_x64.cpp
			core/rec-x64/x64_regalloc.h)
	endif()
endif()

if((USE_OPENGL OR USE_GLES2 OR USE_GLES) AND NOT LIBRETRO)
	add_library(glad STATIC core/deps/glad/src/gl.c)
	if(NOT APPLE AND NOT WIN32 AND NOT SDL2_FOUND)
		# When SDL2 is not found, we can use EGL with ANativeWindow (Android) or X11
		target_sources(glad PRIVATE core/deps/glad/src/egl.c)
	endif()
	target_include_directories(glad PUBLIC core/deps/glad/include)
	target_link_libraries(${PROJECT_NAME} PRIVATE glad)
endif()

if(NOT LIBRETRO)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/ggpo/include core/deps/ggpo/lib/ggpo)
	target_sources(${PROJECT_NAME} PRIVATE
		core/deps/ggpo/lib/ggpo/bitvector.cpp
		core/deps/ggpo/lib/ggpo/bitvector.h
		core/deps/ggpo/lib/ggpo/game_input.cpp
		core/deps/ggpo/lib/ggpo/game_input.h
		core/deps/ggpo/lib/ggpo/input_queue.cpp
		core/deps/ggpo/lib/ggpo/input_queue.h
		core/deps/ggpo/lib/ggpo/main.cpp
		core/deps/ggpo/lib/ggpo/platform_linux.cpp
		core/deps/ggpo/lib/ggpo/platform_linux.h
		core/deps/ggpo/lib/ggpo/platform_windows.cpp
		core/deps/ggpo/lib/ggpo/platform_windows.h
		core/deps/ggpo/lib/ggpo/poll.cpp
		core/deps/ggpo/lib/ggpo/ggpo_poll.h
		core/deps/ggpo/lib/ggpo/ring_buffer.h
		core/deps/ggpo/lib/ggpo/static_buffer.h
		core/deps/ggpo/lib/ggpo/sync.cpp
		core/deps/ggpo/lib/ggpo/sync.h
		core/deps/ggpo/lib/ggpo/timesync.cpp
		core/deps/ggpo/lib/ggpo/timesync.h
		core/deps/ggpo/lib/ggpo/ggpo_types.h

		core/deps/ggpo/lib/ggpo/backends/backend.h
		core/deps/ggpo/lib/ggpo/backends/p2p.cpp
		core/deps/ggpo/lib/ggpo/backends/p2p.h
		core/deps/ggpo/lib/ggpo/backends/spectator.cpp
		core/deps/ggpo/lib/ggpo/backends/spectator.h
		core/deps/ggpo/lib/ggpo/backends/synctest.cpp
		core/deps/ggpo/lib/ggpo/backends/synctest.h

		core/deps/ggpo/lib/ggpo/network/udp_msg.h
		core/deps/ggpo/lib/ggpo/network/udp_proto.cpp
		core/deps/ggpo/lib/ggpo/network/udp_proto.h
		core/deps/ggpo/lib/ggpo/network/udp.cpp
		core/deps/ggpo/lib/ggpo/network/udp.h)

	if(ANDROID)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)
		add_subdirectory(shell/android-studio/flycast/src/main/jni/src)
		target_link_libraries(${PROJECT_NAME} PRIVATE android log)

	elseif(APPLE)
		string(TIMESTAMP YEAR "%Y")
		string(REGEX MATCH "^[^-]+" TAG_VERSION ${GIT_VERSION})
		if(IOS)
			set_property(TARGET ${PROJECT_NAME} PROPERTY XCODE_ATTRIBUTE_SWIFT_VERSION "5.0")
			add_subdirectory(shell/apple/emulator-ios/AltKit)
			target_link_libraries(${PROJECT_NAME} PRIVATE AltKit)

			target_sources(${PROJECT_NAME} PRIVATE
					shell/apple/common/http_client.mm
					shell/apple/common/util.mm
					shell/apple/emulator-ios/emulator/AppDelegate.h
					shell/apple/emulator-ios/emulator/AppDelegate.mm
					shell/apple/emulator-ios/emulator/ios_main.mm
					shell/apple/emulator-ios/emulator/ios_gamepad.h
					shell/apple/emulator-ios/emulator/ios_keyboard.h
					shell/apple/emulator-ios/emulator/ios_mouse.h
					shell/apple/emulator-ios/emulator/FlycastViewController.h
					shell/apple/emulator-ios/emulator/FlycastViewController.mm
					shell/apple/emulator-ios/emulator/PadViewController.h
					shell/apple/emulator-ios/emulator/PadViewController.mm
					shell/apple/emulator-ios/emulator/EditPadViewController.h
					shell/apple/emulator-ios/emulator/EditPadViewController.mm
					shell/apple/emulator-ios/emulator/EmulatorView.h
					shell/apple/emulator-ios/emulator/EmulatorView.mm
					shell/apple/emulator-ios/emulator/main.m
					shell/apple/emulator-ios/emulator/iCade-iOS/iCadeReaderView.h
					shell/apple/emulator-ios/emulator/iCade-iOS/iCadeReaderView.m
					shell/apple/emulator-ios/emulator/iCade-iOS/iCadeState.h)
			target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fobjc-arc -fmodules>)
			set_source_files_properties(shell/apple/emulator-ios/emulator/FlycastViewController.mm PROPERTIES COMPILE_OPTIONS -fcxx-modules)
			set_source_files_properties(shell/apple/emulator-ios/emulator/iCade-iOS/iCadeReaderView.m PROPERTIES COMPILE_OPTIONS -fno-objc-arc)

			set(IOS_RESOURCES
				shell/apple/emulator-ios/emulator/Images.xcassets
				shell/apple/emulator-ios/emulator/FlycastStoryboard.storyboard
				shell/apple/emulator-ios/emulator/LaunchScreen.storyboard
				shell/apple/emulator-ios/emulator/PadViewController.xib
				shell/apple/emulator-ios/emulator/EditPadViewController.xib)
			target_sources(${PROJECT_NAME} PRIVATE ${IOS_RESOURCES})
			source_group("Resources" FILES ${IOS_RESOURCES})
			set_source_files_properties(${IOS_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

			set_target_properties(${PROJECT_NAME} PROPERTIES
				OUTPUT_NAME "Flycast"
				MACOSX_BUNDLE YES
				MACOSX_BUNDLE_EXECUTABLE_NAME "Flycast"
				MACOSX_BUNDLE_INFO_STRING ""
				MACOSX_BUNDLE_GUI_IDENTIFIER "com.flyinghead.Flycast"
				MACOSX_BUNDLE_BUNDLE_NAME "com.flyinghead.Flycast"
				MACOSX_BUNDLE_LONG_VERSION_STRING "${GIT_VERSION}"
				MACOSX_BUNDLE_SHORT_VERSION_STRING "${TAG_VERSION}"
				MACOSX_BUNDLE_BUNDLE_VERSION "${GIT_HASH}"
				MACOSX_BUNDLE_COPYRIGHT "Copyright © ${YEAR} Flycast contributors. All rights reserved."
				MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/emulator-ios/plist.in

				XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
				XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/emulator-ios/emulator/flycast-ios-Prefix.pch"
				XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES"
				XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
				XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
				XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
				XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
				XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES NO
				XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
				XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.flyinghead.Flycast")

			find_library(UIKIT UIKit)
			find_library(FOUNDATION Foundation)
			find_library(GLKIT GLKit)
			find_library(GAMECONTROLLER GameController)
			find_library(AUDIOTOOLBOX AudioToolbox)
			find_library(AVFOUNDATION AVFoundation)
			target_link_libraries(${PROJECT_NAME} PRIVATE
				${UIKIT}
				${FOUNDATION}
				${GLKIT}
				${GAMECONTROLLER}
				${AUDIOTOOLBOX}
				${AVFOUNDATION})

			add_custom_target(Flycast.IPA ALL
				DEPENDS ${PROJECT_NAME}
				COMMENT "Building IPA"
				COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/Payload
				COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/Payload/*
				COMMAND cp -r ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>-${CMAKE_OSX_SYSROOT}/Flycast.app ${CMAKE_CURRENT_BINARY_DIR}/Payload
				COMMAND for lib in ${CMAKE_CURRENT_BINARY_DIR}/Payload/Flycast.app/Frameworks/*.dylib
				COMMAND do xcrun bitcode_strip -r $lib -o $lib
				COMMAND done
				COMMAND ldid -S${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/emulator-ios/emulator/flycast.entitlements ${CMAKE_CURRENT_BINARY_DIR}/Payload/Flycast.app/Flycast
				COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>-${CMAKE_OSX_SYSROOT}/Flycast.ipa" --format=zip ${CMAKE_CURRENT_BINARY_DIR}/Payload)
		else()

			add_subdirectory(core/deps/Syphon)
			target_link_libraries(${PROJECT_NAME} PRIVATE Syphon)
			add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy ${SYPHON_METALLIB}
				$<TARGET_FILE_DIR:flycast>/../Resources/)
			target_compile_definitions(${PROJECT_NAME} PRIVATE VIDEO_ROUTING)

			target_sources(${PROJECT_NAME} PRIVATE
					shell/apple/common/http_client.mm
					shell/apple/common/util.mm
					shell/apple/emulator-osx/emulator-osx/SDLApplicationDelegate.h
					shell/apple/emulator-osx/emulator-osx/SDLApplicationDelegate.mm
					shell/apple/emulator-osx/emulator-osx/osx-main.mm)
			set(ASSETS shell/apple/emulator-osx/emulator-osx/Images.xcassets)
			target_sources(${PROJECT_NAME} PRIVATE ${ASSETS})
			source_group("Resources" FILES ${ASSETS})
			set_source_files_properties(${ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

			set_target_properties(${PROJECT_NAME} PROPERTIES
				OUTPUT_NAME "Flycast"
				MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/emulator-osx/MacOSXBundleInfo.plist.in
				MACOSX_BUNDLE_EXECUTABLE_NAME "Flycast"
				MACOSX_BUNDLE_INFO_STRING ""
				MACOSX_BUNDLE_ICON_FILE "AppIcon"
				MACOSX_BUNDLE_GUI_IDENTIFIER "com.flyinghead.Flycast"
				MACOSX_BUNDLE_LONG_VERSION_STRING "${GIT_VERSION}"
				MACOSX_BUNDLE_SHORT_VERSION_STRING "${TAG_VERSION}"
				MACOSX_BUNDLE_BUNDLE_VERSION "${GIT_HASH}"
				MACOSX_BUNDLE_BUNDLE_NAME "Flycast"
				MACOSX_BUNDLE_COPYRIGHT "Copyright © ${YEAR} Flycast contributors. All rights reserved."
				XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf"
				XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=RelWithDebInfo] "dwarf-with-dsym"
				XCODE_ATTRIBUTE_CLANG_DEBUG_INFORMATION_LEVEL[variant=RelWithDebInfo] "line-tables-only"
				XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=RelWithDebInfo] YES
				XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
				XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.flyinghead.Flycast"
				BUILD_WITH_INSTALL_RPATH TRUE
				INSTALL_RPATH "@loader_path/../Frameworks"
			)

			find_library(AUDIO_UNIT_LIBRARY AudioUnit)
			find_library(FOUNDATION_LIBRARY Foundation)
			find_library(AUDIO_TOOLBOX_LIBRARY AudioToolbox)
			find_library(METAL_LIBRARY Metal)
			find_library(IOSURFACE_LIBRARY IOSurface)
			find_library(MULTITOUCH_SUPPORT_LIBRARY MultitouchSupport /System/Library/PrivateFrameworks)

			target_link_libraries(${PROJECT_NAME} PRIVATE
				${AUDIO_UNIT_LIBRARY}
				${FOUNDATION_LIBRARY}
				${AUDIO_TOOLBOX_LIBRARY}
				${METAL_LIBRARY}
				${IOSURFACE_LIBRARY}
				${MULTITOUCH_SUPPORT_LIBRARY})

			add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy "$ENV{VULKAN_SDK}/lib/libMoltenVK.dylib"
				$<TARGET_FILE_DIR:flycast>/../Frameworks/libvulkan.dylib)
		endif()
	elseif(UNIX)
		if(NOT BUILD_TESTING)
			target_sources(${PROJECT_NAME} PRIVATE
					core/linux-dist/main.cpp)
		endif()
	elseif(WIN32)
		target_sources(${PROJECT_NAME} PRIVATE
			core/windows/rawinput.cpp
			core/windows/rawinput.h)
		if(NOT BUILD_TESTING)
			target_sources(${PROJECT_NAME} PRIVATE core/windows/winmain.cpp)
		endif()
		if(WINDOWS_STORE)
			file(READ shell/uwp/Package.appxmanifest MANIFEST)
			string(REPLACE "9.9.9.9" ${MS_VERSION} MANIFEST ${MANIFEST})
			file(WRITE ${CMAKE_BINARY_DIR}/Package.appxmanifest ${MANIFEST})
			set(ResourceFiles ${CMAKE_BINARY_DIR}/Package.appxmanifest
					shell/uwp/flycast150.png
					shell/uwp/flycast50.png
					shell/uwp/flycast44.png
					shell/uwp/splash.png
					shell/uwp/flycast44.targetsize-48_altform-unplated.png
					shell/uwp/flycast44.targetsize-48_altform-lightunplated.png
					core/deps/SDL/src/main/winrt/SDL2-WinRTResource_BlankCursor.cur
					core/deps/SDL/src/main/winrt/SDL2-WinRTResources.rc)
			target_sources(${PROJECT_NAME} PRIVATE
				${ResourceFiles}
				shell/uwp/http_client.cpp)
			if(NOT BUILD_TESTING)
				target_sources(${PROJECT_NAME} PRIVATE core/deps/SDL/src/main/winrt/SDL_winrt_main_NonXAML.cpp)
			endif()
			set_target_properties(${PROJECT_NAME} PROPERTIES RESOURCE "${ResourceFiles}")
		else()
			if(NOT BUILD_TESTING AND ("x86" IN_LIST ARCHITECTURE OR "x86_64" IN_LIST ARCHITECTURE))
				target_include_directories(${PROJECT_NAME} PRIVATE core/deps/Spout/SPOUTSDK/SpoutGL)
				target_include_directories(${PROJECT_NAME} PRIVATE core/deps/Spout/SPOUTSDK/SpoutDirectX/SpoutDX)
				SET(SPOUT_BUILD_CMT OFF CACHE BOOL "For Visual Studio - build /MT to link runtime libraries" FORCE)
				SET(SPOUT_BUILD_SPOUTDX ON CACHE BOOL "Build SpoutDX DirectX11 support library" FORCE)
				SET(SPOUT_BUILD_LIBRARY OFF CACHE BOOL "Build C-compatible cross compiler library" FORCE)
				SET(SKIP_INSTALL_ALL ON CACHE BOOL "Install headers and libraries" FORCE)
				add_subdirectory(core/deps/Spout)
				target_link_libraries(${PROJECT_NAME} PRIVATE Spout_static SpoutDX_static)
				target_compile_definitions(${PROJECT_NAME} PRIVATE VIDEO_ROUTING)
			endif()

			target_sources(${PROJECT_NAME} PRIVATE shell/windows/flycast.rc)
			target_link_libraries(${PROJECT_NAME} PRIVATE dsound winmm ws2_32 wsock32 xinput9_1_0 cfgmgr32 wininet psapi setupapi)
		endif()
	endif()
endif()

if(BUILD_TESTING)
	add_subdirectory(core/deps/googletest EXCLUDE_FROM_ALL)
	add_subdirectory(tests)
endif()

if(NINTENDO_SWITCH)
	if(LIBRETRO)
		add_custom_target(combined ALL
			COMMAND ${CMAKE_AR} -x $<TARGET_FILE:xxHash::xxhash>
			COMMAND ${CMAKE_AR} -x $<TARGET_FILE:chdr-static>
			COMMAND ${CMAKE_AR} -x $<TARGET_FILE:zip>
			COMMAND ${CMAKE_AR} -x $<TARGET_FILE:elf>
			COMMAND ${CMAKE_AR} -x $<TARGET_FILE:flycast::res>
			COMMAND ${CMAKE_AR} -rs flycast_libretro_libnx.a *.o
			COMMAND rm *.o
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
			DEPENDS xxHash::xxhash chdr-static zip ${PROJECT_NAME})
	else()
		nx_generate_nacp(flycast.nacp NAME "Flycast" AUTHOR "flyinghead, M4xw" VERSION "${GIT_VERSION}")
		nx_create_nro(flycast NACP flycast.nacp ICON "${CMAKE_SOURCE_DIR}/shell/switch/flycast.jpeg")
	endif()
endif()

if(IOS)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>-${CMAKE_OSX_SYSROOT}/Flycast.ipa" TYPE BIN)
elseif(NINTENDO_SWITCH AND NOT LIBRETRO)
	install(FILES ${CMAKE_BINARY_DIR}/flycast.nro DESTINATION "${CMAKE_INSTALL_BINDIR}")
elseif(LIBRETRO)
	install(TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_LIBDIR}/libretro")
else()
	install(TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT LIBRETRO)
	install(FILES shell/linux/man/${PROJECT_NAME}.1
		DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
	)
	install(FILES shell/linux/${PROJECT_NAME}.desktop
		DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
	)
	install(FILES shell/linux/${PROJECT_NAME}.png
		DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/pixmaps"
	)
	install(FILES shell/linux/org.${PROJECT_NAME}.Flycast.metainfo.xml
		DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo"
	)
	foreach(size 16 32 64 128 256 512)
		install(FILES
			shell/apple/emulator-osx/emulator-osx/Images.xcassets/AppIcon.appiconset/Icon-${size}.png
			DESTINATION
			"${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${size}x${size}/apps"
			RENAME ${PROJECT_NAME}.png
		)
	endforeach()
endif()

if(${CMAKE_GENERATOR} MATCHES "^Xcode.*|^Visual Studio.*")
	file(GLOB_RECURSE SRC_FILES *.h *.cpp *.c *.cc *.mm)
	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})
endif()
