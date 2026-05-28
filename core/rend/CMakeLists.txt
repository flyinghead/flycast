target_sources(${PROJECT_NAME} PRIVATE
        CustomTexture.cpp
        CustomTexture.h
        osd.cpp
        osd.h
        sorter.cpp
        sorter.h
        TexCache.cpp
        TexCache.h
        texconv.cpp
        texconv.h
        transform_matrix.cpp
        transform_matrix.h
        norend/norend.cpp)

if(USE_OPENGL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OPENGL)
    add_subdirectory(gl4)
    add_subdirectory(gles)

    if (NOT LIBRETRO)
        target_sources(${PROJECT_NAME} PRIVATE
                "${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp"
                "${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.h")
    endif()
endif()

if(USE_VULKAN)
    add_subdirectory(vulkan)
endif()

if(WIN32 AND USE_DX9 AND NOT LIBRETRO AND NOT WINDOWS_STORE AND ("x86" IN_LIST ARCHITECTURE OR "x86_64" IN_LIST ARCHITECTURE))
    add_subdirectory(dx9)
endif()

if(WIN32 AND USE_DX11)
    add_subdirectory(dx11)
endif()
