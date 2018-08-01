//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//
#import <Carbon/Carbon.h>

#include "types.h"
#include <sys/stat.h>

#include <OpenGL/gl3.h>

int msgboxf(const wchar* text,unsigned int type,...)
{
    va_list args;
    
    wchar temp[2048];
    va_start(args, type);
    vsprintf(temp, text, args);
    va_end(args);
    
    puts(temp);
    return 0;
}

int darw_printf(const wchar* text,...) {
    va_list args;
    
    wchar temp[2048];
    va_start(args, text);
    vsprintf(temp, text, args);
    va_end(args);
    
    NSLog(@"%s", temp);
    
    return 0;
}

u16 kcode[4] = { 0xFFFF };
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {
    
}


void UpdateInputState(u32 port) {
    
}

void UpdateVibration(u32 port, u32 value) {

}

void os_CreateWindow() {
    
}

void* libPvr_GetRenderTarget() {
    return 0;
}

void* libPvr_GetRenderSurface() {
    return 0;
    
}

bool gl_init(void*, void*) {
    return true;
}

void gl_term() {
    
}

void gl_swap() {
    
}

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();
void dc_term();
void dc_stop();

bool has_init = false;
void* emuthread(void*) {
    settings.profile.run_counts=0;
    string home = (string)getenv("HOME");
    if(home.c_str())
    {
        home += "/.reicast";
        mkdir(home.c_str(), 0755); // create the directory if missing
        set_user_config_dir(home);
        set_user_data_dir(home);
    }
    else
    {
        set_user_config_dir(".");
        set_user_data_dir(".");
    }
    common_linux_setup();
    char* argv[] = { "reicast" };
    
    dc_init(1,argv);
    
    has_init = true;
    
    dc_run();
    
    has_init = false;
    
    dc_term();

    return 0;
}

extern "C" void emu_dc_stop()
{
    dc_stop();
}

pthread_t emu_thread;
extern "C" void emu_main() {
    pthread_create(&emu_thread, 0, &emuthread, 0);
}

extern int screen_width,screen_height;
bool rend_single_frame();
bool rend_framePending();
bool gles_init();

extern "C" int emu_single_frame(int w, int h) {
    if (!has_init)
        return true;
    if (!rend_framePending())
        return 0;
    screen_width = w;
    screen_height = h;

    return rend_single_frame();
}

extern "C" void emu_gles_init() {
    gles_init();
}

extern "C" bool emu_frame_pending()
{
    return rend_framePending();
}

enum DCPad {
    Btn_C		= 1,
    Btn_B		= 1<<1,
    Btn_A		= 1<<2,
    Btn_Start	= 1<<3,
    DPad_Up		= 1<<4,
    DPad_Down	= 1<<5,
    DPad_Left	= 1<<6,
    DPad_Right	= 1<<7,
    Btn_Z		= 1<<8,
    Btn_Y		= 1<<9,
    Btn_X		= 1<<10,
    Btn_D		= 1<<11,
    DPad2_Up	= 1<<12,
    DPad2_Down	= 1<<13,
    DPad2_Left	= 1<<14,
    DPad2_Right	= 1<<15,
    
    Axis_LT= 0x10000,
    Axis_RT= 0x10001,
    Axis_X= 0x20000,
    Axis_Y= 0x20001,
};

static void handle_key(int dckey, int state) {
    if (state)
        kcode[0] &= ~dckey;
    else
        kcode[0] |= dckey;
}

static void handle_trig(u8* dckey, int state) {
    if (state)
        dckey[0] = 255;
    else
        dckey[0] = 0;
}

extern "C" void emu_key_input(UInt16 keyCode, int state) {
    switch(keyCode) {
        // Z
        case 0x06:     handle_key(Btn_X, state); break;
        // X
        case 0x07:     handle_key(Btn_Y, state); break;
        // C
        case 0x08:     handle_key(Btn_B, state); break;
        // V
        case 0x09:     handle_key(Btn_A, state); break;

        // A
        case 0x00:     handle_trig(lt, state); break;
        // S
        case 0x01:     handle_trig(rt, state); break;

        // Left arrow
        case 0x7b:     handle_key(DPad_Left, state); break;
        // Down arrow
        case 0x7d:     handle_key(DPad_Down, state); break;
        // Right arrow
        case 0x7c:     handle_key(DPad_Right, state); break;
        // Up arrow
        case 0x7e:     handle_key(DPad_Up, state); break;
        // Enter
        case 0x24:     handle_key(Btn_Start, state); break;
    }
}
