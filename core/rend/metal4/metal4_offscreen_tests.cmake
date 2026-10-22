# Shared native CLI target; never part of the default app build.
function(add_flycast_metal_offscreen_test target root freetype_target xxhash_target)
    find_library(offscreen_metal Metal REQUIRED)
    find_library(offscreen_foundation Foundation REQUIRED)
    find_library(offscreen_quartzcore QuartzCore REQUIRED)
    find_library(offscreen_appkit AppKit REQUIRED)
    find_library(offscreen_metalfx MetalFX REQUIRED)
    add_executable(${target} EXCLUDE_FROM_ALL
        ${root}/tests/metal/metal_offscreen_renderer_tests.mm
        ${root}/tests/metal/metal_offscreen_services.cpp
        ${root}/tests/metal/metal_offscreen_allocations.cpp
        ${root}/tests/metal/metal_pvr_lifecycle_tests.mm
        ${root}/tests/metal/metal_accumulator_validation.inc
        ${root}/tests/metal/metal_custom_texture_validation.inc
        ${root}/tests/metal/metal_texture_session_validation.inc
        ${root}/tests/metal/metal_recovery_validation.inc
        ${root}/tests/metal/metal_context_presentation_validation.inc
        ${root}/tests/metal/metal_storage_profile_validation.inc
        ${root}/tests/metal/metal_storage_lifetime_validation.inc
        ${root}/tests/metal/metal_intermediates_validation.inc
        ${root}/tests/metal/metal_proof_profile_validation.inc
        ${root}/tests/metal/metal_plan_profile_validation.inc
        ${root}/tests/metal/metal_pool_profile_validation.inc
        ${root}/tests/metal/metal_cache_profile_validation.inc
        ${root}/tests/metal/metal_offscreen_test_hooks.h
        ${root}/core/rend/metal4/metal4_context.mm ${root}/core/rend/metal4/metal4_renderer.mm
        ${root}/core/rend/metal4/metal4_texture.mm ${root}/core/rend/metal4/metal4_commands.mm
        ${root}/core/rend/metal4/metal4_upload_state.mm ${root}/core/rend/metal4/metal4_preload.mm
        ${root}/core/rend/metal4/metal4_render_commands.mm ${root}/core/rend/metal4/metal4_presentation.mm
        ${root}/core/rend/metal4/metal4_fx.mm ${root}/core/rend/metal4/metal4_pipeline_cache.mm
        ${root}/core/rend/metal4/metal4_support.mm
        ${root}/core/rend/TexCache.cpp ${root}/core/rend/texconv.cpp
        ${root}/core/rend/transform_matrix.cpp ${root}/core/rend/sorter.cpp
        ${root}/core/rend/CustomTexture.cpp ${root}/core/rend/custom_texture/CustomTextureTypes.cpp
        ${root}/core/rend/custom_texture/TextureTranscoder.cpp
        ${root}/core/rend/custom_texture/GpuTexturePreload.cpp
        ${root}/core/oslib/storage.cpp
        ${root}/core/stdclass.cpp
        ${root}/core/cfg/option.cpp ${root}/core/wsi/context.cpp
        ${root}/core/hw/pvr/pvr_mem.cpp ${root}/core/deps/xbrz/xbrz.cpp
        ${root}/core/hw/pvr/Renderer_if.cpp ${root}/core/hw/pvr/ta_ctx.cpp
        ${root}/core/hw/pvr/ta.cpp ${root}/core/hw/pvr/ta_util.cpp
        ${root}/core/rend/norend/norend.cpp
        ${root}/core/deps/imgui/imgui.cpp ${root}/core/deps/imgui/imgui_draw.cpp
        ${root}/core/deps/imgui/imgui_tables.cpp ${root}/core/deps/imgui/imgui_widgets.cpp
        ${root}/core/deps/imgui/misc/freetype/imgui_freetype.cpp)
    target_include_directories(${target} PRIVATE
        ${root}/core ${root}/core/deps ${root}/core/deps/glm ${root}/core/deps/stb
        ${root}/core/deps/nowide/include ${root}/core/deps/imgui ${root}/core/deps/SDL/include)
    target_compile_definitions(${target} PRIVATE
        USE_METAL TARGET_MAC NOWIDE_NO_LIB FLYCAST_METAL_STANDALONE METAL_RENDERER_TESTS
        IMGUI_DEFINE_MATH_OPERATORS IMGUI_DISABLE_DEBUG_TOOLS IMGUI_DISABLE_DEMO_WINDOWS)
    target_compile_definitions(${target} PRIVATE
        FLYCAST_METAL_TEST_FIXTURE_ROOT="${root}/core/deps/basis_universal/shader_deblocking_d3d11/bin")
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>" -mmacosx-version-min=26.0)
    target_link_options(${target} PRIVATE -mmacosx-version-min=26.0 -Wl,-dead_strip)
    target_link_libraries(${target} PRIVATE ${freetype_target} ${xxhash_target} flycast_basis_transcoder
        ${offscreen_metal} ${offscreen_foundation} ${offscreen_quartzcore}
        ${offscreen_appkit} ${offscreen_metalfx})
    set_target_properties(${target} PROPERTIES
        OSX_ARCHITECTURES "arm64" OSX_DEPLOYMENT_TARGET "26.0" XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "26.0"
        OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED ON)
    # Use the actual region/tile-clipping implementation. Only the raw parser
    # entry is renamed in this object; Process uses the explicit parsed-fixture
    # adapter in metal_offscreen_services.cpp. No app target inherits this define.
    add_library(${target}_ta OBJECT EXCLUDE_FROM_ALL ${root}/core/hw/pvr/ta_vtx.cpp)
    target_include_directories(${target}_ta PRIVATE $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>)
    target_compile_definitions(${target}_ta PRIVATE $<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>
        ta_parse=metalOffscreenRawTaParse)
    target_compile_features(${target}_ta PRIVATE cxx_std_17)
    set_target_properties(${target}_ta PROPERTIES OSX_ARCHITECTURES "arm64" OSX_DEPLOYMENT_TARGET "26.0")
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:${target}_ta>)
endfunction()
