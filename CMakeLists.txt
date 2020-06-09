cmake_minimum_required(VERSION 3.10.2 FATAL_ERROR)

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

project(flycast)

if(ENABLE_CTEST)
    include(CTest)
endif()

if(MSVC)
    enable_language(ASM_MASM)
else()
    enable_language(ASM)
endif()

if(APPLE)
    set(CMAKE_Swift_LANGUAGE_VERSION 5.0)
    enable_language(Swift)
endif()

find_package(Git QUIET)
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

if(ANDROID)
    add_library(${PROJECT_NAME} SHARED core/nullDC.cpp)
else()
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE core/nullDC.cpp)
endif()

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11 cxx_std_11)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        -fno-strict-aliasing
        -ffast-math)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<BOOL:APPLE>:GL_SILENCE_DEPRECATION>
        $<$<BOOL:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
        $<$<BOOL:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<BOOL:MSVC>:_USE_MATH_DEFINES>
        $<$<BOOL:MSVC>:NOMINMAX>
        $<$<BOOL:${TEST_AUTOMATION}>:TEST_AUTOMATION>
        $<$<BOOL:${ENABLE_LOG}>:DEBUGFAST>
        ENABLE_MODEM)

target_include_directories(${PROJECT_NAME} PRIVATE core core/deps core/deps/stb core/deps/xbyak core/khronos)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
    if(ANDROID_NDK_MAJOR VERSION_GREATER_EQUAL 21)
        target_link_libraries(${PROJECT_NAME} PRIVATE -static-openmp)
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

add_subdirectory(core/deps/glslang)
target_link_libraries(${PROJECT_NAME} PRIVATE SPIRV)

find_package(ALSA QUIET)
if(ALSA_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ALSA)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ALSA_LIBRARIES})
endif()

if(MINGW)
    set(SDL2_FOUND TRUE)
    add_library(SDL2::SDL2-static STATIC IMPORTED)
    set_target_properties(SDL2::SDL2-static PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/core/deps/SDL2-2.0.12/x86_64-w64-mingw32/include"
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/core/deps/SDL2-2.0.12/x86_64-w64-mingw32/lib/libSDL2.a"
            INTERFACE_LINK_LIBRARIES "-L${CMAKE_CURRENT_SOURCE_DIR}/core/deps/SDL2-2.0.12/x86_64-w64-mingw32/lib -lmingw32 -lSDL2main -lSDL2 -mwindows  -Wl,--no-undefined -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -static-libgcc")
else()
    find_package(SDL2 QUIET)
endif()

if(SDL2_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SDL USE_SDL_AUDIO)
    target_sources(${PROJECT_NAME} PRIVATE core/sdl/sdl.cpp core/sdl/sdl.h core/sdl/sdl_gamepad.h core/sdl/sdl_keyboard.h)
    if(TARGET SDL2::SDL2)
        target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
    elseif(MINGW AND TARGET SDL2::SDL2-static)
        target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static)
    else()
        target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})
    endif()
endif()

find_package(ZLIB QUIET)
if(TARGET ZLIB::ZLIB)
    target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
