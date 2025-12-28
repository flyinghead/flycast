option(BUILD_GMOCK "Builds the googlemock subproject" OFF)
option(INSTALL_GTEST "Enable installation of googletest. (Projects embedding googletest may want to turn this OFF.)" OFF)
if(WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest_main)
        
target_include_directories(${PROJECT_NAME} PUBLIC inc)
target_sources(${PROJECT_NAME} PRIVATE
        src/CheatManagerTest.cpp
        src/ConfigFileTest.cpp
        src/div32_test.cpp
        src/test_stubs.cpp
        src/serialize_test.cpp
        src/AicaArmTest.cpp
        src/Sh4InterpreterTest.cpp
        src/MmuTest.cpp
        src/HttpTest.cpp
        src/IniFileTest.cpp
        src/hw/modem/v42Test.cpp
        src/hw/modem/v42bisTest.cpp
        src/imgread/CueTest.cpp
        src/imgread/GdiTest.cpp
        src/input/ButtonComboTest.cpp
        src/input/GamepadInputHandlingTest.cpp
        src/input/MultiBindMappingTest.cpp
        src/input/InputDefTest.cpp
        src/input/InputMappingConfigFileTest.cpp
        src/input/InputSetTest.cpp
        src/input/SDLControllerMappingTest.cpp
        src/oslib/I18nTest.cpp
        src/util/PeriodicThreadTest.cpp
        src/util/TsQueueTest.cpp
        src/util/WorkerThreadTest.cpp)
