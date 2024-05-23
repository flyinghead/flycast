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
#include "types.h"
#include <vector>
#include <string>

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

std::string os_Locale(){
    return [[[NSLocale preferredLanguages] objectAtIndex:0] UTF8String];
}

std::string os_PrecomposedString(std::string string){
    return [[[NSString stringWithUTF8String:string.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}

namespace hostfs
{

void saveScreenshot(const std::string& name, const std::vector<u8>& data)
{
	NSData* imageData = [NSData dataWithBytes:&data[0] length:data.size()];
	UIImage* pngImage = [UIImage imageWithData:imageData];
	UIImageWriteToSavedPhotosAlbum(pngImage, nil, nil, nil);
}

}