else()
    set(SKIP_INSTALL_ALL ON)
    add_subdirectory(core/deps/zlib)
    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/core/deps/zlib" "${CMAKE_CURRENT_BINARY_DIR}/core/deps/zlib")
    target_link_libraries(${PROJECT_NAME} PRIVATE zlibstatic)
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_search_module(AO ao)
    if(AO_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LIBAO)
        target_include_directories(${PROJECT_NAME} PRIVATE ${AO_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${AO_LIBRARIES})
    endif()

    pkg_search_module(FLAC flac)
    if(FLAC_FOUND)
        target_include_directories(${PROJECT_NAME} PRIVATE ${FLAC_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${FLAC_LIBRARIES})
    endif()

    pkg_search_module(LIBEVDEV libevdev)
    if(LIBEVDEV_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_EVDEV)
        target_include_directories(${PROJECT_NAME} PRIVATE ${LIBEVDEV_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBEVDEV_LIBRARIES})

        pkg_search_module(LIBUDEV libudev)
        if(LIBUDEV_FOUND)
            target_compile_definitions(${PROJECT_NAME} PRIVATE USE_UDEV)
            target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUDEV_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUDEV_LIBRARIES})
        endif()
    endif()

    pkg_search_module(LIBPULSE libpulse)
    if(LIBPULSE_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PULSEAUDIO)
        target_include_directories(${PROJECT_NAME} PRIVATE ${LIBPULSE_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBPULSE_LIBRARIES})
    endif()

    pkg_search_module(LIBZIP libzip)
    if(LIBZIP_FOUND)
        target_include_directories(${PROJECT_NAME} PRIVATE ${LIBZIP_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBZIP_LIBRARIES})
    endif()

    if(ENABLE_OPROFILE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DYNA_OPROF)
        target_link_libraries(${PROJECT_NAME} PRIVATE opagent)
    endif()
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|x86_64.*|AMD64.*")
        set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE False)
        if(${CMAKE_VERSION} VERSION_LESS "3.14.0")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
        endif()
    endif()

    if(NOT SDL2_FOUND)
        find_package(X11 REQUIRED)
        if(X11_FOUND)
            target_compile_definitions(${PROJECT_NAME} PRIVATE SUPPORT_X11)
            target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
        endif()
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS} rt)
endif()

if(ASAN)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -static-libasan)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -static-libasan")
endif()

target_sources(${PROJECT_NAME} PRIVATE
        core/deps/chdpsr/cdipsr.cpp
        core/deps/chdpsr/cdipsr.h)

target_compile_definitions(${PROJECT_NAME} PRIVATE CHD5_FLAC CHD5_LZMA)
target_sources(${PROJECT_NAME} PRIVATE
        core/deps/chdr/bitstream.c
        core/deps/chdr/bitstream.h
        core/deps/chdr/cdrom.c
        core/deps/chdr/cdrom.h
        core/deps/chdr/chd.c
        core/deps/chdr/chd.h
        core/deps/chdr/coretypes.h
        core/deps/chdr/flac.c
        core/deps/chdr/flac.h
        core/deps/chdr/huffman.c
        core/deps/chdr/huffman.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/deps/coreio/coreio.cpp
        core/deps/coreio/coreio.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/deps/crypto/md5.cpp
        core/deps/crypto/md5.h
        core/deps/crypto/sha1.cpp
        core/deps/crypto/sha1.h
        core/deps/crypto/sha256.cpp
        core/deps/crypto/sha256.h)

if(NOT FLAC_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CONFIG_H $<$<BOOL:MINGW>:HAVE_FSEEKO>)
    target_include_directories(${PROJECT_NAME} PRIVATE core/deps/flac/include core/deps/flac/src/libFLAC/include)
    target_sources(${PROJECT_NAME} PRIVATE
            core/deps/flac/src/libFLAC/bitmath.c
            core/deps/flac/src/libFLAC/bitreader.c
            core/deps/flac/src/libFLAC/cpu.c
            core/deps/flac/src/libFLAC/crc.c
            core/deps/flac/src/libFLAC/fixed.c
            core/deps/flac/src/libFLAC/fixed_intrin_sse2.c
            core/deps/flac/src/libFLAC/fixed_intrin_ssse3.c
            core/deps/flac/src/libFLAC/float.c
            core/deps/flac/src/libFLAC/format.c
            core/deps/flac/src/libFLAC/lpc.c
            core/deps/flac/src/libFLAC/lpc_intrin_avx2.c
            core/deps/flac/src/libFLAC/lpc_intrin_sse.c
            core/deps/flac/src/libFLAC/lpc_intrin_sse2.c
            core/deps/flac/src/libFLAC/lpc_intrin_sse41.c
            core/deps/flac/src/libFLAC/md5.c
            core/deps/flac/src/libFLAC/memory.c
            core/deps/flac/src/libFLAC/metadata_iterators.c
            core/deps/flac/src/libFLAC/metadata_object.c
            core/deps/flac/src/libFLAC/stream_decoder.c
            core/deps/flac/src/libFLAC/window.c
            core/deps/flac/src/libFLAC/windows_unicode_filenames.c)
