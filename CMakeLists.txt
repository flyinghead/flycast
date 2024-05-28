cmake_minimum_required(VERSION 3.10.2)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
	set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "Compiler launcher for C.")
	set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "Compiler launcher for CXX.")
endif()

set(SENTRY_UPLOAD_URL "" CACHE STRING "Sentry upload URL")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shell/cmake")

if(APPLE)
	if(CMAKE_SYSTEM_NAME STREQUAL iOS)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS deployment version")
		set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
	elseif(CMAKE_SYSTEM_NAME STREQUAL tvOS)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum tvOS deployment version")
		set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
	else()
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "Minimum macOS deployment version")
		set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
	endif()
endif()

if(LIBRETRO)
	project(flycast_libretro)
else()
	project(flycast)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NINTENDO_SWITCH)
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
if(NOT APPLE AND (NOT UNIX OR CMAKE_SYSTEM_NAME MATCHES "(FreeBSD|OpenBSD|NetBSD)"))
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
option(USE_OPENMP "Use OpenMP if available" ON)
option(USE_VULKAN "Build with Vulkan support" ON)
option(USE_DX9 "Build with Direct3D 9 support" ON)
option(USE_DX11 "Build with Direct3D 11 support" ON)
option(LIBRETRO "Build libretro core" OFF)
option(USE_OPENGL "Use OpenGL API" ON)
option(USE_VIDEOCORE "RPI: use the legacy Broadcom GLES libraries" OFF)
option(APPLE_BREAKPAD "macOS: Build breakpad client and dump symbols" OFF)
option(ENABLE_GDB_SERVER "Build with GDB debugging support" OFF)
option(ENABLE_DC_PROFILER "Build with support for target machine (SH4) profiler" OFF)
option(ENABLE_FC_PROFILER "Build with support for host app (Flycast) profiler" OFF)
option(USE_DISCORD "Use Discord Presence API" OFF)

if(IOS AND NOT LIBRETRO)
	set(USE_VULKAN OFF CACHE BOOL "Force vulkan off" FORCE)
endif()

include(GNUInstallDirs)
include(CMakeRC)

if(ENABLE_CTEST)
	include(CTest)
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

elseif(LIBRETRO)
	add_library(${PROJECT_NAME} SHARED core/emulator.cpp)
	if(APPLE)
		if(NOT IOS)
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,error")
		endif()
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/shell/libretro/libretro.osx.def")
	else()
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
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
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3 HAVE_OPENGLES HAVE_OPENGLES3)
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
	add_executable(${PROJECT_NAME} WIN32 core/emulator.cpp)
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

if(${CMAKE_OSX_SYSROOT} MATCHES "\/MacOSX.platform\/Developer\/SDKs")
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
		$<$<BOOL:${WINDOWS_STORE}>:NOCRYPT>
		$<$<OR:$<BOOL:${MINGW}>,$<BOOL:${MSVC}>>:_USE_MATH_DEFINES>)

if(UNIX AND NOT ANDROID AND NOT APPLE)
	execute_process(COMMAND getconf PAGESIZE OUTPUT_VARIABLE PAGE_SIZE OUTPUT_STRIP_TRAILING_WHITESPACE)
	target_compile_definitions(${PROJECT_NAME} PRIVATE PAGE_SIZE=${PAGE_SIZE})
endif()

if(NOT "${SENTRY_UPLOAD_URL}" STREQUAL "")
	target_compile_definitions(${PROJECT_NAME} PRIVATE SENTRY_UPLOAD="${SENTRY_UPLOAD_URL}")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE core core/deps core/deps/stb core/deps/json)
if(LIBRETRO)
	target_include_directories(${PROJECT_NAME} PRIVATE shell/libretro)
endif()
if(NINTENDO_SWITCH)
	target_include_directories(${PROJECT_NAME} PRIVATE shell/switch)
endif()

