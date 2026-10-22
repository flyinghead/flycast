// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(USE_METAL) || defined(FLYCAST_METAL_STANDALONE)
#include "metal4_presentation.h"
#include <stdexcept>
#include <limits>
#ifdef FLYCAST_METAL_STANDALONE
using MetalPresentationException=std::runtime_error;
#else
#include "hw/pvr/Renderer_if.h"
using MetalPresentationException=RendererException;
#endif

typedef void (^FCMetal4Replay)(FCMetal4RenderEncoder *,BOOL) API_AVAILABLE(macos(26.0));
static void requireRecording(bool ok,const char *message) {if(!ok)throw MetalPresentationException(message);}

namespace metal_presentation {
struct SlotLease {
    dispatch_semaphore_t available;
    explicit SlotLease(dispatch_semaphore_t available):available(available) {}
    ~SlotLease(){dispatch_semaphore_signal(available);}
};
std::shared_ptr<MetalDisplayImage> leaseImage(dispatch_semaphore_t available)
{
    if(dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)!=0)return {};
    std::shared_ptr<SlotLease> lease;
    try {lease=std::make_shared<SlotLease>(available);}
    catch(...) {dispatch_semaphore_signal(available);throw;}
    auto image=std::make_shared<MetalDisplayImage>();image->storageLease=std::move(lease);return image;
}
bool snapshotFits(id<MTLTexture> source,NSUInteger width,NSUInteger height)
{
    // Equal-size copies save no storage and measured worse than composition.
    // Both renderer and spatial MetalFX output use these four-byte formats.
    return source && source.width && source.height && width && height
        && source.sampleCount==1 && source.textureType==MTLTextureType2D
        && (source.pixelFormat==MTLPixelFormatRGBA8Unorm || source.pixelFormat==MTLPixelFormatBGRA8Unorm)
        && width<=std::numeric_limits<NSUInteger>::max()/height && source.width<=(width*height-1)/source.height;
}
id<MTLTexture> snapshotTexture(id<MTLDevice> device,id<MTLTexture> source,id<MTLTexture> previous)
{
    requireRecording(device && source && source.device==device && source.textureType==MTLTextureType2D
        && source.sampleCount==1,"Invalid Metal presentation snapshot source");
    const auto swizzle=source.swizzle,old=previous ? previous.swizzle : swizzle;
    if(previous && previous.device==device && previous.textureType==MTLTextureType2D && previous.sampleCount==1
        && previous.width==source.width && previous.height==source.height && previous.pixelFormat==source.pixelFormat
        && previous.storageMode==MTLStorageModePrivate && previous.mipmapLevelCount==1
        && (previous.usage&MTLTextureUsageShaderRead) && swizzle.red==old.red && swizzle.green==old.green
        && swizzle.blue==old.blue && swizzle.alpha==old.alpha)return previous;
    auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:source.pixelFormat width:source.width height:source.height mipmapped:NO];
    descriptor.storageMode=MTLStorageModePrivate;descriptor.hazardTrackingMode=MTLHazardTrackingModeUntracked;
    descriptor.usage=MTLTextureUsageShaderRead;descriptor.swizzle=swizzle;
    auto texture=[device newTextureWithDescriptor:descriptor];requireRecording(texture!=nil,"Cannot allocate Metal presentation snapshot");return texture;
}
void encodeSnapshot(id<MTLTexture> source,id<MTLTexture> snapshot,FCMetal4Commands *command)
{
    requireRecording(source && snapshot && command && source!=snapshot && source.device==command.device
        && snapshot.device==command.device && source.width==snapshot.width && source.height==snapshot.height
        && source.pixelFormat==snapshot.pixelFormat,"Invalid Metal presentation snapshot destination");
    auto copy=[command transferEncoder];
    [copy copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(source.width,source.height,1) toTexture:snapshot destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
    [copy endEncoding];
}
}

@interface FCMetal4RecordedPass ()
- (void)append:(FCMetal4Replay)command;
- (void)seal;
- (void)requireComposition;
- (void)cancel;
@end
@implementation FCMetal4RecordedPass {
    NSMutableArray *_commands;
    bool _canceled;
}
- (instancetype)init {if((self=[super init]))_commands=[NSMutableArray new];return self;}
- (void)append:(FCMetal4Replay)command {
    requireRecording(!_sealed,"Metal presentation recording modified after seal");[_commands addObject:[command copy]];
}
- (void)seal {_sealed=YES;}
- (void)requireComposition {_requiresComposition=YES;}
- (void)cancel {_canceled=true;_sealed=YES;[_commands removeAllObjects];}
- (void)encodeTo:(FCMetal4RenderEncoder *)encoder opaque:(BOOL)opaque {
    requireRecording(!_canceled,"Canceled Metal presentation recording cannot be replayed");
    requireRecording(encoder!=nil,"Metal presentation replay has no encoder");
    requireRecording(!opaque || _sealed,"Unsealed Metal presentation published to display");
    requireRecording(!opaque || !_requiresComposition,"Unknown Metal pipeline needs composition fallback");
    for(FCMetal4Replay command in _commands)command(encoder,opaque);
}
@end