endif()

if(MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE core/deps/dirent)
else()
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
            core/deps/picotcp/modules/pico_dns_client.c
            core/deps/picotcp/modules/pico_dns_common.c
            core/deps/picotcp/modules/pico_ethernet.c
            core/deps/picotcp/modules/pico_fragments.c
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

    if(NOT LIBZIP_FOUND)
        target_include_directories(${PROJECT_NAME} PRIVATE core/deps/libzip)
        target_sources(${PROJECT_NAME} PRIVATE
                core/deps/libzip/config.h
                core/deps/libzip/mkstemp.c
                core/deps/libzip/zip.h
                core/deps/libzip/zip_add.c
                core/deps/libzip/zip_add_dir.c
                core/deps/libzip/zip_close.c
                core/deps/libzip/zip_delete.c
                core/deps/libzip/zip_dirent.c
                core/deps/libzip/zip_entry_free.c
                core/deps/libzip/zip_entry_new.c
                core/deps/libzip/zip_error.c
                core/deps/libzip/zip_error_clear.c
                core/deps/libzip/zip_error_get.c
                core/deps/libzip/zip_error_get_sys_type.c
                core/deps/libzip/zip_error_strerror.c
                core/deps/libzip/zip_error_to_str.c
                core/deps/libzip/zip_err_str.c
                core/deps/libzip/zip_fclose.c
                core/deps/libzip/zip_file_error_clear.c
                core/deps/libzip/zip_file_error_get.c
                core/deps/libzip/zip_file_get_offset.c
                core/deps/libzip/zip_filerange_crc.c
                core/deps/libzip/zip_file_strerror.c
                core/deps/libzip/zip_fopen.c
                core/deps/libzip/zip_fopen_index.c
                core/deps/libzip/zip_fread.c
                core/deps/libzip/zip_free.c
                core/deps/libzip/zip_get_archive_comment.c
                core/deps/libzip/zip_get_archive_flag.c
                core/deps/libzip/zip_get_file_comment.c
                core/deps/libzip/zip_get_name.c
                core/deps/libzip/zip_get_num_files.c
                core/deps/libzip/zip_memdup.c
                core/deps/libzip/zip_name_locate.c
                core/deps/libzip/zip_new.c
                core/deps/libzip/zip_open.c
                core/deps/libzip/zip_rename.c
                core/deps/libzip/zip_replace.c
                core/deps/libzip/zip_set_archive_comment.c
                core/deps/libzip/zip_set_archive_flag.c
                core/deps/libzip/zip_set_file_comment.c
                core/deps/libzip/zip_set_name.c
                core/deps/libzip/zip_source_buffer.c
                core/deps/libzip/zip_source_file.c
                core/deps/libzip/zip_source_filep.c
                core/deps/libzip/zip_source_free.c
                core/deps/libzip/zip_source_function.c
                core/deps/libzip/zip_source_zip.c
                core/deps/libzip/zip_stat.c
                core/deps/libzip/zip_stat_index.c
                core/deps/libzip/zip_stat_init.c
                core/deps/libzip/zip_strerror.c
                core/deps/libzip/zip_unchange_all.c
                core/deps/libzip/zip_unchange_archive.c
                core/deps/libzip/zip_unchange.c
                core/deps/libzip/zip_unchange_data.c
                core/deps/libzip/zipint.h)
    endif()
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE _7ZIP_ST)
target_sources(${PROJECT_NAME} PRIVATE core/deps/lzma/7zArcIn.c core/deps/lzma/7zBuf.c core/deps/lzma/7zCrc.c core/deps/lzma/7zCrcOpt.c core/deps/lzma/7zDec.c core/deps/lzma/7zFile.c core/deps/lzma/7zStream.c core/deps/lzma/Alloc.c core/deps/lzma/Bcj2.c core/deps/lzma/Bra86.c core/deps/lzma/Bra.c core/deps/lzma/BraIA64.c core/deps/lzma/CpuArch.c core/deps/lzma/Delta.c core/deps/lzma/LzFind.c core/deps/lzma/Lzma2Dec.c core/deps/lzma/Lzma86Dec.c core/deps/lzma/Lzma86Enc.c core/deps/lzma/LzmaDec.c core/deps/lzma/LzmaEnc.c core/deps/lzma/LzmaLib.c core/deps/lzma/Sort.c)
target_sources(${PROJECT_NAME} PRIVATE core/deps/libelf/elf32.cpp core/deps/libelf/elf64.cpp core/deps/libelf/elf.cpp)
target_sources(${PROJECT_NAME} PRIVATE core/deps/imgui/imgui.cpp core/deps/imgui/imgui_demo.cpp core/deps/imgui/imgui_draw.cpp core/deps/imgui/imgui_widgets.cpp)
target_sources(${PROJECT_NAME} PRIVATE core/deps/xbrz/xbrz.cpp)