if(NOT LIBRETRO)
	find_program(SH_EXECUTABLE sh)
	if((ANDROID AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows") OR (MINGW AND SH_EXECUTABLE) OR (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
		add_subdirectory(core/deps/breakpad)
		if(WIN32)
			target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--build-id")
		endif()
		target_link_libraries(${PROJECT_NAME} PUBLIC breakpad_client)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_BREAKPAD)
	elseif(APPLE AND NOT IOS AND APPLE_BREAKPAD)
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
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Flycast.app/Contents/MacOS/Flycast > ../symbols/Flycast-x64.sym
				COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/breakpad/dump_syms
					-a arm64
					-g ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Flycast.app.dSYM
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Flycast.app/Contents/MacOS/Flycast > ../symbols/Flycast-arm64.sym
			)
		else()
			add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND sleep 20
				COMMAND mkdir -p ../symbols
				COMMAND dsymutil ${CMAKE_CURRENT_BINARY_DIR}/Flycast.app/Contents/MacOS/Flycast
					-o ${CMAKE_CURRENT_BINARY_DIR}/Flycast.app.dSYM
				COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/breakpad/dump_syms
					-a x86_64
					-g ${CMAKE_CURRENT_BINARY_DIR}/Flycast.app.dSYM
					${CMAKE_CURRENT_BINARY_DIR}/Flycast.app/Contents/MacOS/Flycast > ../symbols/Flycast-x64.sym
				COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/breakpad/dump_syms
					-a arm64
					-g ${CMAKE_CURRENT_BINARY_DIR}/Flycast.app.dSYM
					${CMAKE_CURRENT_BINARY_DIR}/Flycast.app/Contents/MacOS/Flycast > ../symbols/Flycast-arm64.sym
			)
		endif()
	endif()
endif()

if(USE_OPENMP)
	find_package(OpenMP)
	if(OpenMP_CXX_FOUND)
		if(MINGW)
			target_link_libraries(${PROJECT_NAME} PRIVATE "-static -lgomp -lpthread")
			target_compile_options(${PROJECT_NAME} PRIVATE -fopenmp)
		elseif(ANDROID)
			# Reference: https://android.googlesource.com/platform/ndk/+/refs/heads/master/tests/device/openmp/CMakeLists.txt
			set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_OPTIONS -fopenmp)
			target_link_libraries(${PROJECT_NAME} PRIVATE -fopenmp -static-openmp)
		else()
			target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
		endif()
	endif()
	if(IOS)
		message("-- Finding OpenMP.xcframework for iOS")
		set(OPENMP_XCFRAMEWORK "${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/emulator-ios/OpenMP/OpenMP.xcframework")
		if(IS_DIRECTORY ${OPENMP_XCFRAMEWORK})
			# CMake does not support linking xcframework directly atm. (see https://gitlab.kitware.com/cmake/cmake/-/issues/21752)
			set(OPENMP_FRAMEWORK ${OPENMP_XCFRAMEWORK}/ios-arm64_arm64e/OpenMP.framework/Versions/A/libomp.a)
			if(EXISTS ${OPENMP_FRAMEWORK})
				message("-- Found OpenMP.xcframework: ${OPENMP_FRAMEWORK} (Linking static library before CMake support linking xcframework)")
				target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENMP_FRAMEWORK})
				target_include_directories(${PROJECT_NAME} PRIVATE ${OPENMP_XCFRAMEWORK}/ios-arm64_arm64e/OpenMP.framework/Versions/A/Headers)
				set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fopenmp")
			endif()
		else()
			message("-- Could NOT find OpenMP.xcframework, compile it using shell/apple/emulator-ios/OpenMP/build_ios_openmp.sh")
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
		option(SKIP_GLSLANG_INSTALL "Skip installation" ON)
		option(ENABLE_SPVREMAPPER "Enables building of SPVRemapper" OFF)
		option(ENABLE_GLSLANG_BINARIES "Builds glslang and spirv-remap" OFF)
		option(ENABLE_HLSL "Enables HLSL input support" OFF)
		option(ENABLE_PCH "Enables Precompiled header" OFF)
		add_subdirectory(core/deps/glslang EXCLUDE_FROM_ALL)
		add_library(glslang::glslang-default-resource-limits ALIAS glslang-default-resource-limits)
		add_library(glslang::SPIRV ALIAS SPIRV)
	endif()
	target_link_libraries(${PROJECT_NAME} PRIVATE glslang::glslang-default-resource-limits glslang::SPIRV)
endif()

