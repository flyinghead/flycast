# Clang applies this definition to the arm64 compilation only, including PCHs.
# CMake's target definitions alone would enable Metal in both universal slices.
target_compile_options(${PROJECT_NAME} PRIVATE
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:SHELL:-Xarch_arm64 -DUSE_METAL>")
option(METAL_RENDERER_TESTS "Run ROM-free Metal renderer validation on startup" OFF)
if(METAL_RENDERER_TESTS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE METAL_RENDERER_TESTS)
endif()
option(METAL_FRAME_CAPTURE_TESTS "Capture full Metal frames after loading an isolated test state" OFF)
if(METAL_FRAME_CAPTURE_TESTS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FLYCAST_FRAME_CAPTURE_TESTS)
endif()
target_sources(${PROJECT_NAME} PRIVATE
    metal4_support.h metal4_support.mm
    metal4_commands.h metal4_commands.mm
    metal4_context.h metal4_context.mm
    metal4_texture.h metal4_texture.mm
    metal4_pipeline_cache.h metal4_pipeline_cache.mm
    metal4_fx.h metal4_fx.mm
    metal4_syphon.h metal4_syphon.mm
    metal4_shaders.h metal4_shader_options.h metal4_modifier_clip.h metal4_renderer.mm)
set_source_files_properties(metal4_support.mm metal4_commands.mm metal4_context.mm metal4_texture.mm metal4_renderer.mm metal4_pipeline_cache.mm metal4_fx.mm metal4_syphon.mm
    TARGET_DIRECTORY ${PROJECT_NAME}
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc;-Werror=unguarded-availability;-Werror=unguarded-availability-new" SKIP_PRECOMPILE_HEADERS ON)
find_library(FLYCAST_METAL_FRAMEWORK Metal REQUIRED)
find_library(FLYCAST_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(FLYCAST_METALFX_FRAMEWORK MetalFX REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${FLYCAST_METAL_FRAMEWORK} ${FLYCAST_QUARTZCORE_FRAMEWORK})
# MetalFX does not exist on Catalina. A weak framework load lets the Intel
# slice launch there and the arm64 slice use the guarded Metal 4 scaler.
target_link_libraries(${PROJECT_NAME} PRIVATE "$<LINK_LIBRARY:WEAK_FRAMEWORK,${FLYCAST_METALFX_FRAMEWORK}>")
find_library(FLYCAST_FOUNDATION_FRAMEWORK Foundation REQUIRED)
add_executable(flycast_metal_gpu_tests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/metal/metal_gpu_tests.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/metal4_commands.mm)
target_include_directories(flycast_metal_gpu_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/core/deps/glm)
target_compile_features(flycast_metal_gpu_tests PRIVATE cxx_std_17)
target_compile_definitions(flycast_metal_gpu_tests PRIVATE FLYCAST_METAL_STANDALONE)
target_compile_options(flycast_metal_gpu_tests PRIVATE -fobjc-arc -mmacosx-version-min=26.0)
target_link_options(flycast_metal_gpu_tests PRIVATE -mmacosx-version-min=26.0)
set_target_properties(flycast_metal_gpu_tests PROPERTIES
    OSX_ARCHITECTURES "arm64"
    OSX_DEPLOYMENT_TARGET "26.0"
    XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "26.0")
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
