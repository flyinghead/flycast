cmake_minimum_required(VERSION 3.10.2)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

option(ENABLE_CTEST "Enables unit tests" OFF)
option(ENABLE_OPROFILE "Enable OProfile" OFF)
option(TEST_AUTOMATION "Enable test automation" OFF)
option(ENABLE_LOG "Enable full logging" OFF)
option(ASAN "Enable address sanitizer" OFF)
option(USE_GLES "Use GLES[3] API" OFF)
option(USE_GLES2 "Use GLES2 API" OFF)
option(USE_HOST_LIBZIP "Use host libzip" ON)
option(USE_OPENMP "Use OpenMP if available" ON)
option(USE_VULKAN "Build with Vulkan support" ON)
option(LIBRETRO "Build libretro core" OFF)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shell/cmake")

if(APPLE)
	if(IOS)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS deployment version")
		set(CMAKE_OSX_ARCHITECTURES "arm64")
	else()
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum macOS deployment version")
		set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
	endif()
endif()

project(flycast)

if(ENABLE_CTEST)
    include(CTest)
endif()

if(APPLE AND NOT LIBRETRO)
    set(CMAKE_Swift_LANGUAGE_VERSION 5.0)
    enable_language(Swift OBJC OBJCXX)
endif()

find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

string(TIMESTAMP BUILD_TIMESTAMP UTC)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/core/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/core/version.h" @ONLY)

if(NINTENDO_SWITCH)
    set(USE_VULKAN OFF)
    enable_language(ASM)

    if(LIBRETRO)
        add_library(${PROJECT_NAME} STATIC core/emulator.cpp)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LIBRETRO HAVE_LIBNX HAVE_OPENGL HAVE_OIT)
		set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "flycast_libretro_libnx")
		set(CMAKE_STATIC_LIBRARY_PREFIX "")
    else()
        add_executable(${PROJECT_NAME} core/emulator.cpp)
        target_compile_definitions(${PROJECT_NAME} PRIVATE EGL_NO_PLATFORM_SPECIFIC_TYPES)
    endif()
	if(USE_GLES)
        target_compile_definitions(${PROJECT_NAME} PRIVATE GLES)
	endif()
 
elseif(LIBRETRO)
    add_library(${PROJECT_NAME} SHARED core/emulator.cpp)
    if(APPLE)
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,error")
	else()
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
	endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "flycast_libretro")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LIBRETRO)
    if(ANDROID OR USE_GLES)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3 HAVE_OPENGLES HAVE_OPENGLES3)
		target_link_libraries(${PROJECT_NAME} PRIVATE "-lGLESv3")
    elseif(USE_GLES2)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES2 HAVE_OPENGLES HAVE_OPENGLES2)
		target_link_libraries(${PROJECT_NAME} PRIVATE "-lGLESv2")
    else()
		target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENGL)
	    if(APPLE)
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-framework,OpenGL")
	    else()
			target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OIT)
		endif()
	    if(WIN32)
			target_link_libraries(${PROJECT_NAME} PRIVATE "-lopengl32")
		endif()
	endif()
elseif(ANDROID)
    add_library(${PROJECT_NAME} SHARED core/emulator.cpp)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-stack-protector)
    set(CMAKE_ANDROID_STL_TYPE "c++_static")
elseif(MSVC)
    add_executable(${PROJECT_NAME} WIN32 core/emulator.cpp)
else()
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE core/emulator.cpp)
endif()

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11 cxx_std_11)
set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_EXTENSIONS OFF
	LINK_FLAGS_RELEASE -s)
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /GR- /GS-)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE
		$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
		$<$<COMPILE_LANGUAGE:CXX>:-fno-strict-aliasing>
		$<$<COMPILE_LANGUAGE:CXX>:-Wall>)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<BOOL:APPLE>:GL_SILENCE_DEPRECATION>
        $<$<BOOL:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
        $<$<BOOL:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<BOOL:MSVC>:_USE_MATH_DEFINES>
        $<$<BOOL:MSVC>:NOMINMAX>
        $<$<BOOL:${TEST_AUTOMATION}>:TEST_AUTOMATION>
        $<$<BOOL:${ENABLE_LOG}>:DEBUGFAST>)
if(IOS)
	target_compile_definitions(${PROJECT_NAME} PRIVATE
		TARGET_IPHONE
		GLES
		GLES3
		GLES_SILENCE_DEPRECATION)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE core core/deps core/deps/stb core/khronos)