if(NOT LIBRETRO)
	find_package(ALSA)
	if(ALSA_FOUND AND NOT ANDROID)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ALSA)
		target_include_directories(${PROJECT_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
		target_link_libraries(${PROJECT_NAME} PRIVATE ${ALSA_LIBRARIES})
	endif()

	if(MINGW)
		target_link_libraries(${PROJECT_NAME} PRIVATE "-static-libgcc -static-libstdc++")
	endif()

	if(NOT ANDROID AND NOT IOS)
		if(USE_HOST_SDL)
			find_package(SDL2 2.0.9)
		endif()
		if(NOT SDL2_FOUND)
			set(SDL_TEST_ENABLED_BY_DEFAULT OFF)
			add_subdirectory(core/deps/SDL EXCLUDE_FROM_ALL)
			set(SDL2_FOUND 1)
		endif()

		# SDL2::SDL2main may or may not be available. It is e.g. required by Windows GUI applications
		if(TARGET SDL2::SDL2main)
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
		target_sources(${PROJECT_NAME} PRIVATE core/sdl/sdl.cpp core/sdl/sdl.h core/sdl/sdl_gamepad.h core/sdl/sdl_keyboard.h)

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

	find_package(Lua 5.2)
	if(NOT APPLE AND LUA_FOUND)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LUA)
		target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR} core/deps/luabridge/Source)
		list(TRANSFORM LUA_LIBRARIES REPLACE "\.dll" "")
		target_link_libraries(${PROJECT_NAME} PRIVATE ${LUA_LIBRARIES})
	endif()
endif()

add_subdirectory(core/deps/libchdr EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PRIVATE chdr-static)
target_include_directories(${PROJECT_NAME} PRIVATE core/deps/libchdr/include)

if(NOT WITH_SYSTEM_ZLIB)
	set(ZLIB_RELATIVE_PATH "core/deps/libchdr/deps/zlib-1.3.1")
	target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${ZLIB_RELATIVE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${ZLIB_RELATIVE_PATH}")
	# help libzip find the package
	set(ZLIB_FOUND TRUE)
	set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${ZLIB_RELATIVE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${ZLIB_RELATIVE_PATH}")
	
	cmake_policy(SET CMP0026 OLD)
	get_target_property(ZLIB_LIBRARY_RELEASE zlibstatic LOCATION)
	get_target_property(ZLIB_LIBRARY_DEBUG zlibstatic LOCATION_Debug)
endif()

find_package(PkgConfig)
if(PKG_CONFIG_FOUND AND NOT ANDROID AND NOT APPLE AND NOT LIBRETRO)
	pkg_check_modules(AO IMPORTED_TARGET ao)
	if(AO_FOUND)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LIBAO)
		target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::AO)
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

	pkg_check_modules(LIBPULSE IMPORTED_TARGET libpulse)
	if(LIBPULSE_FOUND)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PULSEAUDIO)
		target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBPULSE)
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
	option(ENABLE_ZSTD "Enable use of Zstandard" OFF)
	option(BUILD_TOOLS "Build tools in the src directory (zipcmp, zipmerge, ziptool)" OFF)
	option(BUILD_REGRESS "Build regression tests" OFF)
	option(BUILD_EXAMPLES "Build examples" OFF)
	option(BUILD_DOC "Build documentation" OFF)
	option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
	option(LIBZIP_DO_INSTALL "Install libzip and the related files" OFF)
	add_subdirectory(core/deps/libzip)
	target_link_libraries(${PROJECT_NAME} PRIVATE libzip::zip)
endif()

if(WIN32)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/dirent)
endif()

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
	target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DEFINE_MATH_OPERATORS)
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
	add_subdirectory(core/deps/discord-rpc)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/discord-rpc/include)
	target_link_libraries(${PROJECT_NAME} PRIVATE discord-rpc)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_DISCORD)
	target_sources(${PROJECT_NAME} PRIVATE core/ui/discord.cpp)
endif()

cmrc_add_resource_library(flycast-resources ALIAS flycast::res NAMESPACE flycast)
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
endif()

target_sources(${PROJECT_NAME} PRIVATE
		core/archive/7zArchive.cpp
		core/archive/7zArchive.h
		core/archive/archive.cpp
		core/archive/archive.h
		core/archive/rzip.cpp
		core/archive/rzip.h
		core/archive/ZipArchive.cpp
		core/archive/ZipArchive.h
		core/cfg/option.h)

if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
			core/cfg/cfg.cpp
			core/cfg/cfg.h
			core/cfg/cl.cpp
			core/cfg/ini.cpp
			core/cfg/ini.h
			core/cfg/option.cpp)
endif()

