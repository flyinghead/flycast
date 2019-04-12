## ps4sdk.cmake - devkitpro A64 cross-compile
#
set(CMAKE_SYSTEM_NAME FreeBSD) # this one is important
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 9)   # this one not so much




set(TARGET_PS4 ON)
set(TARGET_BSD ON)



### This shit is very WIP ###
#
## TODO: Check for 


set(PS4SDK $ENV{PS4SDK})
set(SCESDK $ENV{SCESDK})
  
set(USE_SCE ON)
set(PS4_PKG ON)

if(PS4_PKG)
  add_definitions(-DPS4_PKG)
endif()



if ("" STREQUAL "${PS4SDK}")
  if ("Windows" STREQUAL "${CMAKE_HOST_SYSTEM_NAME}")
    set(PS4SDK "C:/Dev/SDK/PS4")
  else()
    set(PS4SDK "/opt/ps4")
  endif()
endif()



set(TAUON_SDK ${PS4SDK}/tauon)



if(USE_SCE)
#
	set(PS4SDK    ${PS4SDK}/SCE/PS4SDK)

	set(PS4HOST   ${PS4SDK}/host_tools)
	set(PS4TARGET ${PS4SDK}/target)

	set(toolPrefix "orbis-")
	set(toolSuffix ".exe")

	set(CMAKE_C_COMPILER   ${PS4HOST}/bin/${toolPrefix}clang${toolSuffix})
	set(CMAKE_CXX_COMPILER ${PS4HOST}/bin/${toolPrefix}clang++${toolSuffix})
	
	set(CMAKE_FIND_ROOT_PATH  ${PS4TARGET}) # where is the target environment


	
	set (PS4_inc_dirs 
		${TAUON_SDK}/include 
		${PS4TARGET}/include 
		${PS4TARGET}/include_common
	)

#	set (PS4_link_dirs
#		"${PS4TARGET}/lib"
#		"${PS4TARGET}/tauon/lib"
#	)

	
#LDFLAGS += -L $(TAUON_SDK_DIR)/lib -L $(SCE_ORBIS_SDK_DIR)/target/lib
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -Wl,--addressing=non-aslr,--strip-unused-data ")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L ${TAUON_SDK}/lib -L ${PS4TARGET}/lib")

	message("CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS}")
#
else()
#
	set(triple "x86_64-scei-ps4")

	set(CMAKE_C_COMPILER_TARGET ${triple})
	set(CMAKE_CXX_COMPILER_TARGET ${triple})
	
	set(CMAKE_C_COMPILER   clang)
	set(CMAKE_CXX_COMPILER clang++)

	
	set (PS4_inc_dirs 
		${TAUON_SDK}/include 

		${PS4SDK}/include 
		${PS4SDK}/tauon/include 
	)

#	set (PS4_link_dirs
#		"${PS4SDK}/lib"
#		"${PS4SDK}/tauon/lib"
#	)

	
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -Wl,--addressing=non-aslr,--strip-unused-data -L${TAUON_SDK}/lib")
#
endif()




set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) # search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)  # for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)


include_directories(${PS4_inc_dirs})




	### Add a helper to add libSce PREFIX and [_tau]*_stub[_weak]*.a SUFFIX
	#
link_libraries(
	kernel_tau_stub_weak SceSysmodule_tau_stub_weak SceSystemService_stub_weak SceSystemService_tau_stub_weak SceShellCoreUtil_tau_stub_weak ScePigletv2VSH_tau_stub_weak kernel_util
	ScePad_stub_weak SceNet_stub_weak SceCommonDialog_stub_weak ScePosix_stub_weak
)