if(LIBRETRO)
	target_include_directories(${PROJECT_NAME} PRIVATE shell/libretro)
endif()
if(NINTENDO_SWITCH)
	target_include_directories(${PROJECT_NAME} PRIVATE shell/switch "${DEVKITPRO}/portlibs/switch/include")
endif()

if(NOT LIBRETRO)
	if(WIN32 AND NOT MSVC)
		target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--build-id")
		target_link_libraries(${PROJECT_NAME} PRIVATE breakpad_client)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_BREAKPAD)
#	elseif(APPLE)
#		add_subdirectory(core/deps/breakpad)
#		target_link_libraries(${PROJECT_NAME} PRIVATE breakpad_client)
#		target_include_directories(${PROJECT_NAME} PRIVATE
#			core/deps/breakpad/src/client/apple/Framework
#			core/deps/breakpad/src/client/mac/Framework)
##		target_include_directories(${PROJECT_NAME} PRIVATE $<INSTALL_INTERFACE:${CMAKE_INSTALL_LIBDIR}/Breakpad.framework>)
#		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_BREAKPAD)
	elseif(UNIX AND NOT CMAKE_SYSTEM_NAME MATCHES "(FreeBSD|OpenBSD|NetBSD)" AND NOT IOS)
		add_subdirectory(core/deps/breakpad)
		target_link_libraries(${PROJECT_NAME} PRIVATE breakpad_client)
		target_include_directories(${PROJECT_NAME} PRIVATE core/deps/breakpad/src)
		target_compile_definitions(${PROJECT_NAME} PRIVATE USE_BREAKPAD)
	endif()
endif()

find_package(OpenMP)
if(OpenMP_CXX_FOUND AND NOT APPLE AND USE_OPENMP)
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
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE TARGET_NO_OPENMP)
endif()

option(BUILD_SHARED_LIBS "Build shared library" OFF)
set(XXHASH_BUILD_XXHSUM OFF CACHE BOOL "Build the xxhsum binary")
add_subdirectory(core/deps/xxHash/cmake_unofficial)
target_link_libraries(${PROJECT_NAME} PRIVATE xxHash::xxhash)

option(BUILD_SHARED_LIBS "Build shared library" OFF)
add_subdirectory(core/deps/glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)

if(NOT APPLE AND USE_VULKAN)
	option(BUILD_EXTERNAL "Build external dependencies in /External" OFF)
	add_subdirectory(core/deps/glslang)
	target_link_libraries(${PROJECT_NAME} PRIVATE SPIRV)
endif()

if(NOT LIBRETRO)
	find_package(ALSA)
	if(ALSA_FOUND AND NOT ANDROID)
	    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ALSA)
	    target_include_directories(${PROJECT_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
	    target_link_libraries(${PROJECT_NAME} PRIVATE ${ALSA_LIBRARIES})
	endif()
	
	if(WIN32)
		# Build SDL
		file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/core/deps/SDL")
		if(CMAKE_GENERATOR MATCHES "Visual Studio")
			set(SDL_CMAKE_ARCH_ARG -A ${CMAKE_GENERATOR_PLATFORM})
		endif()
		execute_process(COMMAND ${CMAKE_COMMAND}
			"-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/core/deps/SDL"
			-DLIBC=ON
			-DBUILD_SHARED_LIBS=OFF
			-G "${CMAKE_GENERATOR}"
			${SDL_CMAKE_ARCH_ARG}
			"${CMAKE_CURRENT_SOURCE_DIR}/core/deps/SDL"
	    	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/core/deps/SDL")
		execute_process(COMMAND ${CMAKE_COMMAND} --build "${CMAKE_CURRENT_BINARY_DIR}/core/deps/SDL" --config Release --target install)
	endif()
	
	if(MINGW)
		target_link_libraries(${PROJECT_NAME} PRIVATE "-static-libgcc -static-libstdc++")
	endif()
	
	if((APPLE AND NOT IOS) OR WIN32)
	    find_package(SDL2 HINTS "${CMAKE_CURRENT_BINARY_DIR}/core/deps/SDL/cmake")
	elseif(NOT ANDROID AND NOT IOS)
	    find_package(SDL2)
	endif()
	if(SDL2_FOUND)
	    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SDL USE_SDL_AUDIO)
	    target_sources(${PROJECT_NAME} PRIVATE core/sdl/sdl.cpp core/sdl/sdl.h core/sdl/sdl_gamepad.h core/sdl/sdl_keyboard.h)
	    if(APPLE)
	        target_include_directories(${PROJECT_NAME} PRIVATE "${SDL2_INCLUDE_DIRS}/..")
	    elseif(NINTENDO_SWITCH)
	        target_include_directories(${PROJECT_NAME} PRIVATE "${DEVKITPRO}/portlibs/switch/include/SDL2")
	    else()
	        target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
	    endif()
	    if((APPLE OR WIN32) AND TARGET SDL2::SDL2-static)
	        target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static)
	    elseif(TARGET SDL2::SDL2)
	        target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
	    else()
	        target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})
	    endif()
	endif()
	
	find_package(ZLIB)
	if(TARGET ZLIB::ZLIB AND NOT ANDROID AND NOT WIN32)
	    set(WITH_SYSTEM_ZLIB ON CACHE BOOL "Use system provided zlib library")
	    target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
	endif()
