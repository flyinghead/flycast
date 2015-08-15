#include "linux-dist/evdev_mappings.h"

#if defined(USE_EVDEV)

  ControllerMapping controller_mapping_generic = {
    "Generic Controller",
    BTN_A,
    BTN_B,
    BTN_C,
    BTN_THUMBL,
    BTN_X,
    BTN_Y,
    BTN_Z,
    BTN_START,
    BTN_SELECT,
    BTN_DPAD_LEFT,
    BTN_DPAD_RIGHT,
    BTN_DPAD_UP,
    BTN_DPAD_DOWN,
    -1,
    -1,
    -1,
    -1,
    BTN_TL,
    BTN_TR,
    ABS_HAT0X,
    ABS_HAT0Y,
    ABS_HAT1X,
    ABS_HAT1Y,
    ABS_X,
    ABS_Y,
    ABS_Z,
    ABS_RZ,
  };

  ControllerMapping controller_mapping_xbox360 = {
    "XBox360 Controller",
    BTN_A,
    BTN_B,
    BTN_TL,
    BTN_TR,
    BTN_X,
    BTN_Y,
    BTN_THUMBL,
    BTN_START,
    BTN_SELECT,
    BTN_TRIGGER_HAPPY1,
    BTN_TRIGGER_HAPPY2,
    BTN_TRIGGER_HAPPY3,
    BTN_TRIGGER_HAPPY4,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    ABS_HAT0X,
    ABS_HAT0Y,
    ABS_HAT1X,
    ABS_HAT1Y,
    ABS_X,
    ABS_Y,
    ABS_Z,
    ABS_RZ
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
    KEY_PAGEDOWN,
    KEY_END,
    KEY_SPACE,
    -1,
    KEY_HOME,
    KEY_PAGEUP,
    -1,
    KEY_LEFTALT,
    KEY_MENU,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    -1,
    -1,
    -1,
    -1,
    KEY_RIGHTSHIFT,
    KEY_RIGHTCTRL,
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