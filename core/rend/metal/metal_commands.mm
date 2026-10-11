// SPDX-License-Identifier: GPL-2.0-or-later
#include "metal_commands.h"
#ifdef FLYCAST_METAL_STANDALONE
#include <stdexcept>
using MetalCommandException = std::runtime_error;
#else
#include "hw/pvr/Renderer_if.h"
using MetalCommandException = RendererException;
#endif
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <exception>
#include <limits>

static void requireCommand(bool valid, const char *message)
{
    if (!valid) throw MetalCommandException(message);
}

@interface FCMetal4Queue ()
- (id<MTL4CommandAllocator>)acquireAllocator;
- (void)recycleAllocator:(id<MTL4CommandAllocator>)allocator;
@end
@interface FCMetal4Commands ()
@property (readwrite) FCMetal4Status status;
@property (readwrite, strong) NSError *error;
@property (readwrite) CFTimeInterval GPUStartTime, GPUEndTime;
- (instancetype)initWithQueue:(FCMetal4Queue *)queue;
- (void)beginEncoder;
- (void)endEncoder;
- (id<MTL4ArgumentTable>)newTable;
- (MTLGPUAddress)copyBytes:(const void *)bytes length:(NSUInteger)length;
@end
@interface FCMetal4RenderEncoder ()
- (instancetype)initWithCommands:(FCMetal4Commands *)commands descriptor:(MTL4RenderPassDescriptor *)descriptor;
@end
@interface FCMetal4ComputeEncoder ()
- (instancetype)initWithCommands:(FCMetal4Commands *)commands;
@end

@implementation FCMetal4Queue {
    NSMutableArray<id<MTL4CommandAllocator>> *_available;
}
- (instancetype)initWithDevice:(id<MTLDevice>)device
{
    if ((self = [super init])) {
        _device = device;
        MTL4CommandQueueDescriptor *descriptor = [MTL4CommandQueueDescriptor new];
        descriptor.label = @"Flycast Metal 4";
        NSError *error = nil;
        _native = [device newMTL4CommandQueueWithDescriptor:descriptor error:&error];
        requireCommand(_native != nil, "Cannot create Metal 4 command queue");
        _available = [NSMutableArray new];
    }
    return self;
}
- (FCMetal4Commands *)commandBuffer { return [[FCMetal4Commands alloc] initWithQueue:self]; }
- (id<MTL4CommandAllocator>)acquireAllocator
{
    @synchronized (self) {
        id<MTL4CommandAllocator> allocator = _available.lastObject;
        if (allocator) { [_available removeLastObject]; return allocator; }
    }
    id<MTL4CommandAllocator> allocator = [_device newCommandAllocator];
    requireCommand(allocator != nil, "Cannot allocate Metal 4 command storage");
    return allocator;
}
- (void)recycleAllocator:(id<MTL4CommandAllocator>)allocator
{
    // Only called after feedback, or for a buffer that was never submitted.
    // Resetting storage still in use by the GPU would invalidate its commands.
    [allocator reset];
    @synchronized (self) {
        if (_available.count < 12) [_available addObject:allocator];
    }
}
@end