endif()

execute_process(COMMAND git apply -p1 ${CMAKE_CURRENT_SOURCE_DIR}/core/deps/patches/libchdr.patch
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/core/deps/libchdr) 
if(IOS)
	set(CMAKE_SYSTEM_PROCESSOR aarch64)
endif()
add_subdirectory(core/deps/libchdr)
target_link_libraries(${PROJECT_NAME} PRIVATE chdr-static)
target_include_directories(${PROJECT_NAME} PRIVATE core/deps/libchdr/include)

if(NOT WITH_SYSTEM_ZLIB)
    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/core/deps/libchdr/deps/zlib-1.2.11")
    target_link_libraries(${PROJECT_NAME} PRIVATE zlib)
    # help libzip find the package
    set(ZLIB_FOUND TRUE)
    set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/deps/libchdr/deps/zlib-1.2.11")
    cmake_policy(SET CMP0026 OLD)
    get_target_property(ZLIB_LIBRARY zlib LOCATION)
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
    if (MINIUPNP_FOUND)
		target_include_directories(${PROJECT_NAME} PRIVATE ${MINIUPNP_INCLUDE_DIRS})
		target_link_libraries(${PROJECT_NAME} PRIVATE ${MINIUPNP_LIBRARIES})
    endif()
endif()

if(NINTENDO_SWITCH AND USE_GLES)
	pkg_check_modules(GLESV2 IMPORTED_TARGET glesv2)
	target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::GLESV2)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	if(USE_GLES2)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES2)
		target_link_libraries(${PROJECT_NAME} PRIVATE GLESv2)
	elseif(USE_GLES)
		target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)
		if(NOT SDL2_FOUND)
			target_link_libraries(${PROJECT_NAME} PRIVATE EGL)
		endif()
	else()
	    set(OpenGL_GL_PREFERENCE LEGACY)
	    find_package(OpenGL REQUIRED)
	    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
	endif()

    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|x86_64.*|AMD64.*" AND NOT LIBRETRO)
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
	else()
		target_compile_definitions(${PROJECT_NAME} PRIVATE EGL_NO_X11)
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS} rt)
endif()

if(ASAN)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -static-libasan)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -static-libasan")
endif()

if(ANDROID AND NOT LIBRETRO)
	find_package(oboe CONFIG)
	if(OBOE_FOUND)
		target_link_libraries(${PROJECT_NAME} PRIVATE oboe::oboe)
	else()
		include("core/deps/oboe/gamesdk.cmake")
		add_gamesdk_target(PACKAGE_DIR core/deps/oboe ANDROID_API_LEVEL 28 ANDROID_NDK_VERSION 21.0.0)
		target_link_libraries(${PROJECT_NAME} PRIVATE oboe OpenSLES) 
	endif()
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OBOE)
endif()

target_sources(${PROJECT_NAME} PRIVATE
        core/deps/chdpsr/cdipsr.cpp
        core/deps/chdpsr/cdipsr.h)

target_include_directories(${PROJECT_NAME} PRIVATE core/deps/nowide/include)

if(NOT MINIUPNP_FOUND)
	if(NINTENDO_SWITCH)
		target_compile_definitions(${PROJECT_NAME} PRIVATE FEAT_NO_MINIUPNPC)
	else()
		option(UPNPC_BUILD_SHARED "Build shared library" OFF)
		option(UPNPC_BUILD_TESTS "Build test executables" OFF)
		option(UPNPC_BUILD_SAMPLE "Build sample executables" OFF)
		option(UPNPC_NO_INSTALL "Disable installation" ON)
		add_subdirectory(core/deps/miniupnpc)
		target_include_directories(${PROJECT_NAME} PRIVATE core/deps/miniupnpc)
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
	target_include_directories(${PROJECT_NAME} PRIVATE core/deps/libzip/lib)
	target_link_libraries(${PROJECT_NAME} PRIVATE zip)
