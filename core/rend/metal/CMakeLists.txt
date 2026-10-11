target_compile_definitions(${PROJECT_NAME} PRIVATE USE_METAL)
option(METAL_RENDERER_TESTS "Run ROM-free Metal renderer validation on startup" OFF)
if(METAL_RENDERER_TESTS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE METAL_RENDERER_TESTS)
endif()
option(METAL_FRAME_CAPTURE_TESTS "Capture full Metal frames after loading an isolated test state" OFF)
if(METAL_FRAME_CAPTURE_TESTS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FLYCAST_FRAME_CAPTURE_TESTS)
endif()
target_sources(${PROJECT_NAME} PRIVATE
    metal_commands.h metal_commands.mm
    metal_context.h metal_context.mm
    metal_texture.h metal_texture.mm
    metal_pipeline_cache.h metal_pipeline_cache.mm
    metal_fx.h metal_fx.mm
    metal_shaders.h metal_shader_options.h metal_modifier_clip.h metal_renderer.mm)
set_source_files_properties(metal_commands.mm metal_context.mm metal_texture.mm metal_renderer.mm metal_pipeline_cache.mm metal_fx.mm
    TARGET_DIRECTORY ${PROJECT_NAME}
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc" SKIP_PRECOMPILE_HEADERS ON)
find_library(FLYCAST_METAL_FRAMEWORK Metal REQUIRED)
find_library(FLYCAST_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(FLYCAST_METALFX_FRAMEWORK MetalFX REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${FLYCAST_METAL_FRAMEWORK} ${FLYCAST_QUARTZCORE_FRAMEWORK})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FLYCAST_METALFX_FRAMEWORK})
find_library(FLYCAST_FOUNDATION_FRAMEWORK Foundation REQUIRED)
add_executable(flycast_metal_gpu_tests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/metal/metal_gpu_tests.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/metal_commands.mm)
target_include_directories(flycast_metal_gpu_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/core/deps/glm)
target_compile_features(flycast_metal_gpu_tests PRIVATE cxx_std_17)
target_compile_definitions(flycast_metal_gpu_tests PRIVATE FLYCAST_METAL_STANDALONE)
target_compile_options(flycast_metal_gpu_tests PRIVATE -fobjc-arc -mmacosx-version-min=26.0)
target_link_options(flycast_metal_gpu_tests PRIVATE -mmacosx-version-min=26.0)
set_target_properties(flycast_metal_gpu_tests PROPERTIES XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "26.0")
target_link_libraries(flycast_metal_gpu_tests PRIVATE
    ${FLYCAST_METAL_FRAMEWORK} ${FLYCAST_FOUNDATION_FRAMEWORK})
add_executable(flycast_metal_presentation_tests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/metal/metal_presentation_timing_tests.cpp)
target_include_directories(flycast_metal_presentation_tests PRIVATE ${CMAKE_SOURCE_DIR}/core)
target_compile_features(flycast_metal_presentation_tests PRIVATE cxx_std_17)
add_executable(flycast_metal_modifier_clip_tests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/metal/metal_modifier_clip_tests.cpp)
target_include_directories(flycast_metal_modifier_clip_tests PRIVATE ${CMAKE_SOURCE_DIR}/core)
target_compile_features(flycast_metal_modifier_clip_tests PRIVATE cxx_std_17)
