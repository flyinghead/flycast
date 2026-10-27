// SPDX-License-Identifier: GPL-2.0-or-later
// Actual production MetalFX bridge with production command synchronization.
#include "rend/metal4/metal4_fx.h"
#include "log/Log.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <thread>
void GenericLog(LogTypes::LOG_LEVELS,LogTypes::LOG_TYPE,const char*,int,const char *format,...)
{va_list args;va_start(args,format);std::vfprintf(stderr,format,args);std::fputc('\n',stderr);va_end(args);}
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void error(NSError *e){if(e)throw std::runtime_error(e.localizedDescription.UTF8String);}
static void finish(FCMetal4Commands *command)
{
    [command commit];[command waitUntilCompleted];error(command.error);
}
static void solid(id<MTLDevice> device,FCMetal4Queue *queue,id<MTLTexture> texture,const std::array<uint8_t,4>& rgba)
{
    const NSUInteger row=(texture.width*4+255)&~NSUInteger(255);
    auto bytes=[device newBufferWithLength:row*texture.height options:MTLResourceStorageModeShared];require(bytes!=nil,"Cannot allocate FX source pixels");
    for(NSUInteger y=0;y<texture.height;++y)for(NSUInteger x=0;x<texture.width;++x)
        std::memcpy(static_cast<uint8_t *>(bytes.contents)+y*row+x*4,rgba.data(),4);
    auto command=[queue commandBuffer];auto copy=[command transferEncoder];
    [copy copyFromBuffer:bytes sourceOffset:0 sourceBytesPerRow:row sourceBytesPerImage:bytes.length sourceSize:MTLSizeMake(texture.width,texture.height,1)
        toTexture:texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[copy endEncoding];finish(command);
}
static void checkImage(id<MTLDevice> device,FCMetal4Commands *command,id<MTLTexture> image,const std::array<uint8_t,4>& rgba,const char *message)
{
    const NSUInteger row=(image.width*4+255)&~NSUInteger(255);
    auto pixels=[device newBufferWithLength:row*image.height options:MTLResourceStorageModeShared];require(pixels!=nil,"Cannot allocate FX result pixels");
    auto copy=[command transferEncoder];[copy copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(image.width,image.height,1) toBuffer:pixels destinationOffset:0 destinationBytesPerRow:row destinationBytesPerImage:pixels.length];[copy endEncoding];finish(command);
    for(NSUInteger y=0;y<image.height;++y)for(NSUInteger x=0;x<image.width;++x)for(NSUInteger c=0;c<4;++c)
        require(std::abs(int(static_cast<const uint8_t *>(pixels.contents)[y*row+x*4+c])-int(rgba[c]))<=1,message);
}
static void validatePublication(id<MTLDevice> device,id<MTL4Compiler> compiler)
{
    const std::array<uint8_t,4> rgba{37,139,211,255},poison{217,41,73,255};
    unsigned recovered=0;
    for(bool full:{true,false})for(unsigned failure=0;failure<4;++failure) { @autoreleasepool {
        auto queue=[[FCMetal4Queue alloc] initWithDevice:device];MetalFXUpscaler fx(device,compiler);
        require(fx.supported(),"Metal4 FX unsupported; cannot validate cache publication");
        auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:16 height:16 mipmapped:NO];
        d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead;
        auto input=[device newTextureWithDescriptor:d];require(input!=nil,"Cannot create publication input");solid(device,queue,input,rgba);
        __weak FCMetal4Commands *weakProducer=nil;FCMetal4Commands *held=nil;id<MTLTexture> unpublished=nil;
        @autoreleasepool {
            auto producer=[queue commandBuffer];[producer setFullSynchronizationForTesting:full];weakProducer=producer;
            unpublished=fx.upscale(input,producer,32,32,0,91);require(unpublished!=nil,"Cannot encode initial FX producer");
            solid(device,queue,unpublished,poison); // Deterministic bytes; never rely on undefined new texture contents.
            if(failure==1)[producer setCommitFailureForTesting:YES];
            if(failure==2)[producer setQueueCommitFailureForTesting:YES];
            if(failure==1 || failure==2){bool threw=false;try{[producer commit];}catch(const std::exception&){threw=true;}
                require(threw && producer.status==FCMetal4StatusEncoding,"FX cancellation fixture did not reject before GPU submission");}
            if(failure==3)held=producer;
            producer=nil;
        }
        if(failure!=3)require(weakProducer==nil,"FX cache retained an abandoned producer command");
        auto retry=[queue commandBuffer];[retry setFullSynchronizationForTesting:full];
        auto output=fx.upscale(input,retry,32,32,failure==3?1:0,91);require(output!=nil,"FX retry lost its optional scaler");
        checkImage(device,retry,output,rgba,"Canceled or foreign unsubmitted FX producer reused unpublished output");
        if(held){finish(held);held=nil;}
        ++recovered;
    }}
    unsigned controls=0;
    for(bool full:{true,false}) { @autoreleasepool {
        auto queue=[[FCMetal4Queue alloc] initWithDevice:device];MetalFXUpscaler fx(device,compiler);
        auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:16 height:16 mipmapped:NO];
        d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead;
        auto input=[device newTextureWithDescriptor:d];require(input!=nil,"Cannot allocate cache control input");solid(device,queue,input,rgba);
        // A committed producer may be reused before completion on the same
        // queue. An unsignaled event proves this call adds no CPU GPU-wait.
        auto gate=[device newSharedEvent];require(gate!=nil,"Cannot allocate FX control event");
        struct Release {id<MTLSharedEvent> event;~Release(){event.signaledValue=1;}} release{gate};
        auto producer=[queue commandBuffer];[producer setFullSynchronizationForTesting:full];[producer waitForEvent:gate value:1];
        auto output=fx.upscale(input,producer,32,32,0,101);require(output!=nil,"Cannot encode gated FX producer");[producer commit];
        auto consumer=[queue commandBuffer];[consumer setFullSynchronizationForTesting:full];
        require(fx.upscale(input,consumer,32,32,1,101)==output && gate.signaledValue==0,
            "Queued FX result was recomputed or waited for before same-queue reuse");
        gate.signaledValue=1;checkImage(device,consumer,output,rgba,"Same-queue pending FX reuse changed output");[producer waitUntilCompleted];error(producer.error);++controls;
        // Successful completion is remembered without retaining the producer.
        __weak FCMetal4Commands *retired=nil;id<MTLTexture> completed=nil;
        @autoreleasepool {
            auto command=[queue commandBuffer];retired=command;[command setFullSynchronizationForTesting:full];
            completed=fx.upscale(input,command,32,32,0,102);require(completed!=nil,"Cannot encode completed FX control");finish(command);command=nil;
        }
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(retired!=nil && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        require(retired==nil,"Completed FX cache retained its producer wrapper");
        consumer=[queue commandBuffer];[consumer setFullSynchronizationForTesting:full];
        require(fx.upscale(input,consumer,32,32,1,102)==completed,"Retiring a completed producer lost its reusable FX result");
        checkImage(device,consumer,completed,rgba,"Retired producer cache changed complete pixels");++controls;
        // A different queue has no inherited pending dependency proof. Even
        // after retirement, use its own slot's properly encoded scaler result.
        auto foreign=[[FCMetal4Queue alloc] initWithDevice:device];consumer=[foreign commandBuffer];[consumer setFullSynchronizationForTesting:full];
        auto regenerated=fx.upscale(input,consumer,32,32,2,102);require(regenerated!=nil && regenerated!=completed,"FX reused a producer-queue cache on another queue");
        checkImage(device,consumer,regenerated,rgba,"Other-queue regenerated FX result changed pixels");++controls;
        // A post-enqueue exception still has a real producer. Keep that queued
        // result reusable rather than confusing it with a pre-submit failure.
        producer=[queue commandBuffer];[producer setFullSynchronizationForTesting:full];output=fx.upscale(input,producer,32,32,0,103);
        [producer setPostCommitFailureForTesting:YES];bool threw=false;try{[producer commit];}catch(const std::exception&){threw=true;}
        require(threw && producer.status!=FCMetal4StatusEncoding,"Post-submit FX fixture was not actually enqueued");
        consumer=[queue commandBuffer];[consumer setFullSynchronizationForTesting:full];
        require(fx.upscale(input,consumer,32,32,1,103)==output,"Post-enqueue exception invalidated submitted FX content");
        checkImage(device,consumer,output,rgba,"Submitted FX content after host exception changed pixels");[producer waitUntilCompleted];error(producer.error);++controls;
        producer=[queue commandBuffer];[producer setFullSynchronizationForTesting:full];output=fx.upscale(input,producer,32,32,0,104);
        [producer setFeedbackErrorForTesting:[NSError errorWithDomain:@"org.flycast.test.fx-feedback" code:17 userInfo:nil]];
        [producer commit];[producer waitUntilCompleted];require(producer.error!=nil,"FX feedback fixture did not report its synthetic error");
        consumer=[queue commandBuffer];require(fx.upscale(input,consumer,32,32,1,104)==nil,"Failed GPU feedback remained a reusable FX result");++controls;
        // Completion closures own proof tokens, never the destroyed scaler.
        auto transient=std::make_unique<MetalFXUpscaler>(device,compiler);auto delayed=[device newSharedEvent];Release releaseDelayed{delayed};
        producer=[queue commandBuffer];[producer setFullSynchronizationForTesting:full];[producer waitForEvent:delayed value:1];
        output=transient->upscale(input,producer,32,32,0,105);require(output!=nil,"Cannot encode retiring FX owner");[producer commit];transient.reset();
        require(delayed.signaledValue==0,"FX owner destruction unexpectedly waited on GPU completion");delayed.signaledValue=1;[producer waitUntilCompleted];error(producer.error);
        consumer=[queue commandBuffer];[consumer setFullSynchronizationForTesting:full];checkImage(device,consumer,output,rgba,"FX owner retirement changed pending pixels");++controls;
    }}
    std::printf("PASS MetalFX publication controls: %u same-queue pending, completed/deallocated, other-queue, post-enqueue exception, failed-feedback and retired-owner cases; no CPU wait on gated producers, whole CPU-color images, conservative/planned synchronization\n",controls);
    std::printf("PASS MetalFX cache publication: %u complete CPU-color images after abandonment, pre-commit/pre-enqueue rejection and a still-unsubmitted other producer; deterministic poisoned old output, conservative/planned synchronization\n",recovered);
}
int main() {
 @autoreleasepool {try {
    auto device=MTLCreateSystemDefaultDevice();require(device!=nil,"No GPU");NSError *failure=nil;
    auto compiler=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&failure];error(failure);
    validatePublication(device,compiler);
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
