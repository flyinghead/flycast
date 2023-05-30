cmake_minimum_required(VERSION 3.10)

project(FlycastFonts)
add_executable(biosfont biosfont.cpp fontutil.cpp)
add_executable(printerfont printerfont.cpp fontutil.cpp)

target_compile_features(biosfont PRIVATE cxx_std_17)
target_compile_features(printerfont PRIVATE cxx_std_17)
set_target_properties(biosfont PROPERTIES CXX_EXTENSIONS OFF LINK_FLAGS_RELEASE -s)
set_target_properties(printerfont PROPERTIES CXX_EXTENSIONS OFF LINK_FLAGS_RELEASE -s)
target_compile_options(biosfont PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall>)
target_compile_options(printerfont PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall>)

find_package(PkgConfig REQUIRED)
pkg_check_modules(freetype2 IMPORTED_TARGET freetype2)
target_link_libraries(biosfont PRIVATE PkgConfig::freetype2)
target_link_libraries(printerfont PRIVATE PkgConfig::freetype2)
