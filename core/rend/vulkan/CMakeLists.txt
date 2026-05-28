target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN HAVE_VULKAN)

target_sources(${PROJECT_NAME} PRIVATE
        oit/oit_buffer.h
        oit/oit_drawer.cpp
        oit/oit_drawer.h
        oit/oit_pipeline.cpp
        oit/oit_pipeline.h
        oit/oit_renderer.cpp
        oit/oit_renderpass.cpp
        oit/oit_renderpass.h
        oit/oit_shaders.cpp
        oit/oit_shaders.h
        adreno.cpp
        buffer.cpp
        buffer.h
        commandpool.cpp
        commandpool.h
        compiler.cpp
        compiler.h
        drawer.cpp
        drawer.h
        pipeline.cpp
        pipeline.h
        quad.cpp
        quad.h
        shaders.cpp
        shaders.h
        texture.cpp
        texture.h
        utils.h
        vmallocator.cpp
        vmallocator.h
        overlay.cpp
        overlay.h
        vulkan_context.h
        vulkan.h
        vulkan_renderer.cpp)

if(LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            vk_context_lr.cpp
            vk_context_lr.h)
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
    target_sources(${PROJECT_NAME} PRIVATE
            vulkan_driver.h
            vulkan_context.cpp
            "${IMGUI_BACKENDS_DIR}/imgui_impl_vulkan.cpp"
            "${IMGUI_BACKENDS_DIR}/imgui_impl_vulkan.h")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_definitions(${PROJECT_NAME} PRIVATE ImTextureID=ImU64)
    endif()
endif()
