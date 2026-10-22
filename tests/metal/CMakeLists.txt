cmake_minimum_required(VERSION 3.24)
project(FlycastMetalOffscreenTests LANGUAGES C CXX OBJCXX)
if(NOT APPLE)
    message(FATAL_ERROR "The native offscreen renderer tests require macOS with Metal 4")
endif()
get_filename_component(flycast_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_package(Freetype REQUIRED)
add_library(flycast_basis_transcoder STATIC EXCLUDE_FROM_ALL
    "${flycast_root}/core/deps/basis_universal/transcoder/basisu_transcoder.cpp"
    "${flycast_root}/core/deps/basis_universal/zstd/zstddeclib.c")
target_compile_features(flycast_basis_transcoder PRIVATE cxx_std_17)
target_compile_definitions(flycast_basis_transcoder PRIVATE
    BASISD_SUPPORT_KTX2=1 BASISD_SUPPORT_KTX2_ZSTD=1 BASISD_SUPPORT_DXT1=1
    BASISD_SUPPORT_DXT5A=1 BASISD_SUPPORT_ETC2_EAC_A8=1 BASISD_SUPPORT_XUASTC=1)
target_compile_options(flycast_basis_transcoder PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-strict-aliasing>")
target_include_directories(flycast_basis_transcoder PUBLIC "${flycast_root}/core/deps")
set_target_properties(flycast_basis_transcoder PROPERTIES OSX_ARCHITECTURES "arm64" OSX_DEPLOYMENT_TARGET "26.0")
add_library(flycast_offscreen_xxhash STATIC EXCLUDE_FROM_ALL "${flycast_root}/core/deps/xxHash/xxhash.c")
target_include_directories(flycast_offscreen_xxhash PUBLIC "${flycast_root}/core/deps/xxHash")
set_target_properties(flycast_offscreen_xxhash PROPERTIES OSX_ARCHITECTURES "arm64" OSX_DEPLOYMENT_TARGET "26.0")
include("${flycast_root}/core/rend/metal4/metal4_offscreen_tests.cmake")
add_flycast_metal_offscreen_test(flycast_metal_offscreen_renderer_tests "${flycast_root}" Freetype::Freetype flycast_offscreen_xxhash)
