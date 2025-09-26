if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            game_scanner.cpp
            game_scanner.h
            imgui_driver.h
            gui.cpp
            gui.h
            gui_achievements.cpp
            vgamepad.cpp
            vgamepad.h
            gui_chat.h
            gui_cheats.cpp
            gui_util.cpp
            gui_util.h
            mainui.cpp
            mainui.h
            settings.cpp
            settings.h
            settings_about.cpp
            settings_audio.cpp
            settings_controls.cpp
            settings_general.cpp
            settings_network.cpp
            settings_video.cpp
            boxart/arcade_scraper.h
            boxart/boxart.cpp
            boxart/boxart.h
            boxart/gamesdb.cpp
            boxart/gamesdb.h
            boxart/pvrparser.h
            boxart/scraper.cpp
            boxart/scraper.h)

    if(USE_DISCORD)
        target_sources(${PROJECT_NAME} PRIVATE discord.cpp)
    endif()
endif()
