target_sources(${PROJECT_NAME} PRIVATE
        BitSet.h
        Log.h
        StringUtil.h)

if(NOT LIBRETRO)
    target_sources(${PROJECT_NAME} PRIVATE
            ConsoleListener.h
            ConsoleListenerDroid.cpp
            ConsoleListenerNix.cpp
            ConsoleListenerWin.cpp
            LogManager.cpp
            LogManager.h)
endif()