@implementation FCMetal4Commands {
    id<MTL4CommandAllocator> _allocator;
    id<MTLResidencySet> _residency;
    NSMutableSet *_objects;
    NSMutableArray *_handlers;
    NSMutableSet<id<MTLDrawable>> *_waitDrawables;
    dispatch_group_t _finished;
    BOOL _encoderOpen, _ended, _begun;
    id<MTLBuffer> _uniformPage;
    NSUInteger _uniformOffset;
    id<MTLDrawable> _drawable;
    CFTimeInterval _duration;
    id<MTLEvent> _signalEvent, _waitEvent;
    uint64_t _signalValue, _waitValue;
}
@synthesize status = _status, error = _error, GPUStartTime = _GPUStartTime, GPUEndTime = _GPUEndTime;
- (instancetype)initWithQueue:(FCMetal4Queue *)queue
{
    if ((self = [super init])) {
        _commandQueue = queue;
        _allocator = [queue acquireAllocator];
        _native = [queue.device newCommandBuffer];
        requireCommand(_native != nil, "Cannot create Metal 4 command buffer");
        MTLResidencySetDescriptor *descriptor = [MTLResidencySetDescriptor new];
        descriptor.label = @"Flycast submission resources";
        descriptor.initialCapacity = 64;
        NSError *error = nil;
        _residency = [queue.device newResidencySetWithDescriptor:descriptor error:&error];
        requireCommand(_residency != nil, "Cannot create Metal 4 residency set");
        _objects = [NSMutableSet new];
        _handlers = [NSMutableArray new];
        _waitDrawables = [NSMutableSet new];
        _finished = dispatch_group_create();
        [_native beginCommandBufferWithAllocator:_allocator]; _begun = YES;
        [_native useResidencySet:_residency];
    }
    return self;
}
- (void)dealloc
{
    if (_allocator) {
        // An unsubmitted batch is canceled by its renderer owner. Encoders
        // retain this owner until they have ended; no GPU can reference it.
        if (_native && _begun && !_ended) [_native endCommandBuffer];
        [_commandQueue recycleAllocator:_allocator];
    }
}
- (id<MTLDevice>)device { return _commandQueue.device; }
- (NSString *)label { return _native.label; }
- (void)setLabel:(NSString *)label { _native.label = label; }
- (void)retainObject:(id)object
{
    requireCommand(self.status == FCMetal4StatusEncoding, "Metal 4 object retained after commit");
    if (object) [_objects addObject:object];
}
- (void)useResource:(id<MTLResource>)resource
{
    requireCommand(self.status == FCMetal4StatusEncoding, "Metal 4 resource added after commit");
    if (!resource || [_objects containsObject:resource]) return;
    requireCommand(resource.device == self.device, "Metal 4 resource belongs to another device");
    [_objects addObject:resource];
    // Retain the view as well as its backing allocation. Residency is needed
    // for the root allocation, not another allocation for each stencil view.
    if ([resource conformsToProtocol:@protocol(MTLTexture)]) {
        id<MTLTexture> texture = (id<MTLTexture>)resource;
        if (texture.parentTexture || texture.buffer) {
            [self useResource:texture.parentTexture];
            [self useResource:texture.buffer];
            return;
        }
    }
    if (resource.heap) {
        [_residency addAllocation:resource.heap];
        [_objects addObject:resource.heap];
    } else [_residency addAllocation:resource];
}
- (void)beginEncoder
{
    requireCommand(self.status == FCMetal4StatusEncoding && !_encoderOpen && !_ended,
        "Metal 4 encoder overlaps another encoder or a submitted buffer");
    _encoderOpen = YES;
}
- (void)endEncoder { _encoderOpen = NO; }
- (id<MTL4ArgumentTable>)newTable
{
    MTL4ArgumentTableDescriptor *descriptor = [MTL4ArgumentTableDescriptor new];
    descriptor.maxBufferBindCount = 31;
    descriptor.maxTextureBindCount = 128;
    descriptor.maxSamplerStateBindCount = 16;
    descriptor.initializeBindings = YES;
    NSError *error = nil;
    id<MTL4ArgumentTable> table = [self.device newArgumentTableWithDescriptor:descriptor error:&error];
    requireCommand(table != nil, "Cannot create Metal 4 argument table");
    [self retainObject:table];
    return table;
}
- (MTLGPUAddress)copyBytes:(const void *)bytes length:(NSUInteger)length
{
    requireCommand(bytes && length && length <= self.device.maxBufferLength, "Invalid Metal 4 inline uniforms");
    if (!_uniformPage || length > _uniformPage.length - _uniformOffset) {
        const NSUInteger size = std::max<NSUInteger>(length, std::min<NSUInteger>(65536, self.device.maxBufferLength));
        _uniformPage = [self.device newBufferWithLength:size options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
        requireCommand(_uniformPage != nil, "Cannot allocate Metal 4 inline uniform storage");
        _uniformOffset = 0;
        [self useResource:_uniformPage];
    }
    const NSUInteger offset = _uniformOffset;
    memcpy((char *)_uniformPage.contents + offset, bytes, length);
    // Every encoded value owns a distinct, 256-byte-aligned range until feedback.
    const NSUInteger end = offset + length;
    _uniformOffset = end <= std::numeric_limits<NSUInteger>::max() - 255 ? (end + 255) & ~NSUInteger(255) : _uniformPage.length;
    if (_uniformOffset > _uniformPage.length) _uniformOffset = _uniformPage.length;
    return _uniformPage.gpuAddress + offset;
}
- (FCMetal4RenderEncoder *)renderCommandEncoderWithDescriptor:(MTL4RenderPassDescriptor *)descriptor
{ return [[FCMetal4RenderEncoder alloc] initWithCommands:self descriptor:descriptor]; }
- (FCMetal4ComputeEncoder *)computeCommandEncoder
{ return [[FCMetal4ComputeEncoder alloc] initWithCommands:self]; }
- (FCMetal4ComputeEncoder *)transferEncoder { return [self computeCommandEncoder]; }
- (void)queueBarrier
{
    // A pass boundary on either side of an opaque MetalFX encoder provides
    // queue-wide ordering, including prior/subsequent command buffers.
    FCMetal4ComputeEncoder *encoder = [self computeCommandEncoder];
    [encoder endEncoding];
}
- (void)addCompletedHandler:(void (^)(FCMetal4Commands *))handler
{
    requireCommand(self.status == FCMetal4StatusEncoding, "Metal 4 completion registered after commit");
    [_handlers addObject:[handler copy]];
}
- (void)useDrawable:(id<MTLDrawable>)drawable
{
    [self retainObject:drawable];
    if (drawable) [_waitDrawables addObject:drawable];
}
- (void)presentDrawable:(id<MTLDrawable>)drawable { [self presentDrawable:drawable afterMinimumDuration:0]; }
- (void)presentDrawable:(id<MTLDrawable>)drawable afterMinimumDuration:(CFTimeInterval)duration
{
    requireCommand(self.status == FCMetal4StatusEncoding && !_drawable && drawable, "Invalid Metal 4 drawable presentation");
    _drawable = drawable; _duration = duration;
    [self useDrawable:drawable];
}
- (void)signalEvent:(id<MTLEvent>)event value:(uint64_t)value
{
    requireCommand(!_signalEvent && event, "Invalid Metal 4 signal event");
    [self retainObject:event]; _signalEvent = event; _signalValue = value;
}
- (void)waitForEvent:(id<MTLEvent>)event value:(uint64_t)value
{
    requireCommand(!_waitEvent && event, "Invalid Metal 4 wait event");
    [self retainObject:event]; _waitEvent = event; _waitValue = value;
}
- (void)commit
{
    requireCommand(self.status == FCMetal4StatusEncoding && !_encoderOpen && !_ended, "Invalid Metal 4 commit");
    MTL4CommitOptions *options = [MTL4CommitOptions new];
    requireCommand(options != nil, "Cannot allocate Metal 4 commit feedback");
    // Metal owns this block until feedback. It keeps the entire submission alive
    // even when the renderer/context releases its reference immediately after commit.
    [options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback) {
        self.error = feedback.error;
        self.GPUStartTime = feedback.GPUStartTime;
        self.GPUEndTime = feedback.GPUEndTime;
        self.status = feedback.error ? FCMetal4StatusError : FCMetal4StatusCompleted;
        NSArray *handlers = [self->_handlers copy];
        [self->_handlers removeAllObjects];
        for (void (^handler)(FCMetal4Commands *) in handlers) {
            @try {
                try { handler(self); }
                catch (const std::exception& failure) { fprintf(stderr, "Metal 4 completion: %s\n", failure.what()); }
                catch (...) { fprintf(stderr, "Metal 4 completion failed\n"); }
            } @catch (NSException *failure) {
                fprintf(stderr, "Metal 4 completion: %s\n", failure.reason.UTF8String);
            }
        }
        [self->_objects removeAllObjects];
        [self->_waitDrawables removeAllObjects];
        self->_uniformPage = nil;
        self->_residency = nil;
        // Release native command references before their storage can be recycled.
        self->_native = nil;
        [self.commandQueue recycleAllocator:self->_allocator];
        self->_allocator = nil;
        dispatch_group_leave(self->_finished);
    }];
    [_residency commit];
    [_native endCommandBuffer]; _ended = YES;
    id<MTL4CommandBuffer> buffers[] = {_native};
    dispatch_group_enter(_finished);
    self.status = FCMetal4StatusCommitted;
    // Acquired drawables need a wait even when waitIdle submits a frame
    // without presentation during resize/backend shutdown.
    // DisplayLink and emulation encode independently. Keep each native
    // wait/commit/signal sequence indivisible on their shared Metal 4 queue.
    @synchronized (_commandQueue) {
    if (_waitEvent) [_commandQueue.native waitForEvent:_waitEvent value:_waitValue];
    for (id<MTLDrawable> drawable in [_waitDrawables allObjects]) [_commandQueue.native waitForDrawable:drawable];
    [_commandQueue.native commit:buffers count:1 options:options];
    if (_signalEvent) [_commandQueue.native signalEvent:_signalEvent value:_signalValue];
    if (_drawable) {
        [_commandQueue.native signalDrawable:_drawable];
        if (_duration > 0) [_drawable presentAfterMinimumDuration:_duration];
        else [_drawable present];
        _drawable = nil;
    }
    }
}
- (void)waitUntilCompleted
{
    requireCommand(self.status != FCMetal4StatusEncoding, "Cannot wait for unsubmitted Metal 4 work");
    dispatch_group_wait(_finished, DISPATCH_TIME_FOREVER);
}
@end