target_sources(${PROJECT_NAME} PRIVATE
		core/hw/hwreg.h
		core/hw/aica/aica.cpp
		core/hw/aica/aica.h
		core/hw/aica/aica_if.cpp
		core/hw/aica/aica_if.h
		core/hw/aica/aica_mem.cpp
		core/hw/aica/aica_mem.h
		core/hw/aica/dsp.cpp
		core/hw/aica/dsp.h
		core/hw/aica/dsp_arm32.cpp
		core/hw/aica/dsp_arm64.cpp
		core/hw/aica/dsp_interp.cpp
		core/hw/aica/dsp_x64.cpp
		core/hw/aica/dsp_x86.cpp
		core/hw/aica/sgc_if.cpp
		core/hw/aica/sgc_if.h
		core/hw/arm7/arm7.cpp
		core/hw/arm7/arm7.h
		core/hw/arm7/arm_mem.cpp
		core/hw/arm7/arm_mem.h
		core/hw/arm7/arm7_rec_arm32.cpp
		core/hw/arm7/arm7_rec_arm64.cpp
		core/hw/arm7/arm7_rec_x64.cpp
		core/hw/arm7/arm7_rec.cpp
		core/hw/arm7/arm7_rec.h
		core/hw/arm7/arm-new.h
		core/hw/bba/bba.h
		core/hw/bba/bba.cpp
		core/hw/bba/rtl8139c.h
		core/hw/bba/rtl8139c.cpp
		core/hw/flashrom/at93cxx.cpp
		core/hw/flashrom/at93cxx.h
		core/hw/flashrom/flashrom.cpp
		core/hw/flashrom/flashrom.h
		core/hw/flashrom/nvmem.cpp
		core/hw/flashrom/nvmem.h
		core/hw/flashrom/x76f100.cpp
		core/hw/flashrom/x76f100.h
		core/hw/gdrom/gdrom_if.h
		core/hw/gdrom/gdrom_response.cpp
		core/hw/gdrom/gdromv3.cpp
		core/hw/gdrom/gdromv3.h
		core/hw/holly/holly_intc.cpp
		core/hw/holly/holly_intc.h
		core/hw/holly/sb.cpp
		core/hw/holly/sb.h
		core/hw/holly/sb_mem.cpp
		core/hw/holly/sb_mem.h
		core/hw/maple/maple_cfg.cpp
		core/hw/maple/maple_cfg.h
		core/hw/maple/maple_devs.cpp
		core/hw/maple/maple_devs.h
		core/hw/maple/maple_helper.cpp
		core/hw/maple/maple_helper.h
		core/hw/maple/maple_if.cpp
		core/hw/maple/maple_if.h
		core/hw/maple/maple_jvs.cpp
		core/hw/mem/addrspace.cpp
		core/hw/mem/addrspace.h
		core/hw/mem/mem_watch.cpp
		core/hw/mem/mem_watch.h
		core/hw/modem/modem.cpp
		core/hw/modem/modem.h
		core/hw/modem/modem_regs.h
		core/hw/naomi/awave_regs.h
		core/hw/naomi/awcartridge.cpp
		core/hw/naomi/awcartridge.h
		core/hw/naomi/decrypt.cpp
		core/hw/naomi/decrypt.h
		core/hw/naomi/gdcartridge.cpp
		core/hw/naomi/gdcartridge.h
		core/hw/naomi/m1cartridge.cpp
		core/hw/naomi/m1cartridge.h
		core/hw/naomi/m4cartridge.cpp
		core/hw/naomi/m4cartridge.h
		core/hw/naomi/multiboard.h
		core/hw/naomi/multiboard.cpp
		core/hw/naomi/naomi_cart.cpp
		core/hw/naomi/naomi_cart.h
		core/hw/naomi/naomi_flashrom.cpp
		core/hw/naomi/naomi_flashrom.h
		core/hw/naomi/naomi_m3comm.cpp
		core/hw/naomi/naomi_m3comm.h
		core/hw/naomi/naomi.cpp
		core/hw/naomi/naomi.h
		core/hw/naomi/naomi_regs.h
		core/hw/naomi/naomi_roms.cpp
		core/hw/naomi/naomi_roms.h
		core/hw/naomi/naomi_roms_input.h
		core/hw/naomi/netdimm.cpp
		core/hw/naomi/netdimm.h
		core/hw/naomi/card_reader.h
		core/hw/naomi/card_reader.cpp
		core/hw/naomi/touchscreen.h
		core/hw/naomi/touchscreen.cpp
		core/hw/naomi/printer.h
		core/hw/naomi/printer.cpp
		core/hw/naomi/systemsp.h
		core/hw/naomi/systemsp.cpp
		core/hw/naomi/hopper.h
		core/hw/naomi/hopper.cpp
		core/hw/pvr/elan.cpp
		core/hw/pvr/elan.h
		core/hw/pvr/elan_struct.h
		core/hw/pvr/pvr.cpp
		core/hw/pvr/pvr.h
		core/hw/pvr/pvr_mem.cpp
		core/hw/pvr/pvr_mem.h
		core/hw/pvr/pvr_regs.cpp
		core/hw/pvr/pvr_regs.h
		core/hw/pvr/pvr_sb_regs.cpp
		core/hw/pvr/pvr_sb_regs.h
		core/hw/pvr/Renderer_if.cpp
		core/hw/pvr/Renderer_if.h
		core/hw/pvr/spg.cpp
		core/hw/pvr/spg.h
		core/hw/pvr/ta_const_df.h
		core/hw/pvr/ta.cpp
		core/hw/pvr/ta_ctx.cpp
		core/hw/pvr/ta_ctx.h
		core/hw/pvr/ta.h
		core/hw/pvr/ta_structs.h
		core/hw/pvr/ta_util.cpp
		core/hw/pvr/ta_vtx.cpp
		core/hw/sh4/dyna/blockmanager.cpp
		core/hw/sh4/dyna/blockmanager.h
		core/hw/sh4/dyna/decoder.cpp
		core/hw/sh4/dyna/decoder.h
		core/hw/sh4/dyna/decoder_opcodes.h
		core/hw/sh4/dyna/driver.cpp
		core/hw/sh4/dyna/ngen.h
		core/hw/sh4/dyna/shil_canonical.h
		core/hw/sh4/dyna/shil.cpp
		core/hw/sh4/dyna/shil.h
		core/hw/sh4/dyna/ssa.cpp
		core/hw/sh4/dyna/ssa.h
		core/hw/sh4/dyna/ssa_regalloc.h
		core/hw/sh4/fsca-table.h
		core/hw/sh4/interpr/sh4_fpu.cpp
		core/hw/sh4/interpr/sh4_interpreter.cpp
		core/hw/sh4/interpr/sh4_opcodes.cpp
		core/hw/sh4/interpr/sh4_opcodes.h
		core/hw/sh4/modules/bsc.cpp
		core/hw/sh4/modules/ccn.cpp
		core/hw/sh4/modules/ccn.h
		core/hw/sh4/modules/cpg.cpp
		core/hw/sh4/modules/dmac.cpp
		core/hw/sh4/modules/dmac.h
		core/hw/sh4/modules/fastmmu.cpp
		core/hw/sh4/modules/intc.cpp
		core/hw/sh4/modules/mmu.cpp
		core/hw/sh4/modules/mmu.h
		core/hw/sh4/modules/modules.h
		core/hw/sh4/modules/rtc.cpp
		core/hw/sh4/modules/serial.cpp
		core/hw/sh4/modules/tmu.cpp
		core/hw/sh4/modules/ubc.cpp
		core/hw/sh4/modules/wince.h
		core/hw/sh4/sh4_cache.h
		core/hw/sh4/sh4_cycles.cpp
		core/hw/sh4/sh4_cycles.h
		core/hw/sh4/sh4_core.h
		core/hw/sh4/sh4_core_regs.cpp
		core/hw/sh4/sh4_if.h
		core/hw/sh4/sh4_interpreter.h
		core/hw/sh4/sh4_interrupts.cpp
		core/hw/sh4/sh4_interrupts.h
		core/hw/sh4/sh4_mem.cpp
		core/hw/sh4/sh4_mem.h
		core/hw/sh4/sh4_mmr.cpp
		core/hw/sh4/sh4_mmr.h
		core/hw/sh4/sh4_opcode_list.cpp
		core/hw/sh4/sh4_opcode_list.h
		core/hw/sh4/sh4_rom.cpp
		core/hw/sh4/sh4_rom.h
		core/hw/sh4/sh4_sched.cpp
		core/hw/sh4/sh4_sched.h
		core/hw/sh4/storeq.cpp)

