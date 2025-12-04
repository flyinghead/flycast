target_sources(${PROJECT_NAME} PRIVATE
        ini.cpp
        ini.h
        option.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            cfg.cpp
            cfg.h
            cl.cpp
            option.cpp)
endif()
