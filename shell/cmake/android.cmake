## android.cmake
#



function(TestPathNDK ndkPath)
#
	file(TO_CMAKE_PATH "${ndkPath}" testPath)
	
	if(NOT NDK AND EXISTS "${testPath}")		
		if(EXISTS "${testPath}/ndk-bundle")
			set(NDK ${testPath}/ndk-bundle PARENT_SCOPE)
		elseif(EXISTS "${testPath}/sysroot")
			set(NDK ${testPath} PARENT_SCOPE)
		endif()
	endif()
#
endfunction(TestPathNDK)


TestPathNDK("$ENV{ANDROID_HOME}")
TestPathNDK("$ENV{NDK}")
TestPathNDK("$ENV{NDK_ROOT}")

if(NOT NDK)
	message("Failed to find NDK !")
endif()






### option for ARM || ARM64 ?  HOST isn't useful it's a cross ...

#set(CMAKE_SYSTEM_PROCESSOR aarch64)



set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 22) # API level

set(CMAKE_ANDROID_NDK ${NDK})
set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a) #arm64-v8a , armeabi-v7a , armeabi
set(CMAKE_ANDROID_STL_TYPE c++_static)  #gnustl_static    libc++ will allow C++17, if you use _shared you must include in apk !
set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)


#arm	$TOOLCHAIN/ arm-linux-androideabi	/lib/
#arm64	$TOOLCHAIN/ aarch64-linux-android	/lib/
#x86	$TOOLCHAIN/ i686-linux-android		/lib/
#x86_64	$TOOLCHAIN/ x86_64-linux-android	/lib/



#include(${NDK}/build/cmake/android.toolchain.cmake)


set(ANDROID ON)

add_definitions(-D_ANDROID -DANDROID)
add_definitions(-DANDROID_STL=c++_static)


add_definitions(-DTARGET_ANDROID)

add_definitions(-DGLES)





## FML

#[[
CMAKE_ANDROID_ANT_ADDITIONAL_OPTIONS
CMAKE_ANDROID_API
CMAKE_ANDROID_API_MIN
CMAKE_ANDROID_ARCH
CMAKE_ANDROID_ARCH_ABI
CMAKE_ANDROID_ARM_MODE
CMAKE_ANDROID_ARM_NEON
CMAKE_ANDROID_ASSETS_DIRECTORIES
CMAKE_ANDROID_GUI
CMAKE_ANDROID_JAR_DEPENDENCIES
CMAKE_ANDROID_JAR_DIRECTORIES
CMAKE_ANDROID_JAVA_SOURCE_DIR
CMAKE_ANDROID_NATIVE_LIB_DEPENDENCIES
CMAKE_ANDROID_NATIVE_LIB_DIRECTORIES
CMAKE_ANDROID_NDK
CMAKE_ANDROID_NDK_DEPRECATED_HEADERS
CMAKE_ANDROID_NDK_TOOLCHAIN_HOST_TAG
CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION
CMAKE_ANDROID_PROCESS_MAX
CMAKE_ANDROID_PROGUARD
CMAKE_ANDROID_PROGUARD_CONFIG_PATH
CMAKE_ANDROID_SECURE_PROPS_PATH
CMAKE_ANDROID_SKIP_ANT_STEP
CMAKE_ANDROID_STANDALONE_TOOLCHAIN
CMAKE_ANDROID_STL_TYPE
#]]
