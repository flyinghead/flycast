target_sources(${PROJECT_NAME} PRIVATE
        context.h
        libretro.cpp
        libretro.h
        switcher.cpp)

if(USE_OPENGL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OPENGL)
    target_sources(${PROJECT_NAME} PRIVATE
            egl.cpp
            egl.h
            gl_context.cpp
            gl_context.h
            osx.cpp
            osx.h
            sdl.cpp
            sdl.h)
endif()