cmrc_add_resources(flycast-resources
		WHENCE resources
		resources/flash/alienfnt.nvmem.zip	# network settings
		resources/flash/gunsur2.nvmem.zip
		resources/flash/mazan.nvmem.zip		# free play, calibrated
		resources/flash/otrigger.nvmem.zip
		resources/flash/wldkicks.nvmem.zip	# free play
		resources/flash/wldkicksj.nvmem.zip	# free play
		resources/flash/wldkicksu.nvmem.zip	# free play
		resources/flash/f355.nvmem.zip		# printer on
		resources/flash/f355twin.nvmem.zip	# printer on
		resources/flash/f355twn2.nvmem.zip	# printer on
		resources/flash/dirtypig.nvmem.zip	# 4 players
		resources/flash/dirtypig.nvmem2.zip
		resources/flash/vf4.nvmem.zip		# card all day, stage select
		resources/flash/vf4evob.nvmem.zip	# card all day, stage select
		resources/flash/vf4tuned.nvmem.zip  # card all day, stage select, 45 sec time limit, 3 match
		resources/flash/magicpop.nvmem.zip	# debug: comm and all errors disabled
		resources/flash/ochaken.nvmem.zip	# debug: comm and all errors disabled
		resources/flash/puyomedal.nvmem.zip	# debug: comm and all errors disabled
		resources/flash/unomedal.nvmem.zip	# debug: comm and all errors disabled
		resources/flash/westdrmg.nvmem.zip	# debug: comm and all errors disabled
		resources/picture/f355_print_template.png)

