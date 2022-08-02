/*
	Copyright 2021 flyinghead
	Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.

	This file is part of Flycast.

	Flycast is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	Flycast is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#import <Foundation/Foundation.h>

#include <string>
#include <mach/task.h>
#include <mach/mach_init.h>

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
    if (getppid() != 1) {
        /* Make LLDB ignore EXC_BAD_ACCESS for debugging */
        task_set_exception_ports(mach_task_self(), EXC_MASK_BAD_ACCESS, MACH_PORT_NULL, EXCEPTION_DEFAULT, 0);
    }
}

void UpdateInputState() {
}

void os_SetupInput() {
}
void os_TermInput() {
}

std::string os_Locale(){
    return [[[NSLocale preferredLanguages] objectAtIndex:0] UTF8String];
}

std::string os_PrecomposedString(std::string string){
    return [[[NSString stringWithUTF8String:string.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}