@implementation FCMetal4RenderEncoder {
    FCMetal4Commands *_commands;
    id<MTL4RenderCommandEncoder> _encoder;
    id<MTL4ArgumentTable> _vertex, _fragment;
    BOOL _opened;
    id<MTLBuffer> _vertexBuffers[31], _fragmentBuffers[31];
}
- (instancetype)initWithCommands:(FCMetal4Commands *)commands descriptor:(MTL4RenderPassDescriptor *)descriptor
{
    if ((self = [super init])) {
        _commands = commands;
        [commands beginEncoder]; _opened = YES;
        for (NSUInteger i = 0; i < 8; ++i) {
            [commands useResource:descriptor.colorAttachments[i].texture];
            [commands useResource:descriptor.colorAttachments[i].resolveTexture];
        }
        [commands useResource:descriptor.depthAttachment.texture];
        [commands useResource:descriptor.depthAttachment.resolveTexture];
        [commands useResource:descriptor.stencilAttachment.texture];
        [commands useResource:descriptor.stencilAttachment.resolveTexture];
        [commands useResource:descriptor.visibilityResultBuffer];
        [commands retainObject:descriptor.rasterizationRateMap];
        _encoder = [commands.native renderCommandEncoderWithDescriptor:descriptor];
        requireCommand(_encoder != nil, "Cannot create Metal 4 render encoder");
        [_encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
        _vertex = [commands newTable]; _fragment = [commands newTable];
        [_encoder setArgumentTable:_vertex atStages:MTLRenderStageVertex];
        [_encoder setArgumentTable:_fragment atStages:MTLRenderStageFragment];
    }
    return self;
}
- (NSString *)label { return _encoder.label; }
- (void)setLabel:(NSString *)label { _encoder.label = label; }
- (void)dealloc { [self endEncoding]; }
- (void)endEncoding
{
    if (_encoder) {
        [_encoder barrierAfterStages:MTLStageAll beforeQueueStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
        [_encoder endEncoding]; _encoder = nil;
    }
    if (_opened) { [_commands endEncoder]; _opened = NO; }
}
- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)state { [_commands retainObject:state]; [_encoder setRenderPipelineState:state]; }
- (void)setDepthStencilState:(id<MTLDepthStencilState>)state { [_commands retainObject:state]; [_encoder setDepthStencilState:state]; }
- (void)setViewport:(MTLViewport)value { [_encoder setViewport:value]; }
- (void)setScissorRect:(MTLScissorRect)value { [_encoder setScissorRect:value]; }
- (void)setCullMode:(MTLCullMode)value { [_encoder setCullMode:value]; }
- (void)setFrontFacingWinding:(MTLWinding)value { [_encoder setFrontFacingWinding:value]; }
- (void)setStencilReferenceValue:(uint32_t)value { [_encoder setStencilReferenceValue:value]; }
- (void)setDepthStoreAction:(MTLStoreAction)value { [_encoder setDepthStoreAction:value]; }
- (void)setStencilStoreAction:(MTLStoreAction)value { [_encoder setStencilStoreAction:value]; }
- (void)setVertexBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    requireCommand(index < 31 && (!buffer || offset <= buffer.length), "Invalid Metal 4 vertex buffer range");
    [_commands useResource:buffer]; _vertexBuffers[index] = buffer;
    [_vertex setAddress:buffer ? buffer.gpuAddress + offset : 0 atIndex:index];
}
- (void)setFragmentBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    requireCommand(index < 31 && (!buffer || offset <= buffer.length), "Invalid Metal 4 fragment buffer range");
    [_commands useResource:buffer]; _fragmentBuffers[index] = buffer;
    [_fragment setAddress:buffer ? buffer.gpuAddress + offset : 0 atIndex:index];
}
- (void)setVertexBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    requireCommand(index < 31 && _vertexBuffers[index], "Metal 4 vertex buffer offset has no buffer");
    [self setVertexBuffer:_vertexBuffers[index] offset:offset atIndex:index];
}
- (void)setFragmentBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    requireCommand(index < 31 && _fragmentBuffers[index], "Metal 4 fragment buffer offset has no buffer");
    [self setFragmentBuffer:_fragmentBuffers[index] offset:offset atIndex:index];
}
- (void)setVertexBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
    requireCommand(index < 31, "Invalid Metal 4 vertex uniform binding");
    _vertexBuffers[index] = nil;
    [_vertex setAddress:[_commands copyBytes:bytes length:length] atIndex:index];
}
- (void)setFragmentBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
    requireCommand(index < 31, "Invalid Metal 4 fragment uniform binding");
    _fragmentBuffers[index] = nil;
    [_fragment setAddress:[_commands copyBytes:bytes length:length] atIndex:index];
}
- (void)setFragmentTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index
{
    requireCommand(index < 128, "Invalid Metal 4 fragment texture binding");
    [_commands useResource:texture];
    [_fragment setTexture:texture ? texture.gpuResourceID : MTLResourceID{} atIndex:index];
}
- (void)setFragmentSamplerState:(id<MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
    requireCommand(index < 16, "Invalid Metal 4 fragment sampler binding");
    [_commands retainObject:sampler];
    [_fragment setSamplerState:sampler ? sampler.gpuResourceID : MTLResourceID{} atIndex:index];
}
- (void)drawPrimitives:(MTLPrimitiveType)type vertexStart:(NSUInteger)start vertexCount:(NSUInteger)count
{ [_encoder drawPrimitives:type vertexStart:start vertexCount:count]; }
- (void)drawPrimitives:(MTLPrimitiveType)type vertexStart:(NSUInteger)start vertexCount:(NSUInteger)count instanceCount:(NSUInteger)instances
{ [_encoder drawPrimitives:type vertexStart:start vertexCount:count instanceCount:instances]; }
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type indexCount:(NSUInteger)count indexType:(MTLIndexType)indexType indexBuffer:(id<MTLBuffer>)buffer indexBufferOffset:(NSUInteger)offset
{ [self drawIndexedPrimitives:type indexCount:count indexType:indexType indexBuffer:buffer indexBufferOffset:offset instanceCount:1 baseVertex:0 baseInstance:0]; }
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type indexCount:(NSUInteger)count indexType:(MTLIndexType)indexType indexBuffer:(id<MTLBuffer>)buffer indexBufferOffset:(NSUInteger)offset instanceCount:(NSUInteger)instances baseVertex:(NSInteger)vertex baseInstance:(NSUInteger)instance
{
    const NSUInteger stride = indexType == MTLIndexTypeUInt16 ? 2 : 4;
    requireCommand(buffer && offset <= buffer.length && count <= (buffer.length - offset) / stride,
        "Invalid Metal 4 index buffer range");
    [_commands useResource:buffer];
    [_encoder drawIndexedPrimitives:type indexCount:count indexType:indexType
        indexBuffer:buffer.gpuAddress + offset indexBufferLength:buffer.length - offset
        instanceCount:instances baseVertex:vertex baseInstance:instance];
}
@end

