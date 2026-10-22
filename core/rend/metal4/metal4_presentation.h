// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_commands.h"
#include <memory>
#include <cstdint>

@class FCMetal4RecordedPass;
// The lease is destroyed last, after the immutable resource/command snapshot.
struct API_AVAILABLE(macos(26.0)) MetalDisplayImage {
    std::shared_ptr<void> storageLease;
    id<MTLTexture> texture;
    FCMetal4RecordedPass *recording;
    NSUInteger width=0,height=0;
    MTLClearColor clearColor{};
    id<MTLSharedEvent> readyEvent;
    uint64_t readyValue=0;
    double target=0;
    uint64_t content=0;
};
namespace metal_presentation {
API_AVAILABLE(macos(26.0))
std::shared_ptr<MetalDisplayImage> leaseImage(dispatch_semaphore_t available);
API_AVAILABLE(macos(26.0))
bool snapshotFits(id<MTLTexture> source,NSUInteger width,NSUInteger height);
API_AVAILABLE(macos(26.0))
id<MTLTexture> snapshotTexture(id<MTLDevice> device,id<MTLTexture> source,id<MTLTexture> previous);
API_AVAILABLE(macos(26.0))
void encodeSnapshot(id<MTLTexture> source,id<MTLTexture> snapshot,FCMetal4Commands *command);
}

// CPU-only command snapshots. They own values/resources, never ImGui data or
// the renderer/context. Producer-owned mutable resources still need a lease.
API_AVAILABLE(macos(26.0))
@interface FCMetal4RecordedPass : NSObject
@property (readonly) BOOL sealed;
@property (readonly) BOOL requiresComposition;
- (void)encodeTo:(FCMetal4RenderEncoder *)encoder opaque:(BOOL)opaque;
@end

API_AVAILABLE(macos(26.0))
@interface FCMetal4RecordingEncoder : FCMetal4RenderEncoder
@property (readonly, strong) FCMetal4RecordedPass *recordedPass;
@property (readonly, strong) id<MTLTexture> snapshotSource;
- (instancetype)initWithOpaquePipelines:(NSMapTable<id<MTLRenderPipelineState>, id<MTLRenderPipelineState>> *)pipelines;
- (void)registerPipeline:(id<MTLRenderPipelineState>)pipeline opaque:(id<MTLRenderPipelineState>)opaque;
// Declare before binding the source. A producer-side redirect uses the source
// directly; a display recording must replace it with its leased snapshot before
// endEncoding seals the immutable packet.
- (void)deferSnapshotOf:(id<MTLTexture>)source;
- (void)useSnapshot:(id<MTLTexture>)snapshot;
// Replay the recorded prefix once, then forward subsequent calls. Call before
// a user callback/opaque operation needs immediate producer-side rendering.
- (void)redirectTo:(FCMetal4RenderEncoder *)encoder;
@end
