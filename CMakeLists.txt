cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(TNAME reicast)

project(${TNAME})

enable_language(ASM)
enable_language(ASM_MASM)

set(DEBUG_CMAKE ON)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)


set(reicast_root_path  "${CMAKE_CURRENT_SOURCE_DIR}")
set(reicast_core_path  "${reicast_root_path}/core")
set(reicast_shell_path "${reicast_root_path}/shell")

list(APPEND CMAKE_MODULE_PATH "${reicast_shell_path}/cmake")


include(GetGitRevisionDescription)
git_describe(GIT_VERSION --tags)
configure_file(${reicast_core_path}/version.h.in ${reicast_core_path}/version.h @ONLY)




## reicast build modules #
#

set(reicast_SRCS "")

include(config)			# configure build settings, must be first



### libdreamcast.cmake #########################################################################

set(d_core ${reicast_core_path})

file(GLOB_RECURSE hw_SRCS	${d_core}/hw/*.cpp			${d_core}/hw/*.h)

file(GLOB cfg_SRCS			${d_core}/cfg/*.cpp			${d_core}/cfg/*.h)
file(GLOB rend_SRCS			${d_core}/rend/*.cpp		${d_core}/rend/*.h)
file(GLOB input_SRCS		${d_core}/input/*.cpp		${d_core}/input/*.h)
file(GLOB reios_SRCS		${d_core}/reios/*.cpp		${d_core}/reios/*.h)
file(GLOB imgread_SRCS		${d_core}/imgread/*.cpp		${d_core}/imgread/*.h)
file(GLOB profiler_SRCS		${d_core}/profiler/*.cpp	${d_core}/profiler/*.h)
file(GLOB archive_SRCS		${d_core}/archive/*.cpp		${d_core}/archive/*.h)

#### option(rend)
file(GLOB gl4_SRCS			${d_core}/rend/gl4/*.cpp	${d_core}/rend/gl4/*.h)
file(GLOB gles_SRCS			${d_core}/rend/gles/*.cpp	${d_core}/rend/gles/*.h)

set(core_SRCS
  ${hw_SRCS}
  ${cfg_SRCS}
  ${rend_SRCS}
  ${gl4_SRCS}
  ${gles_SRCS}
  ${input_SRCS}
  ${reios_SRCS}
  ${imgread_SRCS}
  ${profiler_SRCS}
  ${d_core}/archive/archive.cpp ${d_core}/archive/archive.h
  ${d_core}/nullDC.cpp
  ${d_core}/stdclass.cpp
  ${d_core}/dispframe.cpp
  ${d_core}/serialize.cpp
)

  
if(${BUILD_COMPILER} EQUAL ${COMPILER_GCC})	# Add Clang if NOT WIN32 *FIXME* 
  list(APPEND core_SRCS ${archive_SRCS})
endif()

if(${FEAT_SHREC} EQUAL ${DYNAREC_JIT})
#
  if(${HOST_CPU} EQUAL ${CPU_X86})
    list(APPEND core_SRCS 
      ${d_core}/rec-x86/rec_x86_driver.cpp
      ${d_core}/rec-x86/rec_x86_il.cpp
      ${d_core}/rec-x86/rec_x86_asm.cpp	# change for linux , rec_lin86_asm.S
	  ${d_core}/rec-x86/rec_x86_ngen.h
     )
  elseif(${HOST_CPU} EQUAL ${CPU_ARM})
    list(APPEND core_SRCS 
      ${d_core}/rec-ARM/ngen_arm.S
      ${d_core}/rec-ARM/rec_arm.cpp
	)
  elseif(${HOST_CPU} EQUAL ${CPU_X64})

	### FIXME: asm with cmake ninja+VC
    if(${BUILD_COMPILER} EQUAL ${COMPILER_VC})
	  list(APPEND core_SRCS ${d_core}/rec-x64/msvc.asm)
	endif()

    list(APPEND core_SRCS ${d_core}/rec-x64/rec_x64.cpp ${d_core}/rec-x64/x64_regalloc.h)

  elseif(${HOST_CPU} EQUAL ${CPU_A64})
    list(APPEND core_SRCS ${d_core}/rec-ARM64/rec_arm64.cpp ${d_core}/rec-ARM64/arm64_regalloc.h)

  else()
    message(" FEAT_SHREC==DYNAREC_JIT && HOST_CPU Unknown Default add arch or disable rec if not avail.")
	error()
  endif()
#
elseif(${FEAT_SHREC} EQUAL ${DYNAREC_CPP})
    list(APPEND core_SRCS ${d_core}/rec-cpp/rec_cpp.cpp)
endif()

add_definitions(/DFEAT_HAS_SOFTREND=0)


### deps.cmake #################################################################################

set(d_deps ${reicast_core_path}/deps)
include_directories ("${d_deps}")
include_directories ("${d_deps}/picotcp/include")
include_directories ("${d_deps}/picotcp/modules")

file(GLOB xbyak_H	${d_deps}/xbyak/*.h)		# include headers into cmake target/project view 

file(GLOB chdr_SRCS  ${d_deps}/chdr/*.c)
file(GLOB lzma_SRCS  ${d_deps}/lzma/*.c)
file(GLOB lz_SRCS    ${d_deps}/zlib/*.c)
file(GLOB lzip_SRCS  ${d_deps}/libzip/*.c)
file(GLOB lpng_SRCS  ${d_deps}/libpng/*.c)
file(GLOB lelf_SRCS  ${d_deps}/libelf/el*.cpp)
file(GLOB crypt_SRCS ${d_deps}/crypto/*.cpp)
file(GLOB imgui_SRCS ${d_deps}/imgui/*.cpp)
file(GLOB lws_SRCS   ${d_deps}/libwebsocket/*.c)

file(GLOB picoModS   ${d_deps}/picotcp/modules/*.c)
file(GLOB picoStkS   ${d_deps}/picotcp/stack/*.c)
set(pico_SRCS ${picoModS} ${picoStkS})

set(deps_SRCS
  ${lz_SRCS}
  ${lpng_SRCS}
  ${lelf_SRCS}
  ${chdr_SRCS}
  ${crypt_SRCS}
  ${imgui_SRCS}
  ${d_deps}/xbrz/xbrz.cpp
  ${d_deps}/dirent/dirent.c
  ${d_deps}/xxhash/xxhash.c
  ${d_deps}/chdpsr/cdipsr.cpp  # sigh,  this dir is named chdpsr for some reason ...
  ${d_deps}/coreio/coreio.cpp
# ${d_deps}/ifaddrs/ifaddrs.c
  ${xbyak_H}
)
  
if(${BUILD_COMPILER} EQUAL ${COMPILER_GCC})	# Add Clang if NOT WIN32 *FIXME* 
  list(APPEND deps_SRCS
    ${lzip_SRCS}
    ${lzma_SRCS}
    ${pico_SRCS}
  )
  add_definitions(-D_7ZIP_ST -DCHD5_LZMA)
endif()

### libosd.cmake ################################################################################


set(d_aout ${reicast_core_path}/oslib)

include_directories ("${d_core}/khronos")

## I really should just glob all of the dirs and ;shrug; if guards don't do it all ##

set(osd_SRCS "")

list(APPEND osd_SRCS ${d_aout}/audiostream.cpp)

if (${HOST_OS} EQUAL ${OS_WINDOWS})

  list(APPEND osd_SRCS ${d_core}/windows/winmain.cpp)
  list(APPEND osd_SRCS ${d_core}/windows/win_vmem.cpp)
  list(APPEND osd_SRCS ${d_aout}/audiobackend_directsound.cpp)
  
  
  link_libraries(dsound.lib winmm.lib xinput.lib wsock32.lib opengl32.lib)

  
elseif (${HOST_OS} EQUAL ${OS_LINUX} OR ${HOST_OS} EQUAL ${OS_ANDROID})

    list(APPEND osd_SRCS 
      ${d_core}/linux/common.cpp
      ${d_core}/linux/context.cpp
      ${d_core}/linux/nixprof/nixprof.cpp

      ${d_aout}/audiobackend_oss.cpp # add option
    ) # todo: configure linux audio lib options
  
    if(NOT ANDROID)
      list(APPEND osd_SRCS 
		${d_core}/linux-dist/x11.cpp
		${d_core}/linux-dist/main.cpp
		${d_core}/linux-dist/evdev.cpp)
	
      add_definitions(-DSUPPORT_X11)  ## don't use GLES ?
      link_libraries(X11)
    else()
      list(APPEND osd_SRCS 
        .//android-studio/reicast/src/main/jni/src/Android.cpp
        .//android-studio/reicast/src/main/jni/src/utils.cpp
    #   .//android-studio/reicast/src/main/jni/src/XperiaPlay.c
      )
    endif() # ANDROID
	
  add_definitions(-DGLES -DUSE_EVDEV)
  
  link_libraries(pthread dl rt asound Xext GLESv2 EGL)

elseif(${HOST_OS} EQUAL ${OS_DARWIN})
#
  list(APPEND objc_SRCS
    ./shell/apple/emulator-osx/emulator-osx/osx-main.mm
    ./shell/apple/emulator-osx/emulator-osx/AppDelegate.swift
    ./shell/apple/emulator-osx/emulator-osx/EmuGLView.swift
  )

  set_source_files_properties(${objc_SRCS} PROPERTIES COMPILE_FLAGS "-x objective-c++")
  
  list(APPEND osd_SRCS ${objc_SRCS}
    ${d_osd}/linux/common.cpp
    ${d_osd}/linux/context.cpp
    ${d_osd}/audiobackend/audiobackend_coreaudio.cpp
    # if NOT USE_SWIFT / ObjC
    #${d_osd}/apple/osx_osd.cpp
    )

else()
#
  message("OS Unhandled")
  error()
#
endif()



## 

include_directories ("${reicast_core_path}")




set(reicast_SRCS ${core_SRCS} ${deps_SRCS} ${osd_SRCS})

add_executable(${TNAME}${binSuffix} ${reicast_SRCS} ${deps_SRCS}) 





if(APPLE)
  enable_language(Swift)
  set_property(TARGET ${TNAME} PROPERTY XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "./shell/apple/emulator-osx/emulator-osx/emulator-osx-Bridging-Header.h")

  target_link_libraries(${TNAME}
# "-framework Cocoa"
# "-framework AppKit"
  "-framework CoreData"
  "-framework CoreAudio"
  "-framework AudioUnit"
  "-framework AudioToolbox"
  "-framework Foundation"
)

#### OSX Notes,  when not using xcode you have to make app bundle,  edit plist and copy,  convert MainMenu.xib to nib and copy, 
#null@devpc:~$ /Users/null/Documents/projects/reicast-emulator/bin/RelWithDebInfo/Reicast.app/Contents/MacOS/reicast ; exit;
#2019-03-18 14:28:44.842 reicast[11468:131797] Unknown class _TtC12emulator_osx9EmuGLView in Interface Builder file at path /Users/null/Documents/projects/reicast-emulator/bin/RelWithDebInfo/Reicast.app/Contents/Resources/MainMenu.nib.
#2019-03-18 14:28:44.842 reicast[11468:131797] Unknown class _TtC12emulator_osx11AppDelegate in Interface Builder file at path /Users/null/Documents/projects/reicast-emulator/bin/RelWithDebInfo/Reicast.app/Contents/Resources/MainMenu.nib.
#2019-03-18 14:28:44.860 reicast[11468:131797] Failed to connect (window) outlet from (NSObject) to (NSWindow): missing setter or instance variable
#

endif() #APPLE


if(DEBUG_CMAKE)
  message(" ------------------------------------------------")
  message(" - HOST_OS: ${HOST_OS} - HOST_CPU: ${HOST_CPU}   ")
  message(" - host_os: ${host_os} - host_arch: ${host_arch} ")
  message(" ------------------------------------------------")
  message("  C  Flags: ${CMAKE_C_FLAGS} ")
  message(" CXX Flags: ${CMAKE_CXX_FLAGS} ")
  message(" LINK_DIRS: ${LINK_DIRECTORIES}")
  message("LINK_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
  message(" ------------------------------------------------\n")
endif()



















