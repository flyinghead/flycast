target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/aica.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/aica.h
        ${CMAKE_CURRENT_SOURCE_DIR}/aica_if.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/aica_if.h
        ${CMAKE_CURRENT_SOURCE_DIR}/aica_mem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/aica_mem.h
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp.h
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp_arm32.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp_arm64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp_interp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp_x64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp_x86.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sgc_if.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sgc_if.h)
