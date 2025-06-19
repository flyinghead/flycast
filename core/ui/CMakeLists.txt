if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/game_scanner.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/game_scanner.h
            ${CMAKE_CURRENT_SOURCE_DIR}/imgui_driver.h
            ${CMAKE_CURRENT_SOURCE_DIR}/gui.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/gui.h
            ${CMAKE_CURRENT_SOURCE_DIR}/gui_achievements.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/vgamepad.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/vgamepad.h
            ${CMAKE_CURRENT_SOURCE_DIR}/gui_chat.h
            ${CMAKE_CURRENT_SOURCE_DIR}/gui_cheats.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/gui_util.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/gui_util.h
            ${CMAKE_CURRENT_SOURCE_DIR}/mainui.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/mainui.h
            ${CMAKE_CURRENT_SOURCE_DIR}/boxart/boxart.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/boxart/boxart.h
            ${CMAKE_CURRENT_SOURCE_DIR}/boxart/gamesdb.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/boxart/gamesdb.h
            ${CMAKE_CURRENT_SOURCE_DIR}/boxart/scraper.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/boxart/scraper.h)

    if(USE_DISCORD)
        target_sources(${PROJECT_NAME} PRIVATE core/ui/discord.cpp)
    endif()
endif()
