#pragma once
#include <linux/input.h>
#include "types.h"

struct EvdevControllerMapping
{
	const char* name;
	int Btn_A;
	int Btn_B;
	int Btn_C;
	int Btn_D;
	int Btn_X;
	int Btn_Y;
	int Btn_Z;
	int Btn_Start;
	int Btn_Escape;
	int Btn_DPad_Left;
	int Btn_DPad_Right;
	int Btn_DPad_Up;
	int Btn_DPad_Down;
	int Btn_DPad2_Left;
	int Btn_DPad2_Right;
	int Btn_DPad2_Up;
	int Btn_DPad2_Down;
	int Btn_Trigger_Left;
	int Btn_Trigger_Right;
	int Axis_DPad_X;
	int Axis_DPad_Y;
	int Axis_DPad2_X;
	int Axis_DPad2_Y;
	int Axis_Analog_X;
	int Axis_Analog_Y;
	int Axis_Trigger_Left;
	int Axis_Trigger_Right;
	bool Axis_Analog_X_Inverted;
	bool Axis_Analog_Y_Inverted;
	bool Axis_Trigger_Left_Inverted;
	bool Axis_Trigger_Right_Inverted;
};

struct EvdevAxisData
{
	s32 range; // smaller size than 32 bit might cause integer overflows
	s32 min;
	void init(int fd, int code, bool inverted);
	s8 convert(int value);
};

struct EvdevController
{
	int fd;
	EvdevControllerMapping* mapping;
	EvdevAxisData data_x;
	EvdevAxisData data_y;
	EvdevAxisData data_trigger_left;
	EvdevAxisData data_trigger_right;
	void init();
};

#define EVDEV_DEVICE_CONFIG_KEY "evdev_device_id_%d"
#define EVDEV_MAPPING_CONFIG_KEY "evdev_mapping_%d"
#define EVDEV_DEVICE_STRING "/dev/input/event%d"
#define EVDEV_MAPPING_PATH "/mappings/%s"

#ifdef TARGET_PANDORA
	#define EVDEV_DEFAULT_DEVICE_ID_1 4
#else
	#define EVDEV_DEFAULT_DEVICE_ID_1 0
#endif

#define EVDEV_DEFAULT_DEVICE_ID(port) (port == 1 ? EVDEV_DEFAULT_DEVICE_ID_1 : -1)

extern int input_evdev_init(EvdevController* controller, const char* device, const char* mapping_fname);
extern bool input_evdev_handle(EvdevController* controller, u32 port);
