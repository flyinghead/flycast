## config module 
#
#	get luserx0 to doc this shit or something, vars in all caps are to be exported as defs if they aren't in build.h already
#		handle options for FEAT per platform, so rec isn't built for targets w.o one...
#		*TODO* fix Android for build system in emu too,  OS_LINUX is hardly fitting:  prob works better as PLATFORM_ANDROID_{S,N}DK or something
#		*TODO* setup git version like it's done in VS/make and configure header so --version works still
#		*TODO* lots of other shit to improve build, add enabling/disabling libs/features, setting 3rd party libs as either built in  static, dynamic  or shared/system
#
#


set(ZBUILD Off)



set(BUILD_LIBS OFF)         ## Scope:Local If set will build libs { dreamcast, osd, ... }
set(BUILD_LIB_TYPE STATIC)  ## Scope:Local If BUILD_LIBS is set, will use this as type of lib to use { STATIC, SHARED, MODULE (plugin) } *TODO*
set(BUILD_SHARED_LIBS OFF)  ## Scope:CMAKE If type is not specified in add_library, use SHARED




## Build flags ##
#

set(DC_PLATFORM_MASK        7)  # Z: Uh, not a bitset
set(DC_PLATFORM_DREAMCAST   0)  # /* Works, for the most part */
set(DC_PLATFORM_DEV_UNIT    1)  # /* This is missing hardware */
set(DC_PLATFORM_NAOMI       2)  # /* Works, for the most part */ 
set(DC_PLATFORM_NAOMI2      3)  # /* Needs to be done, 2xsh4 + 2xpvr + custom TNL */
set(DC_PLATFORM_ATOMISWAVE  4)  # /* Needs to be done, DC-like hardware with possibly more ram */
set(DC_PLATFORM_HIKARU      5)  # /* Needs to be done, 2xsh4, 2x aica , custom vpu */
set(DC_PLATFORM_AURORA      6)  # /* Needs to be done, Uses newer 300 mhz sh4 + 150 mhz pvr mbx SoC */



set(OS_WINDOWS     0x10000001)  # HOST_OS
set(OS_LINUX       0x10000002)
set(OS_DARWIN      0x10000003)
set(OS_IOS         0x10000004)  # todo: iOS != OS_DARWIN
set(OS_ANDROID     0x10000005)  # todo: should be SYSTEM_ANDROID but ! OS_LINUX

set(OS_UWP         0x10000011)
set(OS_NSW_HOS     0x80000001)
set(OS_PS4_BSD     0x80000002)



set(CPU_X86        0x20000001)  # HOST_CPU
set(CPU_X64        0x20000004)
set(CPU_ARM        0x20000002)
set(CPU_A64        0x20000008)
set(CPU_MIPS       0x20000003)
set(CPU_MIPS64     0x20000009)
set(CPU_PPC        0x20000006)
set(CPU_PPC64      0x20000007)
set(CPU_GENERIC    0x20000005)  # used for pnacl, emscripten, etc

set(DYNAREC_NONE   0x40000001)  # FEAT_SHREC, FEAT_AREC, FEAT_DSPREC
set(DYNAREC_JIT    0x40000002)
set(DYNAREC_CPP    0x40000003)

set(COMPILER_VC    0x30000001)  # BUILD_COMPILER
set(COMPILER_GCC   0x30000002)
set(COMPILER_CLANG 0x30000003)
set(COMPILER_INTEL 0x30000004)







## These default to host, but are used for cross so make sure not to contaminate
#
#	CMAKE_SYSTEM		${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}.
#	CMAKE_SYSTEM_NAME		CMAKE_HOST_SYSTEM_NAME			uname -s	Linux, Windows, and Darwin
#	CMAKE_SYSTEM_VERSION	CMAKE_HOST_SYSTEM_VERSION		uname -r
#	CMAKE_SYSTEM_PROCESSOR	CMAKE_HOST_SYSTEM_PROCESSOR		uname -p
#
#
#
#	BOOL: CMAKE_HOST_UNIX	CMAKE_HOST_WIN32  CMAKE_HOST_APPLE
#
#
#	CMAKE_LIBRARY_ARCHITECTURE	CMAKE_<LANG>_LIBRARY_ARCHITECTURE	<prefix>/lib/<arch>
#
#




