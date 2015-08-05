//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

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


u16 kcode[4];
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

int dc_init(int argc,wchar* argv[]);
void dc_run();

bool has_init = false;
void* emuthread(void*) {
    settings.profile.run_counts=0;
    string home = (string)getenv("HOME");
    if(home.c_str())
    {
        home += "/.reicast";
        mkdir(home.c_str(), 0755); // create the directory if missing
        SetHomeDir(home);
    }
    else
        SetHomeDir(".");
    char* argv[] = { "reicast" };
    
    dc_init(1,argv);
    
    has_init = true;
    
    dc_run();
    
    return 0;
}

pthread_t emu_thread;
extern "C" void emu_main() {
    pthread_create(&emu_thread, 0, &emuthread, 0);
}

extern int screen_width,screen_height;
bool rend_single_frame();
bool gles_init();

extern "C" bool emu_single_frame(int w, int h) {
    if (!has_init)
        return true;
    screen_width = w;
    screen_height = h;
    return rend_single_frame();
}

extern "C" void emu_gles_init() {
    gles_init();
}