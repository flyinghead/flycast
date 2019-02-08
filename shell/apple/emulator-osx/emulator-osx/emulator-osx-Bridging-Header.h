//
//  osx-main-Bridging-Header.h
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#ifndef emulator_osx_osx_main_Bridging_Header_h
#define emulator_osx_osx_main_Bridging_Header_h
#include <MacTypes.h>

void emu_main();
void emu_dc_stop();
int emu_single_frame(int w, int h);
void emu_gles_init();
void emu_key_input(UInt16 keyCode, int state, UInt32 modifierFlags);
bool emu_frame_pending();
extern unsigned int mo_buttons;
extern int mo_x_abs;
extern int mo_y_abs;
#endif