target_sources(${PROJECT_NAME} PRIVATE
        core/archive/7zArchive.cpp
        core/archive/7zArchive.h
        core/archive/archive.cpp
        core/archive/archive.h)
if(NOT MSVC)
    target_sources(${PROJECT_NAME} PRIVATE
            core/archive/ZipArchive.cpp
            core/archive/ZipArchive.h)
endif()

target_sources(${PROJECT_NAME} PRIVATE
        core/arm_emitter/arm_coding.h
        core/arm_emitter/arm_disasm.h
        core/arm_emitter/arm_emitter.h
        core/arm_emitter/arm_registers.h
        core/arm_emitter/E_Branches.h
        core/arm_emitter/E_DataOp.h
        core/arm_emitter/E_Extend.h
        core/arm_emitter/E_LoadStore.h
        core/arm_emitter/E_Misc.h
        core/arm_emitter/E_Multiply.h
        core/arm_emitter/E_Parallel.h
        core/arm_emitter/E_Special.h
        core/arm_emitter/E_Status.h
        core/arm_emitter/E_VDataOp.h
        core/arm_emitter/E_VLoadStore.h
        core/arm_emitter/E_VRegXfer.h
        core/arm_emitter/H_Branches.h
        core/arm_emitter/H_fp.h
        core/arm_emitter/H_LoadStore.h
        core/arm_emitter/H_psuedo.h
        core/arm_emitter/H_state.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/cfg/cfg.cpp
        core/cfg/cfg.h
        core/cfg/cl.cpp
        core/cfg/ini.cpp
        core/cfg/ini.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/emitter/generated_class_names.h
        core/emitter/generated_class_names_string.h
        core/emitter/generated_descriptors.h
        core/emitter/generated_indexes.h
        core/emitter/x86_emitter.cpp
        core/emitter/x86_emitter.h
        core/emitter/x86_matcher.h
        core/emitter/x86_op_classes.h
        core/emitter/x86_op_encoder.h
        core/emitter/x86_op_table.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/hw/aica/aica.cpp
        core/hw/aica/aica.h
        core/hw/aica/aica_if.cpp
        core/hw/aica/aica_if.h
        core/hw/aica/aica_mem.cpp
        core/hw/aica/aica_mem.h
        core/hw/aica/dsp.cpp
        core/hw/aica/dsp.h
        core/hw/aica/dsp_arm64.cpp
        core/hw/aica/dsp_interp.cpp
        core/hw/aica/dsp_x64.cpp
        core/hw/aica/sgc_if.cpp
        core/hw/aica/sgc_if.h
        core/hw/arm7/arm64.cpp
        core/hw/arm7/arm7.cpp
        core/hw/arm7/arm7.h
        core/hw/arm7/arm_mem.cpp
        core/hw/arm7/arm_mem.h
        core/hw/arm7/arm-new.h
        core/hw/arm7/vbaARM.cpp
        core/hw/arm7/virt_arm.cpp
        core/hw/arm7/virt_arm.h
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
        core/hw/mem/vmem32.cpp
        core/hw/mem/vmem32.h
        core/hw/mem/_vmem.cpp
        core/hw/mem/_vmem.h
        core/hw/modem/dns.cpp
        core/hw/modem/modem.cpp
        core/hw/modem/modem.h
        core/hw/modem/modem_regs.h
        core/hw/modem/picoppp.cpp
        core/hw/modem/picoppp.h
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
        core/hw/sh4/dyna/rec_config.h
        core/hw/sh4/dyna/regalloc.h
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
        core/hw/sh4/modules/mmu_impl.h
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
        core/hw/sh4/sh4_opcode.h
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
        core/imgread/gd_driver.h
        core/imgread/gdi.cpp
        core/imgread/ImgReader.cpp
        core/imgread/ioctl.cpp
        core/imgread/SCSIDEFS.H)

