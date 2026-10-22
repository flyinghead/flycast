// SPDX-License-Identifier: GPL-2.0-or-later
// Actual recorded-pass and display-delegate code, offscreen GPU destinations.
#include "rend/metal4/metal4_presentation.h"
#include "rend/metal4/metal4_submission.h"
#include "rend/metal4/metal4_fx.h"
#include "hw/pvr/Renderer_if.h"
#include "log/Log.h"
#import <QuartzCore/CAMetalLayer.h>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <memory>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include "rend/metal4/metal4_display_link.h"
#include "rend/metal4/metal4_shaders.h"
#include "rend/metal4/metal4_shader_options.h"
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void error(NSError *e){if(e)throw std::runtime_error(e.localizedDescription.UTF8String);}
void GenericLog(LogTypes::LOG_LEVELS,LogTypes::LOG_TYPE,const char*,int,const char *format,...)
{va_list args;va_start(args,format);std::vfprintf(stderr,format,args);std::fputc('\n',stderr);va_end(args);}

// Only CoreAnimation's surface acquisition/presentation is substituted. Native
// commit/event/feedback, production command wrappers and delegate run unchanged.
@interface QueueSurfaceProxy : NSObject
@property (strong) id<MTL4CommandQueue> actual;
@end
@implementation QueueSurfaceProxy
- (void)waitForDrawable:(id<MTLDrawable>)drawable {}
- (void)signalDrawable:(id<MTLDrawable>)drawable {}
- (id)forwardingTargetForSelector:(SEL)selector {return _actual;}
@end
@interface OffscreenQueue : FCMetal4Queue
@property (strong) FCMetal4Commands *lastCommands;
@property NSUInteger nextFailure;
@end
@implementation OffscreenQueue {QueueSurfaceProxy *_surfaceProxy;}
- (instancetype)initWithDevice:(id<MTLDevice>)device {
    if((self=[super initWithDevice:device])){_surfaceProxy=[QueueSurfaceProxy new];_surfaceProxy.actual=[super native];}return self;
}
- (id<MTL4CommandQueue>)native {return (id<MTL4CommandQueue>)_surfaceProxy;}
- (FCMetal4Commands *)commandBuffer {
    const auto failure=_nextFailure;_nextFailure=0;
    if(failure==1){_lastCommands=nil;return nil;}
    _lastCommands=[super commandBuffer];
    if(failure==2)[_lastCommands setCommitFailureForTesting:YES];
    if(failure==3)[_lastCommands setQueueCommitFailureForTesting:YES];
    if(failure==4)[_lastCommands setPostCommitFailureForTesting:YES];
    if(failure==5)[_lastCommands setFeedbackErrorForTesting:[NSError errorWithDomain:@"org.flycast.test.synthetic-feedback" code:31 userInfo:nil]];
    return _lastCommands;
}
@end
@interface OffscreenDrawable : NSObject <CAMetalDrawable>
@property (strong) id<MTLTexture> texture;
@property (readonly) CAMetalLayer *layer;
@property (readonly) NSUInteger drawableID;
@property (readonly) CFTimeInterval presentedTime;
@property NSUInteger presents;
@end
@implementation OffscreenDrawable
- (CAMetalLayer *)layer {return nil;}
- (NSUInteger)drawableID {return (NSUInteger)(__bridge void *)self;}
- (CFTimeInterval)presentedTime {return 0;}
- (void)present {++_presents;}
- (void)presentAtTime:(CFTimeInterval)time {[self present];}
- (void)presentAfterMinimumDuration:(CFTimeInterval)duration {[self present];}
- (void)addPresentedHandler:(MTLDrawablePresentedHandler)handler {}
@end
@interface OffscreenLayer : NSObject
@property (strong) id<MTLDevice> device;
@property NSUInteger width,height,acquisitions;
@property (strong) OffscreenDrawable *last;
@end
@implementation OffscreenLayer
- (id<CAMetalDrawable>)nextDrawable {
    ++_acquisitions;_last=[OffscreenDrawable new];auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:_width height:_height mipmapped:NO];
    d.storageMode=MTLStorageModePrivate;d.hazardTrackingMode=MTLHazardTrackingModeUntracked;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;_last.texture=[_device newTextureWithDescriptor:d];return _last;
}
@end
@interface DisplayTick : NSObject
@property CFTimeInterval timestamp,targetTimestamp;
@end
@implementation DisplayTick @end
struct alignas(16) Quad {float color[4];uint32_t rotate,ignoreAlpha;};
struct UIVertex {float x,y,u,v;uint8_t color[4];};
struct alignas(16) Gun {float rectangle[4],color[4];};
struct GPU {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();FCMetal4Queue *producer;
    id<MTLLibrary> library;id<MTL4Compiler> compiler;
    id<MTLRenderPipelineState> quad,overlay,opaqueOverlay,ui,opaqueUI,gun,opaqueGun;
    id<MTLSamplerState> nearest,linear;id<MTLDepthStencilState> disabledDepth;
    id<MTLTexture> source,icon;
    GPU() {
        require(device!=nil,"No GPU");producer=[[FCMetal4Queue alloc] initWithDevice:device];NSError *e=nil;
        compiler=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&e];error(e);
        auto d=[MTL4LibraryDescriptor new];d.source=[NSString stringWithUTF8String:MetalShaderSource];d.options=metalShaderOptions(device);
        library=[compiler newLibraryWithDescriptor:d error:&e];error(e);
        quad=pipeline(@"quad_vertex",@"quad_fragment",false,false);overlay=pipeline(@"quad_vertex",@"quad_fragment",true,false);opaqueOverlay=pipeline(@"quad_vertex",@"quad_fragment",true,true);
        gun=pipeline(@"overlay_vertex",@"overlay_fragment",true,false);opaqueGun=pipeline(@"overlay_vertex",@"overlay_fragment",true,true);
        ui=pipeline(@"ui_vertex",@"ui_fragment",true,false,true);opaqueUI=pipeline(@"ui_vertex",@"ui_fragment",true,true,true);
        auto sampler=[MTLSamplerDescriptor new];sampler.supportArgumentBuffers=YES;sampler.sAddressMode=sampler.tAddressMode=MTLSamplerAddressModeClampToEdge;
        sampler.minFilter=sampler.magFilter=MTLSamplerMinMagFilterNearest;nearest=[device newSamplerStateWithDescriptor:sampler];sampler.minFilter=sampler.magFilter=MTLSamplerMinMagFilterLinear;linear=[device newSamplerStateWithDescriptor:sampler];
        auto depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=MTLCompareFunctionAlways;disabledDepth=[device newDepthStencilStateWithDescriptor:depth];
        source=pattern(13,9,false);icon=pattern(7,5,true);
    }
    id<MTLRenderPipelineState> pipeline(NSString *vertex,NSString *fragment,bool blend,bool opaque,bool uiLayout=false) {
        auto function=[&](NSString *name){auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=name;return f;};
        auto d=[MTL4RenderPipelineDescriptor new];d.vertexFunctionDescriptor=function(vertex);d.fragmentFunctionDescriptor=function(fragment);auto c=d.colorAttachments[0];c.pixelFormat=MTLPixelFormatBGRA8Unorm;c.blendingState=MTL4BlendStateDisabled;
        if(blend){c.blendingState=MTL4BlendStateEnabled;c.sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;c.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;c.sourceAlphaBlendFactor=uiLayout?MTLBlendFactorOne:MTLBlendFactorSourceAlpha;c.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;}
        if(opaque)c.writeMask=MTLColorWriteMaskRed|MTLColorWriteMaskGreen|MTLColorWriteMaskBlue;
        if(uiLayout) {
            auto layout=[MTLVertexDescriptor new];layout.attributes[0].format=MTLVertexFormatFloat2;layout.attributes[0].offset=0;layout.attributes[1].format=MTLVertexFormatFloat2;layout.attributes[1].offset=8;layout.attributes[2].format=MTLVertexFormatUChar4Normalized;layout.attributes[2].offset=16;layout.layouts[0].stride=sizeof(UIVertex);d.vertexDescriptor=layout;
        }
        NSError *e=nil;auto result=[compiler newRenderPipelineStateWithDescriptor:d compilerTaskOptions:nil error:&e];error(e);require(result!=nil,"Cannot compile presentation pipeline");return result;
    }
    id<MTLTexture> texture(NSUInteger width,NSUInteger height,MTLPixelFormat format=MTLPixelFormatBGRA8Unorm) {
        auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.hazardTrackingMode=MTLHazardTrackingModeUntracked;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;return [device newTextureWithDescriptor:d];
    }
    id<MTLTexture> pattern(NSUInteger width,NSUInteger height,bool alpha,bool swizzle=false) {
        auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead;
        if(swizzle)d.swizzle=MTLTextureSwizzleChannelsMake(MTLTextureSwizzleBlue,MTLTextureSwizzleRed,MTLTextureSwizzleGreen,MTLTextureSwizzleAlpha);
        auto image=[device newTextureWithDescriptor:d];std::vector<uint8_t> data(width*height*4);
        for(NSUInteger y=0;y<height;++y)for(NSUInteger x=0;x<width;++x){auto *p=data.data()+(y*width+x)*4;p[0]=uint8_t(x*17+y*3);p[1]=uint8_t(x*7+y*23);p[2]=uint8_t(x*11+y*19);p[3]=alpha?uint8_t(20+(x*31+y*17)%210):255;}
        [image replaceRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0 withBytes:data.data() bytesPerRow:width*4];return image;
    }
    FCMetal4RenderEncoder *pass(FCMetal4Commands *cb,id<MTLTexture> output,MTLClearColor color=MTLClearColorMake(.08,.15,.22,1)) {
        auto p=[MTL4RenderPassDescriptor new];p.colorAttachments[0].texture=output;p.colorAttachments[0].loadAction=MTLLoadActionClear;p.colorAttachments[0].storeAction=MTLStoreActionStore;p.colorAttachments[0].clearColor=color;return [cb renderCommandEncoderWithDescriptor:p];
    }
    void copyQuad(FCMetal4RenderEncoder *e,id<MTLTexture> image,NSUInteger width,NSUInteger height) {
        const Quad u{{1,1,1,1},0,1};[e setRenderPipelineState:quad];[e setCullMode:MTLCullModeNone];[e setViewport:MTLViewport{0,0,double(width),double(height),0,1}];[e setSharedBytes:&u length:sizeof(u) atIndex:0];[e setFragmentTexture:image atIndex:0];[e setFragmentSamplerState:nearest atIndex:0];[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
    }
    void finish(FCMetal4Commands *cb){[cb commit];[cb waitUntilCompleted];error(cb.error);}
    std::vector<uint8_t> pixels(id<MTLTexture> image) {
        const NSUInteger row=(image.width*4+255)&~NSUInteger(255);auto data=[device newBufferWithLength:row*image.height options:MTLResourceStorageModeShared];auto cb=[producer commandBuffer];auto e=[cb transferEncoder];
        [e copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(image.width,image.height,1) toBuffer:data destinationOffset:0 destinationBytesPerRow:row destinationBytesPerImage:data.length];[e endEncoding];finish(cb);
        std::vector<uint8_t> out(image.width*image.height*4);for(NSUInteger y=0;y<image.height;++y)std::memcpy(out.data()+y*image.width*4,static_cast<const uint8_t *>(data.contents)+y*row,image.width*4);return out;
    }
    FCMetal4RecordingEncoder *recording() {
        auto e=[[FCMetal4RecordingEncoder alloc] initWithOpaquePipelines:nil];[e registerPipeline:quad opaque:quad];[e registerPipeline:overlay opaque:opaqueOverlay];[e registerPipeline:ui opaque:opaqueUI];[e registerPipeline:gun opaque:opaqueGun];return e;
    }
    void scene(FCMetal4RenderEncoder *e,id<MTLTexture> game,NSUInteger width,NSUInteger height,unsigned flags) {
        [e setLabel:@"presentation snapshot fixture"];[e setCullMode:MTLCullModeNone];[e setFrontFacingWinding:MTLWindingCounterClockwise];[e setDepthStencilState:disabledDepth];[e setStencilReferenceValue:0];
        const bool rotate=flags&1,filtered=flags&2,shift=flags&4,letterbox=flags&8;
        const NSUInteger dx=letterbox?width/7:0,dy=letterbox?height/9:0;
        [e setScissorRect:MTLScissorRect{dx,dy,width-2*dx,height-2*dy}];[e setViewport:MTLViewport{double(dx)+(shift?2.25:0),double(dy)+(shift?-1.5:0),double(width-2*dx),double(height-2*dy),0,1}];
        Quad value{{1,1,1,1},uint32_t(rotate),1};[e setRenderPipelineState:quad];[e setSharedBytes:&value length:sizeof(value) atIndex:0];std::memset(&value,0,sizeof(value));
        [e setFragmentTexture:game atIndex:0];[e setFragmentSamplerState:filtered?linear:nearest atIndex:0];[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];[e setScissorRect:MTLScissorRect{0,0,width,height}];
        if(!(flags&16))return;
        value={{.75,.85,.65,.75},0,0};[e setRenderPipelineState:overlay];[e setViewport:MTLViewport{2,3,double(width/4),double(height/3),0,1}];[e setSharedBytes:&value length:sizeof(value) atIndex:0];std::memset(&value,0,sizeof(value));[e setFragmentTexture:icon atIndex:0];[e setFragmentSamplerState:nearest atIndex:0];[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:1];
        Gun guns[]={{{float(width/2),float(height/3),13,11},{1,.2,.6,.55}},{{float(width/3),float(height/2),7,9},{.2,1,.4,.7}}};float screen[]={float(width),float(height)};
        [e setRenderPipelineState:gun];[e setViewport:MTLViewport{0,0,double(width),double(height),0,1}];[e setVertexBytes:guns length:sizeof(guns) atIndex:0];[e setVertexBytes:screen length:sizeof(screen) atIndex:1];std::memset(guns,0,sizeof(guns));[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:2];
        UIVertex vertices[]={ {0,0,0,0,{210,140,200,171}},{float(width/2),0,1,0,{190,230,110,199}},{float(width/2),float(height/4),1,1,{210,140,200,171}},{0,float(height/4),0,1,{210,140,200,171}},
            {float(width/3),float(height/2),0,0,{210,140,200,171}},{float(width-1),float(height/2),1,0,{210,140,200,171}},{float(width-1),float(height-1),1,1,{210,140,200,171}},{float(width/3),float(height-1),0,1,{210,140,200,171}}};
        const uint16_t indices[]={0,1,2,0,2,3};auto vb=[device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceStorageModeShared];auto ib=[device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeShared];
        [e setRenderPipelineState:ui];float transform[]={2.f/width,-2.f/height,-1,1};[e setVertexBytes:transform length:sizeof(transform) atIndex:1];[e setVertexBuffer:vb offset:0 atIndex:0];[e setFragmentTexture:icon atIndex:0];[e setFragmentSamplerState:linear atIndex:0];[e setScissorRect:MTLScissorRect{3,2,width-5,height-4}];
        [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:6 indexType:MTLIndexTypeUInt16 indexBuffer:ib indexBufferOffset:0];[e setVertexBufferOffset:4*sizeof(UIVertex) atIndex:0];
        [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:6 indexType:MTLIndexTypeUInt16 indexBuffer:ib indexBufferOffset:0 instanceCount:1 baseVertex:0 baseInstance:1];
    }
};
struct Display {
    std::shared_ptr<MetalDisplayState> state=std::make_shared<MetalDisplayState>();
    OffscreenLayer *layer=[OffscreenLayer new];OffscreenQueue *queue;FCMetalDisplayDelegate *delegate;DisplayTick *tick=[DisplayTick new];
    Display(GPU& g,NSUInteger w,NSUInteger h) {
        queue=[[OffscreenQueue alloc] initWithDevice:g.device];layer.device=g.device;layer.width=w;layer.height=h;
        state->queue=queue;state->layer=(CAMetalLayer *)layer;state->pipeline=g.quad;state->sampler=g.nearest;state->submissions=dispatch_group_create();state->active=true;state->repeats=true;
        delegate=[[FCMetalDisplayDelegate alloc] initWithState:state];tick.timestamp=99;tick.targetTimestamp=100;
    }
    ~Display(){wait();state->active=false;state->pending.clear();state->visible.reset();queue.lastCommands=nil;}
    void update(){[delegate displayUpdate:(CADisplayLink *)tick];}
    void wait(){dispatch_group_wait(state->submissions,DISPATCH_TIME_FOREVER);if(queue.lastCommands)error(queue.lastCommands.error);}
    void publish(const std::shared_ptr<MetalDisplayImage>& image){std::lock_guard<std::mutex> lock(state->mutex);state->pending.push_back(image);}
};
static std::shared_ptr<MetalDisplayImage> image(GPU& g,NSUInteger w,NSUInteger h,unsigned flags,id<MTLTexture> input=nil,dispatch_semaphore_t available=nil) {
    auto lease=metal_presentation::leaseImage(available ?: dispatch_semaphore_create(1));require(lease!=nullptr,"Cannot lease image");lease->width=w;lease->height=h;lease->clearColor=MTLClearColorMake(.08,.15,.22,1);lease->readyEvent=[g.device newSharedEvent];lease->readyValue=1;lease->target=1;lease->content=flags+1;
    input=input ?: g.source;auto cb=[g.producer commandBuffer];
    auto recorder=g.recording();[recorder deferSnapshotOf:input];g.scene(recorder,input,w,h,flags);
    lease->texture=metal_presentation::snapshotTexture(g.device,input,nil);metal_presentation::encodeSnapshot(input,lease->texture,cb);
    [recorder useSnapshot:lease->texture];[recorder endEncoding];lease->recording=recorder.recordedPass;[cb signalEvent:lease->readyEvent value:1];g.finish(cb);return lease;
}
static id<MTLTexture> reference(GPU& g,NSUInteger w,NSUInteger h,unsigned flags,NSUInteger outputW=0,NSUInteger outputH=0,id<MTLTexture> source=nil) {
    auto composed=g.texture(w,h),output=g.texture(outputW?:w,outputH?:h);auto cb=[g.producer commandBuffer];auto e=g.pass(cb,composed);g.scene(e,source?:g.source,w,h,flags);[e endEncoding];e=g.pass(cb,output);g.copyQuad(e,composed,output.width,output.height);[e endEncoding];g.finish(cb);return output;
}
static void flightCount(dispatch_semaphore_t flight,unsigned expected) {
    unsigned count=0;while(count<4 && dispatch_semaphore_wait(flight,DISPATCH_TIME_NOW)==0)++count;
    for(unsigned i=0;i<count;++i)dispatch_semaphore_signal(flight);
    require(count==expected,"Display slot leaked or was returned twice");
}
static void failedPresentation(GPU& g) {
    for(NSUInteger failure=1;failure<=5;++failure){@autoreleasepool {
        Display display(g,80,64);display.state->repeats=false;
        auto packet=image(g,64,48,16);packet->readyEvent=[g.device newSharedEvent];packet->readyValue=2;
        auto expected=reference(g,64,48,16,80,64);display.publish(packet);
        display.queue.nextFailure=failure;
        @autoreleasepool {
            display.update();auto command=display.queue.lastCommands;
            const bool enqueued=command && command.status>=FCMetal4StatusCommitted;
            require(enqueued==(failure>=4),"Delegate lost pre-/post-enqueue failure boundary");
            if(enqueued){
                flightCount(display.state->flight,1);
                require(dispatch_group_wait(display.state->submissions,DISPATCH_TIME_NOW)!=0,"Delegate lost gated submission group");
                packet->readyEvent.signaledValue=2;[command waitUntilCompleted];
            }
            // Only this test queue retains the last wrapper. Releasing it
            // models the real queue's ownership when encoding was canceled.
            display.queue.lastCommands=nil;command=nil;
        }
        require(dispatch_group_wait(display.state->submissions,DISPATCH_TIME_NOW)==0,"Failed display left group entered");
        flightCount(display.state->flight,2);
        id<MTLTexture> failedCache=display.state->resizedComposition;
        const auto acquisitions=display.layer.acquisitions;
        packet->readyEvent.signaledValue=2;display.update();display.wait();
        require(display.layer.acquisitions==acquisitions+1 && display.layer.last.presents==1,"Failed output was not retried with DupeFrames off");
        require(display.state->resizedComposition && display.state->resizedComposition!=failedCache,"Failed resize composition was reused after feedback error");
        require(g.pixels(expected)==g.pixels(display.layer.last.texture),"Retry changed the recorded display image");
        flightCount(display.state->flight,2);
        const auto idleAcquisitions=display.layer.acquisitions;display.update();
        require(display.layer.acquisitions==idleAcquisitions,"Successful retry kept acquiring idle drawables");
    }}
    std::puts("PASS production display error recovery: allocation, pre-commit, pre-enqueue, post-enqueue and synthetic feedback failures; two flight slots conserved, group drained, failed resize cache discarded, exact retry with DupeFrames off and subsequent idle tick");
}
static void validation(GPU& g) {
    require(metal_presentation::snapshotFits(g.source,67,49),"Smaller presentation source rejected");
    require(!metal_presentation::snapshotFits(g.source,13,9) && !metal_presentation::snapshotFits(g.source,1,1),"Equal/larger presentation source accepted");
    require(!metal_presentation::snapshotFits(g.source,NSUIntegerMax,2) && !metal_presentation::snapshotFits(g.source,0,1),"Invalid presentation dimensions accepted");
    auto highPrecision=g.texture(2,2,MTLPixelFormatRGBA16Float);require(!metal_presentation::snapshotFits(highPrecision,67,49),"Unbudgeted presentation pixel format accepted");
    auto snapshot=metal_presentation::snapshotTexture(g.device,g.source,nil);
    require(metal_presentation::snapshotTexture(g.device,g.source,snapshot)==snapshot,"Unchanged source failed snapshot reuse");
    auto swizzled=g.pattern(13,9,false,true);require(metal_presentation::snapshotTexture(g.device,swizzled,snapshot)!=snapshot,"Changed swizzle reused incompatible snapshot");
    unsigned pairs=0;
    for(unsigned flags=0;flags<32;++flags) {
        auto source=(flags&1)?g.pattern(13,9,false,true):g.source;
        auto expected=reference(g,67,49,flags,0,0,source);auto packet=image(g,67,49,flags,source);Display display(g,67,49);display.publish(packet);display.update();display.wait();
        require(display.layer.last.presents==1,"Display delegate did not present");require(g.pixels(expected)==g.pixels(display.layer.last.texture),"Direct recorded display differs from composed reference");++pairs;
        if(flags%8==0) {
            display.layer.width=91;display.layer.height=61;display.update();display.wait();auto cached=display.state->resizedComposition;
            expected=reference(g,67,49,flags,91,61,source);require(g.pixels(expected)==g.pixels(display.layer.last.texture),"Resized recorded fallback differs");++pairs;
            display.update();display.wait();require(display.state->resizedComposition==cached,"Repeated resize fallback was reallocated");
        }
    }
    auto recorder=g.recording();g.scene(recorder,g.source,67,49,31);auto cb=[g.producer commandBuffer];auto target=g.texture(67,49);auto actual=g.pass(cb,target);
    [recorder redirectTo:actual];g.scene(recorder,g.source,67,49,0);[recorder endEncoding];g.finish(cb);
    auto base=g.texture(67,49);cb=[g.producer commandBuffer];actual=g.pass(cb,base);g.scene(actual,g.source,67,49,31);g.scene(actual,g.source,67,49,0);[actual endEncoding];g.finish(cb);require(g.pixels(base)==g.pixels(target),"Producer callback redirect changed recorded prefix/state");
    bool rejected=false;try{[recorder setViewport:MTLViewport{0,0,1,1,0,1}];}catch(const std::exception&){rejected=true;}require(rejected,"Sealed recording accepted mutation");
    auto unknown=[[FCMetal4RecordingEncoder alloc] initWithOpaquePipelines:nil];g.scene(unknown,g.source,67,49,0);[unknown endEncoding];require(unknown.recordedPass.requiresComposition,"Unknown pipeline was trusted for direct opaque output");
    auto gate=dispatch_semaphore_create(1);auto first=metal_presentation::leaseImage(gate),extra=first;require(!metal_presentation::leaseImage(gate),"Leased UI slot reused early");first.reset();require(!metal_presentation::leaseImage(gate),"Shared lease released before final owner");extra.reset();require(metal_presentation::leaseImage(gate)!=nullptr,"Final owner failed to release UI slot");
    std::printf("PASS recorded display: %u exact full RGBA pairs, actual production shaders/delegate, rotation/filter/shift/windowbox, swizzled snapshots, VMU/guns/indexed UI, opaque alpha, resize cache/repeats, callback redirect, seal/unknown-pipeline fallback and lease ownership\n",pairs);
}
static void deferredGuards(GPU& g) {
    auto rejected=[&](auto action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,"Invalid deferred snapshot transition accepted");};
    auto recorder=g.recording();[recorder deferSnapshotOf:g.source];g.scene(recorder,g.source,67,49,31);
    rejected([&]{[recorder endEncoding];});
    rejected([&]{[recorder deferSnapshotOf:g.source];});
    rejected([&]{[recorder useSnapshot:g.source];});
    auto wrong=g.texture(2,2);rejected([&]{[recorder useSnapshot:wrong];});
    auto snapshot=metal_presentation::snapshotTexture(g.device,g.source,nil);
    auto cb=[g.producer commandBuffer];metal_presentation::encodeSnapshot(g.source,snapshot,cb);
    [recorder useSnapshot:snapshot];[recorder endEncoding];
    rejected([&]{[recorder useSnapshot:snapshot];});g.finish(cb);
    auto late=g.recording();g.scene(late,g.source,67,49,0);rejected([&]{[late deferSnapshotOf:g.source];});[late endEncoding];
    FCMetal4RecordedPass *abandoned;
    @autoreleasepool {
        auto pending=g.recording();[pending deferSnapshotOf:g.source];g.scene(pending,g.source,67,49,0);
        abandoned=pending.recordedPass;pending=nil;
    }
    auto canceledTarget=g.texture(67,49);cb=[g.producer commandBuffer];auto canceledEncoder=g.pass(cb,canceledTarget);
    rejected([&]{[abandoned encodeTo:canceledEncoder opaque:NO];});[canceledEncoder endEncoding];g.finish(cb);
    for(bool unknown:{false,true}) {
        auto pending=unknown?[[FCMetal4RecordingEncoder alloc] initWithOpaquePipelines:nil]:g.recording();
        [pending deferSnapshotOf:g.source];g.scene(pending,g.source,67,49,31);
        if(unknown)require(pending.recordedPass.requiresComposition,"Unknown pipeline lost fallback requirement");
        auto actual=g.texture(67,49);cb=[g.producer commandBuffer];auto e=g.pass(cb,actual);
        [pending redirectTo:e];rejected([&]{[pending useSnapshot:snapshot];});[pending endEncoding];g.finish(cb);
        auto expected=g.texture(67,49);cb=[g.producer commandBuffer];e=g.pass(cb,expected);g.scene(e,g.source,67,49,31);[e endEncoding];g.finish(cb);
        require(g.pixels(actual)==g.pixels(expected),"Deferred producer fallback changed complete RGBA image");
    }
    std::puts("PASS deferred snapshot: mutable-source seal rejected; duplicate/late registration, source-as-snapshot, incompatible texture, post-seal/post-redirect mutation and abandoned-recording replay rejected; callback and unknown-pipeline fallback exact");
}
static void busyAndSnapshots(GPU& g) {
    auto available=dispatch_semaphore_create(1);
    Display display(g,67,49);auto packet=image(g,67,49,31,nil,available);auto next=image(g,67,49,0);next->content=900;
    auto gate=[g.device newSharedEvent];[display.queue.native waitForEvent:gate value:1];display.publish(packet);display.update();auto firstCommands=display.queue.lastCommands;
    display.state->repeats=false;const auto acquisitions=display.layer.acquisitions;display.update();require(display.layer.acquisitions==acquisitions,"Idle non-repeating display acquired a drawable");
    display.state->repeats=true;display.update();display.publish(next);display.update();
    const bool preserved=display.state->pending.size()==1 && display.state->visible==packet && display.state->busy==1;
    // Mutate the original renderer image while the display GPU is stopped.
    std::vector<uint8_t> changed(g.source.width*g.source.height*4,213);[g.source replaceRegion:MTLRegionMake2D(0,0,g.source.width,g.source.height) mipmapLevel:0 withBytes:changed.data() bytesPerRow:g.source.width*4];
    auto frozen=packet->texture;packet.reset();display.state->visible.reset();
    require(!metal_presentation::leaseImage(available),"GPU readers lost the presentation lease when CPU owners cleared");
    gate.signaledValue=1;display.wait();[firstCommands waitUntilCompleted];[display.queue.lastCommands waitUntilCompleted];require(preserved,"Busy display consumed an unpresented source image");
    require(metal_presentation::leaseImage(available)!=nullptr,"Completed GPU readers retained presentation lease");
    auto expected=reference(g,67,49,31,0,0,frozen);require(g.pixels(expected)==g.pixels(display.layer.last.texture),"Renderer mutation changed frozen display snapshot");
    display.update();display.wait();require(display.state->visible==next && display.state->pending.empty(),"Next content image was not consumed in source order");
    std::puts("PASS display ownership: two in-flight replays, busy reservation preserves pending order, repeats on/off, frozen source, lease held through GPU reads after CPU owners clear and released at completion");
}
static void deadlineController(GPU& g) {
    uint64_t newImages=0,outputs=0;
    for(double videoHz:{50.,60000./1001,60.})for(unsigned interval:{1u,2u})
    for(unsigned hostHz:{60u,120u})for(bool repeats:{false,true}) {@autoreleasepool {
        // Two independent production display queues share immutable packets.
        // Host timestamps are synthetic. This does not measure real VBlank.
        Display first(g,67,49),second(g,67,49);first.state->repeats=second.state->repeats=repeats;
        unsigned produced=0,changed=0;uint64_t previous=0;
        for(unsigned tick=0;tick<=2*hostHz;++tick) {@autoreleasepool {
            const double deadline=100.+double(tick)/hostHz;
            const unsigned expected=deadline<100.002?0:unsigned(std::floor((deadline-100.002)*videoHz/interval+1e-8))+1;
            if(produced<expected) {
                require(expected==produced+1,"Cadence fixture needs a host slower than content");
                auto packet=image(g,67,49,16);packet->content=++produced;packet->target=100.002+(produced-1)*interval/videoHz;
                first.publish(packet);second.publish(packet);++newImages;
            }
            const auto before=first.layer.acquisitions;
            for(auto display:{&first,&second}){display->tick.targetTimestamp=deadline;display->tick.timestamp=deadline-1./hostHz;display->update();}
            first.wait();second.wait();
            const uint64_t visible=first.state->visible?first.state->visible->content:0;
            require(visible==expected && (second.state->visible?second.state->visible->content:0)==expected,"Display changed the source-content cadence/order");
            require(first.state->pending.empty() && second.state->pending.empty(),"Due content accumulated despite available display capacity");
            if(visible!=previous){++changed;previous=visible;}
            // Independent count: each new content presents once, or every host
            // tick after the first image when explicit repetition is enabled.
            const uint64_t expectedOutputs=repeats && expected?tick:produced;
            require(first.layer.acquisitions==expectedOutputs && second.layer.acquisitions==expectedOutputs,"Duplicate/idle drawable acquisition policy changed");
            outputs+=first.layer.acquisitions-before;
        }}
        require(changed==produced,"Not every source content image reached display");
    }}
    std::printf("PASS deadline controller: 24 PAL/NTSC 50/59.94/60 Hz and interval-2 cases, synthetic 60/120 Hz ticks, repeats on/off, two independent queues, %llu new shared images and %llu outputs per view, no skipped source images\n",(unsigned long long)newImages,(unsigned long long)outputs);
}
static void fxSnapshots(GPU& g) {
    MetalFXUpscaler fx(g.device,g.compiler);require(fx.supported(),"Metal 4 FX unavailable: cannot claim FX presentation coverage");
    for(unsigned i=0;i<6;++i) {@autoreleasepool {
        const NSUInteger side=i<3?16:20;const size_t slot=i%MetalContext::PresentationSlots;
        auto input=g.pattern(side,side,false);auto producer=[g.producer commandBuffer];
        if(i&1){auto privateInput=metal_presentation::snapshotTexture(g.device,input,nil);metal_presentation::encodeSnapshot(input,privateInput,producer);input=privateInput;}
        auto scaled=fx.upscale(input,producer,side*2,side*2,slot,i*2+1);require(scaled!=nil,"FX presentation did not encode");
        auto packet=metal_presentation::leaseImage(dispatch_semaphore_create(1));packet->width=67;packet->height=49;packet->target=1;packet->content=i+1;
        packet->clearColor=MTLClearColorMake(.08,.15,.22,1);packet->readyEvent=[g.device newSharedEvent];packet->readyValue=1;
        auto encoder=g.recording();[encoder deferSnapshotOf:scaled];g.scene(encoder,scaled,67,49,31);
        packet->texture=metal_presentation::snapshotTexture(g.device,scaled,nil);metal_presentation::encodeSnapshot(scaled,packet->texture,producer);
        [encoder useSnapshot:packet->texture];[encoder endEncoding];packet->recording=encoder.recordedPass;
        [producer signalEvent:packet->readyEvent value:1];g.finish(producer);
        auto expected=reference(g,67,49,31,0,0,scaled);
        Display display(g,67,49);auto gate=[g.device newSharedEvent];[display.queue.native waitForEvent:gate value:1];display.publish(packet);display.update();
        // Reuse the actual scaler output while the display GPU still waits.
        auto changed=g.pattern(side,side,true);producer=[g.producer commandBuffer];
        auto rewritten=fx.upscale(changed,producer,side*2,side*2,slot,i*2+2);require(rewritten==scaled,"FX fixture did not overwrite the same scaler output");
        fx.releaseSlot(slot);g.finish(producer);gate.signaledValue=1;display.wait();
        require(g.pixels(expected)==g.pixels(display.layer.last.texture),"MetalFX output reuse changed frozen display image");
    }}
    std::puts("PASS production MetalFX presentation: six exact full RGBA pairs, three scaler slots, shared/private input, resize, output overwritten and slot released while display waits");
}
// Producer plus display work, including repeated outputs of the same content.
// CoreAnimation acquisition is substituted above: these are offscreen encoder/
// GPU-feedback costs, not display latency, bandwidth or real presentation times.
struct BenchPath {
    struct Slot {id<MTLTexture> texture,composed;id<MTLSharedEvent> event;uint64_t value=0;dispatch_semaphore_t available=dispatch_semaphore_create(1);};
    GPU& g;Display display;std::array<Slot,3> slots;NSUInteger width,height;bool recorded;unsigned serial=0;
    id<MTLTexture> source;
    bool lateFallback,deferred;
    uint64_t transientSnapshotBytes=0;
    BenchPath(GPU& gpu,NSUInteger w,NSUInteger h,id<MTLTexture> input,bool direct,bool fallback=false,bool defer=false):g(gpu),display(gpu,w,h),width(w),height(h),recorded(direct),source(input),lateFallback(fallback),deferred(defer) {
        for(auto& slot:slots){slot.texture=recorded?(lateFallback?nil:metal_presentation::snapshotTexture(g.device,input,nil)):g.texture(w,h);if(lateFallback)slot.composed=g.texture(w,h);slot.event=[g.device newSharedEvent];}
    }
    struct Cost {double cpu=0,gpu=0,producerGPU=0,displayGPU=0;};
    Cost sample(unsigned flags,unsigned repeats) {
        @autoreleasepool {
            const auto start=std::chrono::steady_clock::now();auto& slot=slots[serial++%slots.size()];
            auto packet=metal_presentation::leaseImage(slot.available);require(packet!=nullptr,"Benchmark reused a leased presentation slot");
            if(lateFallback && !deferred){slot.texture=metal_presentation::snapshotTexture(g.device,source,nil);transientSnapshotBytes=slot.texture.allocatedSize;}
            packet->texture=slot.texture;packet->width=width;packet->height=height;packet->clearColor=MTLClearColorMake(.08,.15,.22,1);
            packet->readyEvent=slot.event;packet->readyValue=++slot.value;packet->target=1;packet->content=serial;
            auto producer=[g.producer commandBuffer];
            if(recorded) {
                auto encoder=g.recording();
                if(deferred)[encoder deferSnapshotOf:source];
                else metal_presentation::encodeSnapshot(source,packet->texture,producer);
                g.scene(encoder,deferred?source:packet->texture,width,height,flags);
                if(lateFallback) {
                    // Same late callback/unknown-pipeline transition as
                    // requirePresentationComposition: replay the recorded
                    // prefix on the producer after its source snapshot.
                    auto immediate=g.pass(producer,slot.composed);[encoder redirectTo:immediate];
                    packet->texture=slot.composed;
                } else {
                    if(deferred){metal_presentation::encodeSnapshot(source,packet->texture,producer);[encoder useSnapshot:packet->texture];}
                    packet->recording=encoder.recordedPass;
                }
                [encoder endEncoding];
            } else {auto encoder=g.pass(producer,packet->texture);g.scene(encoder,source,width,height,flags);[encoder endEncoding];}
            [producer signalEvent:packet->readyEvent value:packet->readyValue];[producer commit];display.publish(packet);
            // Production drops the cached source snapshot after selecting
            // composition. The submitted command still owns its GPU reader.
            if(lateFallback)slot.texture=nil;
            Cost result;result.cpu=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
            for(unsigned repeat=0;repeat<repeats;++repeat) {
                const auto outputStart=std::chrono::steady_clock::now();const auto before=display.layer.acquisitions;
                display.update();result.cpu+=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-outputStart).count();
                require(display.layer.acquisitions==before+1,"Benchmark failed to submit an output");
                display.wait();[display.queue.lastCommands waitUntilCompleted];
                result.displayGPU+=(display.queue.lastCommands.GPUEndTime-display.queue.lastCommands.GPUStartTime)*1e6;
            }
            [producer waitUntilCompleted];error(producer.error);result.producerGPU=(producer.GPUEndTime-producer.GPUStartTime)*1e6;
            result.gpu=result.producerGPU+result.displayGPU;require(result.producerGPU>0 && result.displayGPU>0,"GPU feedback time unavailable");return result;
        }
    }
    uint64_t intermediateBytes()const {uint64_t bytes=0;for(const auto& slot:slots)bytes+=slot.texture.allocatedSize+slot.composed.allocatedSize;return bytes;}
};
static double median(std::vector<double> values){std::sort(values.begin(),values.end());return values[values.size()/2];}
static void benchmark(GPU& g) {
    for(auto dimensions:{std::array<NSUInteger,2>{1920,1080},std::array<NSUInteger,2>{2560,1440},std::array<NSUInteger,2>{3840,2160}})
    for(unsigned sourceSize:{0u,1u,2u,3u,4u,5u})for(unsigned overlays:{0u,16u})for(unsigned repeats:{1u,4u}) {@autoreleasepool {
        const NSUInteger sourceWidth=sourceSize==0?640:sourceSize==1?dimensions[0]:sourceSize==2?dimensions[0]*3/4:sourceSize==3?dimensions[0]-16:dimensions[0]/2;
        const NSUInteger sourceHeight=sourceSize==0?480:sourceSize==2?dimensions[1]*3/4:sourceSize==4?dimensions[1]/2:dimensions[1];
        auto input=g.pattern(sourceWidth,sourceHeight,false);
        // Production renderer/MetalFX output is private, rather than shared CPU memory.
        auto source=metal_presentation::snapshotTexture(g.device,input,nil);auto upload=[g.producer commandBuffer];metal_presentation::encodeSnapshot(input,source,upload);g.finish(upload);
        BenchPath before(g,dimensions[0],dimensions[1],source,false),after(g,dimensions[0],dimensions[1],source,true);
        std::vector<double> oldCPU,newCPU,oldGPU,newGPU,oldProducer,newProducer,oldDisplay,newDisplay;
        for(unsigned pair=0;pair<35;++pair) {
            BenchPath::Cost a,b;if(pair&1){b=after.sample(overlays,repeats);a=before.sample(overlays,repeats);}else{a=before.sample(overlays,repeats);b=after.sample(overlays,repeats);}
            if(pair>=4){oldCPU.push_back(a.cpu);newCPU.push_back(b.cpu);oldGPU.push_back(a.gpu);newGPU.push_back(b.gpu);oldProducer.push_back(a.producerGPU);newProducer.push_back(b.producerGPU);oldDisplay.push_back(a.displayGPU);newDisplay.push_back(b.displayGPU);}
        }
        // Full RGBA comparison outside the timing region, at every measured size.
        require(g.pixels(before.display.layer.last.texture)==g.pixels(after.display.layer.last.texture),"High-resolution benchmark final images differ");
        std::printf("BENCH {\"width\":%lu,\"height\":%lu,\"source_width\":%lu,\"source_height\":%lu,\"overlays\":%s,\"outputs_per_content\":%u,\"pairs\":31,\"warmups\":4,\"cpu_us\":[%.3f,%.3f],\"gpu_feedback_us\":[%.3f,%.3f],\"producer_gpu_us\":[%.3f,%.3f],\"display_gpu_us\":[%.3f,%.3f],\"three_slot_allocated_bytes\":[%llu,%llu],\"exact_rgba\":true}\n",
            (unsigned long)dimensions[0],(unsigned long)dimensions[1],(unsigned long)source.width,(unsigned long)source.height,overlays?"true":"false",repeats,
            median(oldCPU),median(newCPU),median(oldGPU),median(newGPU),median(oldProducer),median(newProducer),median(oldDisplay),median(newDisplay),
            (unsigned long long)before.intermediateBytes(),(unsigned long long)after.intermediateBytes());std::fflush(stdout);
    }}
}
static void fallbackBenchmark(GPU& g,bool timing,bool compareDeferred=false) {
    for(auto dimensions:{std::array<NSUInteger,2>{1920,1080},std::array<NSUInteger,2>{3840,2160}})
    for(unsigned size:{0u,1u,2u})for(unsigned overlays:{0u,16u})for(unsigned repeats:{1u,4u})for(bool late:{false,true}) {@autoreleasepool {
        if(!compareDeferred && !late)continue;
        const NSUInteger w=dimensions[0],h=dimensions[1];
        const NSUInteger sw=size==0?640:size==1?w*3/4:w-16,sh=size==0?480:h;
        auto input=g.pattern(sw,sh,false);require(metal_presentation::snapshotFits(input,w,h),"Late fallback fixture bypasses production recording eligibility");
        auto source=metal_presentation::snapshotTexture(g.device,input,nil);auto upload=[g.producer commandBuffer];metal_presentation::encodeSnapshot(input,source,upload);g.finish(upload);
        BenchPath composed(g,w,h,source,compareDeferred,compareDeferred && late),fallback(g,w,h,source,true,late,compareDeferred);
        std::vector<double> oldCPU,newCPU,oldGPU,newGPU,oldProducer,newProducer,oldDisplay,newDisplay;
        const unsigned warmups=timing?4:0,pairs=timing?31:1;
        for(unsigned pair=0;pair<warmups+pairs;++pair) {
            BenchPath::Cost a,b;if(pair&1){b=fallback.sample(overlays,repeats);a=composed.sample(overlays,repeats);}else{a=composed.sample(overlays,repeats);b=fallback.sample(overlays,repeats);}
            // Every sample must match, including recycled snapshot/target slots.
            require(g.pixels(composed.display.layer.last.texture)==g.pixels(fallback.display.layer.last.texture),"Late fallback changed full RGBA image");
            if(pair>=warmups){oldCPU.push_back(a.cpu);newCPU.push_back(b.cpu);oldGPU.push_back(a.gpu);newGPU.push_back(b.gpu);oldProducer.push_back(a.producerGPU);newProducer.push_back(b.producerGPU);oldDisplay.push_back(a.displayGPU);newDisplay.push_back(b.displayGPU);}
        }
        std::printf("FALLBACK {\"deferred_comparison\":%s,\"late_fallback\":%s,\"baseline_transient_snapshot_bytes\":%llu,\"width\":%lu,\"height\":%lu,\"source_width\":%lu,\"source_height\":%lu,\"overlays\":%s,\"outputs_per_content\":%u,\"pairs\":%u,\"warmups\":%u,\"cpu_us\":[%.3f,%.3f],\"gpu_feedback_us\":[%.3f,%.3f],\"producer_gpu_us\":[%.3f,%.3f],\"display_gpu_us\":[%.3f,%.3f],\"three_slot_cached_bytes\":[%llu,%llu],\"transient_snapshot_bytes\":%llu,\"exact_rgba\":true}\n",
            compareDeferred?"true":"false",late?"true":"false",(unsigned long long)composed.transientSnapshotBytes,
            (unsigned long)w,(unsigned long)h,(unsigned long)sw,(unsigned long)sh,overlays?"true":"false",repeats,pairs,warmups,
            median(oldCPU),median(newCPU),median(oldGPU),median(newGPU),median(oldProducer),median(newProducer),median(oldDisplay),median(newDisplay),
            (unsigned long long)composed.intermediateBytes(),(unsigned long long)fallback.intermediateBytes(),(unsigned long long)fallback.transientSnapshotBytes);std::fflush(stdout);
    }}
}
int main(int argc,char **argv){@autoreleasepool{try{GPU g;if(argc==2 && std::strcmp(argv[1],"--benchmark-only")==0)benchmark(g);else if(argc==2 && std::strcmp(argv[1],"--fallback-benchmark-only")==0)fallbackBenchmark(g,true);else if(argc==2 && std::strcmp(argv[1],"--fallback-validation-only")==0)fallbackBenchmark(g,false);else if(argc==2 && std::strcmp(argv[1],"--deferred-benchmark-only")==0)fallbackBenchmark(g,true,true);else if(argc==2 && std::strcmp(argv[1],"--deferred-validation-only")==0)fallbackBenchmark(g,false,true);else if(argc==2 && std::strcmp(argv[1],"--shutdown-only")==0)failedPresentation(g);else{validation(g);deferredGuards(g);deadlineController(g);fxSnapshots(g);busyAndSnapshots(g);failedPresentation(g);}return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}}
