target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/directory.h
        ${CMAKE_CURRENT_SOURCE_DIR}/host_context.h
        ${CMAKE_CURRENT_SOURCE_DIR}/oslib.h
        ${CMAKE_CURRENT_SOURCE_DIR}/resources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources.h
        ${CMAKE_CURRENT_SOURCE_DIR}/storage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/storage.h
        ${CMAKE_CURRENT_SOURCE_DIR}/unwind_info.h
        ${CMAKE_CURRENT_SOURCE_DIR}/virtmem.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/http_client.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/http_client.h
            ${CMAKE_CURRENT_SOURCE_DIR}/oslib.cpp)
endif()
