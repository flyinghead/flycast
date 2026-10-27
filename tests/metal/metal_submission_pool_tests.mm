// SPDX-License-Identifier: GPL-2.0-or-later
// Real Metal 4 command/residency/table/page reuse, without a Flycast app.
#include "rend/metal4/metal4_commands.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <thread>
#include <string>
#include <stdexcept>
#include <vector>
#include <unistd.h>

static void require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
static void error(NSError *e,const char *stage="Metal") {if(e)throw std::runtime_error(std::string(stage)+": "+e.localizedDescription.UTF8String);}
// A table's null buffer slots cannot be read by a shader under API Validation.
// Inspect the actual setter calls through a device/table test double instead.
// GPU tests below separately check residency, output and feedback-safe reuse.
@interface FCMetal4Table : NSObject
- (instancetype)initWithDevice:(id<MTLDevice>)device family:(NSUInteger)family;
- (void)setAddress:(MTLGPUAddress)address atIndex:(NSUInteger)index;
- (void)setTexture:(MTLResourceID)texture atIndex:(NSUInteger)index;
- (void)setSamplerState:(MTLResourceID)sampler atIndex:(NSUInteger)index;
- (void)clearBindings;
@end
@interface TableSpy : NSObject {
@public
    MTLGPUAddress buffers[31];
    MTLResourceID textures[128],samplers[16];
    NSUInteger writes;
}
@end
@implementation TableSpy
- (void)setAddress:(MTLGPUAddress)value atIndex:(NSUInteger)index {require(index<31,"Invalid table buffer index");buffers[index]=value;++writes;}
- (void)setTexture:(MTLResourceID)value atIndex:(NSUInteger)index {require(index<128,"Invalid table texture index");textures[index]=value;++writes;}
- (void)setSamplerState:(MTLResourceID)value atIndex:(NSUInteger)index {require(index<16,"Invalid table sampler index");samplers[index]=value;++writes;}
@end
@interface TableDeviceSpy : NSObject
@property (strong) TableSpy *table;
@property (strong) MTL4ArgumentTableDescriptor *descriptor;
@end
@implementation TableDeviceSpy
- (id)newArgumentTableWithDescriptor:(MTL4ArgumentTableDescriptor *)descriptor error:(NSError **)failure {
    _descriptor=[descriptor copy];_table=[TableSpy new];return _table;
}
@end
// Observe public native residency objects without extending their lifetime.
@interface FCMetal4Queue (ResidencyObservation)
- (id<MTLResidencySet>)newResidencySet;
@end
@interface PoolResidencySpy : FCMetal4Queue
@property(strong) NSHashTable *sets;
@end
@implementation PoolResidencySpy
- (id<MTLResidencySet>)newResidencySet {
    auto set=[super newResidencySet];[self.sets addObject:set];return set;
}
@end
static constexpr const char Source[]=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Out {float4 position [[position]];};
vertex Out v(uint id [[vertex_id]]) {
    const float2 xy[]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(xy[id],0,1)};
}
fragment float4 f(Out in [[stage_in]],constant uint& value [[buffer(1)]]) {
    return float4(float(value&255)/255, .5, .75, 1);
}
kernel void pool_write_value(device uint *out [[buffer(0)]],constant uint& value [[buffer(1)]],uint i [[thread_position_in_grid]]) {out[i]=value+i;}
)MSL";
struct GPU {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    id<MTLRenderPipelineState> render;
    id<MTLComputePipelineState> compute;
    GPU() {
        require(device!=nil,"No Metal GPU");NSError *failure=nil;
        auto compilerDescriptor=[MTL4CompilerDescriptor new];compilerDescriptor.label=@"Flycast pool test compiler";
        auto compiler=[device newCompilerWithDescriptor:compilerDescriptor error:&failure];error(failure,"Pool compiler");
        auto source=[MTL4LibraryDescriptor new];source.name=@"Flycast pool test shaders";source.source=[NSString stringWithUTF8String:Source];source.options=[MTLCompileOptions new];source.options.languageVersion=MTLLanguageVersion4_0;
        auto library=[compiler newLibraryWithDescriptor:source error:&failure];error(failure,"Pool library");
        auto function=[&](NSString *name) {auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=name;return f;};
        auto d=[MTL4RenderPipelineDescriptor new];d.vertexFunctionDescriptor=function(@"v");d.fragmentFunctionDescriptor=function(@"f");d.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
        render=[compiler newRenderPipelineStateWithDescriptor:d compilerTaskOptions:nil error:&failure];error(failure,"Pool render pipeline");
        auto c=[MTL4ComputePipelineDescriptor new];c.computeFunctionDescriptor=function(@"pool_write_value");compute=[compiler newComputePipelineStateWithDescriptor:c compilerTaskOptions:nil error:&failure];error(failure,"Pool compute pipeline");
        require(render && compute,"Pipeline creation failed");
    }
    id<MTLTexture> texture() {
        auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;return [device newTextureWithDescriptor:d];
    }
};
static double batch(GPU& gpu,FCMetal4Queue *queue,unsigned encoders,unsigned value,double *retirement=nullptr)
{
    @autoreleasepool {
    auto texture=gpu.texture();auto output=[gpu.device newBufferWithLength:512 options:MTLResourceStorageModeShared];
    const uint64_t before=retirement ? [queue submissionPoolStatistics][@"retirementNanoseconds"].unsignedLongLongValue : 0;
    auto started=std::chrono::steady_clock::now();
    auto cb=[queue commandBuffer];
    for(unsigned i=0;i<encoders;++i) {
        auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        auto e=[cb renderCommandEncoderWithDescriptor:pass];[e setRenderPipelineState:gpu.render];[e setViewport:MTLViewport{0,0,1,1,0,1}];
        const uint32_t c=(value+i)&255;[e setFragmentBytes:&c length:4 atIndex:1];
        // Sparse highest slots must be cleared as well as dense low bindings.
        [e setVertexBuffer:output offset:0 atIndex:30];[e setFragmentBuffer:output offset:0 atIndex:30];[e setFragmentTexture:texture atIndex:127];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[e endEncoding];
    }
    auto e=[cb computeCommandEncoder];[e setComputePipelineState:gpu.compute];[e setBuffer:output offset:0 atIndex:0];const uint32_t c=value;
    [e setBytes:&c length:4 atIndex:1];[e dispatchThreads:MTLSizeMake(16,1,1) threadsPerThreadgroup:MTLSizeMake(16,1,1)];[e endEncoding];
    auto transfer=[cb transferEncoder];
    [transfer copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(1,1,1) toBuffer:output destinationOffset:256 destinationBytesPerRow:256 destinationBytesPerImage:256];[transfer endEncoding];
    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();
    [cb commit];[cb waitUntilCompleted];error(cb.error);
    if(retirement)*retirement=([queue submissionPoolStatistics][@"retirementNanoseconds"].unsignedLongLongValue-before)/1000.0;
    const auto values=static_cast<const uint32_t *>(output.contents);for(unsigned i=0;i<16;++i)require(values[i]==value+i,"Recycled compute table/uniform/resource output differs");
    const auto pixel=static_cast<const uint8_t *>(output.contents)+256;
    require(pixel[0]==((value+encoders-1)&255) && pixel[1]==128 && pixel[2]==191 && pixel[3]==255,"Recycled render table/uniform/resource output differs");
    require(cb.status==FCMetal4StatusCompleted && cb.native==nil,"Completed wrapper was reused instead of its private storage");
    return us;
    }
}
static void zeroBindings(GPU&)
{
    for(NSUInteger family=0;family<3;++family) {
        auto device=[TableDeviceSpy new];auto table=[[FCMetal4Table alloc] initWithDevice:(id<MTLDevice>)device family:family];
        const NSUInteger textureCount=family==0?0:128,samplerCount=family==1?16:0;
        require(device.descriptor.maxBufferBindCount==31 && device.descriptor.maxTextureBindCount==textureCount
            && device.descriptor.maxSamplerStateBindCount==samplerCount && device.descriptor.initializeBindings,"Argument table family capacities differ");
        for(NSUInteger i=0;i<31;++i)[table setAddress:100+i atIndex:i];
        for(NSUInteger i=0;i<textureCount;++i)[table setTexture:MTLResourceID{100+i} atIndex:i];
        for(NSUInteger i=0;i<samplerCount;++i)[table setSamplerState:MTLResourceID{100+i} atIndex:i];
        // Explicitly unbinding a slot must leave it zero across the next lease.
        [table setAddress:0 atIndex:0];if(textureCount)[table setTexture:MTLResourceID{} atIndex:0];if(samplerCount)[table setSamplerState:MTLResourceID{} atIndex:0];
        [table clearBindings];
        for(auto value:device.table->buffers)require(value==0,"Pooled buffer slot was not cleared");
        for(auto value:device.table->textures)require(value._impl==0,"Pooled texture slot was not cleared");
        for(auto value:device.table->samplers)require(value._impl==0,"Pooled sampler slot was not cleared");
        const NSUInteger writes=device.table->writes;[table clearBindings];require(device.table->writes==writes,"Empty table performed redundant binding clears");
        [table setAddress:500 atIndex:30];if(textureCount)[table setTexture:MTLResourceID{501} atIndex:127];if(samplerCount)[table setSamplerState:MTLResourceID{502} atIndex:15];
        [table clearBindings];require(device.table->buffers[30]==0 && device.table->textures[127]._impl==0 && device.table->samplers[15]._impl==0,"Sparse highest binding was not cleared");
    }
}
static void cancellation(GPU& gpu)
{
    auto queue=[[FCMetal4Queue alloc] initWithDevice:gpu.device];
    for(unsigned i=0;i<20;++i) {
        __weak id<MTLBuffer> weakBuffer;
        @autoreleasepool {
            auto output=[gpu.device newBufferWithLength:256 options:MTLResourceStorageModeShared];weakBuffer=output;
            auto cb=[queue commandBuffer];auto e=[cb computeCommandEncoder];[e setComputePipelineState:gpu.compute];[e setBuffer:output offset:0 atIndex:0];
            const uint32_t c=i;[e setBytes:&c length:4 atIndex:1];[e dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[e endEncoding];
            // Deliberately release a fully encoded, never submitted batch.
        }
        require(weakBuffer==nil,"Canceled submission retained client resources");
    }
    auto stats=[queue submissionPoolStatistics];require(stats[@"created"].unsignedIntegerValue==1 && stats[@"reused"].unsignedIntegerValue==19,"Cancellation did not recycle its private lease");
    batch(gpu,queue,1,87);
    __block unsigned calls=0;auto cb=[queue commandBuffer];
    [cb addCompletedHandler:^(FCMetal4Commands *) {++calls;throw std::runtime_error("intentional callback test");}];
    [cb addCompletedHandler:^(FCMetal4Commands *) {++calls;}];[cb commit];[cb waitUntilCompleted];require(calls==2,"Callback failure lost subsequent callbacks");
    auto next=[queue commandBuffer];[next commit];[next waitUntilCompleted];require(calls==2,"Recycled submission replayed earlier callbacks");
}
static void inFlight(GPU& gpu)
{
    auto queue=[[FCMetal4Queue alloc] initWithDevice:gpu.device];auto gate=[gpu.device newSharedEvent];
    NSMutableArray<FCMetal4Commands *> *commands=[NSMutableArray new];NSMutableArray<id<MTLBuffer>> *outputs=[NSMutableArray new];NSMutableSet *native=[NSMutableSet new];
    for(unsigned i=0;i<16;++i) {
        auto output=[gpu.device newBufferWithLength:256 options:MTLResourceStorageModeShared];[outputs addObject:output];
        auto cb=[queue commandBuffer];[native addObject:cb.native];[commands addObject:cb];[cb waitForEvent:gate value:1];
        auto e=[cb computeCommandEncoder];[e setComputePipelineState:gpu.compute];[e setBuffer:output offset:0 atIndex:0];
        const uint32_t c=100+i;[e setBytes:&c length:4 atIndex:1];[e dispatchThreads:MTLSizeMake(16,1,1) threadsPerThreadgroup:MTLSizeMake(16,1,1)];[e endEncoding];[cb commit];
    }
    // Release the GPU before asserting, even when this regression fails.
    const auto waiting=[queue submissionPoolStatistics];gate.signaledValue=1;
    for(FCMetal4Commands *cb in commands){[cb waitUntilCompleted];error(cb.error);}
    require(native.count==16 && waiting[@"created"].unsignedIntegerValue==16 && waiting[@"available"].unsignedIntegerValue==0,"In-flight submissions shared command/table/page storage");
    for(unsigned i=0;i<16;++i)for(unsigned j=0;j<16;++j)require(static_cast<const uint32_t *>(outputs[i].contents)[j]==100+i+j,"In-flight uniform or argument table was overwritten");
    auto stats=[queue submissionPoolStatistics];require(stats[@"available"].unsignedIntegerValue==12,"Submission pool exceeded its bounded idle count");
    require(stats[@"pooledTables"].unsignedIntegerValue<=12*64*3 && stats[@"pooledUniformPages"].unsignedIntegerValue<=12*8,"Submission pool exceeded retained table/page caps");
    batch(gpu,queue,1,141);
}
static void pageAndTableLimits(GPU& gpu)
{
    auto queue=[[FCMetal4Queue alloc] initWithDevice:gpu.device];
    for(unsigned repetition=0;repetition<2;++repetition) {
        auto output=[gpu.device newBufferWithLength:256 options:MTLResourceStorageModeShared];auto cb=[queue commandBuffer];
        std::vector<uint32_t> values(16384);
        for(unsigned i=0;i<80;++i) {
            values.front()=i+13;auto e=[cb computeCommandEncoder];[e setComputePipelineState:gpu.compute];[e setBuffer:output offset:0 atIndex:0];
            [e setBytes:values.data() length:values.size()*4 atIndex:1];[e dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[e endEncoding];
        }
        // An oversized inline block remains temporary and must never displace a pooled page.
        values.resize(16385);values.front()=173;auto e=[cb computeCommandEncoder];[e setComputePipelineState:gpu.compute];[e setBuffer:output offset:0 atIndex:0];
        [e setBytes:values.data() length:values.size()*4 atIndex:1];[e dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[e endEncoding];
        const auto lease=[cb synchronizationStatistics];
        require(lease[@"cachedTables"].unsignedIntegerValue==64 && lease[@"cachedUniformPages"].unsignedIntegerValue==8,"Active lease exceeded table/page retention limits");
        [cb commit];[cb waitUntilCompleted];error(cb.error);require(*static_cast<uint32_t *>(output.contents)==173,"Table/page overflow lost final dispatch");
        auto stats=[queue submissionPoolStatistics];
        if(stats[@"available"].unsignedIntegerValue==0) {
            require(lease[@"allocatorBytes"].unsignedLongLongValue>8*1024*1024,"Pool unexpectedly discarded an eligible lease");
            require(stats[@"pooledTables"].unsignedIntegerValue==0 && stats[@"pooledUniformPages"].unsignedIntegerValue==0
                && stats[@"pooledAllocatorBytes"].unsignedLongLongValue==0,"Oversized allocator/bundle remained cached");
            std::printf("PASS oversized allocator retirement: %llu bytes; active tables/pages capped at 64/8, idle bundle discarded\n",lease[@"allocatorBytes"].unsignedLongLongValue);
        } else require(stats[@"pooledTables"].unsignedIntegerValue==64 && stats[@"pooledUniformPages"].unsignedIntegerValue==8,"Per-submission table/page pool caps differ");
    }
}
static void callbackLease(GPU& gpu)
{
    auto queue=[[FCMetal4Queue alloc] initWithDevice:gpu.device];auto first=[queue commandBuffer];
    auto entered=dispatch_semaphore_create(0),release=dispatch_semaphore_create(0);
    id<MTL4CommandBuffer> native=first.native;
    [first addCompletedHandler:^(FCMetal4Commands *) {
        dispatch_semaphore_signal(entered);dispatch_semaphore_wait(release,DISPATCH_TIME_FOREVER);
    }];[first commit];
    const bool callbackEntered=dispatch_semaphore_wait(entered,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC))==0;
    const bool unavailable=[queue submissionPoolStatistics][@"available"].unsignedIntegerValue==0;
    auto second=[queue commandBuffer];const bool distinct=second.native!=native;
    // Always release the feedback thread before assertions or waits.
    dispatch_semaphore_signal(release);[first waitUntilCompleted];[second commit];[second waitUntilCompleted];
    error(first.error);error(second.error);
    require(callbackEntered && unavailable && distinct,"Lease recycled before client callbacks completed");
    auto stale=[queue commandBuffer];auto e=[stale computeCommandEncoder];[e endEncoding];[stale commit];[stale waitUntilCompleted];
    bool rejected=false;try {const uint32_t v=1;[e setBytes:&v length:4 atIndex:1];}catch(const std::exception&){rejected=true;}
    require(rejected,"Completed encoder modified a recycled table/page");batch(gpu,queue,1,95);
}
static void completedOwnership(GPU& gpu)
{
    auto queue=[[FCMetal4Queue alloc] initWithDevice:gpu.device];
    __weak FCMetal4Commands *weakCommands;__weak id<MTLBuffer> weakOutput;
    @autoreleasepool {
        auto output=[gpu.device newBufferWithLength:256 options:MTLResourceStorageModeShared];weakOutput=output;
        auto cb=[queue commandBuffer];weakCommands=cb;auto e=[cb computeCommandEncoder];
        [e setComputePipelineState:gpu.compute];[e setBuffer:output offset:0 atIndex:0];
        const uint32_t value=99;[e setBytes:&value length:4 atIndex:1];[e dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[e endEncoding];
        [cb commit];[cb waitUntilCompleted];error(cb.error);require(*static_cast<uint32_t *>(output.contents)==99,"Ownership test compute differs");
    }
    // The wait group signals at the end of the feedback block. Give that
    // block's final strong capture/autorelease scope a bounded chance to retire.
    for(unsigned i=0;i<1000 && (weakCommands || weakOutput);++i)usleep(1000);
    require(weakCommands==nil,"Idle native command buffer retained a completed wrapper/queue cycle");
    require(weakOutput==nil,"Idle native command/residency bundle retained a client resource");
    require([queue submissionPoolStatistics][@"available"].unsignedIntegerValue==1,"Ownership test failed to retain the empty reusable bundle");
}
static void residencyLifetime(GPU& gpu)
{
    auto queue=[[PoolResidencySpy alloc] initWithDevice:gpu.device];queue.sets=[NSHashTable weakObjectsHashTable];
    NSUInteger peak=0;
    for(unsigned i=0;i<256;++i){@autoreleasepool{
        batch(gpu,queue,1,i);peak=std::max(peak,queue.sets.allObjects.count);
        require(peak<=2,"Fresh residency objects accumulate across completed submissions");
    }}
    auto stats=[queue submissionPoolStatistics];
    require([stats[@"residencySets"] unsignedIntegerValue]==256 && [stats[@"created"] unsignedIntegerValue]==1,"Residency lifetime fixture did not reuse one native lease");
    std::printf("PASS residency lifetime: 256 fresh sets, one native command lease, peak %lu live public residency objects\n",(unsigned long)peak);
}
static void earlyFeedback(GPU& gpu)
{
    for(bool recycling:{false,true}) {
        auto queue=[[FCMetal4Queue alloc] initWithDevice:gpu.device];
        [queue setSubmissionRecyclingForTesting:recycling];
        auto event=[gpu.device newSharedEvent];auto command=[queue commandBuffer];
        [command signalEvent:event value:19];[command setEarlyFeedbackForTesting:YES];
        [command commit];[command waitUntilCompleted];error(command.error);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(event.signaledValue<19){require(std::chrono::steady_clock::now()<deadline,"Early feedback lost the post-submit event signal");std::this_thread::yield();}
        auto stats=[command submissionEncodingStatistics];
        require(![stats[@"retainsEvent"] boolValue],"Completed wrapper retains the signaled event");
    }
    std::puts("PASS early feedback: post-submit event survives wrapper retirement, both recycling modes");
}
static void callbackReentry(GPU& gpu,bool acquire=true,bool recycle=true)
{
    for(bool recycling:{false,true}) for(bool returning:{false,true}) {
        if((returning && !recycle) || (!returning && !acquire))continue;
        auto queue=[[FCMetal4Queue alloc] initWithDevice:gpu.device];
        [queue setSubmissionRecyclingForTesting:recycling];
        auto command=[queue commandBuffer];
        __block FCMetal4Commands *next=nil;
        __block FCMetal4Commands *canceled=nil;
        @autoreleasepool { if(returning)canceled=[queue commandBuffer]; }
        __weak FCMetal4Commands *weakCanceled=canceled;
        [command addCompletedHandler:^(FCMetal4Commands *) {
            // Feedback must be able to acquire/return a CPU lease while the
            // producer is still inside the native queue's commit sequence.
            if(returning)canceled=nil;
            else next=[queue commandBuffer];
        }];
        [command setEarlyFeedbackForTesting:YES];
        [command commit];[command waitUntilCompleted];error(command.error);
        if(returning){require(!weakCanceled,"Feedback did not retire its canceled command lease");next=[queue commandBuffer];}
        require(next && next.status==FCMetal4StatusEncoding,"Feedback could not acquire the next command lease");
        [next commit];[next waitUntilCompleted];error(next.error);next=nil;
    }
    std::puts("PASS callback reentry: feedback acquires/retires CPU leases before commit returns, both recycling modes");
}
int main(int argc,char **argv)
{
    @autoreleasepool {
    try {
        GPU gpu;
        if(argc==2 && std::strcmp(argv[1],"--blocking-only")==0){callbackReentry(gpu);return 0;}
        if(argc==2 && std::strcmp(argv[1],"--blocking-acquire-only")==0){callbackReentry(gpu,true,false);return 0;}
        if(argc==2 && std::strcmp(argv[1],"--blocking-recycle-only")==0){callbackReentry(gpu,false,true);return 0;}
        zeroBindings(gpu);cancellation(gpu);inFlight(gpu);pageAndTableLimits(gpu);callbackLease(gpu);completedOwnership(gpu);earlyFeedback(gpu);callbackReentry(gpu);residencyLifetime(gpu);
        auto base=[[FCMetal4Queue alloc] initWithDevice:gpu.device];[base setSubmissionRecyclingForTesting:NO];
        auto pooled=[[FCMetal4Queue alloc] initWithDevice:gpu.device];
        for(unsigned i=0;i<16;++i){batch(gpu,base,8,10+i);batch(gpu,pooled,8,10+i);}
        auto b=[base submissionPoolStatistics],p=[pooled submissionPoolStatistics];
        require(b[@"created"].unsignedIntegerValue==16 && p[@"created"].unsignedIntegerValue==1,"Submission allocation reuse differs");
        require(b[@"tables"].unsignedIntegerValue==272 && p[@"tables"].unsignedIntegerValue==17,"Argument table allocation reuse differs");
        require(b[@"uniformPages"].unsignedIntegerValue==16 && p[@"uniformPages"].unsignedIntegerValue==1,"Uniform page allocation reuse differs");
        require(b[@"residencySets"].unsignedIntegerValue==16 && p[@"residencySets"].unsignedIntegerValue==16,"Each submission must have fresh residency state");
        std::printf("PASS submission reuse: 16 -> 1 native command leases, 16/16 fresh residency sets; 272 -> 17 tables; 16 -> 1 uniform pages\n");
        std::vector<double> baseline,optimized,baseTotal,poolTotal;
        for(unsigned i=0;i<35;++i) {
            double a,c,ar,cr;if(i&1){c=batch(gpu,pooled,32,40+i,&cr);a=batch(gpu,base,32,40+i,&ar);}else{a=batch(gpu,base,32,40+i,&ar);c=batch(gpu,pooled,32,40+i,&cr);}
            if(i>=4){baseline.push_back(a);optimized.push_back(c);baseTotal.push_back(a+ar);poolTotal.push_back(c+cr);}
        }
        std::sort(baseline.begin(),baseline.end());std::sort(optimized.begin(),optimized.end());std::sort(baseTotal.begin(),baseTotal.end());std::sort(poolTotal.begin(),poolTotal.end());
        std::printf("BENCH encoding 31 alternating pairs after 4 warmups, 32 render passes + compute + readback: no reuse %.3f us, pooled %.3f us; GPU wait excluded, validation state inherited\n",baseline[15],optimized[15]);
        std::printf("BENCH encoding + lease retirement CPU elapsed: no reuse %.3f us, pooled %.3f us; includes binding clears/residency reset, excludes GPU wait and final unpooled object destruction\n",baseTotal[15],poolTotal[15]);
        // Independent renderer/display-style queues own independent leases.
        auto other=[[FCMetal4Queue alloc] initWithDevice:gpu.device];
        auto work=std::async(std::launch::async,[&]{@autoreleasepool{for(unsigned i=0;i<8;++i)batch(gpu,other,2,100+i);}});
        for(unsigned i=0;i<8;++i)batch(gpu,pooled,2,200+i);work.get();
        std::puts("PASS pool: GPU exact outputs, all-slot setter-spy reset, 16 gated in-flight batches, 12 idle cap, 64 tables/stage, 8 pages/submission, oversized fallback, cancellation/completed ownership release, throwing/gated callbacks, stale encoder rejection, separate queues");return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
    }
}
