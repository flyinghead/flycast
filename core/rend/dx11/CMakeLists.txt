target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_D3D11)

target_sources(${PROJECT_NAME} PRIVATE
        dx11_driver.h
        dx11_overlay.cpp
        dx11_overlay.h
        dx11_renderer.cpp
        dx11_renderer.h
        dx11_renderstate.h
        dx11_renderstate.cpp
        dx11_shaders.cpp
        dx11_shaders.h
        dx11_texture.cpp
        dx11_texture.h
        dx11context.cpp
        dx11context.h
        dx11context_lr.cpp
        dx11context_lr.h
        dx11_driver.h
        dx11_naomi2.cpp
        dx11_naomi2.h
        oit/dx11_oitbuffers.h
        oit/dx11_oitrenderer.cpp
        oit/dx11_oitshaders.cpp
        oit/dx11_oitshaders.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            "${IMGUI_BACKENDS_DIR}/imgui_impl_dx11.cpp"
            "${IMGUI_BACKENDS_DIR}/imgui_impl_dx11.h")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 d3dcompiler dxgi)