endif()

if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE core/deps/dirent)
endif()

if(NOT MSVC)
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
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE _7ZIP_ST)
target_sources(${PROJECT_NAME} PRIVATE core/deps/lzma/7zArcIn.c core/deps/lzma/7zBuf.c core/deps/lzma/7zCrc.c core/deps/lzma/7zCrcOpt.c core/deps/lzma/7zDec.c core/deps/lzma/7zFile.c core/deps/lzma/7zStream.c core/deps/lzma/Alloc.c core/deps/lzma/Bcj2.c core/deps/lzma/Bra86.c core/deps/lzma/Bra.c core/deps/lzma/BraIA64.c core/deps/lzma/CpuArch.c core/deps/lzma/Delta.c core/deps/lzma/LzFind.c core/deps/lzma/Lzma2Dec.c core/deps/lzma/Lzma86Dec.c core/deps/lzma/Lzma86Enc.c core/deps/lzma/LzmaDec.c core/deps/lzma/LzmaEnc.c core/deps/lzma/LzmaLib.c core/deps/lzma/Sort.c)
target_sources(${PROJECT_NAME} PRIVATE core/deps/libelf/elf32.cpp core/deps/libelf/elf64.cpp core/deps/libelf/elf.cpp)
if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE core/deps/imgui/imgui.cpp core/deps/imgui/imgui_demo.cpp core/deps/imgui/imgui_draw.cpp core/deps/imgui/imgui_widgets.cpp)
endif()
target_sources(${PROJECT_NAME} PRIVATE core/deps/xbrz/xbrz.cpp)

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
		core/deps/libretro-common/string/stdstring.c
		core/deps/libretro-common/glsm/glsm.c
		core/deps/libretro-common/glsym/rglgen.c)
	if(ANDROID OR USE_GLES)
		target_sources(${PROJECT_NAME} PRIVATE core/deps/libretro-common/glsym/glsym_es3.c) 
	elseif(USE_GLES2)
		target_sources(${PROJECT_NAME} PRIVATE core/deps/libretro-common/glsym/glsym_es2.c) 
	else()
		target_sources(${PROJECT_NAME} PRIVATE core/deps/libretro-common/glsym/glsym_gl.c) 
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
        core/hw/arm7/vbaARM.cpp
        core/hw/bba/bba.h
        core/hw/bba/bba.cpp
        core/hw/bba/rtl8139c.h
        core/hw/bba/rtl8139c.cpp
        core/hw/flashrom/flashrom.cpp
        core/hw/flashrom/flashrom.h
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
        core/hw/mem/_vmem.cpp
        core/hw/mem/_vmem.h
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
        core/hw/pvr/drkPvr.cpp
        core/hw/pvr/helper_classes.h
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
        core/hw/pvr/ta_vtx.cpp
        core/hw/sh4/dyna
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
        core/hw/sh4/interpr
        core/hw/sh4/interpr/sh4_fpu.cpp
        core/hw/sh4/interpr/sh4_interpreter.cpp
        core/hw/sh4/interpr/sh4_opcodes.cpp
        core/hw/sh4/interpr/sh4_opcodes.h
        core/hw/sh4/modules
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
        core/hw/sh4/sh4_sched.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/imgread/cdi.cpp
        core/imgread/chd.cpp
        core/imgread/common.cpp
        core/imgread/common.h
        core/imgread/cue.cpp
        core/imgread/gdi.cpp
        core/imgread/ImgReader.cpp
        core/imgread/ioctl.cpp)

if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
	        core/input/gamepad.h
	        core/input/gamepad_device.cpp
	        core/input/gamepad_device.h
	        core/input/keyboard_device.h
	        core/input/mapping.cpp
	        core/input/mapping.h)
endif()

if(WIN32)
	target_sources(${PROJECT_NAME} PRIVATE
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
        		core/linux/libnx_vmem.cpp
        		shell/switch/stubs.c
        		shell/switch/context_switch.S)
    endif()
endif()

if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
	        core/linux-dist/dispmanx.cpp
	        core/linux-dist/dispmanx.h
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
        core/network/picoppp.cpp
        core/network/picoppp.h)

if(ANDROID)
    target_sources(${PROJECT_NAME} PRIVATE
            core/network/ifaddrs.c)
endif()

