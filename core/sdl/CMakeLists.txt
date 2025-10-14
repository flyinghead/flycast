target_sources(${PROJECT_NAME} PRIVATE
        sdl.cpp
        sdl.h
        sdl_gamepad.cpp
        sdl_gamepad.h
        sdl_keyboard.h
        sdl_keyboard_mac.h
        sdl_mappingparser.h)

if (USE_DREAMLINK_DEVICES)
target_sources(${PROJECT_NAME} PRIVATE
        dreamlink.cpp
        dreamlink.h
        dreampicoport.cpp
        dreampicoport.h
        dreamconn.cpp
        dreamconn.h)
endif()