cmrc_add_resources(flycast-resources
		fonts/printer_ascii8x16.bin.zip
		fonts/printer_ascii12x24.bin.zip
		fonts/printer_kanji16x16.bin.zip
		fonts/printer_kanji24x24.bin.zip)

if(NOT LIBRETRO)
	cmrc_add_resources(flycast-resources
		fonts/Roboto-Medium.ttf.zip
		fonts/Roboto-Regular.ttf.zip
		fonts/fa-solid-900.ttf.zip)
	if(ANDROID)
		cmrc_add_resources(flycast-resources
			WHENCE resources
			resources/picture/buttons.png)
	endif()
endif()

target_sources(${PROJECT_NAME} PRIVATE
		core/imgread/cdi.cpp
		core/imgread/chd.cpp
		core/imgread/common.cpp
		core/imgread/common.h
		core/imgread/cue.cpp
		core/imgread/gdi.cpp
		core/imgread/ImgReader.cpp
		core/imgread/ioctl.cpp
		core/imgread/iso9660.h
		core/imgread/isofs.cpp
		core/imgread/isofs.h)

if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
			core/input/gamepad.h
			core/input/gamepad_device.cpp
			core/input/gamepad_device.h
			core/input/keyboard_device.h
			core/input/mapping.cpp
			core/input/mapping.h
			core/input/mouse.cpp
			core/input/mouse.h)
endif()

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
		target_sources(${PROJECT_NAME} PRIVATE
				shell/switch/libnx_vmem.cpp
				shell/switch/stubs.c
				shell/switch/context_switch.S
				shell/switch/nswitch.h
				shell/switch/switch_gamepad.h
				shell/switch/switch_main.cpp
				shell/switch/ucontext.h)
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

target_sources(${PROJECT_NAME} PRIVATE
		core/log/BitSet.h
		core/log/Log.h
		core/log/StringUtil.h)
if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
			core/log/ConsoleListener.h
			core/log/ConsoleListenerDroid.cpp
			core/log/ConsoleListenerNix.cpp
			core/log/ConsoleListenerWin.cpp
			core/log/LogManager.cpp
			core/log/LogManager.h)
endif()

target_sources(${PROJECT_NAME} PRIVATE
		core/network/alienfnt_modem.cpp
		core/network/alienfnt_modem.h
		core/network/dns.cpp
		core/network/ggpo.cpp
		core/network/ggpo.h
		core/network/miniupnp.cpp
		core/network/miniupnp.h
		core/network/naomi_network.cpp
		core/network/naomi_network.h
		core/network/net_handshake.cpp
		core/network/net_handshake.h
		core/network/net_platform.h
		core/network/output.cpp
		core/network/output.h
		core/network/picoppp.cpp
		core/network/picoppp.h)

if(ANDROID)
	target_sources(${PROJECT_NAME} PRIVATE
			core/network/ifaddrs.c)
endif()

if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
			core/audio/audiobackend_alsa.cpp
			core/audio/audiobackend_coreaudio.cpp
			core/audio/audiobackend_directsound.cpp
			core/audio/audiobackend_libao.cpp
			core/audio/audiobackend_null.cpp
			core/audio/audiobackend_oboe.cpp
			core/audio/audiobackend_omx.cpp
			core/audio/audiobackend_oss.cpp
			core/audio/audiobackend_pulseaudio.cpp
			core/audio/audiobackend_sdl2.cpp
			core/audio/audiostream.cpp
			core/oslib/http_client.cpp
			core/oslib/http_client.h
			core/oslib/oslib.cpp)
