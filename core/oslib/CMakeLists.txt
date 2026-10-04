target_sources(${PROJECT_NAME} PRIVATE
        directory.cpp
        directory.h
        host_context.h
        http_client.cpp
        http_client.h
        http_server.cpp
        http_server.h
        i18n.cpp
        i18n.h
        oslib.h
        resources.cpp
        resources.h
        storage.cpp
        storage.h
        unwind_info.h
        virtmem.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            oslib.cpp)
endif()
