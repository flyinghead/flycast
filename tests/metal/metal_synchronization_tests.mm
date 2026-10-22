// SPDX-License-Identifier: GPL-2.0-or-later
// Production Metal 4 synchronization, no emulator or application startup.
#include "rend/metal4/metal4_commands.h"
#import <IOSurface/IOSurface.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <unistd.h>
static void require(bool x,const char *message){if(!x)throw std::runtime_error(message);}
static void error(NSError *e){if(e)throw std::runtime_error(e.localizedDescription.UTF8String);}
static constexpr uint8_t Color[4]={37,101,179,255};
struct GPU {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    FCMetal4Queue *queue;
    id<MTLComputePipelineState> read;
    GPU() {
        require(device!=nil,"No GPU");queue=[[FCMetal4Queue alloc] initWithDevice:device];NSError *e=nil;
        auto compiler=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&e];error(e);
        auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=@"#include <metal_stdlib>\nusing namespace metal;kernel void read_pixel(texture2d<float,access::read> image [[texture(0)]],device uint *out [[buffer(0)]]) {float4 c=image.read(uint2(0));out[0]=uint(round(c.r*255)) | (uint(round(c.g*255))<<8) | (uint(round(c.b*255))<<16) | (uint(round(c.a*255))<<24);}";
        descriptor.options=[MTLCompileOptions new];descriptor.options.languageVersion=MTLLanguageVersion4_0;
        auto library=[compiler newLibraryWithDescriptor:descriptor error:&e];error(e);auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=@"read_pixel";
        auto d=[MTL4ComputePipelineDescriptor new];d.computeFunctionDescriptor=f;read=[compiler newComputePipelineStateWithDescriptor:d compilerTaskOptions:nil error:&e];error(e);
    }
    id<MTLBuffer> buffer(NSUInteger bytes,bool privateStorage=false) {return [device newBufferWithLength:bytes options:privateStorage?MTLResourceStorageModePrivate:MTLResourceStorageModeShared];}
    id<MTLTexture> texture(NSUInteger size=16,bool mip=false) {
        auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:size height:size mipmapped:mip];
        d.storageMode=MTLStorageModePrivate;d.hazardTrackingMode=MTLHazardTrackingModeUntracked;
        d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite|MTLTextureUsageRenderTarget|MTLTextureUsagePixelFormatView;return [device newTextureWithDescriptor:d];
    }
};
static void upload(FCMetal4ComputeEncoder *e,id<MTLBuffer> source,id<MTLTexture> target,NSUInteger offset=0,NSUInteger level=0,NSUInteger slice=0) {
    const NSUInteger width=std::max<NSUInteger>(1,target.width>>level),height=std::max<NSUInteger>(1,target.height>>level);
    [e copyFromBuffer:source sourceOffset:offset sourceBytesPerRow:256 sourceBytesPerImage:256*height sourceSize:MTLSizeMake(width,height,1) toTexture:target destinationSlice:slice destinationLevel:level destinationOrigin:MTLOriginMake(0,0,0)];
}
static void download(FCMetal4ComputeEncoder *e,id<MTLTexture> source,id<MTLBuffer> target,NSUInteger offset=0,NSUInteger level=0,NSUInteger slice=0) {
    const NSUInteger width=std::max<NSUInteger>(1,source.width>>level),height=std::max<NSUInteger>(1,source.height>>level);
    [e copyFromTexture:source sourceSlice:slice sourceLevel:level sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toBuffer:target destinationOffset:offset destinationBytesPerRow:256 destinationBytesPerImage:256*height];
}
static void checkImage(id<MTLBuffer> data,NSUInteger width,NSUInteger height,const uint8_t *color,NSUInteger offset=0) {
    const auto *bytes=static_cast<const uint8_t *>(data.contents)+offset;
    for(NSUInteger y=0;y<height;++y)for(NSUInteger x=0;x<width;++x)for(unsigned c=0;c<4;++c)require(bytes[y*256+x*4+c]==color[c],"Transfer/clear/alias image differs");
}
static void finish(FCMetal4Commands *cb) {[cb commit];[cb waitUntilCompleted];error(cb.error);}
static void independentUploads(GPU& g,bool conservative,bool mips) {
    auto source=g.buffer(256*16);for(NSUInteger i=0;i<source.length;i+=4)std::memcpy(static_cast<uint8_t *>(source.contents)+i,Color,4);
    NSMutableArray<id<MTLTexture>> *images=[NSMutableArray new];for(unsigned i=0;i<20;++i)[images addObject:g.texture(16,mips)];
    auto cb=[g.queue commandBuffer];[cb setFullSynchronizationForTesting:conservative];auto e=[cb transferEncoder];
    for(id<MTLTexture> image in images){upload(e,source,image);if(mips)[e generateMipmapsForTexture:image];}
    const auto uploadStats=[cb synchronizationStatistics];
    if(!mips)require(uploadStats[@"barriers"].unsignedIntegerValue==(conservative?19:0),"Independent upload barrier count differs");
    else require(uploadStats[@"barriers"].unsignedIntegerValue==(conservative?39:20),"Upload/mip RAW barrier count differs");
    auto out=g.buffer(20*256*16);
    for(NSUInteger i=0;i<images.count;++i)download(e,images[i],out,i*256*16,mips?images[i].mipmapLevelCount-1:0);
    [e endEncoding];finish(cb);
    for(NSUInteger i=0;i<images.count;++i)checkImage(out,mips?1:16,mips?1:16,Color,i*256*16);
}
static void levelsAndViews(GPU& g,bool conservative) {
    auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:16 height:16 mipmapped:YES];d.textureType=MTLTextureType2DArray;d.arrayLength=4;d.storageMode=MTLStorageModePrivate;d.hazardTrackingMode=MTLHazardTrackingModeUntracked;d.usage=MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
    auto image=[g.device newTextureWithDescriptor:d];const NSUInteger levels=image.mipmapLevelCount,count=4*levels,stride=256*16;
    auto source=g.buffer(count*stride),out=g.buffer(count*stride);std::memset(source.contents,0,source.length);
    for(NSUInteger i=0;i<count;++i)for(NSUInteger j=0;j<stride;++j)static_cast<uint8_t *>(source.contents)[i*stride+j]=uint8_t(20+i);
    auto cb=[g.queue commandBuffer];[cb setFullSynchronizationForTesting:conservative];auto e=[cb transferEncoder];
    for(NSUInteger slice=0;slice<4;++slice)for(NSUInteger level=0;level<levels;++level)upload(e,source,image,(slice*levels+level)*stride,level,slice);
    require([cb synchronizationStatistics][@"barriers"].unsignedIntegerValue==(conservative?count-1:0),"Disjoint mip/slice uploads were serialized");
    for(NSUInteger slice=0;slice<4;++slice)for(NSUInteger level=0;level<levels;++level)download(e,image,out,(slice*levels+level)*stride,level,slice);
    [e endEncoding];finish(cb);
    for(NSUInteger slice=0;slice<4;++slice)for(NSUInteger level=0;level<levels;++level) {
        const uint8_t value=uint8_t(20+slice*levels+level),color[]={value,value,value,value};
        checkImage(out,std::max<NSUInteger>(1,16>>level),std::max<NSUInteger>(1,16>>level),color,(slice*levels+level)*stride);
    }
    auto view=[image newTextureViewWithPixelFormat:image.pixelFormat textureType:MTLTextureType2DArray levels:NSMakeRange(1,3) slices:NSMakeRange(1,2)];
    auto nested=[view newTextureViewWithPixelFormat:image.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(1,1) slices:NSMakeRange(1,1)];require(view && nested,"Cannot make nested texture views");
    auto again=[g.queue commandBuffer];[again setFullSynchronizationForTesting:conservative];auto copy=[again transferEncoder];
    upload(copy,source,image,0,2,2);download(copy,nested,out);[copy endEncoding];
    require([again synchronizationStatistics][@"barriers"].unsignedIntegerValue==1,"Nested-view/root RAW hazard was missed");finish(again);
    const uint8_t color[]={20,20,20,20};checkImage(out,4,4,color);
}
static void rangesAndDispatch(GPU& g,bool conservative) {
    auto privateBuffer=g.buffer(4096,true),out=g.buffer(4096);auto a=g.texture(4),b=g.texture(4),c=g.texture(4);
    auto cb=[g.queue commandBuffer];[cb setFullSynchronizationForTesting:conservative];auto e=[cb computeCommandEncoder];
    [e fillBuffer:privateBuffer range:NSMakeRange(0,1024) value:17];[e fillBuffer:privateBuffer range:NSMakeRange(1024,1024) value:18];
    require([cb synchronizationStatistics][@"barriers"].unsignedIntegerValue==(conservative?1:0),"Adjacent private buffer fills serialized");
    upload(e,privateBuffer,a);[e fillBuffer:privateBuffer range:NSMakeRange(0,1024) value:29];upload(e,privateBuffer,b);
    [e copyFromTexture:b sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(4,4,1) toTexture:c destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
    [e setComputePipelineState:g.read];[e setTexture:c atIndex:0];[e setBuffer:out offset:2048 atIndex:0];[e dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
    [e fillBuffer:privateBuffer range:NSMakeRange(0,0) value:0]; // Must not consume the dispatch dependency epoch.
    [e dispatchThreads:MTLSizeMake(0,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
    [e generateMipmapsForTexture:c]; // No mip levels to generate.
    [e copyFromTexture:c sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(0,1,1)
        toBuffer:out destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:256];
    [e setBuffer:out offset:2052 atIndex:0];[e dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
    download(e,a,out,0);download(e,c,out,1024);[e endEncoding];finish(cb);
    const uint8_t oldColor[]={17,17,17,17},newColor[]={29,29,29,29};checkImage(out,4,4,oldColor);checkImage(out,4,4,newColor,1024);
    const auto *words=static_cast<const uint32_t *>(out.contents);require(words[512]==0x1d1d1d1d && words[513]==0x1d1d1d1d,"Blit/dispatch/empty-blit/dispatch ordering differs");
    require(([cb synchronizationStatistics][@"afterStages"].unsignedIntegerValue&(MTLStageBlit|MTLStageDispatch))==(MTLStageBlit|MTLStageDispatch),"Dispatch/blit stage dependencies missing");
}
static void aliases(GPU& g,bool conservative) {
    auto backing=g.buffer(4096),out=g.buffer(4096);auto image=g.texture(4);
    auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:4 height:4 mipmapped:NO];d.usage=MTLTextureUsageShaderRead;d.storageMode=MTLStorageModeShared;
    auto linear=[backing newTextureWithDescriptor:d offset:0 bytesPerRow:256];require(linear!=nil,"Cannot make linear alias texture");
    auto cb=[g.queue commandBuffer];[cb setFullSynchronizationForTesting:conservative];auto e=[cb transferEncoder];[e fillBuffer:backing range:NSMakeRange(0,1024) value:61];
    [e copyFromTexture:linear sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(4,4,1) toTexture:image destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];download(e,image,out);[e endEncoding];finish(cb);
    const uint8_t color[]={61,61,61,61};checkImage(out,4,4,color);require([cb synchronizationStatistics][@"aliasBarriers"].unsignedIntegerValue>0,"Buffer-backed texture omitted alias visibility");
    const size_t bytes=getpagesize();void *memory=nullptr;require(posix_memalign(&memory,bytes,bytes)==0,"Cannot allocate no-copy backing");
    struct Cleanup {void *p;~Cleanup(){free(p);}} cleanup{memory};
    auto first=[g.device newBufferWithBytesNoCopy:memory length:bytes options:MTLResourceStorageModeShared deallocator:nil];
    auto second=[g.device newBufferWithBytesNoCopy:memory length:bytes options:MTLResourceStorageModeShared deallocator:nil];require(first && second,"No-copy alias buffers unavailable");
    cb=[g.queue commandBuffer];[cb setFullSynchronizationForTesting:conservative];e=[cb transferEncoder];[e fillBuffer:first range:NSMakeRange(0,1024) value:73];upload(e,second,image);download(e,image,out);[e endEncoding];finish(cb);
    const uint8_t other[]={73,73,73,73};checkImage(out,4,4,other);
    require([cb synchronizationStatistics][@"aliasBarriers"].unsignedIntegerValue>0,"No-copy buffer alias omitted visibility");
}
static void surfaceAliases(GPU& g,bool conservative) {
    IOSurfaceRef surface=IOSurfaceCreate((__bridge CFDictionaryRef)@{(id)kIOSurfaceWidth:@4,(id)kIOSurfaceHeight:@4,(id)kIOSurfaceBytesPerElement:@4,(id)kIOSurfaceBytesPerRow:@256,(id)kIOSurfaceAllocSize:@1024,(id)kIOSurfacePixelFormat:@(0x42475241)});
    require(surface!=nullptr,"Cannot create IOSurface");struct Cleanup{IOSurfaceRef surface;~Cleanup(){CFRelease(surface);}} cleanup{surface};
    auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:4 height:4 mipmapped:NO];d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead;
    auto a=[g.device newTextureWithDescriptor:d iosurface:surface plane:0],b=[g.device newTextureWithDescriptor:d iosurface:surface plane:0];require(a && b,"Cannot make IOSurface texture aliases");
    auto source=g.buffer(1024),out=g.buffer(1024);std::memset(source.contents,91,source.length);
    auto cb=[g.queue commandBuffer];[cb setFullSynchronizationForTesting:conservative];auto e=[cb transferEncoder];upload(e,source,a);download(e,b,out);[e endEncoding];finish(cb);
    const uint8_t color[]={91,91,91,91};checkImage(out,4,4,color);require([cb synchronizationStatistics][@"aliasBarriers"].unsignedIntegerValue==1,"Imported IOSurface alias did not fall back conservatively");
}
static void boundariesAndQueues(GPU& g,bool conservative) {
    auto other=[[FCMetal4Queue alloc] initWithDevice:g.device];auto event=[g.device newSharedEvent];
    for(unsigned i=0;i<8;++i) {
        auto texture=g.texture(4);auto out=g.buffer(1024);auto producer=[g.queue commandBuffer];[producer setFullSynchronizationForTesting:conservative];
        auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(37./255,101./255,179./255,1);
        auto render=[producer renderCommandEncoderWithDescriptor:pass];[render endEncoding];
        [producer queueBarrier]; // Explicit opaque-encoder handoff retains All -> All.
        [producer signalEvent:event value:i+1];
        auto consumer=[other commandBuffer];[consumer setFullSynchronizationForTesting:conservative];[consumer waitForEvent:event value:i+1];auto copy=[consumer transferEncoder];download(copy,texture,out);[copy endEncoding];
        [consumer commit];[producer commit];[consumer waitUntilCompleted];[producer waitUntilCompleted];error(consumer.error);error(producer.error);checkImage(out,4,4,Color);
    }
}
static uint32_t boundaryColor(unsigned i) {
    return (37u+i%79) | ((101u+i%47)<<8) | ((179u-i%61)<<16) | 0xff000000u;
}
static void placementAliases(GPU& g) {
    constexpr NSUInteger Side=16,Bytes=256*Side;
    const auto options=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeUntracked;
    auto textureDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:Side height:Side mipmapped:NO];
    textureDesc.storageMode=MTLStorageModePrivate;textureDesc.hazardTrackingMode=MTLHazardTrackingModeUntracked;
    textureDesc.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    const auto bufferSize=[g.device heapBufferSizeAndAlignWithLength:Bytes options:options];
    const auto textureSize=[g.device heapTextureSizeAndAlignWithDescriptor:textureDesc];
    const auto heap=[&](NSUInteger bytes) {
        auto d=[MTLHeapDescriptor new];d.type=MTLHeapTypePlacement;d.storageMode=MTLStorageModePrivate;
        d.hazardTrackingMode=MTLHazardTrackingModeUntracked;d.size=bytes;
        auto h=[g.device newHeapWithDescriptor:d];require(h!=nil,"Cannot allocate placement heap");return h;
    };
    auto bufferHeap=heap(bufferSize.size),textureHeap=heap(textureSize.size);
    auto a=[bufferHeap newBufferWithLength:Bytes options:options offset:0];
    auto b=[bufferHeap newBufferWithLength:Bytes options:options offset:0];
    auto first=[textureHeap newTextureWithDescriptor:textureDesc offset:0];
    auto second=[textureHeap newTextureWithDescriptor:textureDesc offset:0];
    require(a && b && first && second && a!=b && first!=second,"Placement aliases were not created as distinct resources");
    require(a.heap==b.heap && a.heapOffset==b.heapOffset && first.heap==second.heap && first.heapOffset==second.heapOffset,"Fixture resources do not actually overlap");
    auto image=g.texture(Side);auto source=g.buffer(Bytes*3),out=g.buffer(Bytes*2);
    unsigned images=0;
    for(bool legacy:{true,false})for(unsigned boundary=0;boundary<3;++boundary)for(unsigned round=0;round<8;++round) {
        const uint8_t fill=uint8_t(17+round*7);const uint8_t filled[]={fill,fill,fill,fill};
        auto cb=[g.queue commandBuffer];[cb setLegacyEncoderBoundariesForTesting:legacy];auto e=[cb transferEncoder];
        [e fillBuffer:a range:NSMakeRange(0,Bytes) value:fill];
        if(boundary) {
            [e endEncoding];
            if(boundary==2){finish(cb);cb=[g.queue commandBuffer];[cb setLegacyEncoderBoundariesForTesting:legacy];}
            e=[cb transferEncoder];
        }
        upload(e,b,image);download(e,image,out);[e endEncoding];finish(cb);checkImage(out,Side,Side,filled);++images;
        // Same descriptor and placement: upload/clear through one texture,
        // read through the other, then reverse their roles without a CPU wait.
        const uint32_t color=boundaryColor(round),replacement=boundaryColor(round+17),final=boundaryColor(round+39);
        for(NSUInteger i=0;i<Bytes;i+=4) {
            std::memcpy(static_cast<uint8_t *>(source.contents)+i,&color,4);
            std::memcpy(static_cast<uint8_t *>(source.contents)+Bytes+i,&replacement,4);
            std::memcpy(static_cast<uint8_t *>(source.contents)+Bytes*2+i,&final,4);
        }
        cb=[g.queue commandBuffer];[cb setLegacyEncoderBoundariesForTesting:legacy];
        if(boundary) {
            auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=first;
            pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
            pass.colorAttachments[0].clearColor=MTLClearColorMake((color&255)/255.,((color>>8)&255)/255.,((color>>16)&255)/255.,1);
            auto render=[cb renderCommandEncoderWithDescriptor:pass];[render endEncoding];
            if(boundary==2){finish(cb);cb=[g.queue commandBuffer];[cb setLegacyEncoderBoundariesForTesting:legacy];}
            e=[cb transferEncoder];
        } else {e=[cb transferEncoder];upload(e,source,first);}
        download(e,second,out);upload(e,source,second,Bytes);upload(e,source,first,Bytes*2);download(e,second,out,Bytes);[e endEncoding];finish(cb);
        checkImage(out,Side,Side,reinterpret_cast<const uint8_t *>(&color));
        checkImage(out,Side,Side,reinterpret_cast<const uint8_t *>(&final),Bytes);images+=2;
        require([cb synchronizationStatistics][@"aliasBarriers"].unsignedIntegerValue>0,"Placement aliases did not request conservative alias barriers");
    }
    std::printf("PASS actual placement aliases: %u full 16x16 images, distinct private untracked buffers/textures at identical heap offsets, RAW/WAR/WAW, clear/upload/read and reverse alias roles, within-pass/encoder/command boundaries; legacy and fused producers\n",images);
}
static FCMetal4Commands *boundaryChain(GPU& g,bool legacy,unsigned count,id<MTLTexture> image,id<MTLBuffer> out,bool opaque=false) {
    auto cb=[g.queue commandBuffer];[cb setLegacyEncoderBoundariesForTesting:legacy];
    [cb setPassDependenciesForTesting:NO]; // Isolate producer fusion from the later pass graph.
    for(unsigned i=0;i<count;++i) {
        const auto color=boundaryColor(i);auto pass=[MTL4RenderPassDescriptor new];
        pass.colorAttachments[0].texture=image;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake((color&255)/255.,((color>>8)&255)/255.,((color>>16)&255)/255.,1);
        auto render=[cb renderCommandEncoderWithDescriptor:pass];[render endEncoding];
        auto compute=[cb computeCommandEncoder];[compute setComputePipelineState:g.read];
        [compute setTexture:image atIndex:0];[compute setBuffer:out offset:i*4 atIndex:0];
        [compute dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];
        if(opaque && i==count/2)[cb queueBarrier];
    }
    return cb;
}
static void checkBoundaryWords(id<MTLBuffer> out,unsigned count) {
    const auto *words=static_cast<const uint32_t *>(out.contents);
    for(unsigned i=0;i<count;++i)require(words[i]==boundaryColor(i),"Render/dispatch RAW or read/next-render WAR dependency failed");
}
static void deferredBoundaries(GPU& g) {
    constexpr unsigned Count=64;auto image=g.texture(4);auto out=g.buffer(Count*4);
    for(bool legacy:{true,false})for(bool opaque:{false,true}) {
        auto cb=boundaryChain(g,legacy,Count,image,out,opaque);finish(cb);checkBoundaryWords(out,Count);
        const auto stats=[cb synchronizationStatistics];
        require(stats[@"queueProducers"].unsignedIntegerValue==(legacy?Count*2+unsigned(opaque):1+unsigned(opaque)),"Producer boundary fusion count differs");
        require(stats[@"commitFlushes"].unsignedIntegerValue==(legacy?0:1),"Final producer visibility was lost");
        require(stats[@"deferredEnds"].unsignedIntegerValue==(legacy?0:Count*2),"Deferred encoder ownership differs");
        auto pixels=g.buffer(1024);auto copy=[g.queue commandBuffer];auto e=[copy transferEncoder];download(e,image,pixels);[e endEncoding];finish(copy);
        const uint32_t color=boundaryColor(Count-1);checkImage(pixels,4,4,reinterpret_cast<const uint8_t *>(&color));
    }
    // Raw Metal access must see a genuinely ended pass, and its unknown work
    // must get a final producer release even without another wrapper encoder.
    {
        auto pixels=g.buffer(1024);auto cb=[g.queue commandBuffer];[cb useResource:pixels];
        auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=image;
        pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(37./255,101./255,179./255,1);
        auto render=[cb renderCommandEncoderWithDescriptor:pass];[render endEncoding];
        auto native=cb.native;auto raw=[native computeCommandEncoder];
        [raw copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(4,4,1)
            toBuffer:pixels destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:1024];[raw endEncoding];raw=nil;native=nil;
        finish(cb);checkImage(pixels,4,4,Color);
        const auto stats=[cb synchronizationStatistics];require(stats[@"queueProducers"].unsignedIntegerValue==2 && stats[@"fallbackCommitFlushes"].unsignedIntegerValue==1,"Opaque native handoff missed prior/final visibility");
    }
    // Cancellation of a logically ended but native-open pass must return its
    // allocator/tables without ever submitting malformed or stale GPU work.
    for(unsigned i=0;i<32;++i)@autoreleasepool {
        auto cb=[g.queue commandBuffer];auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=image;
        pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        auto e=[cb renderCommandEncoderWithDescriptor:pass];[e endEncoding];
    }
    auto cb=boundaryChain(g,false,Count,image,out);finish(cb);checkBoundaryWords(out,Count);
    std::puts("PASS native encoder boundaries: 64 render/dispatch RAW/WAR pairs, complete clear images across commands, 128 -> 1 producer barriers, opaque queue and raw native handoffs, final CPU visibility and 32 canceled pending encoders");
}
static void passDependencies(GPU& g) {
    unsigned images=0;
    for(unsigned count:{32,80}) {
        NSMutableArray<id<MTLTexture>> *textures=[NSMutableArray new];
        for(unsigned i=0;i<count;++i)[textures addObject:g.texture(4)];
        auto cb=[g.queue commandBuffer];
        for(unsigned i=0;i<count;++i) {
            auto pass=[MTL4RenderPassDescriptor new];const auto color=boundaryColor(i);
            pass.colorAttachments[0].texture=textures[i];pass.colorAttachments[0].loadAction=MTLLoadActionClear;
            pass.colorAttachments[0].storeAction=MTLStoreActionStore;
            pass.colorAttachments[0].clearColor=MTLClearColorMake((color&255)/255.,((color>>8)&255)/255.,((color>>16)&255)/255.,1);
            auto render=[cb renderCommandEncoderWithDescriptor:pass];[render endEncoding];
        }
        finish(cb);const auto stats=[cb synchronizationStatistics];
        require(stats[@"queueConsumers"].unsignedIntegerValue==(count==32?1:2),"Independent render passes were not proven independent or capacity fallback was lost");
        require(stats[@"queueProducers"].unsignedIntegerValue==(count==32?2:3),"Prefix/capacity/final producer releases differ");
        auto out=g.buffer(count*1024);cb=[g.queue commandBuffer];auto copy=[cb transferEncoder];
        for(unsigned i=0;i<count;++i)download(copy,textures[i],out,i*1024);
        [copy endEncoding];finish(cb);
        for(unsigned i=0;i<count;++i){const auto color=boundaryColor(i);checkImage(out,4,4,reinterpret_cast<const uint8_t *>(&color),i*1024);++images;}
    }
    for(bool planned:{false,true}) {
        // RAW/WAR across render clears and declared shader inputs, including
        // the same resource consumed in later encoders after an earlier acquire.
        auto image=g.texture(4);auto words=FCMetal4CreateBuffer(g.device,64*4,MTLResourceStorageModeShared);
        auto cb=[g.queue commandBuffer];[cb setPassDependenciesForTesting:planned];
        for(unsigned i=0;i<64;++i) {
            const auto color=boundaryColor(i);auto pass=[MTL4RenderPassDescriptor new];
            pass.colorAttachments[0].texture=image;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
            pass.colorAttachments[0].clearColor=MTLClearColorMake((color&255)/255.,((color>>8)&255)/255.,((color>>16)&255)/255.,1);
            auto r=[cb renderCommandEncoderWithDescriptor:pass];[r endEncoding];
            auto c=[cb computeCommandEncoder];[c setDirectShaderBindings:YES];[c setComputePipelineState:g.read];
            [c setTexture:image atIndex:0 usage:MTLResourceUsageRead];[c setBuffer:words offset:i*4 atIndex:0 usage:MTLResourceUsageWrite];
            [c dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[c endEncoding];
        }
        finish(cb);checkBoundaryWords(words,64);
        if(planned)require([cb synchronizationStatistics][@"queueNarrowConsumers"].unsignedIntegerValue>0,"Known compute accesses did not narrow producer/consumer stages");
        auto gate=[g.device newSharedEvent];NSMutableArray<FCMetal4Commands *> *pending=[NSMutableArray new];
        NSMutableArray<id<MTLBuffer>> *snapshots=[NSMutableArray new];
        for(unsigned frame=0;frame<3;++frame) {
            const NSUInteger size=4+frame*4;auto target=g.texture(size);auto out=g.buffer(256*size);
            auto command=[g.queue commandBuffer];[command setPassDependenciesForTesting:planned];[command waitForEvent:gate value:1];
            auto e=[command transferEncoder];[e fillBuffer:out range:NSMakeRange(0,out.length) value:0];[e endEncoding];
            const auto color=boundaryColor(frame+9);auto pass=[MTL4RenderPassDescriptor new];
            pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
            pass.colorAttachments[0].clearColor=MTLClearColorMake((color&255)/255.,((color>>8)&255)/255.,((color>>16)&255)/255.,1);
            auto r=[command renderCommandEncoderWithDescriptor:pass];[r endEncoding];
            e=[command transferEncoder];download(e,target,out);[e endEncoding];[command commit];
            require(command.status==FCMetal4StatusCommitted,"Gated pass graph command completed before gate");[pending addObject:command];[snapshots addObject:out];
        }
        gate.signaledValue=1;
        for(unsigned frame=0;frame<3;++frame){auto command=pending[frame];[command waitUntilCompleted];error(command.error);const auto color=boundaryColor(frame+9);checkImage(snapshots[frame],4+frame*4,4+frame*4,reinterpret_cast<const uint8_t *>(&color));++images;}
    }
    // A no-copy import can alias an allocation made by the certified factory.
    // Its unknown identity must acquire all earlier accesses, then publish an
    // alias-visible boundary before subsequent owned-buffer uses.
    {
        const NSUInteger bytes=getpagesize();auto owned=FCMetal4CreateBuffer(g.device,bytes,MTLResourceStorageModeShared);
        require(uintptr_t(owned.contents)%bytes==0,"Factory buffer is not page aligned for the alias fixture");
        auto imported=[g.device newBufferWithBytesNoCopy:owned.contents length:bytes options:MTLResourceStorageModeShared deallocator:nil];
        require(imported && imported!=owned,"Cannot import a no-copy alias of factory-owned storage");
        auto image=g.texture(4);auto out=g.buffer(2048);auto cb=[g.queue commandBuffer];
        auto e=[cb transferEncoder];[e endEncoding]; // Publish the external queue prefix first.
        e=[cb transferEncoder];[e fillBuffer:owned range:NSMakeRange(0,1024) value:97];[e endEncoding];
        e=[cb transferEncoder];upload(e,imported,image);download(e,image,out);[e fillBuffer:imported range:NSMakeRange(0,1024) value:113];[e endEncoding];
        e=[cb transferEncoder];upload(e,owned,image);download(e,image,out,1024);[e endEncoding];finish(cb);
        const uint8_t first[]={97,97,97,97},second[]={113,113,113,113};checkImage(out,4,4,first);checkImage(out,4,4,second,1024);images+=2;
    }
    std::printf("PASS native pass graph: %u exact full images, 32 independent render clears use one consumer, 80 clears exercise bounded fallback, declared render/dispatch RAW/WAR, scoped stages, three GPU-gated size-changing commands in flight; old/fused graph comparison\n",images);
}
static void boundaryBenchmark(GPU& g) {
    for(const char *name:{"MTL_DEBUG_LAYER","MTL_SHADER_VALIDATION"}){const auto value=std::getenv(name);require(!value || std::strcmp(value,"0")==0,"Boundary timing requires validation disabled");}
    constexpr unsigned Count=64;auto image=g.texture(4);auto out=g.buffer(Count*4);std::vector<double> cpu[2],gpu[2];
    for(unsigned round=0;round<35;++round)for(unsigned turn=0;turn<2;++turn) {
        const unsigned variant=turn^(round&1);const auto start=std::chrono::steady_clock::now();
        auto cb=boundaryChain(g,variant==0,Count,image,out);[cb commit];const auto submitted=std::chrono::steady_clock::now();
        [cb waitUntilCompleted];error(cb.error);checkBoundaryWords(out,Count);
        const auto stats=[cb synchronizationStatistics];require(stats[@"queueProducers"].unsignedIntegerValue==(variant==0?128:1),"Measured producer barrier count differs");
        require(cb.GPUStartTime>0 && cb.GPUEndTime>cb.GPUStartTime,"Boundary GPU timing unavailable");
        if(round>=4){cpu[variant].push_back(std::chrono::duration<double,std::micro>(submitted-start).count());gpu[variant].push_back((cb.GPUEndTime-cb.GPUStartTime)*1e6);}
    }
    for(auto *values:{&cpu[0],&cpu[1],&gpu[0],&gpu[1]})std::sort(values->begin(),values->end());
    std::printf("BENCH encoder boundaries, 31 alternating pairs after 4 warmups, 64 render/dispatch pairs per command, producer barriers 128 -> 1, CPU encode+commit median %.3f -> %.3f us, native GPU median %.3f -> %.3f us; all 4480 output words exact\n",cpu[0][15],cpu[1][15],gpu[0][15],gpu[1][15]);
}
static void benchmark(GPU& g) {
    constexpr unsigned Count=128,Side=64;auto source=g.buffer(256*Side);
    for(NSUInteger i=0;i<source.length;i+=4)std::memcpy(static_cast<uint8_t *>(source.contents)+i,Color,4);
    NSMutableArray<id<MTLTexture>> *images=[NSMutableArray new];for(unsigned i=0;i<Count;++i)[images addObject:g.texture(Side)];
    auto sample=[&](bool full) {
        auto start=std::chrono::steady_clock::now();auto cb=[g.queue commandBuffer];[cb setFullSynchronizationForTesting:full];
        [cb setOwnedResourceBenchmarkForTesting:full];auto e=[cb transferEncoder];
        for(id<MTLTexture> image in images)upload(e,source,image);[e endEncoding];
        double cpu=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();finish(cb);
        require([cb synchronizationStatistics][@"barriers"].unsignedIntegerValue==(full?127:2),"Benchmark history cap/barrier count differs");
        return std::array<double,2>{cpu,(cb.GPUEndTime-cb.GPUStartTime)*1e6};
    };
    std::vector<double> baseCPU,optCPU,baseGPU,optGPU;
    for(unsigned i=0;i<35;++i) {
        std::array<double,2>a,b;if(i&1){b=sample(false);a=sample(true);}else{a=sample(true);b=sample(false);}
        if(i>=4){baseCPU.push_back(a[0]);optCPU.push_back(b[0]);baseGPU.push_back(a[1]);optGPU.push_back(b[1]);}
    }
    for(auto *v:{&baseCPU,&optCPU,&baseGPU,&optGPU})std::sort(v->begin(),v->end());
    std::printf("BENCH 128 x 64x64 RGBA uploads, 31 alternating pairs after 4 warmups: original owned-resource barrier policy vs alias-aware plan, intra barriers 127 -> 2, CPU median %.3f -> %.3f us, GPU feedback median %.3f -> %.3f us\n",baseCPU[15],optCPU[15],baseGPU[15],optGPU[15]);
    // Check all final payload bytes outside the measured region.
    auto out=g.buffer(Count*256*Side);auto cb=[g.queue commandBuffer];auto e=[cb transferEncoder];for(NSUInteger i=0;i<Count;++i)download(e,images[i],out,i*256*Side);[e endEncoding];finish(cb);
    for(NSUInteger i=0;i<Count;++i)checkImage(out,Side,Side,Color,i*256*Side);
}
int main(int argc,char **argv) {
    @autoreleasepool {try {
        std::setvbuf(stdout,nullptr,_IOLBF,0);
        GPU g;
        if(argc==2 && std::strcmp(argv[1],"--heap-reuse")==0){placementAliases(g);passDependencies(g);return 0;}
        if(argc==2 && std::strcmp(argv[1],"--heap-aliases")==0){placementAliases(g);return 0;}
        if(argc==2 && std::strcmp(argv[1],"--passes-only")==0){passDependencies(g);return 0;}
        if(argc==2 && std::strcmp(argv[1],"--boundaries-benchmark")==0){boundaryBenchmark(g);return 0;}
        if(argc==2 && std::strcmp(argv[1],"--benchmark-only")==0){benchmark(g);return 0;}
        for(bool conservative:{true,false}) {
            independentUploads(g,conservative,false);independentUploads(g,conservative,true);levelsAndViews(g,conservative);rangesAndDispatch(g,conservative);aliases(g,conservative);surfaceAliases(g,conservative);boundariesAndQueues(g,conservative);
        }
        deferredBoundaries(g);
        placementAliases(g);
        passDependencies(g);
        std::puts("PASS native synchronization: exact full transfer payloads, independent uploads/mips/slices, nested views, RAW/WAR/WAW, private ranges, dispatch/empty-blit transitions, buffer/no-copy/IOSurface aliases, render clears, opaque queue barrier and two-queue events; conservative and planned modes");return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
}
