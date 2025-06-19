target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/option.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/cfg.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/cfg.h
            ${CMAKE_CURRENT_SOURCE_DIR}/cl.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/ini.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/ini.h
            ${CMAKE_CURRENT_SOURCE_DIR}/option.cpp)
endif()
