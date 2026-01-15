if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
    		dreampotato.cpp
    		dreampotato.h
            gamepad.h
            gamepad_device.cpp
            gamepad_device.h
            keyboard_device.h
            maplelink.cpp
            maplelink.h
            mapping.cpp
            mapping.h
            mouse.cpp
            mouse.h)
endif()
