target_sources(${PROJECT_NAME} PRIVATE
        context.cpp
        context.h
        libretro.h)

if(USE_METAL)
    # The Metal context header uses Objective-C types
    target_sources(${PROJECT_NAME} PRIVATE switcher.mm)
    set_source_files_properties(switcher.mm TARGET_DIRECTORY ${PROJECT_NAME} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
else()
    target_sources(${PROJECT_NAME} PRIVATE switcher.cpp)
endif()

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
