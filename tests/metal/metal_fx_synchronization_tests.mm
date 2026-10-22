// SPDX-License-Identifier: GPL-2.0-or-later
// Actual production MetalFX bridge with production command synchronization.
#include "rend/metal4/metal4_fx.h"
#include "log/Log.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
void GenericLog(LogTypes::LOG_LEVELS,LogTypes::LOG_TYPE,const char*,int,const char *format,...)
{va_list args;va_start(args,format);std::vfprintf(stderr,format,args);std::fputc('\n',stderr);va_end(args);}
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void error(NSError *e){if(e)throw std::runtime_error(e.localizedDescription.UTF8String);}
int main() {
 @autoreleasepool {try {
    auto device=MTLCreateSystemDefaultDevice();require(device!=nil,"No GPU");NSError *failure=nil;
    auto compiler=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&failure];error(failure);
    std::vector<std::vector<uint8_t>> reference;unsigned frames=0,cached=0;
    for(bool full:{true,false}) {
        auto queue=[[FCMetal4Queue alloc] initWithDevice:device];MetalFXUpscaler fx(device,compiler);
        require(fx.supported(),"Metal 4 FX unsupported; test cannot claim FX acceptance");
        for(unsigned i=0;i<8;++i) {
            const NSUInteger size=i<4?16:20,width=i%3==0?size*3:size*2,height=size*2;
            auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:size height:size mipmapped:NO];
            d.storageMode=(i&1)?MTLStorageModeShared:MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
            auto input=[device newTextureWithDescriptor:d];require(input!=nil,"Cannot create FX input");
            const uint8_t color[]={uint8_t(31+i*9),uint8_t(97+i*5),uint8_t(173-i*7),255};
            auto source=[device newBufferWithLength:256*size options:MTLResourceStorageModeShared];
            for(NSUInteger offset=0;offset<source.length;offset+=4)std::memcpy(static_cast<uint8_t *>(source.contents)+offset,color,4);
            auto cb=[queue commandBuffer];[cb setFullSynchronizationForTesting:full];
            auto copy=[cb transferEncoder];[copy copyFromBuffer:source sourceOffset:0 sourceBytesPerRow:256 sourceBytesPerImage:source.length
                sourceSize:MTLSizeMake(size,size,1) toTexture:input destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[copy endEncoding];
            const size_t slot=i%MetalContext::PresentationSlots;
            auto output=fx.upscale(input,cb,width,height,slot,i+1);require(output!=nil && output.width==width && output.height==height,"Production FX bridge failed to encode");
            auto repeated=fx.upscale(input,cb,width,height,slot,i+1);require(repeated==output,"Same-content MetalFX result not reused");++cached;
            auto data=[device newBufferWithLength:256*height options:MTLResourceStorageModeShared];copy=[cb transferEncoder];
            [copy copyFromTexture:output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1)
                toBuffer:data destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:data.length];[copy endEncoding];
            [cb commit];[cb waitUntilCompleted];error(cb.error);
            std::vector<uint8_t> packed(width*height*4);const auto *bytes=static_cast<const uint8_t *>(data.contents);
            for(NSUInteger y=0;y<height;++y)std::memcpy(packed.data()+y*width*4,bytes+y*256,width*4);
            for(size_t pixel=0;pixel<packed.size();++pixel)require(std::abs(int(packed[pixel])-int(color[pixel%4]))<=1,"FX constant input changed beyond one byte");
            if(full)reference.push_back(packed);else require(packed==reference[i],"Planned synchronization changed full MetalFX output");
            fx.releaseSlot(slot);++frames;
        }
    }
    std::printf("PASS production Metal4 FX synchronization: %u frames, 8 exact full-image pairs, %u repeated-content reuses, private/shared-input-copy paths, three slots, input/output resizing, releaseSlot; conservative and planned modes\n",frames,cached);
    return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
}
