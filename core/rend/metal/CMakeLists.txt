target_compile_definitions(${PROJECT_NAME} PRIVATE USE_METAL)

set(METAL_SOURCES
        metal.h
        metal_buffer.h
        metal_buffer.mm
        metal_commandpool.h
        metal_commandpool.mm
        metal_context.h
        metal_context.mm
        metal_drawer.h
        metal_drawer.mm
        metal_driver.h
        metal_overlay.h
        metal_overlay.mm
        metal_pipeline.h
        metal_pipeline.mm
        metal_quad.h
        metal_quad.mm
        metal_renderer.h
        metal_renderer.mm
        metal_shaders.h
        metal_shaders.mm
        metal_texture.h
        metal_texture.mm
        oit/metal_oit_buffer.h
        oit/metal_oit_drawer.h
        oit/metal_oit_drawer.mm
        oit/metal_oit_pipeline.h
        oit/metal_oit_shaders.h
        "${IMGUI_BACKENDS_DIR}/imgui_impl_metal.h"
        "${IMGUI_BACKENDS_DIR}/imgui_impl_metal.mm")
target_sources(${PROJECT_NAME} PRIVATE ${METAL_SOURCES})

# The Metal renderer and the imgui Metal backend rely on ARC
set_source_files_properties(${METAL_SOURCES} TARGET_DIRECTORY ${PROJECT_NAME}
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${METAL_FRAMEWORK} ${QUARTZCORE_FRAMEWORK})
