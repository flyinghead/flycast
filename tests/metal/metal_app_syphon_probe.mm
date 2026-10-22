// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone consumer for a real running Flycast app. Does not launch apps or
// change their settings. Link against the project's unmodified Syphon library.
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <ImageIO/ImageIO.h>
#import <Syphon/SyphonMetalClient.h>
#import <Syphon/SyphonServerDirectory.h>
#include <cstdio>
#include <cstdint>
#include <vector>

int main(int argc, const char **argv)
{
    @autoreleasepool {
        if (argc != 2) { std::fprintf(stderr, "usage: metal_app_syphon_probe output.png\n"); return 2; }
        [NSApplication sharedApplication];
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) return 3;
        SyphonServerDirectory *directory = SyphonServerDirectory.sharedDirectory;
        SyphonMetalClient *client = nil;
        const double end = NSDate.timeIntervalSinceReferenceDate + 45;
        size_t frames = 0, changes = 0, width = 0, height = 0;
        uint64_t previousHash = 0;
        bool saved = false;
        while (NSDate.timeIntervalSinceReferenceDate < end && frames < 240) {
            @autoreleasepool {
                [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.005]];
                if (!client) {
                    for (NSDictionary *server in directory.servers) {
                        NSString *app = server[SyphonServerDescriptionAppNameKey];
                        NSString *name = server[SyphonServerDescriptionNameKey];
                        if (![app hasPrefix:@"Flycast"] || ![name isEqualToString:@"Video Content"]) continue;
                        client = [[SyphonMetalClient alloc] initWithServerDescription:server
                            device:device options:nil newFrameHandler:nil];
                        break;
                    }
                }
                if (!client.isValid || !client.hasNewFrame) continue;
                id<MTLTexture> image = [client newFrameImage];
                if (!image || image.pixelFormat != MTLPixelFormatBGRA8Unorm) continue;
                if (image.width > 16384 || image.height > 16384) return 4;
                width = image.width; height = image.height;
                std::vector<uint8_t> bytes(width * height * 4);
                [image getBytes:bytes.data() bytesPerRow:width * 4
                    fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
                // Samples establish changing transported contents, not a golden
                // image or a claim of tear-free cross-process frame ownership.
                uint64_t hash = 14695981039346656037ull;
                for (size_t i = 0; i < bytes.size(); i += 17) { hash ^= bytes[i]; hash *= 1099511628211ull; }
                if (frames && hash != previousHash) ++changes;
                previousHash = hash; ++frames;
                if (!saved && frames >= 3) {
                    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
                    CGContextRef context = CGBitmapContextCreate(bytes.data(), width, height, 8, width * 4,
                        color, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
                    CGImageRef bitmap = context ? CGBitmapContextCreateImage(context) : nullptr;
                    NSURL *url = [NSURL fileURLWithPath:@(argv[1])];
                    CGImageDestinationRef destination = bitmap ? CGImageDestinationCreateWithURL(
                        (__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr) : nullptr;
                    if (destination) {
                        CGImageDestinationAddImage(destination, bitmap, nullptr);
                        saved = CGImageDestinationFinalize(destination);
                        CFRelease(destination);
                    }
                    if (bitmap) CGImageRelease(bitmap);
                    if (context) CGContextRelease(context);
                    CGColorSpaceRelease(color);
                }
            }
        }
        [client stop];
        std::printf("{\"frames\":%zu,\"changed_samples\":%zu,\"width\":%zu,\"height\":%zu,\"png_saved\":%s}\n",
            frames, changes, width, height, saved ? "true" : "false");
        return frames >= 30 && saved ? 0 : 1;
    }
}