if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
	        core/oslib/audiobackend_alsa.cpp
	        core/oslib/audiobackend_coreaudio.cpp
	        core/oslib/audiobackend_directsound.cpp
	        core/oslib/audiobackend_libao.cpp
	        core/oslib/audiobackend_null.cpp
	        core/oslib/audiobackend_oboe.cpp
	        core/oslib/audiobackend_omx.cpp
	        core/oslib/audiobackend_oss.cpp
	        core/oslib/audiobackend_pulseaudio.cpp
	        core/oslib/audiobackend_sdl2.cpp
	        core/oslib/audiostream.cpp
	        core/oslib/oslib.cpp)
endif()

target_sources(${PROJECT_NAME} PRIVATE
        core/oslib/audiostream.h
        core/oslib/directory.h
        core/oslib/host_context.h
        core/oslib/oslib.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/profiler/profiler.cpp
        core/profiler/profiler.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/rec-cpp/rec_cpp.cpp)

target_sources(${PROJECT_NAME} PRIVATE
        core/reios/descrambl.cpp
        core/reios/descrambl.h
        core/reios/font.h
        core/reios/gdrom_hle.cpp
        core/reios/gdrom_hle.h
        core/reios/iso9660.h
        core/reios/reios.cpp
        core/reios/reios.h
        core/reios/reios_elf.cpp
        core/reios/reios_elf.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/wsi/context.h
        core/wsi/egl.cpp
        core/wsi/egl.h
        core/wsi/gl_context.cpp
        core/wsi/gl_context.h
        core/wsi/libretro.cpp
        core/wsi/libretro.h
        core/wsi/osx.cpp
        core/wsi/osx.h
        core/wsi/sdl.cpp
        core/wsi/sdl.h
        core/wsi/switcher.cpp
        core/wsi/wgl.cpp
        core/wsi/wgl.h
        core/wsi/xgl.cpp
        core/wsi/xgl.h)

if(NOT APPLE AND NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            core/wsi/gl32funcs.c
            core/wsi/gl32funcs.h)
endif()
if(NOT (NINTENDO_SWITCH AND LIBRETRO))
	target_sources(${PROJECT_NAME} PRIVATE core/wsi/gl4funcs.cpp)
endif()

target_sources(${PROJECT_NAME} PRIVATE
        core/build.h
        core/cheats.cpp
        core/cheats.h
        core/dispframe.cpp
        core/emulator.h
        core/nullDC.cpp
        core/serialize.cpp
        core/stdclass.cpp
        core/stdclass.h
        core/types.h
        core/debug/gdb_server.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/rend/gles/glcache.h
        core/rend/gles/gldraw.cpp
        core/rend/gles/gles.cpp
        core/rend/gles/gles.h
        core/rend/gles/gltex.cpp
        core/rend/gles/quad.cpp
        core/rend/gles/postprocess.cpp
        core/rend/gles/postprocess.h
        core/rend/CustomTexture.cpp
        core/rend/CustomTexture.h
		core/rend/osd.cpp
		core/rend/osd.h
        core/rend/sorter.cpp
        core/rend/sorter.h
        core/rend/tileclip.h
        core/rend/TexCache.cpp
        core/rend/TexCache.h)
if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
	        core/rend/gles/imgui_impl_opengl3.cpp
	        core/rend/gles/imgui_impl_opengl3.h
	        core/rend/game_scanner.h
	        core/rend/gui.cpp
	        core/rend/gui.h
	        core/rend/gui_android.cpp
	        core/rend/gui_android.h
	        core/rend/gui_cheats.cpp
	        core/rend/gui_util.cpp
	        core/rend/gui_util.h
	        core/rend/mainui.cpp)
endif()

if(NOT (APPLE OR ANDROID OR USE_GLES OR USE_GLES2))
    target_sources(${PROJECT_NAME} PRIVATE
    	core/rend/gl4/abuffer.cpp
    	core/rend/gl4/gl4.h
    	core/rend/gl4/gldraw.cpp
    	core/rend/gl4/gles.cpp)
endif()

