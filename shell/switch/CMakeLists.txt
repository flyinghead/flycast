target_sources(${PROJECT_NAME} PRIVATE
		libnx_vmem.cpp
		stubs.c
		context_switch.S
		nswitch.h
		ucontext.h)
if(NOT LIBRETRO)
	target_sources(${PROJECT_NAME} PRIVATE
		keyboard.cpp
		switch_gamepad.h
		switch_main.cpp)
endif()