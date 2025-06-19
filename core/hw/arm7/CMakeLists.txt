target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/arm7.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/arm7.h
        ${CMAKE_CURRENT_SOURCE_DIR}/arm_mem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/arm_mem.h
        ${CMAKE_CURRENT_SOURCE_DIR}/arm7_rec_arm32.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/arm7_rec_arm64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/arm7_rec_x64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/arm7_rec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/arm7_rec.h
        ${CMAKE_CURRENT_SOURCE_DIR}/arm-new.h)
