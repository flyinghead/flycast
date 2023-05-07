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

@interface NSURLSessionHandler:NSObject <NSURLSessionDataDelegate, NSURLSessionDelegate, NSURLSessionTaskDelegate>
- (void)setSemaphore:(dispatch_semaphore_t)sema;
- (void)setContent:(std::vector<u8>&)content;
- (NSHTTPURLResponse*)httpResponse;
- (NSError*)httpError;
@end
@implementation NSURLSessionHandler
{
	dispatch_semaphore_t _sema;
	NSHTTPURLResponse* _httpResponse;
	NSError* _httpError;
	std::vector<u8>* _content;
}

- (void)setSemaphore:(dispatch_semaphore_t)sema { _sema = sema; }

- (void)setContent:(std::vector<u8>&)content { _content = &content; }

- (NSHTTPURLResponse*)httpResponse { return _httpResponse; }

- (NSError*)httpError { return _httpError; }
	
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler {
	completionHandler(NSURLSessionResponseAllow);
	_httpResponse = (NSHTTPURLResponse *)response;
}
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data {
	_content->insert(_content->end(), (const u8 *)[data bytes], (const u8 *)[data bytes] + [data length]);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
didCompleteWithError:(nullable NSError *)error {
	_httpError = error;
	dispatch_semaphore_signal(_sema);
}

@end

namespace http {

int get(const std::string& url, std::vector<u8>& content, std::string& contentType)
{
	NSString *nsurl = [NSString stringWithCString:url.c_str() 
                                         encoding:[NSString defaultCStringEncoding]];
	NSURLSessionHandler* handler = [[NSURLSessionHandler alloc] init];
	content.clear();
	[handler setContent:content];
	NSURLSessionConfiguration *defaultConfigObject = [NSURLSessionConfiguration defaultSessionConfiguration];
	NSURLSession *defaultSession = [NSURLSession sessionWithConfiguration: defaultConfigObject delegate: handler delegateQueue: [NSOperationQueue mainQueue]];
	NSURLSessionDataTask *dataTask = [defaultSession dataTaskWithURL: [NSURL URLWithString:nsurl]];
	
	dispatch_semaphore_t sema = dispatch_semaphore_create(0);
	[handler setSemaphore:sema];
	
	[dataTask resume];
	
	if (![NSThread isMainThread]) {
		dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
	} else {
		while (dispatch_semaphore_wait(sema, DISPATCH_TIME_NOW)) {
			[[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0]];
		}
	}
	
	if ([handler httpError] != nil)
		return 500;
	
	if ([handler httpResponse].MIMEType != nil)
		contentType = std::string([[handler httpResponse].MIMEType UTF8String]);
	else
		contentType.clear();
	
	return [[handler httpResponse] statusCode];
}

int post(const std::string& url, const std::vector<PostField>& fields)
{
	NSString *nsurl = [NSString stringWithCString:url.c_str() 
                                         encoding:[NSString defaultCStringEncoding]];
	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:nsurl]];
	[request setHTTPMethod:@"POST"];
	[request setCachePolicy:NSURLRequestReloadIgnoringLocalCacheData];
	[request setHTTPShouldHandleCookies:NO];

	NSString *boundary = @"----flycast-boundary-7192397596";
	NSString *contentType = [NSString stringWithFormat:@"multipart/form-data; boundary=%@", boundary];
	[request setValue:contentType forHTTPHeaderField: @"Content-Type"];
	
	NSMutableData *body = [NSMutableData data];
	for (const PostField& field : fields)
	{
        NSString *value = [NSString stringWithCString:field.value.c_str() 
                                         encoding:[NSString defaultCStringEncoding]];
		[body appendData:[[NSString stringWithFormat:@"--%@\r\n", boundary] dataUsingEncoding:NSUTF8StringEncoding]];
		[body appendData:[[NSString stringWithFormat:@"Content-Disposition: form-data; name=\"%@\"", [NSString stringWithCString:field.name.c_str() 
                                         encoding:[NSString defaultCStringEncoding]]] dataUsingEncoding:NSUTF8StringEncoding]];
        if (!field.contentType.empty())
        {
	        [body appendData:[[NSString stringWithFormat:@"; filename=\"%@\"\r\n", value] dataUsingEncoding:NSUTF8StringEncoding]];
	        [body appendData:[[NSString stringWithFormat:@"Content-Type: %@", [NSString stringWithCString:field.contentType.c_str() 
                                         encoding:[NSString defaultCStringEncoding]]] dataUsingEncoding:NSUTF8StringEncoding]];
        }
        [body appendData:[@"\r\n\r\n" dataUsingEncoding:NSUTF8StringEncoding]];
        
        if (field.contentType.empty())
        {
	        [body appendData:[value dataUsingEncoding:NSUTF8StringEncoding]];
        }
        else
        {
        	NSError* error = nil;
			NSData *filedata = [NSData dataWithContentsOfFile:value options:0 error:&error];
			if (error != nil)
				return 500;
	        [body appendData:filedata];
        }
        [body appendData:[@"\r\n" dataUsingEncoding:NSUTF8StringEncoding]];
	}
	[body appendData:[[NSString stringWithFormat:@"--%@--\r\n", boundary] dataUsingEncoding:NSUTF8StringEncoding]];
	
	[request setHTTPBody:body];
	NSString *postLength = [NSString stringWithFormat:@"%ld", (unsigned long)[body length]];
    [request setValue:postLength forHTTPHeaderField:@"Content-Length"];
    
	NSURLResponse *response = nil;
	NSError *error = nil;
	[NSURLConnection sendSynchronousRequest:request
                          returningResponse:&response
                                      error:&error];
	if (error != nil)
		return 500;

	NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response; 
	return [httpResponse statusCode];
}

void init() {
}

void term() {
}

}
