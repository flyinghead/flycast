## devkitA64.cmake - devkitpro A64 cross-compile
#
set(CMAKE_SYSTEM_NAME Linux) # this one is important  // Add Platform/switch to use this name ...
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_SYSTEM_VERSION 1)   # this one not so much


set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITA64 $ENV{DEVKITA64}) 
  
  
if ("" STREQUAL "${DEVKITPRO}")
  set(DEVKITA64 "/opt/devkitpro")
endif()

if ("" STREQUAL "${DEVKITA64}")
  set(DEVKITA64 ${DEVKITPRO}/devkitA64)
endif()


## specify the cross compiler
#
set(CMAKE_C_COMPILER   ${DEVKITA64}/bin/aarch64-none-elf-gcc)
set(CMAKE_CXX_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-g++)


set(CMAKE_FIND_ROOT_PATH  ${DEVKITA64}) # where is the target environment

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) # search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)  # for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)




include_directories(${DEVKITPRO}/libnx/include)



set(TARGET_NSW ON)