@interface FCMetal4RecordedTexture : NSObject
@property (nonatomic, strong) id<MTLTexture> texture;
@end
@implementation FCMetal4RecordedTexture @end

@implementation FCMetal4RecordingEncoder {
    NSMapTable<id<MTLRenderPipelineState>,id<MTLRenderPipelineState>> *_opaquePipelines;
    FCMetal4RenderEncoder *_forward;
    NSString *_recordedLabel;
    id<MTLBuffer> _vertexBuffers[31],_fragmentBuffers[31];
    FCMetal4RecordedTexture *_snapshotBinding;
    bool _snapshotPending, _hasTextureBindings;
}
- (instancetype)initWithOpaquePipelines:(NSMapTable<id<MTLRenderPipelineState>,id<MTLRenderPipelineState>> *)pipelines {
    if((self=[super init])) {_recordedPass=[FCMetal4RecordedPass new];_opaquePipelines=[pipelines copy] ?: [NSMapTable strongToStrongObjectsMapTable];}return self;
}
- (void)dealloc {
    // The base encoder closes itself on destruction. An abandoned CPU-only
    // recording has no GPU encoder to close and must never become publishable.
    if(_snapshotPending && !_forward)[_recordedPass cancel];
    _snapshotPending=false;
}
- (void)registerPipeline:(id<MTLRenderPipelineState>)pipeline opaque:(id<MTLRenderPipelineState>)opaque {
    requireRecording(!_recordedPass.sealed,"Opaque Metal pipeline registered after seal");
    requireRecording(pipeline && opaque,"Cannot register a missing Metal presentation pipeline");
    [_opaquePipelines setObject:opaque forKey:pipeline];
}
- (void)deferSnapshotOf:(id<MTLTexture>)source {
    requireRecording(source && !_recordedPass.sealed && !_forward && !_snapshotSource && !_hasTextureBindings,
        "Invalid deferred Metal presentation source");
    _snapshotSource=source;_snapshotBinding=[FCMetal4RecordedTexture new];
    requireRecording(_snapshotBinding!=nil,"Cannot allocate Metal presentation texture binding");
    _snapshotBinding.texture=source;_snapshotPending=true;
}
- (void)useSnapshot:(id<MTLTexture>)snapshot {
    requireRecording(_snapshotPending && !_recordedPass.sealed && !_forward && snapshot && snapshot!=_snapshotSource
        && snapshot.device==_snapshotSource.device && snapshot.width==_snapshotSource.width
        && snapshot.height==_snapshotSource.height && snapshot.pixelFormat==_snapshotSource.pixelFormat,
        "Invalid deferred Metal presentation snapshot");
    _snapshotBinding.texture=snapshot;_snapshotPending=false;
}
- (void)redirectTo:(FCMetal4RenderEncoder *)encoder {
    requireRecording(!_recordedPass.sealed && !_forward && encoder,"Invalid Metal presentation redirect");
    [_recordedPass encodeTo:encoder opaque:NO];_forward=encoder;
}
- (NSString *)label {return _forward ? _forward.label : _recordedLabel;}
- (void)setLabel:(NSString *)value {
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){forward.label=value;return;}_recordedLabel=[value copy];NSString *copy=_recordedLabel;
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){e.label=copy;}];
}
- (void)endEncoding {
    requireRecording(!_snapshotPending || _forward,"Mutable Metal presentation source cannot cross queues");
    if(_forward)[_forward endEncoding];[_recordedPass seal];[_opaquePipelines removeAllObjects];
}
- (FCMetal4RenderEncoder *)forwardEncoder {
    requireRecording(!_recordedPass.sealed,"Metal presentation encoder modified after end");return _forward;
}
- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)state {
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward setRenderPipelineState:state];return;}
    auto opaque=[_opaquePipelines objectForKey:state];
    if(!opaque)[_recordedPass requireComposition];
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL finalOpaque){[e setRenderPipelineState:finalOpaque ? opaque : state];}];
}
- (void)setDepthStencilState:(id<MTLDepthStencilState>)state {
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward setDepthStencilState:state];return;}[_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e setDepthStencilState:state];}];
}
#define RECORD_VALUE(Selector,Type) \
- (void)Selector:(Type)value {if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward Selector:value];return;} \
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e Selector:value];}];}
RECORD_VALUE(setViewport,MTLViewport)
RECORD_VALUE(setScissorRect,MTLScissorRect)
RECORD_VALUE(setCullMode,MTLCullMode)
RECORD_VALUE(setFrontFacingWinding,MTLWinding)
RECORD_VALUE(setStencilReferenceValue,uint32_t)
RECORD_VALUE(setDepthStoreAction,MTLStoreAction)
RECORD_VALUE(setStencilStoreAction,MTLStoreAction)
#undef RECORD_VALUE
- (void)setVertexBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index {
    requireRecording(index<31 && (!buffer || offset<=buffer.length),"Invalid recorded Metal vertex buffer range");
    _vertexBuffers[index]=buffer;if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward setVertexBuffer:buffer offset:offset atIndex:index];return;}
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e setVertexBuffer:buffer offset:offset atIndex:index];}];
}
- (void)setFragmentBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index {
    requireRecording(index<31 && (!buffer || offset<=buffer.length),"Invalid recorded Metal fragment buffer range");
    _fragmentBuffers[index]=buffer;if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward setFragmentBuffer:buffer offset:offset atIndex:index];return;}
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e setFragmentBuffer:buffer offset:offset atIndex:index];}];
}
- (void)setVertexBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index {
    requireRecording(index<31 && _vertexBuffers[index],"Recorded Metal vertex offset has no buffer");
    [self setVertexBuffer:_vertexBuffers[index] offset:offset atIndex:index];
}
- (void)setFragmentBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index {
    requireRecording(index<31 && _fragmentBuffers[index],"Recorded Metal fragment offset has no buffer");
    [self setFragmentBuffer:_fragmentBuffers[index] offset:offset atIndex:index];
}
#define RECORD_BYTES(Selector,BindingArray) \
- (void)Selector:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index { \
    requireRecording(index<31 && bytes && length,"Invalid recorded Metal inline bytes");BindingArray[index]=nil; \
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward Selector:bytes length:length atIndex:index];return;} \
    NSData *copy=[NSData dataWithBytes:bytes length:length];requireRecording(copy!=nil,"Cannot snapshot Metal presentation uniforms"); \
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e Selector:copy.bytes length:copy.length atIndex:index];}];}
RECORD_BYTES(setVertexBytes,_vertexBuffers)
RECORD_BYTES(setFragmentBytes,_fragmentBuffers)
#undef RECORD_BYTES
- (void)setSharedBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index {
    requireRecording(index<31 && bytes && length,"Invalid recorded Metal shared bytes");_vertexBuffers[index]=_fragmentBuffers[index]=nil;
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward setSharedBytes:bytes length:length atIndex:index];return;}
    NSData *copy=[NSData dataWithBytes:bytes length:length];requireRecording(copy!=nil,"Cannot snapshot Metal shared uniforms");
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e setSharedBytes:copy.bytes length:copy.length atIndex:index];}];
}
- (void)setFragmentTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index {
    requireRecording(index<128,"Invalid recorded Metal texture binding");
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward setFragmentTexture:texture atIndex:index];return;}
    _hasTextureBindings=true;
    if(_snapshotBinding && texture==_snapshotSource) {
        // This one binding is finalized on the producer before sealing. The
        // replay block owns only the finalized texture, never the encoder.
        FCMetal4RecordedTexture *binding=_snapshotBinding;
        [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e setFragmentTexture:binding.texture atIndex:index];}];
    } else [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e setFragmentTexture:texture atIndex:index];}];
}
- (void)setFragmentSamplerState:(id<MTLSamplerState>)sampler atIndex:(NSUInteger)index {
    requireRecording(index<16,"Invalid recorded Metal sampler binding");
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward setFragmentSamplerState:sampler atIndex:index];return;}[_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e setFragmentSamplerState:sampler atIndex:index];}];
}
- (void)drawPrimitives:(MTLPrimitiveType)type vertexStart:(NSUInteger)start vertexCount:(NSUInteger)count {
    [self drawPrimitives:type vertexStart:start vertexCount:count instanceCount:1];
}
- (void)drawPrimitives:(MTLPrimitiveType)type vertexStart:(NSUInteger)start vertexCount:(NSUInteger)count instanceCount:(NSUInteger)instances {
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward drawPrimitives:type vertexStart:start vertexCount:count instanceCount:instances];return;}
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e drawPrimitives:type vertexStart:start vertexCount:count instanceCount:instances];}];
}
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type indexCount:(NSUInteger)count indexType:(MTLIndexType)indexType indexBuffer:(id<MTLBuffer>)buffer indexBufferOffset:(NSUInteger)offset {
    [self drawIndexedPrimitives:type indexCount:count indexType:indexType indexBuffer:buffer indexBufferOffset:offset instanceCount:1 baseVertex:0 baseInstance:0];
}
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type indexCount:(NSUInteger)count indexType:(MTLIndexType)indexType indexBuffer:(id<MTLBuffer>)buffer indexBufferOffset:(NSUInteger)offset instanceCount:(NSUInteger)instances baseVertex:(NSInteger)vertex baseInstance:(NSUInteger)instance {
    const NSUInteger stride=indexType==MTLIndexTypeUInt16?2:4;
    requireRecording(buffer && offset<=buffer.length && count<=(buffer.length-offset)/stride,"Invalid recorded Metal index buffer range");
    if(FCMetal4RenderEncoder *forward=[self forwardEncoder]){[forward drawIndexedPrimitives:type indexCount:count indexType:indexType indexBuffer:buffer indexBufferOffset:offset instanceCount:instances baseVertex:vertex baseInstance:instance];return;}
    [_recordedPass append:^(FCMetal4RenderEncoder *e,BOOL){[e drawIndexedPrimitives:type indexCount:count indexType:indexType indexBuffer:buffer indexBufferOffset:offset instanceCount:instances baseVertex:vertex baseInstance:instance];}];
}
@end
#endif
