//
//  Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.
//

#import <Foundation/Foundation.h>

#include "emulator.h"
#include "log/LogManager.h"
#include "rend/gui.h"

int darw_printf(const char* text,...)
{
    va_list args;

    char temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

void os_DoEvents() {
}

void os_SetWindowText(const char* t) {
}

void os_CreateWindow() {
}

void UpdateInputState() {
}

std::string os_Locale(){
    return [[[NSLocale preferredLanguages] objectAtIndex:0] UTF8String];
}

std::string os_PrecomposedString(std::string string){
    return [[[NSString stringWithUTF8String:string.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}

extern "C" void emu_dc_term(void)
{
	if (dc_is_running())
		dc_exit();
	dc_term();
	LogManager::Shutdown();
}

extern "C" void emu_gui_open(void)
{
	if (!gui_is_open())
		gui_open_settings();
}
