target_sources(${PROJECT_NAME} PRIVATE
        hikaru.cpp
        hikaru.h
        hikaru_cp.cpp
        hikaru_cp.h
        hikaru_gpu.cpp
        hikaru_gpu.h
        hikaru_render.cpp
        hikaru_render.h
        hikaru_drawlist.h)
target_sources(${PROJECT_NAME} PRIVATE
        hikaru_mie.cpp
        hikaru_mie.h
        hikaru_slave.cpp
        hikaru_slave.h
        ${CMAKE_SOURCE_DIR}/core/deps/z80/z80.c
        ${CMAKE_SOURCE_DIR}/core/deps/z80/z80.h)
