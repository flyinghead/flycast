#include <linux/input.h>
#pragma once

struct s_evdev_controller_mapping
{
  const char* name;
  const int Btn_A;
  const int Btn_B;
  const int Btn_C;
  const int Btn_D;
  const int Btn_X;
  const int Btn_Y;
  const int Btn_Z;
  const int Btn_Start;
  const int Btn_Escape;
  const int Btn_DPad_Left;
  const int Btn_DPad_Right;
  const int Btn_DPad_Up;
  const int Btn_DPad_Down;
  const int Btn_DPad2_Left;
  const int Btn_DPad2_Right;
  const int Btn_DPad2_Up;
  const int Btn_DPad2_Down;
  const int Btn_Trigger_Left;
  const int Btn_Trigger_Right;
  const int Axis_DPad_X;
  const int Axis_DPad_Y;
  const int Axis_DPad2_X;
  const int Axis_DPad2_Y;
  const int Axis_Analog_X;
  const int Axis_Analog_Y;
  const int Axis_Trigger_Left;
  const int Axis_Trigger_Right;
};
typedef struct s_evdev_controller_mapping ControllerMapping;

extern ControllerMapping controller_mapping_generic;
extern ControllerMapping controller_mapping_keyboard;
extern ControllerMapping controller_mapping_xbox360;
extern ControllerMapping controller_mapping_gcwz;
extern ControllerMapping controller_mapping_pandora;