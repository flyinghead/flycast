target_sources(${PROJECT_NAME} PRIVATE
		libnx_vmem.cpp
		stubs.c
		context_switch.S
		nswitch.h
		ucontext.h)
if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
		i18n.cpp
		keyboard.cpp
		switch_gamepad.h
		switch_main.cpp)
endif()