## strings are used to append to path/file names, and to filter multiple possibilities down to one 
#		AMD64/x86_64:x64, i*86:x86, ppc/powerpc[64][b|l]e:ppc[64] etc 
#
if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")    # todo: check MATCHES "i.86" ?
  set(host_arch "x86")
  set(HOST_CPU ${CPU_X86})
#
elseif(("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64") OR
       ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64"))
  set(host_arch "x64")
  set(HOST_CPU ${CPU_X64})
#
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
  set(host_arch "arm64")
  set(HOST_CPU ${CPU_A64})
#
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm")
  set(host_arch "arm")
  set(HOST_CPU ${CPU_ARM})
#
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "ppc64")
  set(host_arch "ppc64")
  set(HOST_CPU ${CPU_PPC64})
#
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "ppc")	 # has to be done after ppc64 obv
  set(host_arch "ppc")
  set(HOST_CPU ${CPU_PPC})
#
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "mips64") # todo: check , r* names?
  set(host_arch "mips64")
  set(HOST_CPU ${CPU_MIPS64})
#
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "mips")   # todo: check , r* names?
  set(host_arch "mips")
  set(HOST_CPU ${CPU_MIPS})
#
else()
  message("Warning: Unknown Host System Processor: \"${CMAKE_SYSTEM_PROCESSOR}\"")
  set(host_arch "${CMAKE_SYSTEM_PROCESSOR}")
  set(HOST_CPU ${CPU_GENERIC})
endif()




string(TOLOWER ${CMAKE_SYSTEM_NAME} host_os)

#message(" - testing cmake host_os: \"${host_os}\"")

## HOST_* is not TARGET_*  ;;  change ndc-e internal naming it's wrong , then change this  ;;

if("android" STREQUAL "${host_os}"  OR ANDROID)
  set(HOST_OS ${OS_LINUX}) 	# *FIXME* we might have to keep as OS_LINUX or add to full cleanup list :|

elseif("windowsstore" STREQUAL "${host_os}")
  set(HOST_OS ${OS_UWP}) 
  set(HOST_CPU ${CPU_X64})

elseif(CMAKE_HOST_WIN32)
  set(HOST_OS ${OS_WINDOWS}) 

elseif(CMAKE_HOST_APPLE)

  if("${host_arch}" MATCHES "arm")
    set(HOST_OS ${OS_IOS})
    set(TARGET_IOS On)
    add_definitions(-DTARGET_IPHONE -DTARGET_IOS)
  else()
    set(HOST_OS ${OS_DARWIN})  # todo ios check, check compiler/arch?
    set(TARGET_OSX On)
    add_definitions(-DTARGET_OSX)
  endif()
  
elseif(CMAKE_HOST_UNIX) # GP UNIX MUST BE AFTER OTHER UNIX'ish options such as APPLE , it matches both 

    set(HOST_OS ${OS_LINUX})   # todo android check, just check android vars?
endif()



#option(TARGET_NO_REC  BOOL "")
#option(TARGET_NO_AREC BOOL "")
#option(TARGET_NO_JIT  BOOL "")



## Dynarec avail on x86,x64,arm and aarch64 in arm.32 compat 
#
if((${HOST_CPU} EQUAL ${CPU_X86}) OR (${HOST_CPU} EQUAL ${CPU_X64}) OR
   (${HOST_CPU} EQUAL ${CPU_ARM}) OR (${HOST_CPU} EQUAL ${CPU_A64}))
#
  message("Dynarec Features Available")
  
  set(FEAT_SHREC  ${DYNAREC_JIT})
  set(FEAT_AREC   ${DYNAREC_NONE})
  set(FEAT_DSPREC ${DYNAREC_NONE})
