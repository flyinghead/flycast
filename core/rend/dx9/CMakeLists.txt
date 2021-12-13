project(dx9renderer)

add_library(${PROJECT_NAME} STATIC dxcontext.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<BOOL:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
        $<$<BOOL:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<BOOL:MSVC>:_USE_MATH_DEFINES>
        $<$<BOOL:MSVC>:NOMINMAX>
        $<$<BOOL:${TEST_AUTOMATION}>:TEST_AUTOMATION>
        $<$<BOOL:${ENABLE_LOG}>:DEBUGFAST>
        USE_SDL)

target_sources(${PROJECT_NAME} PRIVATE
	d3d_overlay.h
	d3d_overlay.cpp
	d3d_renderer.h
	d3d_renderer.cpp
	d3d_shaders.h
	d3d_shaders.cpp
	d3d_texture.h
	d3d_texture.cpp
	dx9_driver.h
	dxcontext.h
	imgui_impl_dx9.h
	imgui_impl_dx9.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
	../..
	../../deps
	../../deps/nowide/include
	../../deps/glm
	../../deps/SDL/include)
if(NOT MINGW)
	target_include_directories(${PROJECT_NAME} PRIVATE "$ENV{DXSDK_DIR}/Include")
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		target_link_directories(${PROJECT_NAME} PUBLIC "$ENV{DXSDK_DIR}/Lib/x64")
	else()
		target_link_directories(${PROJECT_NAME} PUBLIC "$ENV{DXSDK_DIR}/Lib/x86")
	endif()
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC d3d9 d3dx9)