endif()

target_sources(${PROJECT_NAME} PRIVATE
		core/audio/audiostream.h
		core/oslib/directory.h
		core/oslib/host_context.h
		core/oslib/oslib.h
		core/oslib/resources.cpp
		core/oslib/resources.h
		core/oslib/storage.cpp
		core/oslib/storage.h
		core/oslib/unwind_info.h
		core/oslib/virtmem.h
		core/lua/lua.cpp
		core/lua/lua.h)

if (ENABLE_DC_PROFILER)
	target_sources(${PROJECT_NAME} PRIVATE
		core/profiler/dc_profiler.cpp
		core/profiler/dc_profiler.h)

		target_compile_definitions(${PROJECT_NAME} PRIVATE DC_PROFILER)
endif()

if (ENABLE_FC_PROFILER)
	target_sources(${PROJECT_NAME} PRIVATE
		core/profiler/fc_profiler.cpp
		core/profiler/fc_profiler.h)

		target_compile_definitions(${PROJECT_NAME} PRIVATE FC_PROFILER)
endif()

target_sources(${PROJECT_NAME} PRIVATE
		core/reios/descrambl.cpp
		core/reios/descrambl.h
		core/reios/gdrom_hle.cpp
		core/reios/gdrom_hle.h
		core/reios/reios.cpp
		core/reios/reios.h
		core/reios/reios_elf.cpp
		core/reios/reios_elf.h)
cmrc_add_resources(flycast-resources fonts/biosfont.bin.zip)

target_sources(${PROJECT_NAME} PRIVATE
			core/achievements/achievements.cpp
			core/achievements/achievements.h)
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

target_sources(${PROJECT_NAME} PRIVATE
		core/wsi/context.h
		core/wsi/libretro.cpp
		core/wsi/libretro.h
		core/wsi/switcher.cpp)

if(USE_OPENGL)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OPENGL)
	target_sources(${PROJECT_NAME} PRIVATE
			core/wsi/egl.cpp
			core/wsi/egl.h
			core/wsi/gl_context.cpp
			core/wsi/gl_context.h
			core/wsi/osx.cpp
			core/wsi/osx.h
			core/wsi/sdl.cpp
			core/wsi/sdl.h)

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

	if(NOT (APPLE OR ANDROID OR USE_GLES OR USE_GLES2))
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
		core/debug/debug_agent.cpp
		core/debug/debug_agent.h
		core/debug/gdb_server.cpp
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
		core/rend/norend/norend.cpp)
if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
			core/ui/game_scanner.cpp
			core/ui/game_scanner.h
			core/ui/imgui_driver.h
			core/ui/gui.cpp
			core/ui/gui.h
			core/ui/gui_achievements.cpp
			core/ui/gui_android.cpp
			core/ui/gui_android.h
			core/ui/gui_chat.h
			core/ui/gui_cheats.cpp
			core/ui/gui_util.cpp
			core/ui/gui_util.h
			core/ui/debugger/gui_debugger.cpp
			core/ui/debugger/gui_debugger.h
			core/ui/debugger/gui_debugger_disasm.cpp
			core/ui/debugger/gui_debugger_disasm.h
			core/ui/mainui.cpp
			core/ui/mainui.h
			core/ui/boxart/boxart.cpp
			core/ui/boxart/boxart.h
			core/ui/boxart/gamesdb.cpp
			core/ui/boxart/gamesdb.h
			core/ui/boxart/scraper.cpp
			core/ui/boxart/scraper.h)
endif()