target_sources(${PROJECT_NAME} PRIVATE
        core/input/gamepad.h
        core/input/gamepad_device.cpp
        core/input/gamepad_device.h
        core/input/keyboard_device.cpp
        core/input/keyboard_device.h
        core/input/mapping.cpp
        core/input/mapping.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/linux/nixprof/nixprof.cpp
        core/linux/common.cpp)

if(NOT WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
            core/linux/context.cpp
            core/linux/context.h
            core/linux/posix_vmem.cpp)
endif()

target_sources(${PROJECT_NAME} PRIVATE
        core/linux-dist/dispmanx.cpp
        core/linux-dist/dispmanx.h
        core/linux-dist/evdev.cpp
        core/linux-dist/evdev.h
        core/linux-dist/icon.h
        core/linux-dist/joystick.cpp
        core/linux-dist/joystick.h
        core/linux-dist/x11.cpp
        core/linux-dist/x11.h
        core/linux-dist/x11_keyboard.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/log/BitSet.h
        core/log/ConsoleListener.h
        core/log/ConsoleListenerDroid.cpp
        core/log/ConsoleListenerNix.cpp
        core/log/ConsoleListenerWin.cpp
        core/log/Log.h
        core/log/LogManager.cpp
        core/log/LogManager.h
        core/log/StringUtil.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/network/naomi_network.cpp
        core/network/naomi_network.h
        core/network/net_platform.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/oslib/audiobackend_alsa.cpp
        core/oslib/audiobackend_coreaudio.cpp
        core/oslib/audiobackend_directsound.cpp
        core/oslib/audiobackend_libao.cpp
        core/oslib/audiobackend_null.cpp
        core/oslib/audiobackend_omx.cpp
        core/oslib/audiobackend_oss.cpp
        core/oslib/audiobackend_pulseaudio.cpp
        core/oslib/audiobackend_sdl2.cpp
        core/oslib/audiostream.cpp
        core/oslib/audiostream.h
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
        core/wsi/gl4funcs.cpp
        core/wsi/gl_context.cpp
        core/wsi/gl_context.h
        core/wsi/osx.cpp
        core/wsi/osx.h
        core/wsi/sdl.cpp
        core/wsi/sdl.h
        core/wsi/switcher.cpp
        core/wsi/wgl.cpp
        core/wsi/wgl.h
        core/wsi/xgl.cpp
        core/wsi/xgl.h)

if(NOT APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
            core/wsi/gl32funcs.c
            core/wsi/gl32funcs.h)
endif()

target_sources(${PROJECT_NAME} PRIVATE
        core/build.h
        core/cheats.cpp
        core/cheats.h
        core/dispframe.cpp
        core/emulator.h
        core/serialize.cpp
        core/stdclass.cpp
        core/stdclass.h
        core/types.h)

