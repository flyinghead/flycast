#include "linux-dist/evdev_mappings.h"

#if defined(USE_EVDEV)

	ControllerMapping controller_mapping_generic = {
		"Generic Controller",
		0x130, // BTN_A
		0x131, // BTN_B
		0x132, // BTN_C
		0x13d, // BTN_THUMBL
		0x133, // BTN_X
		0x134, // BTN_Y
		0x135, // BTN_Z
		0x13b, // BTN_START
		0x13a, // BTN_SELECT
		0x220, // BTN_DPAD_LEFT
		0x221, // BTN_DPAD_RIGHT
		0x222, // BTN_DPAD_UP
		0x223, // BTN_DPAD_DOWN
		-1,
		-1,
		-1,
		-1,
		0x136, // BTN_TL
		0x137, // BTN_TR
		0x10,  // ABS_HAT0X
		0x11,  // ABS_HAT0Y
		0x12,  // ABS_HAT1X
		0x13,  // ABS_HAT1Y
		0x00,  // ABS_X
		0x01,  // ABS_Y
		0x02,  // ABS_Z
		0x05,  // ABS_RZ
	};

	ControllerMapping controller_mapping_keyboard = {
		"Generic Keyboard",
		30,  // KEY_A
		48,  // KEY_B
		46,  // KEY_C
		32,  // KEY_D
		45,  // KEY_X
		21,  // KEY_Y
		44,  // KEY_Z
		28,  // KEY_ENTER
		1,   // KEY_ESC
		105, // KEY_LEFT
		106, // KEY_RIGHT
		103, // KEY_UP
		108, // KEY_DOWN
		-1,
		-1,
		-1,
		-1,
		29,  // KEY_LEFTCTRL
		97,  // KEY_RIGHTCTRL
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1
	};

	ControllerMapping controller_mapping_xpad = {
		"Xbox 360 Controller (xpad driver)",
		0x130, // BTN_A
		0x131, // BTN_B
		0x136, // BTN_TL
		0x137, // BTN_TR
		0x133, // BTN_X
		0x134, // BTN_Y
		0x13d, // BTN_THUMBL
		0x13b, // BTN_START
		0x13a, // BTN_SELECT
		0x2c0, // BTN_TRIGGER_HAPPY1
		0x2c1, // BTN_TRIGGER_HAPPY2
		0x2c2, // BTN_TRIGGER_HAPPY3
		0x2c3, // BTN_TRIGGER_HAPPY4
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		0x10,  // ABS_HAT0X
		0x11,  // ABS_HAT0Y
		0x12,  // ABS_HAT1X
		0x13,  // ABS_HAT1Y
		0x00,  // ABS_X
		0x01,  // ABS_Y
		0x02,  // ABS_Z
		0x05,  // ABS_RZ
	};

	ControllerMapping controller_mapping_xboxdrv = {
		"Xbox 360 Controller (xboxdrv userspace driver)",
		0x130, // BTN_A
		0x131, // BTN_B
		0x136, // BTN_TL
		0x137, // BTN_TR
		0x133, // BTN_X
		0x134, // BTN_Y
		0x13d, // BTN_THUMBL
		0x13b, // BTN_START
		0x13a, // BTN_SELECT
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		0x10,  // ABS_HAT0X
		0x11,  // ABS_HAT0Y
		0x12,  // ABS_HAT1X
		0x13,  // ABS_HAT1Y
		0x00,  // ABS_X
		0x01,  // ABS_Y
		0x0a,  // ABS_BRAKE
		0x09,  // ABS_GAS
	};

	ControllerMapping controller_mapping_gcwz = {
		"GCW Zero",
		0x1D, // GCWZ_BTN_A
		0x38, // GCWZ_BTN_B
		0x0F, // GCWZ_BTN_L
		0x0E, // GCWZ_BTN_R
		0x2A, // GCWZ_BTN_X
		0x39, // GCWZ_BTN_Y
		-1,
		0x1C, // GCWZ_BTN_START
		0x01, // GCWZ_BTN_SELECT
		0x69, // GCWZ_BTN_LEFT
		0x6A, // GCWZ_BTN_RIGHT
		0x67, // GCWZ_BTN_UP
		0x6C, // GCWZ_BTN_DOWN
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1
	};

	ControllerMapping controller_mapping_pandora = {
		"Pandora",
		109, // KEY_PAGEDOWN
		107, // KEY_END
		57,  // KEY_SPACE
		-1,
		102, // KEY_HOME
		104, // KEY_PAGEUP
		-1,
		56,  // KEY_LEFTALT
		139, // KEY_MENU,
		105, // KEY_LEFT
		106, // KEY_RIGHT
		103, // KEY_UP
		108, // KEY_DOWN
		-1,
		-1,
		-1,
		-1,
		54,  // KEY_RIGHTSHIFT
		97,  // KEY_RIGHTCTRL
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1
	};

#endif