if(USE_VULKAN)
	target_compile_definitions(${PROJECT_NAME} PUBLIC VK_NO_PROTOTYPES)
	if(ANDROID)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
	elseif(X11_FOUND)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_XLIB_KHR)
	elseif(WIN32)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_WIN32_KHR)
	elseif(APPLE)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_MACOS_MVK)
		target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_METAL_EXT)
	endif()

	add_subdirectory(core/deps/Vulkan-Headers)
	target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers)

	add_subdirectory(core/deps/VulkanMemoryAllocator)
	target_compile_definitions(VulkanMemoryAllocator INTERFACE $<$<BOOL:${APPLE}>:VMA_USE_STL_SHARED_MUTEX=0>) # Only available on macOS 10.12+
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

		target_sources(${PROJECT_NAME} PRIVATE
			shell/android-studio/flycast/src/main/jni/src/Android.cpp
			shell/android-studio/flycast/src/main/jni/src/android_gamepad.h
			shell/android-studio/flycast/src/main/jni/src/android_keyboard.h)

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
					shell/apple/emulator-ios/emulator/EmulatorView.h
					shell/apple/emulator-ios/emulator/EmulatorView.mm
					shell/apple/emulator-ios/emulator/main.m
					shell/apple/emulator-ios/emulator/iCade-iOS/iCadeReaderView.h
					shell/apple/emulator-ios/emulator/iCade-iOS/iCadeReaderView.m
					shell/apple/emulator-ios/emulator/iCade-iOS/iCadeState.h)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -fmodules")
			set_source_files_properties(shell/apple/emulator-ios/emulator/FlycastViewController.mm PROPERTIES COMPILE_OPTIONS -fcxx-modules)
			set_source_files_properties(shell/apple/emulator-ios/emulator/iCade-iOS/iCadeReaderView.m PROPERTIES COMPILE_OPTIONS -fno-objc-arc)

			set(IOS_RESOURCES
				shell/apple/emulator-ios/emulator/Images.xcassets
				shell/apple/emulator-ios/emulator/FlycastStoryboard.storyboard
				shell/apple/emulator-ios/emulator/LaunchScreen.storyboard
				shell/apple/emulator-ios/emulator/PadViewController.xib)
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
			if(${CMAKE_GENERATOR} MATCHES "^Xcode.*")
				add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy ${SYPHON_METALLIB}
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Flycast.app/Contents/Resources/)
			else()
				add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy ${SYPHON_METALLIB}
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Flycast.app/Contents/Resources/)
			endif()
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

			if(${CMAKE_GENERATOR} MATCHES "^Xcode.*")
				add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy "$ENV{VULKAN_SDK}/lib/libMoltenVK.dylib"
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Flycast.app/Contents/Frameworks/libvulkan.dylib)
			else()
				add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy "$ENV{VULKAN_SDK}/lib/libMoltenVK.dylib"
					${CMAKE_CURRENT_BINARY_DIR}/Flycast.app/Contents/Frameworks/libvulkan.dylib)
			endif()
		endif()
	elseif(UNIX)
		if(NOT BUILD_TESTING)
			target_sources(${PROJECT_NAME} PRIVATE
					core/linux-dist/main.cpp)
		endif()
	elseif(WIN32)
		target_sources(${PROJECT_NAME} PRIVATE
			core/windows/clock.c
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
				core/deps/SDL/src/main/winrt/SDL_winrt_main_NonXAML.cpp
				shell/uwp/http_client.cpp)
			set_target_properties(${PROJECT_NAME} PROPERTIES RESOURCE "${ResourceFiles}")
		else()
			if("x86" IN_LIST ARCHITECTURE OR "x86_64" IN_LIST ARCHITECTURE)
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
			target_link_libraries(${PROJECT_NAME} PRIVATE dsound winmm ws2_32 wsock32 xinput9_1_0 cfgmgr32 wininet)
		endif()
	endif()
endif()

if(NOT LIBRETRO)
	set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
	set(CAPSTONE_SH_SUPPORT ON CACHE BOOL "" FORCE)
	add_subdirectory(core/deps/capstone EXCLUDE_FROM_ALL)
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/capstone/include)
	target_link_libraries(${PROJECT_NAME} PRIVATE capstone)
	if(APPLE)
		find_package(Patch)
		set(CAPSTONE_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/capstone.patch")
		add_custom_target(
			CAPSTONE_PATCH_TARGET
			COMMAND ${Patch_EXECUTABLE} -p1 --forward -i ${CAPSTONE_PATCH} -d ${CMAKE_CURRENT_SOURCE_DIR}/core/deps/capstone || true
  			COMMENT "Applying patch: ${CAPSTONE_PATCH}"
		)
		ADD_DEPENDENCIES(capstone CAPSTONE_PATCH_TARGET)
	endif()
endif()

if(BUILD_TESTING)
	target_sources(${PROJECT_NAME} PRIVATE
			core/deps/gtest/src/gtest-all.cc
			core/deps/gtest/src/gtest_main.cc)

	target_sources(${PROJECT_NAME} PRIVATE
			tests/src/CheatManagerTest.cpp
			tests/src/ConfigFileTest.cpp
			tests/src/div32_test.cpp
			tests/src/test_stubs.cpp
			tests/src/serialize_test.cpp
			tests/src/AicaArmTest.cpp
			tests/src/Sh4InterpreterTest.cpp
			tests/src/MmuTest.cpp)
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