target_sources(${PROJECT_NAME} PRIVATE
        core/rend/gles/glcache.h
        core/rend/gles/gldraw.cpp
        core/rend/gles/gles.cpp
        core/rend/gles/gles.h
        core/rend/gles/gltex.cpp
        core/rend/gles/imgui_impl_opengl3.cpp
        core/rend/gles/imgui_impl_opengl3.h
        core/rend/CustomTexture.cpp
        core/rend/CustomTexture.h
        core/rend/game_scanner.h
        core/rend/gui.cpp
        core/rend/gui.h
        core/rend/gui_android.cpp
        core/rend/gui_android.h
        core/rend/gui_util.cpp
        core/rend/gui_util.h
        core/rend/osd.cpp
        core/rend/osd.h
        core/rend/sorter.cpp
        core/rend/sorter.h
        core/rend/tileclip.h
        core/rend/TexCache.cpp
        core/rend/TexCache.h)

if(NOT APPLE AND NOT ANDROID)
    target_sources(${PROJECT_NAME} PRIVATE core/rend/gl4/abuffer.cpp core/rend/gl4/gl4.h core/rend/gl4/gldraw.cpp core/rend/gl4/gles.cpp core/rend/gl4/gltex.cpp)
endif()

if(NOT APPLE)
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

    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN)
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
            core/rend/vulkan/imgui_impl_vulkan.cpp
            core/rend/vulkan/imgui_impl_vulkan.h
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
            core/rend/vulkan/vmu.cpp
            core/rend/vulkan/vmu.h
            core/rend/vulkan/vulkan_context.cpp
            core/rend/vulkan/vulkan_context.h
            core/rend/vulkan/vulkan.h
            core/rend/vulkan/vulkan_renderer.cpp)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm.*|ARM.*)")
    target_sources(${PROJECT_NAME} PRIVATE core/rec-ARM/ngen_arm.S core/rec-ARM/rec_arm.cpp)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*)")
    target_include_directories(${PROJECT_NAME} PRIVATE core/deps/vixl)
    target_sources(${PROJECT_NAME} PRIVATE core/deps/vixl/aarch64/abi-aarch64.h core/deps/vixl/aarch64/assembler-aarch64.cc core/deps/vixl/aarch64/assembler-aarch64.h core/deps/vixl/aarch64/constants-aarch64.h core/deps/vixl/aarch64/cpu-aarch64.cc core/deps/vixl/aarch64/cpu-aarch64.h core/deps/vixl/aarch64/cpu-features-auditor-aarch64.cc core/deps/vixl/aarch64/cpu-features-auditor-aarch64.h core/deps/vixl/aarch64/decoder-aarch64.cc core/deps/vixl/aarch64/decoder-aarch64.h core/deps/vixl/aarch64/disasm-aarch64.cc core/deps/vixl/aarch64/disasm-aarch64.h core/deps/vixl/aarch64/instructions-aarch64.cc core/deps/vixl/aarch64/instructions-aarch64.h core/deps/vixl/aarch64/instrument-aarch64.cc core/deps/vixl/aarch64/instrument-aarch64.h core/deps/vixl/aarch64/logic-aarch64.cc core/deps/vixl/aarch64/macro-assembler-aarch64.cc core/deps/vixl/aarch64/macro-assembler-aarch64.h core/deps/vixl/aarch64/operands-aarch64.cc core/deps/vixl/aarch64/operands-aarch64.h core/deps/vixl/aarch64/pointer-auth-aarch64.cc core/deps/vixl/aarch64/simulator-aarch64.cc core/deps/vixl/aarch64/simulator-aarch64.h core/deps/vixl/aarch64/simulator-constants-aarch64.h core/deps/vixl/assembler-base-vixl.h core/deps/vixl/code-buffer-vixl.cc core/deps/vixl/code-buffer-vixl.h core/deps/vixl/code-generation-scopes-vixl.h core/deps/vixl/compiler-intrinsics-vixl.cc core/deps/vixl/compiler-intrinsics-vixl.h core/deps/vixl/cpu-features.cc core/deps/vixl/cpu-features.h core/deps/vixl/globals-vixl.h core/deps/vixl/invalset-vixl.h core/deps/vixl/macro-assembler-interface.h core/deps/vixl/platform-vixl.h core/deps/vixl/pool-manager.h core/deps/vixl/pool-manager-impl.h core/deps/vixl/utils-vixl.cc core/deps/vixl/utils-vixl.h)
    target_sources(${PROJECT_NAME} PRIVATE core/rec-ARM64/rec_arm64.cpp core/rec-ARM64/arm64_regalloc.h)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*|amd64.*|x86_64.*|AMD64.*")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_sources(${PROJECT_NAME} PRIVATE core/rec-x86/rec_x86_driver.cpp core/rec-x86/rec_x86_il.cpp core/rec-x86/rec_x86_asm.cpp core/rec-x86/rec_x86_ngen.h)
        if(UNIX AND NOT APPLE)
            target_sources(${PROJECT_NAME} PRIVATE core/rec-x86/rec_lin86_asm.S)
        endif()
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_sources(${PROJECT_NAME} PRIVATE core/rec-x64/rec_x64.cpp core/rec-x64/x64_regalloc.h)
        if(MSVC)
            target_sources(${PROJECT_NAME} PRIVATE core/rec-x64/msvc.asm)
        else()
            set_source_files_properties(core/rec-x64/rec_x64.cpp PROPERTIES COMPILE_FLAGS -fno-operator-names)
            set_source_files_properties(core/hw/aica/dsp_x64.cpp PROPERTIES COMPILE_FLAGS -fno-operator-names)
        endif()
    endif()
