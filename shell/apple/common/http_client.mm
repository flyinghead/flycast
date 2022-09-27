/*
	Copyright 2022 flyinghead

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
#include "rend/boxart/http_client.h"

namespace http {

int get(const std::string& url, std::vector<u8>& content, std::string& contentType)
{
	NSString *nsurl = [NSString stringWithCString:url.c_str() 
                                         encoding:[NSString defaultCStringEncoding]];
	NSURLRequest *urlRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:nsurl]];
	NSURLResponse *response = nil;
	NSError *error = nil;
	NSData *data = [NSURLConnection sendSynchronousRequest:urlRequest
                                         returningResponse:&response
                                                     error:&error];
	if (error != nil)
		return 500;

	NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response; 
	if (httpResponse.MIMEType != nil)
		contentType = std::string([httpResponse.MIMEType UTF8String]);
	else
		contentType.clear();
		
	content.clear();
	content.insert(content.begin(), (const u8 *)[data bytes], (const u8 *)[data bytes] + [data length]);
	
	return [httpResponse statusCode];
}

void init() {
}

void term() {
}

}