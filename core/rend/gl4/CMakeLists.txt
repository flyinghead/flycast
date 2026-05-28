if(NOT (APPLE OR USE_GLES2))
target_sources(${PROJECT_NAME} PRIVATE
        abuffer.cpp
        gl4.h
        gldraw.cpp
        gles.cpp
        gl4naomi2.cpp
        gl4naomi2.h)
endif()
