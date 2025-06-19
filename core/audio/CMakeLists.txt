target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/audiostream.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_alsa.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_coreaudio.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_directsound.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_libao.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_null.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_oboe.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_omx.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_oss.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_pulseaudio.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiobackend_sdl2.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/audiostream.cpp)
endif()
