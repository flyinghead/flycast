target_compile_definitions(${PROJECT_NAME} PRIVATE USE_DX9)

set(REND_DX9_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/d3d_overlay.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/d3d_overlay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/d3d_renderer.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/d3d_renderer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/d3d_shaders.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/d3d_shaders.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/d3d_texture.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/d3d_texture.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/dx9_driver.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/dxcontext.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/dxcontext.h")

if(NOT LIBRETRO)
    list(APPEND REND_DX9_FILES
            "${IMGUI_BACKENDS_DIR}/imgui_impl_dx9.cpp"
            "${IMGUI_BACKENDS_DIR}/imgui_impl_dx9.h")
endif()

target_sources(${PROJECT_NAME} PRIVATE ${REND_DX9_FILES})

# TODO: This is a lousy long-term solution.
# Renderers should separate targets to avoid this kind of header clash.
if(NOT MINGW)
    set_property(SOURCE ${REND_DX9_FILES}
            TARGET_DIRECTORY ${PROJECT_NAME}
            PROPERTY INCLUDE_DIRECTORIES "$ENV{DXSDK_DIR}/Include")
endif()