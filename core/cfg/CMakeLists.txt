target_sources(${PROJECT_NAME} PRIVATE
        option.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            cfg.cpp
            cfg.h
            cl.cpp
            ini.cpp
            ini.h
            option.cpp)
endif()