#
else()
  set(FEAT_SHREC  ${DYNAREC_CPP})
  set(FEAT_AREC   ${DYNAREC_NONE})
  set(FEAT_DSPREC ${DYNAREC_NONE})
endif()

## Handle TARGET_* to FEAT_  *FIXME* stupid use one or the other and propogate : part of build cleanup , TARGET_ will only be for platform specifics and FEAT_ as OPTIONS
#
if(TARGET_NO_REC)
  set(FEAT_SHREC  ${DYNAREC_NONE})
  set(FEAT_AREC   ${DYNAREC_NONE})
  set(FEAT_DSPREC ${DYNAREC_NONE})
endif()

if(TARGET_NO_AREC)
  set(FEAT_SHREC  ${DYNAREC_JIT})
  set(FEAT_AREC   ${DYNAREC_NONE})
  set(FEAT_DSPREC ${DYNAREC_NONE})
endif()

if(TARGET_NO_JIT)
  set(FEAT_SHREC  ${DYNAREC_CPP})
  set(FEAT_AREC   ${DYNAREC_NONE})
  set(FEAT_DSPREC ${DYNAREC_NONE})
endif()





######## Looks like something to delete, but if we're going to handle options here and NOT in libosd/lib* #########

# FindNativeCompilers()
## options BUILD_COMPILER { GCC, Clang, Intel, RealView? }


#set(CMAKE_C_COMPILER clang)
#set(CMAKE_C_COMPILER_TARGET ${triple})
#set(CMAKE_CXX_COMPILER clang++)
#set(CMAKE_CXX_COMPILER_TARGET ${triple})



if(${HOST_OS} EQUAL ${OS_LINUX})
# option SUPPORT_X11
# option FEAT_HAS_NIXPROF
# option EMSCripten
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
#
  set(BUILD_COMPILER ${COMPILER_VC})
  message("MSVC Platform: ${CMAKE_VS_PLATFORM_NAME}")
  message("MSVC Toolset:  ${CMAKE_VS_PLATFORM_TOOLSET}")
  
  
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1)

  if("${HOST_OS}" STREQUAL "${OS_UWP}")
    set(_CXX_FLAGS "/ZW ")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /AppContainer")
  endif()
#
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") 
  set(BUILD_COMPILER ${COMPILER_GCC})
#
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")  # AppleClang ffs
  set(BUILD_COMPILER ${COMPILER_CLANG})
#
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(BUILD_COMPILER ${COMPILER_INTEL})
#
else()
  message("Unknown Compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()



## Setup some common flags 
#
if ((${BUILD_COMPILER} EQUAL ${COMPILER_VC}) OR
	(${BUILD_COMPILER} EQUAL ${COMPILER_CLANG}) AND (${HOST_OS} STREQUAL ${OS_WINDOWS}))

		
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1)

  if(${BUILD_COMPILER} EQUAL ${COMPILER_CLANG})
    add_definitions(/DXBYAK_NO_OP_NAMES /DTARGET_NO_OPENMP)  #*FIXME* check openmp on clang-cl
    remove_definitions(/U_HAS_STD_BYTE)
    set(_CXX_FLAGS "/std:c++14")	# /U_HAS_STD_BYTE not working, have to use c++14 not 17 :|
    set(_C_FLAGS "-Wno-unused-function -Wno-unused-variable")
  endif()


elseif ((${BUILD_COMPILER} EQUAL ${COMPILER_GCC}) OR
		(${BUILD_COMPILER} EQUAL ${COMPILER_CLANG})) # AND NOT ${HOST_OS} EQUAL ${OS_WINDOWS}))
  
  if(USE_32B OR TARGET_LINUX_X86)
    set(_C_FLAGS "${_C_FLAGS} -m32")
  endif()
  
  if((${HOST_CPU} EQUAL ${CPU_X86}) OR (${HOST_CPU} EQUAL ${CPU_X64}))
    set(_C_FLAGS "${_C_FLAGS} -msse4")
    
    if(NOT CMAKE_HOST_APPLE)
      set(_C_FLAGS "${_C_FLAGS} -fopenmp")
    endif()
  endif() # X86 family
  
    
  set(_CXX_FLAGS "${_CXX_FLAGS} -fno-operator-names -fpermissive -std=c++11") # -fcxx-exceptions") ## xbyak needs exceptions


