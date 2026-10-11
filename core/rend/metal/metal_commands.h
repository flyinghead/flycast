// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#pragma push_macro("Rect")
#pragma push_macro("RGBColor")
#pragma push_macro("PAGE_SIZE")
#pragma push_macro("PAGE_MASK")
// PVR core and QuickDraw use the same type names. Keep SDK names local.
#define Rect MacOSRect
#define RGBColor MacOSRGBColor
#import <Metal/Metal.h>
#pragma pop_macro("PAGE_MASK")
#pragma pop_macro("PAGE_SIZE")
#pragma pop_macro("RGBColor")
#pragma pop_macro("Rect")
#import <dispatch/dispatch.h>

@class FCMetal4Queue, FCMetal4RenderEncoder, FCMetal4ComputeEncoder;
// CPU submission state. This is not the state of a legacy MTLCommandBuffer.
typedef NS_ENUM(NSUInteger, FCMetal4Status) {
    FCMetal4StatusEncoding, FCMetal4StatusCommitted, FCMetal4StatusCompleted, FCMetal4StatusError
};

// Owns native Metal 4 commands, their allocator, argument tables and resources
// through commit feedback. All encoding happens on the renderer's owning thread.
@interface FCMetal4Commands : NSObject
@property (readonly, strong) id<MTL4CommandBuffer> native;
@property (readonly, strong) FCMetal4Queue *commandQueue;
@property (readonly, strong) id<MTLDevice> device;
@property (copy) NSString *label;
@property (readonly) FCMetal4Status status;
@property (readonly, strong) NSError *error;
@property (readonly) CFTimeInterval GPUStartTime, GPUEndTime;
- (FCMetal4RenderEncoder *)renderCommandEncoderWithDescriptor:(MTL4RenderPassDescriptor *)descriptor;
- (FCMetal4ComputeEncoder *)computeCommandEncoder;
// Transfers are encoded by MTL4ComputeCommandEncoder, never a legacy blit encoder.
- (FCMetal4ComputeEncoder *)transferEncoder;
- (void)addCompletedHandler:(void (^)(FCMetal4Commands *))handler;
- (void)commit;
- (void)waitUntilCompleted;
- (void)retainObject:(id)object;
- (void)useResource:(id<MTLResource>)resource;
// Orders opaque encoders such as MetalFX against all other work on this queue.
- (void)queueBarrier;
- (void)useDrawable:(id<MTLDrawable>)drawable;
- (void)presentDrawable:(id<MTLDrawable>)drawable;
- (void)presentDrawable:(id<MTLDrawable>)drawable afterMinimumDuration:(CFTimeInterval)duration;
- (void)signalEvent:(id<MTLEvent>)event value:(uint64_t)value;
- (void)waitForEvent:(id<MTLEvent>)event value:(uint64_t)value;
@end

@interface FCMetal4Queue : NSObject
@property (readonly, strong) id<MTL4CommandQueue> native;
@property (readonly, strong) id<MTLDevice> device;
- (instancetype)initWithDevice:(id<MTLDevice>)device;
- (FCMetal4Commands *)commandBuffer;
@end

@interface FCMetal4RenderEncoder : NSObject
@property (copy) NSString *label;
- (void)endEncoding;
- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)state;
- (void)setDepthStencilState:(id<MTLDepthStencilState>)state;
- (void)setViewport:(MTLViewport)viewport;
- (void)setScissorRect:(MTLScissorRect)rect;
- (void)setCullMode:(MTLCullMode)mode;
- (void)setFrontFacingWinding:(MTLWinding)winding;
- (void)setStencilReferenceValue:(uint32_t)value;
- (void)setDepthStoreAction:(MTLStoreAction)action;
- (void)setStencilStoreAction:(MTLStoreAction)action;
- (void)setVertexBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setFragmentBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setVertexBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setFragmentBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setVertexBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)setFragmentBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)setFragmentTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index;
- (void)setFragmentSamplerState:(id<MTLSamplerState>)sampler atIndex:(NSUInteger)index;
- (void)drawPrimitives:(MTLPrimitiveType)type vertexStart:(NSUInteger)start vertexCount:(NSUInteger)count;
- (void)drawPrimitives:(MTLPrimitiveType)type vertexStart:(NSUInteger)start vertexCount:(NSUInteger)count instanceCount:(NSUInteger)instances;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type indexCount:(NSUInteger)count indexType:(MTLIndexType)indexType indexBuffer:(id<MTLBuffer>)buffer indexBufferOffset:(NSUInteger)offset;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type indexCount:(NSUInteger)count indexType:(MTLIndexType)indexType indexBuffer:(id<MTLBuffer>)buffer indexBufferOffset:(NSUInteger)offset instanceCount:(NSUInteger)instances baseVertex:(NSInteger)vertex baseInstance:(NSUInteger)instance;
@end

@interface FCMetal4ComputeEncoder : NSObject
@property (copy) NSString *label;
- (void)endEncoding;
- (void)setComputePipelineState:(id<MTLComputePipelineState>)state;
- (void)setBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index;
- (void)dispatchThreads:(MTLSize)threads threadsPerThreadgroup:(MTLSize)group;
- (void)fillBuffer:(id<MTLBuffer>)buffer range:(NSRange)range value:(uint8_t)value;
- (void)generateMipmapsForTexture:(id<MTLTexture>)texture;
- (void)copyFromTexture:(id<MTLTexture>)source sourceSlice:(NSUInteger)slice sourceLevel:(NSUInteger)level sourceOrigin:(MTLOrigin)origin sourceSize:(MTLSize)size toTexture:(id<MTLTexture>)destination destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin;
- (void)copyFromTexture:(id<MTLTexture>)source sourceSlice:(NSUInteger)slice sourceLevel:(NSUInteger)level sourceOrigin:(MTLOrigin)origin sourceSize:(MTLSize)size toBuffer:(id<MTLBuffer>)destination destinationOffset:(NSUInteger)offset destinationBytesPerRow:(NSUInteger)row destinationBytesPerImage:(NSUInteger)image;
- (void)copyFromBuffer:(id<MTLBuffer>)source sourceOffset:(NSUInteger)offset sourceBytesPerRow:(NSUInteger)row sourceBytesPerImage:(NSUInteger)image sourceSize:(MTLSize)size toTexture:(id<MTLTexture>)destination destinationSlice:(NSUInteger)slice destinationLevel:(NSUInteger)level destinationOrigin:(MTLOrigin)origin;
@end
