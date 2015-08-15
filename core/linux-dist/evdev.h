#include "types.h"
#include "linux-dist/evdev_mappings.h"

#pragma once

struct s_controller
{
  int fd;
  ControllerMapping* mapping;
};

typedef struct s_controller Controller;

#define EVDEV_DEVICE_STRING "/dev/input/event%d"

#ifdef TARGET_PANDORA
  #define EVDEV_DEFAULT_DEVICE_ID_1 4
#else
  #define EVDEV_DEFAULT_DEVICE_ID_1 0
#endif

#define EVDEV_DEFAULT_DEVICE_ID(port) (port == 1 ? EVDEV_DEFAULT_DEVICE_ID_1 : -1)

extern int input_evdev_init(Controller* controller, const char* device);
extern bool input_evdev_handle(Controller* controller, u32 port);