endif()


set(_CXX_FLAGS "${_CXX_FLAGS} ${_C_FLAGS}")



set(CMAKE_C_FLAGS " ${_C_FLAGS}") # ${CMAKE_C_FLAGS}   -- these hold default VC flags for non VC Build ?
set(CMAKE_CXX_FLAGS " ${_CXX_FLAGS}") # ${CMAKE_CXX_FLAGS}


#if defined(TARGET_NO_NIXPROF)
#define FEAT_HAS_NIXPROF 0
#endif

#if defined(TARGET_NO_COREIO_HTTP)
#define FEAT_HAS_COREIO_HTTP 0
#endif

if (TARGET_NSW) # -DCMAKE_TOOLCHAIN_FILE=./cmake/devkitA64.cmake -DTARGET_NSW=ON
  set(HOST_OS ${OS_NSW_HOS}) 

  message(" DEVKITA64: ${DEVKITA64} ")
  message("HOST_OS ${HOST_OS}")

  add_definitions(-D__SWITCH__ -DGLES -DMESA_EGL_NO_X11_HEADERS)
  add_definitions(-DTARGET_NO_THREADS -DTARGET_NO_EXCEPTIONS -DTARGET_NO_NIXPROF)
  add_definitions(-DTARGET_NO_COREIO_HTTP)
  add_definitions(-D_GLIBCXX_USE_C99_MATH_TR1 -D_LDBL_EQ_DBL)

endif()

if (TARGET_PS4) # -DCMAKE_TOOLCHAIN_FILE=./cmake/{ps4sdk,clang_scei}.cmake -DTARGET_PS4=ON
  set(HOST_OS ${OS_PS4_BSD})
  message("HOST_OS ${HOST_OS}")
  

  add_definitions(-DPS4 -DTARGET_PS4 -DTARGET_BSD -D__ORBIS__ -DGLES -DMESA_EGL_NO_X11_HEADERS)  ## last needed for __unix__ on eglplatform.h
  add_definitions(-DTARGET_NO_THREADS -DTARGET_NO_EXCEPTIONS -DTARGET_NO_NIXPROF)
  add_definitions(-DTARGET_NO_COREIO_HTTP)


  message("*******FIXME******** LARGE PAGES !!")
endif()

if(ZBUILD)
  set(DEBUG_CMAKE ON)
  add_definitions(-D_Z_)  # Get rid of some warnings and internal dev testing
  
  if(NOT TARGET_PS4 AND NOT TARGET_NSW AND 
     NOT TARGET_OSX AND NOT TARGET_IOS )
     set(USE_QT ON)
  endif()
endif()

# configure options for osd/ui 
# osd_default, osd_qt
# ui_default, ui_sdl, ui_qt
# USE_NATIVE , USE_SDL , USE_QT  -- these (can) define multiple

option(USE_QT False "Use Qt5 for UI and support OS Deps.")

#option(BUILD_TESTS "Build tests" OFF)	# todo: luserx0 this is your arena, you want tests add em

add_definitions(-DCMAKE_BUILD)

add_definitions(-DHOST_OS=${HOST_OS})
add_definitions(-DHOST_CPU=${HOST_CPU})

add_definitions(-DFEAT_AREC=${FEAT_AREC})
add_definitions(-DFEAT_SHREC=${FEAT_SHREC})
add_definitions(-DFEAT_DSPREC=${FEAT_DSPREC})

add_definitions(-DBUILD_COMPILER=${BUILD_COMPILER})

add_definitions(-DDEF_CONSOLE)


set(RE_CMAKE_CONFIGURED 1)
#add_definitions(-D=${})
