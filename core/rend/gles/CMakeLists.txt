target_sources(${PROJECT_NAME} PRIVATE
        glcache.h
        gldraw.cpp
        gles.cpp
        gles.h
        gltex.cpp
        quad.cpp
        postprocess.cpp
        postprocess.h
        naomi2.cpp
        naomi2.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            opengl_driver.cpp
            opengl_driver.h)
endif()