endif()

if(ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)

    target_sources(${PROJECT_NAME} PRIVATE
            shell/android-studio/reicast/src/main/jni/src/Android.cpp
            shell/android-studio/reicast/src/main/jni/src/utils.cpp)

    target_link_libraries(${PROJECT_NAME} PRIVATE android EGL GLESv2 log)
elseif(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
            shell/apple/emulator-osx/emulator-osx/AppDelegate.swift
            shell/apple/emulator-osx/emulator-osx/EmuGLView.swift
            shell/apple/emulator-osx/emulator-osx/osx-main.mm
            shell/apple/emulator-osx/emulator-osx/osx_gamepad.h
            shell/apple/emulator-osx/emulator-osx/osx_keyboard.h)

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_EXECUTABLE_NAME "Flycast"
        MACOSX_BUNDLE_INFO_STRING ""
        MACOSX_BUNDLE_ICON_FILE "AppIcon"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.reicast.Flycast"
        MACOSX_BUNDLE_LONG_VERSION_STRING ""
        MACOSX_BUNDLE_BUNDLE_NAME "Flycast"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2019 reicast contributors. All rights reserved."
        XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "shell/apple/emulator-osx/emulator-osx/emulator-osx-Bridging-Header.h"
    )

    find_library(AUDIO_UNIT_LIBRARY AudioUnit)
    find_library(FOUNDATION_LIBRARY Foundation)

    target_link_libraries(${PROJECT_NAME} PRIVATE ${AUDIO_UNIT_LIBRARY} ${FOUNDATION_LIBRARY})
elseif(UNIX)
    if(NOT BUILD_TESTING)
        target_sources(${PROJECT_NAME} PRIVATE
                core/linux-dist/main.cpp)
    endif()
elseif(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
            core/windows/win_keyboard.h
            core/windows/win_vmem.cpp
            core/windows/winmain.cpp
            core/windows/xinput_gamepad.h
            shell/windows/reicast.rc)

    target_link_libraries(${PROJECT_NAME} PRIVATE dsound opengl32 winmm ws2_32 wsock32 xinput9_1_0)
endif()

if(BUILD_TESTING)
    target_sources(${PROJECT_NAME} PRIVATE
            core/deps/gtest/src/gtest-all.cc
            core/deps/gtest/src/gtest_main.cc)

    target_sources(${PROJECT_NAME} PRIVATE
            tests/src/div32_test.cpp
            tests/src/test_stubs.cpp
            tests/src/serialize_test.cpp)
endif()