if(USE_VULKAN AND NOT APPLE)
	if(NOT LIBRETRO)
	    if(ANDROID)
	        set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_ANDROID_KHR)
	    elseif(X11_FOUND)
	        set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
	    elseif(WIN32)
	        set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
	    endif()
	
	    set(VOLK_PULL_IN_VULKAN OFF)
	    add_subdirectory(core/deps/volk)
	    target_include_directories(volk PRIVATE core/khronos)
	    target_link_libraries(${PROJECT_NAME} PRIVATE volk)
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
            core/rend/vulkan/buffer.cpp
            core/rend/vulkan/buffer.h
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
            core/rend/vulkan/vk_mem_alloc.h
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
	            core/rend/vulkan/vk_context_lr.h
	            core/deps/libretro-common/vulkan/vulkan_symbol_wrapper.c)
	else()
	    target_sources(${PROJECT_NAME} PRIVATE
	            core/rend/vulkan/vulkan_context.cpp
	            core/rend/vulkan/imgui_impl_vulkan.cpp
	            core/rend/vulkan/imgui_impl_vulkan.h)
	endif()
endif()

if(WIN32 AND NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
		core/rend/dx9/comptr.h
		core/rend/dx9/d3d_overlay.h
		core/rend/dx9/d3d_overlay.cpp
		core/rend/dx9/d3d_renderer.h
		core/rend/dx9/d3d_renderer.cpp
		core/rend/dx9/d3d_shaders.h
		core/rend/dx9/d3d_shaders.cpp
		core/rend/dx9/d3d_texture.h
		core/rend/dx9/d3d_texture.cpp
		core/rend/dx9/dxcontext.h
		core/rend/dx9/dxcontext.cpp
		core/rend/dx9/imgui_impl_dx9.h
		core/rend/dx9/imgui_impl_dx9.cpp)
	if(NOT MINGW)
		target_include_directories(${PROJECT_NAME} PRIVATE "$ENV{DXSDK_DIR}/Include")
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			target_link_directories(${PROJECT_NAME} PRIVATE "$ENV{DXSDK_DIR}/Lib/x64")
		else()
			target_link_directories(${PROJECT_NAME} PRIVATE "$ENV{DXSDK_DIR}/Lib/x86")
		endif()
	endif()
	target_link_libraries(${PROJECT_NAME} PRIVATE d3d9 d3dx9)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm.*|ARM.*)" AND NOT APPLE)
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
		core/deps/vixl/aarch32/disasm-aarch32.cc
		core/deps/vixl/aarch32/disasm-aarch32.h
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
     set(KNOWN_ARCHITECTURE_DETECTED ON)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*|arm64.*)" OR CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    target_include_directories(${PROJECT_NAME} PRIVATE core/deps/vixl)
    target_sources(${PROJECT_NAME} PRIVATE
    	core/deps/vixl/aarch64/abi-aarch64.h
    	core/deps/vixl/aarch64/assembler-aarch64.cc
    	core/deps/vixl/aarch64/assembler-aarch64.h
    	core/deps/vixl/aarch64/constants-aarch64.h
    	core/deps/vixl/aarch64/cpu-aarch64.cc
    	core/deps/vixl/aarch64/cpu-aarch64.h
    	core/deps/vixl/aarch64/cpu-features-auditor-aarch64.cc
    	core/deps/vixl/aarch64/cpu-features-auditor-aarch64.h
    	core/deps/vixl/aarch64/decoder-aarch64.cc
    	core/deps/vixl/aarch64/decoder-aarch64.h
    	core/deps/vixl/aarch64/disasm-aarch64.cc
    	core/deps/vixl/aarch64/disasm-aarch64.h
    	core/deps/vixl/aarch64/instructions-aarch64.cc
    	core/deps/vixl/aarch64/instructions-aarch64.h
    	core/deps/vixl/aarch64/instrument-aarch64.cc
    	core/deps/vixl/aarch64/instrument-aarch64.h
    	core/deps/vixl/aarch64/logic-aarch64.cc
    	core/deps/vixl/aarch64/macro-assembler-aarch64.cc
    	core/deps/vixl/aarch64/macro-assembler-aarch64.h
    	core/deps/vixl/aarch64/operands-aarch64.cc
    	core/deps/vixl/aarch64/operands-aarch64.h
    	core/deps/vixl/aarch64/pointer-auth-aarch64.cc
    	core/deps/vixl/aarch64/simulator-aarch64.cc
    	core/deps/vixl/aarch64/simulator-aarch64.h
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
    set(KNOWN_ARCHITECTURE_DETECTED ON)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*|amd64.*|x86_64.*|AMD64.*" OR CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
    add_subdirectory(core/deps/xbyak)
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
    set(KNOWN_ARCHITECTURE_DETECTED ON)
endif()
if(NOT KNOWN_ARCHITECTURE_DETECTED)
    message(FATAL_ERROR "Unknown target processor: ${CMAKE_SYSTEM_PROCESSOR}")
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
		core/deps/ggpo/lib/ggpo/log.cpp
		core/deps/ggpo/lib/ggpo/log.h
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
	
	    target_sources(${PROJECT_NAME} PRIVATE shell/android-studio/flycast/src/main/jni/src/Android.cpp)
	
	    target_link_libraries(${PROJECT_NAME} PRIVATE android EGL GLESv2 log)
	elseif(APPLE)
		set_property(TARGET ${PROJECT_NAME} PROPERTY XCODE_ATTRIBUTE_SWIFT_VERSION "5.0")
		if(IOS)
			add_subdirectory(shell/apple/emulator-ios/AltKit)
			target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/shell/apple/emulator-ios")
			target_link_libraries(${PROJECT_NAME} PRIVATE AltKit)

			target_sources(${PROJECT_NAME} PRIVATE
					shell/apple/emulator-ios/emulator/AppDelegate.h
					shell/apple/emulator-ios/emulator/AppDelegate.mm
					shell/apple/emulator-ios/emulator/ios_main.mm
					shell/apple/emulator-ios/emulator/ios_gamepad.h
					shell/apple/emulator-ios/emulator/ios_keyboard.h
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

			set_target_properties(${PROJECT_NAME} PROPERTIES
				OUTPUT_NAME "Flycast"
				MACOSX_BUNDLE YES
				MACOSX_BUNDLE_EXECUTABLE_NAME "Flycast"
				MACOSX_BUNDLE_INFO_STRING ""
				MACOSX_BUNDLE_GUI_IDENTIFIER "com.flyinghead.Flycast"
				MACOSX_BUNDLE_BUNDLE_NAME "com.flyinghead.Flycast"
				MACOSX_BUNDLE_LONG_VERSION_STRING "${GIT_VERSION}"
				MACOSX_BUNDLE_SHORT_VERSION_STRING "${GIT_VERSION}"
				MACOSX_BUNDLE_BUNDLE_VERSION "1"
				MACOSX_BUNDLE_COPYRIGHT "Copyright © 2021 Flycast contributors. All rights reserved."
				MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/emulator-ios/plist.in

				XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
				XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/emulator-ios/emulator/flycast-ios-Prefix.pch"
				RESOURCE "${IOS_RESOURCES}"
				XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES"
				XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "12.0"
				XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
				XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
				XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
				XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
				XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES NO
				XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
				
			find_library(UIKIT UIKit)
			find_library(FOUNDATION Foundation)
			find_library(OPENGLES OpenGLES)
			find_library(GLKIT GLKit)
			find_library(GAMECONTROLLER GameController)
			find_library(AUDIOTOOLBOX AudioToolbox)
			find_library(AVFOUNDATION AVFoundation)
			target_link_libraries(${PROJECT_NAME} PRIVATE
				${UIKIT}
				${FOUNDATION}
				${OPENGLES}
				${GLKIT}
				${GAMECONTROLLER}
				${AUDIOTOOLBOX}
				${AVFOUNDATION})

			add_custom_target(Flycast.IPA ALL
				DEPENDS ${PROJECT_NAME}
				COMMENT "Building IPA"
				COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/Payload
				COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/Payload/*
				COMMAND cp -r ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/Flycast.app ${CMAKE_CURRENT_BINARY_DIR}/Payload
				COMMAND for lib in ${CMAKE_CURRENT_BINARY_DIR}/Payload/Flycast.app/Frameworks/*.dylib
				COMMAND do xcrun bitcode_strip -r $lib -o $lib
				COMMAND done
				COMMAND ldid -S${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/emulator-ios/emulator/flycast.entitlements ${CMAKE_CURRENT_BINARY_DIR}/Payload/Flycast.app/Flycast
				COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/Flycast.ipa" --format=zip ${CMAKE_CURRENT_BINARY_DIR}/Payload
				COMMAND rm -rf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/Flycast.app ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/Flycast.swiftmodule)
		else()
			target_sources(${PROJECT_NAME} PRIVATE
					shell/apple/emulator-osx/emulator-osx/AppDelegate.swift
					shell/apple/emulator-osx/emulator-osx/EmuGLView.swift
					shell/apple/emulator-osx/emulator-osx/osx-main.mm
					shell/apple/emulator-osx/emulator-osx/osx_gamepad.h
					shell/apple/emulator-osx/emulator-osx/osx_keyboard.h)
		
			set(XIB shell/apple/emulator-osx/emulator-osx/Base.lproj/MainMenu.xib)
			set(ASSETS shell/apple/emulator-osx/emulator-osx/Images.xcassets)
			target_sources(${PROJECT_NAME} PRIVATE ${XIB} ${ASSETS})
			source_group("Resources" FILES ${XIB} ${ASSETS})
			set_source_files_properties(${ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
		
			set_target_properties(${PROJECT_NAME} PROPERTIES
				OUTPUT_NAME "Flycast"
				MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/shell/cmake/MacOSXBundleInfo.plist.in
				MACOSX_BUNDLE_EXECUTABLE_NAME "Flycast"
				MACOSX_BUNDLE_INFO_STRING ""
				MACOSX_BUNDLE_ICON_FILE "AppIcon"
				MACOSX_BUNDLE_GUI_IDENTIFIER "com.flyinghead.Flycast"
				MACOSX_BUNDLE_LONG_VERSION_STRING "${GIT_VERSION}"
				MACOSX_BUNDLE_SHORT_VERSION_STRING "${GIT_VERSION}"
				MACOSX_BUNDLE_BUNDLE_VERSION "1"
				MACOSX_BUNDLE_BUNDLE_NAME "Flycast"
				MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
				MACOSX_BUNDLE_BUNDLE_VERSION "1"
				MACOSX_BUNDLE_COPYRIGHT "Copyright © 2019 Flycast contributors. All rights reserved."
				XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "shell/apple/emulator-osx/emulator-osx/emulator-osx-Bridging-Header.h"
				RESOURCE "${XIB}"
				XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
				BUILD_WITH_INSTALL_RPATH TRUE
				INSTALL_RPATH "@loader_path/../Frameworks"
			)
		
			find_library(AUDIO_UNIT_LIBRARY AudioUnit)
			find_library(FOUNDATION_LIBRARY Foundation)
			find_library(AUDIO_TOOLBOX_LIBRARY AudioToolbox)
		
			target_link_libraries(${PROJECT_NAME} PRIVATE ${AUDIO_UNIT_LIBRARY} ${FOUNDATION_LIBRARY} ${AUDIO_TOOLBOX_LIBRARY})
		endif()
	elseif(UNIX OR NINTENDO_SWITCH)
	    if(NOT BUILD_TESTING)
	        target_sources(${PROJECT_NAME} PRIVATE
	                core/linux-dist/main.cpp)
	    endif()
	elseif(WIN32)
	    if(NOT BUILD_TESTING)
	        target_sources(${PROJECT_NAME} PRIVATE
	        	core/windows/rawinput.cpp
	        	core/windows/rawinput.h
	            core/windows/win_keyboard.h
	            core/windows/winmain.cpp
	            core/windows/xinput_gamepad.h)
	    endif()
	    target_sources(${PROJECT_NAME} PRIVATE shell/windows/flycast.rc)

	    target_link_libraries(${PROJECT_NAME} PRIVATE dsound opengl32 winmm ws2_32 wsock32 xinput9_1_0)
	endif()
endif()

if(BUILD_TESTING)
    target_sources(${PROJECT_NAME} PRIVATE
            core/deps/gtest/src/gtest-all.cc
            core/deps/gtest/src/gtest_main.cc)

    target_sources(${PROJECT_NAME} PRIVATE
            tests/src/div32_test.cpp
            tests/src/test_stubs.cpp
            tests/src/serialize_test.cpp
            tests/src/AicaArmTest.cpp
            tests/src/Sh4InterpreterTest.cpp)
endif()

if(NINTENDO_SWITCH)
	if(LIBRETRO)
		add_custom_target(combined ALL
	    	COMMAND ${CMAKE_AR} -x $<TARGET_FILE:xxHash::xxhash>
	    	COMMAND ${CMAKE_AR} -x $<TARGET_FILE:chdr-static>
	    	COMMAND ${CMAKE_AR} -x $<TARGET_FILE:zip>
	    	COMMAND ${CMAKE_AR} -rs flycast_libretro_libnx.a *.obj
	    	COMMAND rm *.obj
	    	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
	    	DEPENDS xxHash::xxhash chdr-static zip ${PROJECT_NAME})
	else()
		nx_generate_nacp(flycast.nacp NAME "Flycast" AUTHOR "flyinghead, M4xw" VERSION "${GIT_VERSION}")
		nx_create_nro(flycast NACP flycast.nacp ICON "${CMAKE_SOURCE_DIR}/shell/switch/flycast.jpeg")
	endif()
endif()