@implementation FCMetal4ComputeEncoder {
    FCMetal4Commands *_commands;
    id<MTL4ComputeCommandEncoder> _encoder;
    id<MTL4ArgumentTable> _table;
    BOOL _hasWork, _opened;
}
- (instancetype)initWithCommands:(FCMetal4Commands *)commands
{
    if ((self = [super init])) {
        _commands = commands;
        [commands beginEncoder]; _opened = YES;
        _encoder = [commands.native computeCommandEncoder];
        requireCommand(_encoder != nil, "Cannot create Metal 4 compute encoder");
        [_encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];

    }
    return self;
}
- (NSString *)label { return _encoder.label; }
- (void)setLabel:(NSString *)label { _encoder.label = label; }
- (void)dealloc { [self endEncoding]; }
- (void)endEncoding
{
    if (_encoder) {
        [_encoder barrierAfterStages:MTLStageAll beforeQueueStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
        [_encoder endEncoding]; _encoder = nil;
    }
    if (_opened) { [_commands endEncoder]; _opened = NO; }
}
- (void)beforeWork
{
    // A mip generation can consume a preceding upload in this very pass;
    // OIT clears/copies/dispatches can likewise share a compute encoder.
    if (_hasWork) [_encoder barrierAfterEncoderStages:MTLStageBlit | MTLStageDispatch
        beforeEncoderStages:MTLStageBlit | MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
    _hasWork = YES;
}
- (id<MTL4ArgumentTable>)table
{
    if (!_table) {
        _table = [_commands newTable];
        [_encoder setArgumentTable:_table];
    }
    return _table;
}
- (void)setComputePipelineState:(id<MTLComputePipelineState>)state { [_commands retainObject:state]; [_encoder setComputePipelineState:state]; }
- (void)setBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    requireCommand(index < 31 && (!buffer || offset <= buffer.length), "Invalid Metal 4 compute buffer range");
    [_commands useResource:buffer]; [[self table] setAddress:buffer ? buffer.gpuAddress + offset : 0 atIndex:index];
}
- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
    requireCommand(index < 31, "Invalid Metal 4 compute uniform binding");
    [[self table] setAddress:[_commands copyBytes:bytes length:length] atIndex:index];
}
- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index
{
    requireCommand(index < 128, "Invalid Metal 4 compute texture binding");
    [_commands useResource:texture]; [[self table] setTexture:texture ? texture.gpuResourceID : MTLResourceID{} atIndex:index];
}
- (void)dispatchThreads:(MTLSize)threads threadsPerThreadgroup:(MTLSize)group
{ [self beforeWork]; [_encoder dispatchThreads:threads threadsPerThreadgroup:group]; }
- (void)fillBuffer:(id<MTLBuffer>)buffer range:(NSRange)range value:(uint8_t)value
{
    requireCommand(range.location <= buffer.length && range.length <= buffer.length - range.location, "Invalid Metal 4 buffer fill range");
    [_commands useResource:buffer]; [self beforeWork]; [_encoder fillBuffer:buffer range:range value:value];
}
- (void)generateMipmapsForTexture:(id<MTLTexture>)texture
{ [_commands useResource:texture]; [self beforeWork]; [_encoder generateMipmapsForTexture:texture]; }
- (void)copyFromTexture:(id<MTLTexture>)source sourceSlice:(NSUInteger)slice sourceLevel:(NSUInteger)level sourceOrigin:(MTLOrigin)origin sourceSize:(MTLSize)size toTexture:(id<MTLTexture>)destination destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin
{
    [_commands useResource:source]; [_commands useResource:destination]; [self beforeWork];
    [_encoder copyFromTexture:source sourceSlice:slice sourceLevel:level sourceOrigin:origin sourceSize:size toTexture:destination destinationSlice:destinationSlice destinationLevel:destinationLevel destinationOrigin:destinationOrigin];
}
- (void)copyFromTexture:(id<MTLTexture>)source sourceSlice:(NSUInteger)slice sourceLevel:(NSUInteger)level sourceOrigin:(MTLOrigin)origin sourceSize:(MTLSize)size toBuffer:(id<MTLBuffer>)destination destinationOffset:(NSUInteger)offset destinationBytesPerRow:(NSUInteger)row destinationBytesPerImage:(NSUInteger)image
{
    [_commands useResource:source]; [_commands useResource:destination]; [self beforeWork];
    [_encoder copyFromTexture:source sourceSlice:slice sourceLevel:level sourceOrigin:origin sourceSize:size toBuffer:destination destinationOffset:offset destinationBytesPerRow:row destinationBytesPerImage:image];
}
- (void)copyFromBuffer:(id<MTLBuffer>)source sourceOffset:(NSUInteger)offset sourceBytesPerRow:(NSUInteger)row sourceBytesPerImage:(NSUInteger)image sourceSize:(MTLSize)size toTexture:(id<MTLTexture>)destination destinationSlice:(NSUInteger)slice destinationLevel:(NSUInteger)level destinationOrigin:(MTLOrigin)origin
{
    [_commands useResource:source]; [_commands useResource:destination]; [self beforeWork];
    [_encoder copyFromBuffer:source sourceOffset:offset sourceBytesPerRow:row sourceBytesPerImage:image sourceSize:size toTexture:destination destinationSlice:slice destinationLevel:level destinationOrigin:origin];
}
@end
