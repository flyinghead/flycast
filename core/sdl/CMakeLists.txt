target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/sdl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sdl.h
        ${CMAKE_CURRENT_SOURCE_DIR}/sdl_gamepad.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sdl_gamepad.h
        ${CMAKE_CURRENT_SOURCE_DIR}/sdl_keyboard.h
        ${CMAKE_CURRENT_SOURCE_DIR}/sdl_keyboard_mac.h
        ${CMAKE_CURRENT_SOURCE_DIR}/dreamlink.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dreamlink.h
        ${CMAKE_CURRENT_SOURCE_DIR}/dreampicoport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dreampicoport.h
        ${CMAKE_CURRENT_SOURCE_DIR}/dreamconn.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dreamconn.h)
