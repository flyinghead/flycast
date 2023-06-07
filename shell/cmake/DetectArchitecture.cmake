# From https://github.com/merryhime/dynarmic/blob/39c59b6c46bec9e4c7a3fae315fc778afc55fc45/CMakeModules/DetectArchitecture.cmake

include(CheckSymbolExists)

if (CMAKE_OSX_ARCHITECTURES)
    set(ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")
    return()
endif()

function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET YES)
        check_symbol_exists("${symbol}" "" DETECT_ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        if (DETECT_ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
        endif()

        unset(DETECT_ARCHITECTURE_${arch} CACHE)
    endif()
endfunction()

detect_architecture("__ARM64__" arm64)
detect_architecture("__aarch64__" arm64)
detect_architecture("_M_ARM64" arm64)

detect_architecture("__arm__" arm)
detect_architecture("__TARGET_ARCH_ARM" arm)
detect_architecture("_M_ARM" arm)

detect_architecture("__x86_64" x86_64)
detect_architecture("__x86_64__" x86_64)
detect_architecture("__amd64" x86_64)
detect_architecture("_M_X64" x86_64)

detect_architecture("__i386" x86)
detect_architecture("__i386__" x86)
detect_architecture("_M_IX86" x86)
