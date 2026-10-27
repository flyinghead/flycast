// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(USE_METAL) || defined(FLYCAST_METAL_STANDALONE)
#include "metal4_commands.h"
#include "metal4_transfer_hazards.h"
#include "metal4_pass_hazards.h"
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
#include <array>
#import <objc/runtime.h>
#ifdef FLYCAST_METAL_STANDALONE
#include <chrono>
#include <atomic>
#include <thread>
#endif

static void requireCommand(bool valid, const char *message)
{
    if (!valid) throw MetalCommandException(message);
}
static const char OwnedBufferKey=0;
id<MTLBuffer> FCMetal4CreateBuffer(id<MTLDevice> device,NSUInteger length,MTLResourceOptions options)
{
    id<MTLBuffer> buffer=[device newBufferWithLength:length options:options];
    if(buffer && buffer.storageMode!=MTLStorageModePrivate)objc_setAssociatedObject(buffer,&OwnedBufferKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return buffer;
}
id<MTLBuffer> FCMetal4CreateBufferWithBytes(id<MTLDevice> device,const void *bytes,NSUInteger length,MTLResourceOptions options)
{
    id<MTLBuffer> buffer=[device newBufferWithBytes:bytes length:length options:options];
    if(buffer && buffer.storageMode!=MTLStorageModePrivate)objc_setAssociatedObject(buffer,&OwnedBufferKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return buffer;
}

namespace {
constexpr NSUInteger SubmissionPoolLimit = 12;
constexpr NSUInteger TablePoolLimit = 64; // Per shader stage and submission.
constexpr NSUInteger UniformPageLimit = 8;
constexpr NSUInteger UniformPageBytes = 65536;
constexpr uint64_t AllocatorPoolBytes = 8 * 1024 * 1024;
constexpr NSUInteger RetainedObjectPoolLimit = 4096;
enum TableFamily : NSUInteger { VertexTable, FragmentTable, ComputeTable, TableFamilyCount };
// All transfers and dispatches this wrapper encodes belong to these stages.
API_AVAILABLE(macos(26.0))
constexpr MTLStages ComputeStages = MTLStageBlit | MTLStageDispatch;

}

// Each table is leased for a whole submission, including every encoder that
// records it. Clearing bindings is legal only after feedback or cancellation.
API_AVAILABLE(macos(26.0))
@interface FCMetal4Table : NSObject
@property (readonly, strong) id<MTL4ArgumentTable> native;
- (instancetype)initWithDevice:(id<MTLDevice>)device family:(TableFamily)family;
- (void)setAddress:(MTLGPUAddress)address atIndex:(NSUInteger)index;
- (void)setTexture:(MTLResourceID)texture atIndex:(NSUInteger)index;
- (void)setSamplerState:(MTLResourceID)sampler atIndex:(NSUInteger)index;
- (void)clearBindings;
@end
@implementation FCMetal4Table {
    uint32_t _buffers;
    std::array<uint64_t, 2> _textures;
    uint16_t _samplers;
}
- (instancetype)initWithDevice:(id<MTLDevice>)device family:(TableFamily)family
{
    if ((self = [super init])) {
        MTL4ArgumentTableDescriptor *descriptor = [MTL4ArgumentTableDescriptor new];
        descriptor.maxBufferBindCount = 31;
        descriptor.maxTextureBindCount = family == VertexTable ? 0 : 128;
        descriptor.maxSamplerStateBindCount = family == FragmentTable ? 16 : 0;
        descriptor.initializeBindings = YES;
        NSError *error = nil;
        _native = [device newArgumentTableWithDescriptor:descriptor error:&error];
        requireCommand(_native != nil, "Cannot create Metal 4 argument table");
    }
    return self;
}
- (void)setAddress:(MTLGPUAddress)address atIndex:(NSUInteger)index
{
    [_native setAddress:address atIndex:index];
    if (address) _buffers |= uint32_t(1) << index;
    else _buffers &= ~(uint32_t(1) << index);
}
- (void)setTexture:(MTLResourceID)texture atIndex:(NSUInteger)index
{
    [_native setTexture:texture atIndex:index];
    const uint64_t bit = uint64_t(1) << (index % 64);
    if (texture._impl) _textures[index / 64] |= bit;
    else _textures[index / 64] &= ~bit;
}
- (void)setSamplerState:(MTLResourceID)sampler atIndex:(NSUInteger)index
{
    [_native setSamplerState:sampler atIndex:index];
    if (sampler._impl) _samplers |= uint16_t(1) << index;
    else _samplers &= ~(uint16_t(1) << index);
}
- (void)clearBindings
{
    while (_buffers) {
        [_native setAddress:0 atIndex:__builtin_ctz(_buffers)];
        _buffers &= _buffers - 1;
    }
    for (NSUInteger word = 0; word < _textures.size(); ++word) while (_textures[word]) {
        [_native setTexture:MTLResourceID{} atIndex:word * 64 + __builtin_ctzll(_textures[word])];
        _textures[word] &= _textures[word] - 1;
    }
    while (_samplers) {
        [_native setSamplerState:MTLResourceID{} atIndex:__builtin_ctz(_samplers)];
        _samplers &= _samplers - 1;
    }
}
@end

API_AVAILABLE(macos(26.0))
@interface FCMetal4Submission : NSObject
@property (strong) id<MTL4CommandBuffer> native;
@property (strong) id<MTL4CommandAllocator> allocator;
@property (strong) id<MTLResidencySet> residency;
@property (strong) NSMutableSet *objects;
@property (strong) NSMutableArray *handlers;
@property (strong) NSMutableSet<id<MTLDrawable>> *waitDrawables;
@property (strong) NSMutableArray<id<MTLBuffer>> *pages;
@property BOOL usedHeap;
- (NSMutableArray<FCMetal4Table *> *)tables:(TableFamily)family;
- (metal_sync::TransferHazards *)resetTransferHazards;
- (metal_sync::PassHazards *)resetPassHazards;
- (void)reset;
@end
@implementation FCMetal4Submission {
    NSMutableArray<FCMetal4Table *> *_tables[TableFamilyCount];
    metal_sync::TransferHazards _transferHazards;
    metal_sync::PassHazards _passHazards;
}
- (instancetype)init
{
    if ((self = [super init])) {
        _objects = [NSMutableSet new]; _handlers = [NSMutableArray new];
        _waitDrawables = [NSMutableSet new]; _pages = [NSMutableArray new];
        for (auto& tables : _tables) tables = [NSMutableArray new];
    }
    return self;
}
- (NSMutableArray<FCMetal4Table *> *)tables:(TableFamily)family { return _tables[family]; }
- (metal_sync::TransferHazards *)resetTransferHazards
{ _transferHazards.reset(); return &_transferHazards; }
- (metal_sync::PassHazards *)resetPassHazards
{ _passHazards.reset(); return &_passHazards; }
- (void)reset
{
    for (auto& tables : _tables) for (FCMetal4Table *table in tables) [table clearBindings];
    [_residency removeAllAllocations]; [_residency commit];
    [_objects removeAllObjects]; [_handlers removeAllObjects]; [_waitDrawables removeAllObjects];
    _native.label = nil;
    [_allocator reset];
}
@end

namespace {
API_AVAILABLE(macos(26.0))
metal_sync::Access allocationAccess(id<MTLResource> resource,bool write,uint64_t begin=0,
    uint64_t end=std::numeric_limits<uint64_t>::max())
{
    if(!resource)return {};
    if(begin==end)return {1,0,0,write,false};
    if([resource conformsToProtocol:@protocol(MTLTexture)]) {
        id<MTLTexture> texture=(id<MTLTexture>)resource;
        while(texture.parentTexture)texture=texture.parentTexture;
        if(texture.buffer) {
            auto access=allocationAccess(texture.buffer,write);access.alias=access.passAlias=true;return access;
        }
        // Imported/shared textures and heaps can alias through another Metal
        // object or CPU mapping. Their allocation extent is not proven here.
        if(texture.iosurface || texture.shareable)return {0,0,
            std::numeric_limits<uint64_t>::max(),write,true};
        resource=texture;
    }
    if(resource.heap)return {0,0,
        std::numeric_limits<uint64_t>::max(),write,true};
    if([resource conformsToProtocol:@protocol(MTLBuffer)] && resource.storageMode!=MTLStorageModePrivate) {
        // Metal does not expose whether CPU-visible buffers wrap remapped or
        // no-copy memory. Keep one conservative alias domain for all of them;
        // shared upload reads still overlap when their texture writes differ.
        const bool owned=objc_getAssociatedObject(resource,&OwnedBufferKey)!=nil;
        return {1,0,std::numeric_limits<uint64_t>::max(),write,true,
            owned?reinterpret_cast<uintptr_t>((__bridge void *)resource):0,!owned};
    }
    return {reinterpret_cast<uintptr_t>((__bridge void *)resource),begin,end,write,false};
}
API_AVAILABLE(macos(26.0))
metal_sync::Access textureAccess(id<MTLTexture> texture,NSUInteger level,NSUInteger slice,bool write)
{
    if(!texture)return {};
    uint64_t rootLevel=level,rootSlice=slice;
    id<MTLTexture> root=texture;
    while(root.parentTexture) {
        if(rootLevel>std::numeric_limits<uint64_t>::max()-root.parentRelativeLevel
            || rootSlice>std::numeric_limits<uint64_t>::max()-root.parentRelativeSlice)return allocationAccess(root,write);
        rootLevel+=root.parentRelativeLevel;rootSlice+=root.parentRelativeSlice;root=root.parentTexture;
    }
    if(root.heap || root.buffer || root.iosurface || root.shareable)return allocationAccess(root,write);
    const uint64_t levels=root.mipmapLevelCount;
    if(!levels || rootLevel>=levels || rootSlice>(std::numeric_limits<uint64_t>::max()-rootLevel-1)/levels)
        return allocationAccess(root,write);
    const uint64_t subresource=rootSlice*levels+rootLevel;
    return allocationAccess(root,write,subresource,subresource+1);
}
}

@interface FCMetal4Queue ()
// Native wait/commit/signal sequences must not hold the CPU lease-pool lock:
// feedback can allocate or recycle a lease before native commit returns.
@property (readonly, nonatomic, strong) NSObject *submissionLock;
- (id<MTL4CommandAllocator>)acquireAllocator;
- (void)recycleAllocator:(id<MTL4CommandAllocator>)allocator;
// Transfer the private lease directly. A discarded failed lease must not keep
// client images alive until the caller's surrounding autorelease pool drains.
- (FCMetal4Submission *)acquireSubmission NS_RETURNS_RETAINED;
- (id<MTLResidencySet>)newResidencySet;
- (void)recycleSubmission:(FCMetal4Submission *)submission reusable:(BOOL)reusable;
- (void)createdTable;
- (void)createdUniformPage;
@property (readonly) BOOL recycling;
@end
@interface FCMetal4Commands ()
@property (readwrite) FCMetal4Status status;
@property (readwrite, strong) NSError *error;
@property (readwrite) CFTimeInterval GPUStartTime, GPUEndTime;
- (instancetype)initWithQueue:(FCMetal4Queue *)queue;
- (void)beginEncoder;
- (void)endEncoder;
- (void)deferEncoderEnd:(id<MTL4CommandEncoder>)encoder;
- (void)finishPendingEncoder:(BOOL)publish;
- (BOOL)keepsEncoderProducerBarriers;
- (void)didPublishEncoder;
- (BOOL)plansPassDependencies;
- (void)consumeBefore:(id<MTL4CommandEncoder>)encoder stages:(MTLStages)stages accesses:(const metal_sync::Access *)accesses count:(size_t)count opaque:(BOOL)opaque;
@property (readonly, nonatomic) id<MTLBuffer> inlineUniformBuffer;
- (FCMetal4Table *)newTable:(TableFamily)family;
- (MTLGPUAddress)copyBytes:(const void *)bytes length:(NSUInteger)length;
- (metal_sync::TransferHazards *)resetTransferHazards;
@property (readonly, nonatomic) BOOL needsAliasVisibility;
#ifdef FLYCAST_METAL_STANDALONE
- (BOOL)fullSynchronizationForTesting;
- (BOOL)ownedResourceBenchmarkForTesting;
- (void)recordTransferBarrier:(BOOL)barrier afterStages:(MTLStages)stages alias:(BOOL)alias;
- (void)recordQueueConsumer;
#endif
@end
@interface FCMetal4RenderEncoder ()
- (instancetype)initWithCommands:(FCMetal4Commands *)commands descriptor:(MTL4RenderPassDescriptor *)descriptor;
@end
@interface FCMetal4ComputeEncoder ()
- (instancetype)initWithCommands:(FCMetal4Commands *)commands;
- (instancetype)initWithCommands:(FCMetal4Commands *)commands fullBoundary:(BOOL)fullBoundary;
@end

@implementation FCMetal4Queue {
    NSMutableArray<id<MTL4CommandAllocator>> *_available;
    NSMutableArray<FCMetal4Submission *> *_submissions;
    NSUInteger _createdSubmissions, _reusedSubmissions, _createdTables, _createdUniformPages;
#ifdef FLYCAST_METAL_STANDALONE
    uint64_t _retirementNanoseconds;
    NSUInteger _createdAllocators;
    NSUInteger _createdResidencySets;
    BOOL _reuseResidencyForTesting;
#endif
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
        _submissions = [NSMutableArray new];
        _submissionLock = [NSObject new];
        requireCommand(_submissionLock != nil, "Cannot allocate Metal 4 submission lock");
        _recycling = YES;
    }
    return self;
}
#ifdef FLYCAST_METAL_STANDALONE
- (void)setSubmissionRecyclingForTesting:(BOOL)enabled
{
    @synchronized (self) { _recycling = enabled; [_submissions removeAllObjects]; }
}
- (void)setResidencyReuseForTesting:(BOOL)enabled { _reuseResidencyForTesting=enabled; }
- (NSDictionary<NSString *, NSNumber *> *)submissionPoolStatistics
{
    @synchronized (self) {
        NSUInteger tables = 0, pages = 0;
        uint64_t bytes = 0;
        for (FCMetal4Submission *submission in _submissions) {
            for (NSUInteger family = 0; family < TableFamilyCount; ++family)
                tables += [submission tables:TableFamily(family)].count;
            pages += submission.pages.count; bytes += submission.allocator.allocatedSize;
        }
        for (id<MTL4CommandAllocator> allocator in _available) bytes += allocator.allocatedSize;
        return @{ @"created": @(_createdSubmissions), @"reused": @(_reusedSubmissions),
            @"available": @(_submissions.count), @"tables": @(_createdTables), @"uniformPages": @(_createdUniformPages),
            @"pooledTables": @(tables), @"pooledUniformPages": @(pages), @"pooledAllocatorBytes": @(bytes),
            @"pooledAllocatorCount": @(_available.count+_submissions.count),
            @"allocators": @(_createdAllocators),
            @"residencySets": @(_createdResidencySets),
            @"retirementNanoseconds": @(_retirementNanoseconds) };
    }
}
#endif
- (void)createdTable { @synchronized (self) { ++_createdTables; } }
- (void)createdUniformPage { @synchronized (self) { ++_createdUniformPages; } }
- (FCMetal4Submission *)acquireSubmission
{
    FCMetal4Submission *submission;
    @synchronized (self) {
        submission = _submissions.lastObject;
        if (submission) { [_submissions removeLastObject]; ++_reusedSubmissions; }
        else ++_createdSubmissions;
    }
    if(submission) {
        // Refilling a previously submitted residency set can serialize CPU
        // commit against queued readers. Reuse commands/tables/pages, with a
        // fresh set describing this submission's unchanged resource contract.
#ifdef FLYCAST_METAL_STANDALONE
        if(!_reuseResidencyForTesting)
#endif
            submission.residency=[self newResidencySet];
        return submission;
    }
    submission = [FCMetal4Submission new];
    submission.allocator = [self acquireAllocator];
    submission.native = [_device newCommandBuffer];
    requireCommand(submission.native != nil, "Cannot create Metal 4 command buffer");
    submission.residency=[self newResidencySet];
    return submission;
}
- (id<MTLResidencySet>)newResidencySet
{
    MTLResidencySetDescriptor *descriptor = [MTLResidencySetDescriptor new];
    descriptor.label = @"Flycast submission resources"; descriptor.initialCapacity = 64;
    NSError *error = nil;
    id<MTLResidencySet> residency=[_device newResidencySetWithDescriptor:descriptor error:&error];
    requireCommand(residency != nil, "Cannot create Metal 4 residency set");
#ifdef FLYCAST_METAL_STANDALONE
    @synchronized(self){++_createdResidencySets;}
#endif
    return residency;
}
- (void)recycleSubmission:(FCMetal4Submission *)submission reusable:(BOOL)reusable
{
    // Feedback errors retire the entire bundle through its owner, without
    // resetting failed native state or caching its allocator separately.
    // A reused native command buffer can retain heap-usage metadata even
    // after residency removal. Do not detach its heap resources and keep that
    // native object cached: Metal validation would later dereference them in
    // preCommit. Retire the complete heap submission without reset/recycling.
    if (!reusable || submission.usedHeap) return;
#ifdef FLYCAST_METAL_STANDALONE
    const auto started = std::chrono::steady_clock::now();
#endif
    reusable = submission.objects.count <= RetainedObjectPoolLimit
        && submission.handlers.count <= 64 && submission.waitDrawables.count <= 16
        && submission.allocator.allocatedSize <= AllocatorPoolBytes;
    [submission reset];
    BOOL kept = NO;
    @synchronized (self) {
        if (_recycling && reusable && _submissions.count + _available.count < SubmissionPoolLimit) {
            [_submissions addObject:submission]; kept = YES;
        }
    }
    if (!kept) [self recycleAllocator:submission.allocator];
#ifdef FLYCAST_METAL_STANDALONE
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started).count();
    @synchronized (self) { _retirementNanoseconds += elapsed; }
#endif
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
#ifdef FLYCAST_METAL_STANDALONE
    @synchronized(self){++_createdAllocators;}
#endif
    return allocator;
}
- (void)recycleAllocator:(id<MTL4CommandAllocator>)allocator
{
    // Only called after feedback, or for a buffer that was never submitted.
    // Resetting storage still in use by the GPU would invalidate its commands.
    [allocator reset];
    @synchronized (self) {
        if (_available.count + _submissions.count < SubmissionPoolLimit
            && allocator.allocatedSize <= AllocatorPoolBytes) [_available addObject:allocator];
    }
}
@end

@implementation FCMetal4Commands {
    FCMetal4Submission *_submission;
    std::array<NSUInteger, TableFamilyCount> _tableCursor;
    NSUInteger _pageCursor;
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
    BOOL _needsAliasVisibility;
    // Logically ended encoders remain native-open until the next pass or
    // commit. The next consumer's queue barrier orders that earlier pass;
    // commit instead appends the final producer release before closing it.
    id<MTL4CommandEncoder> _pendingEncoder;
    BOOL _needsFinalProducer;
    metal_sync::PassHazards *_passHazards;
    BOOL _passPublished;
#ifdef FLYCAST_METAL_STANDALONE
    BOOL _fullSynchronization;
    BOOL _legacyEncoderBoundaries;
    BOOL _disablePassDependencies;
    BOOL _ownedResourceBenchmark;
    NSError *_feedbackErrorForTesting;
    BOOL _commitFailureForTesting;
    BOOL _queueCommitFailureForTesting, _postCommitFailureForTesting;
    NSUInteger _transferOperations, _transferBarriers, _aliasBarriers;
    MTLStages _barrierAfterStages;
    NSUInteger _queueConsumers, _queueProducers, _deferredEnds, _commitFlushes, _fallbackCommitFlushes;
    NSUInteger _queueDependencyBarriers,_queueNarrowConsumers,_queueIndependentUses,_queueGlobalSources;
    NSUInteger _inlinePagesForTesting;
    uint64_t _inlineBytesForTesting, _encodedAllocatorBytes, _encodedResidencyCount;
    uintptr_t _encodedAllocatorIdentity;
    double _residencyCommitUs, _nativeCommitUs;
    BOOL _earlyFeedbackForTesting;
    std::atomic<bool> _retiredReferencesForTesting;
#endif
}
@synthesize status = _status, error = _error, GPUStartTime = _GPUStartTime, GPUEndTime = _GPUEndTime;
@synthesize native = _native;
- (instancetype)initWithQueue:(FCMetal4Queue *)queue
{
    if ((self = [super init])) {
#ifdef FLYCAST_METAL_STANDALONE
        _retiredReferencesForTesting.store(false,std::memory_order_relaxed);
#endif
        _commandQueue = queue;
        _submission = [queue acquireSubmission];
        _passHazards=[_submission resetPassHazards];
        _native = _submission.native; _residency = _submission.residency;
        _objects = _submission.objects; _handlers = _submission.handlers;
        _waitDrawables = _submission.waitDrawables;
        _finished = dispatch_group_create();
        [_native beginCommandBufferWithAllocator:_submission.allocator]; _begun = YES;
        [_native useResidencySet:_residency];
    }
    return self;
}
- (void)dealloc
{
    if (_submission) {
        // An unsubmitted batch is canceled by its renderer owner. Encoders
        // retain this owner until they have ended; no GPU can reference it.
        [self finishPendingEncoder:NO];
        if (_native && _begun && !_ended) [_native endCommandBuffer];
        [_commandQueue recycleSubmission:_submission reusable:YES];
    }
}
#ifdef FLYCAST_METAL_STANDALONE
- (void)setEarlyFeedbackForTesting:(BOOL)enabled { _earlyFeedbackForTesting=enabled; }
- (NSDictionary<NSString *, NSNumber *> *)submissionEncodingStatistics
{
    return @{ @"tables": @(_tableCursor[0]+_tableCursor[1]+_tableCursor[2]),
        @"inlinePages": @(_inlinePagesForTesting), @"inlineBytes": @(_inlineBytesForTesting),
        @"allocatorBytes": @(_encodedAllocatorBytes), @"residencyCount": @(_encodedResidencyCount),
        @"allocatorIdentity": @(_encodedAllocatorIdentity),
        @"residencyCommitUs": @(_residencyCommitUs), @"nativeCommitUs": @(_nativeCommitUs),
        @"retainsDrawable": @(_drawable!=nil), @"retainsEvent": @(_waitEvent!=nil || _signalEvent!=nil) };
}
- (void)setFeedbackErrorForTesting:(NSError *)error
{
    requireCommand(self.status==FCMetal4StatusEncoding && !_encoderOpen,"Feedback injection after encoding/submission");
    _feedbackErrorForTesting=error;
}
- (void)setCommitFailureForTesting:(BOOL)enabled
{
    requireCommand(self.status==FCMetal4StatusEncoding && !_encoderOpen,"Commit injection after encoding/submission");
    _commitFailureForTesting=enabled;
}
- (void)setQueueCommitFailureForTesting:(BOOL)enabled
{
    requireCommand(self.status==FCMetal4StatusEncoding && !_encoderOpen,"Queue injection after encoding/submission");
    _queueCommitFailureForTesting=enabled;
}
- (void)setPostCommitFailureForTesting:(BOOL)enabled
{
    requireCommand(self.status==FCMetal4StatusEncoding && !_encoderOpen,"Post-commit injection after encoding/submission");
    _postCommitFailureForTesting=enabled;
}
- (void)setFullSynchronizationForTesting:(BOOL)enabled
{
    requireCommand(self.status==FCMetal4StatusEncoding && !_encoderOpen, "Synchronization policy set after encoding");
    _fullSynchronization=enabled;
}
- (BOOL)fullSynchronizationForTesting {return _fullSynchronization;}
- (void)setLegacyEncoderBoundariesForTesting:(BOOL)enabled
{
    requireCommand(self.status==FCMetal4StatusEncoding && !_encoderOpen,"Boundary policy changed during encoding");
    _legacyEncoderBoundaries=enabled;
}
- (void)setPassDependenciesForTesting:(BOOL)enabled
{
    requireCommand(self.status==FCMetal4StatusEncoding && !_encoderOpen,"Pass policy changed during encoding");
    _disablePassDependencies=!enabled;_passHazards->reset();
}
- (void)setOwnedResourceBenchmarkForTesting:(BOOL)enabled
{
    requireCommand(self.status==FCMetal4StatusEncoding && !_encoderOpen,"Benchmark policy set after encoding");
    _ownedResourceBenchmark=enabled;
}
- (BOOL)ownedResourceBenchmarkForTesting {return _ownedResourceBenchmark;}
- (void)recordTransferBarrier:(BOOL)barrier afterStages:(MTLStages)stages alias:(BOOL)alias
{
    ++_transferOperations;if(barrier) {++_transferBarriers;_barrierAfterStages|=stages;if(alias)++_aliasBarriers;}
}
- (void)recordQueueConsumer {++_queueConsumers;}
- (NSDictionary<NSString *,NSNumber *> *)synchronizationStatistics
{
    NSUInteger tables=0;
    for(NSUInteger family=0;family<TableFamilyCount;++family)tables+=[_submission tables:TableFamily(family)].count;
    return @{@"operations":@(_transferOperations),@"barriers":@(_transferBarriers),
        @"aliasBarriers":@(_aliasBarriers),@"afterStages":@(_barrierAfterStages),
        @"queueConsumers":@(_queueConsumers),@"queueProducers":@(_queueProducers),
        @"deferredEnds":@(_deferredEnds),@"commitFlushes":@(_commitFlushes),
        @"fallbackCommitFlushes":@(_fallbackCommitFlushes),
        @"queueDependencyBarriers":@(_queueDependencyBarriers),@"queueNarrowConsumers":@(_queueNarrowConsumers),@"queueIndependentUses":@(_queueIndependentUses),
        @"queueGlobalSources":@(_queueGlobalSources),
        @"allocatorBytes":@(_submission.allocator.allocatedSize),@"cachedTables":@(tables),
        @"cachedUniformPages":@(_submission.pages.count),@"retainedObjects":@(_objects.count)};
}
#endif
- (id<MTLDevice>)device { return _commandQueue.device; }
- (id<MTL4CommandBuffer>)native
{
    if(self.status==FCMetal4StatusEncoding && !_ended && !_encoderOpen) {
        // Raw clients (MetalFX or timestamp/debug encoders) expect a native
        // pass boundary. Publish earlier work before giving them the buffer,
        // then conservatively publish their opaque work again at commit.
        [self finishPendingEncoder:YES];
        _passHazards->reset();
        _needsFinalProducer=YES;_needsAliasVisibility=YES;
    }
    return _native;
}
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
    _needsAliasVisibility |= resource.heap!=nil;
    if([resource conformsToProtocol:@protocol(MTLBuffer)] && resource.storageMode!=MTLStorageModePrivate)
        _needsAliasVisibility=YES;
    [_objects addObject:resource];
    // Retain the view as well as its backing allocation. Residency is needed
    // for the root allocation, not another allocation for each stencil view.
    if ([resource conformsToProtocol:@protocol(MTLTexture)]) {
        id<MTLTexture> texture = (id<MTLTexture>)resource;
        _needsAliasVisibility |= texture.buffer!=nil || texture.iosurface!=nil || texture.shareable;
        if (texture.parentTexture || texture.buffer) {
            [self useResource:texture.parentTexture];
            [self useResource:texture.buffer];
            return;
        }
    }
    if (resource.heap) {
        _submission.usedHeap=YES;
        [_residency addAllocation:resource.heap];
        [_objects addObject:resource.heap];
    } else [_residency addAllocation:resource];
}
- (void)beginEncoder
{
    requireCommand(self.status == FCMetal4StatusEncoding && !_encoderOpen && !_ended,
        "Metal 4 encoder overlaps another encoder or a submitted buffer");
    [self finishPendingEncoder:NO];
    _passHazards->begin();_passPublished=NO;
    _encoderOpen = YES;
}
- (void)endEncoder
{
    if([self plansPassDependencies])_passHazards->end(_passPublished);
    _encoderOpen = NO;
}
- (BOOL)plansPassDependencies
{
#ifdef FLYCAST_METAL_STANDALONE
    return !_disablePassDependencies && !_fullSynchronization && !_legacyEncoderBoundaries;
#else
    return YES;
#endif
}
- (void)consumeBefore:(id<MTL4CommandEncoder>)encoder stages:(MTLStages)stages accesses:(const metal_sync::Access *)accesses count:(size_t)count opaque:(BOOL)opaque
{
    const auto barrier=_passHazards->before(stages==MTLStageAll?~uint64_t(0):uint64_t(stages),accesses,count,opaque);
    if(!barrier.needed){
#ifdef FLYCAST_METAL_STANDALONE
        ++_queueIndependentUses;
#endif
        return;
    }
    [encoder barrierAfterQueueStages:MTLStages(barrier.after)&MTLStageAll beforeStages:stages
        visibilityOptions:MTL4VisibilityOptionDevice | (barrier.alias?MTL4VisibilityOptionResourceAlias:0)];
#ifdef FLYCAST_METAL_STANDALONE
    ++_queueConsumers;
    ++_queueDependencyBarriers;
    if((MTLStages(barrier.after)&MTLStageAll)==MTLStageAll)++_queueGlobalSources;
    if((MTLStages(barrier.after)&MTLStageAll)!=MTLStageAll || stages!=MTLStageAll)++_queueNarrowConsumers;
#endif
}
- (BOOL)keepsEncoderProducerBarriers
{
#ifdef FLYCAST_METAL_STANDALONE
    if(_fullSynchronization || _legacyEncoderBoundaries)return YES;
#endif
    return [self plansPassDependencies] && _passHazards->requiresRelease();
}
- (void)deferEncoderEnd:(id<MTL4CommandEncoder>)encoder
{
    requireCommand(_encoderOpen && !_pendingEncoder && encoder,"Invalid deferred Metal encoder end");
    _pendingEncoder=encoder;_needsFinalProducer=YES;
#ifdef FLYCAST_METAL_STANDALONE
    ++_deferredEnds;
#endif
}
- (void)didPublishEncoder
{
    _needsFinalProducer=NO;
    _passPublished=YES;
#ifdef FLYCAST_METAL_STANDALONE
    ++_queueProducers;
#endif
}
- (void)finishPendingEncoder:(BOOL)publish
{
    id<MTL4CommandEncoder> encoder=_pendingEncoder;_pendingEncoder=nil;
    if(publish && _needsFinalProducer) {
        // A failed next-encoder allocation can leave prior native work closed
        // without a new consumer. Preserve commit visibility in that case too.
        if(!encoder) {
            encoder=[_native computeCommandEncoder];
            requireCommand(encoder!=nil,"Cannot finalize Metal queue visibility");
#ifdef FLYCAST_METAL_STANDALONE
            ++_fallbackCommitFlushes;
#endif
        }
        [encoder barrierAfterStages:MTLStageAll beforeQueueStages:MTLStageAll
            visibilityOptions:MTL4VisibilityOptionDevice | (_needsAliasVisibility ? MTL4VisibilityOptionResourceAlias : 0)];
        [self didPublishEncoder];
#ifdef FLYCAST_METAL_STANDALONE
        ++_commitFlushes;
#endif
    }
    [encoder endEncoding];
}
- (metal_sync::TransferHazards *)resetTransferHazards { return [_submission resetTransferHazards]; }
- (id<MTLBuffer>)inlineUniformBuffer {return _uniformPage;}
- (FCMetal4Table *)newTable:(TableFamily)family
{
    requireCommand(self.status == FCMetal4StatusEncoding && !_ended, "Metal 4 argument table acquired after commit");
    NSMutableArray<FCMetal4Table *> *tables = [_submission tables:family];
    const NSUInteger index = _tableCursor[family]++;
    FCMetal4Table *table = index < tables.count ? tables[index] : nil;
    if (!table) {
        table = [[FCMetal4Table alloc] initWithDevice:self.device family:family];
        [_commandQueue createdTable];
        if (_commandQueue.recycling && tables.count < TablePoolLimit) [tables addObject:table];
    }
    [self retainObject:table];
    return table;
}
- (MTLGPUAddress)copyBytes:(const void *)bytes length:(NSUInteger)length
{
    requireCommand(self.status == FCMetal4StatusEncoding && !_ended, "Metal 4 inline uniforms set after commit");
    requireCommand(bytes && length && length <= self.device.maxBufferLength, "Invalid Metal 4 inline uniforms");
    if (!_uniformPage || length > _uniformPage.length - _uniformOffset) {
        const NSUInteger pageSize = std::min<NSUInteger>(UniformPageBytes, self.device.maxBufferLength);
        const BOOL reusable = length <= pageSize && _commandQueue.recycling;
        const NSUInteger index = reusable ? _pageCursor++ : UniformPageLimit;
        _uniformPage = index < _submission.pages.count ? _submission.pages[index] : nil;
        if (!_uniformPage) {
            _uniformPage = FCMetal4CreateBuffer(self.device,std::max(length,pageSize),
                MTLResourceStorageModeShared|MTLResourceCPUCacheModeWriteCombined);
            requireCommand(_uniformPage != nil, "Cannot allocate Metal 4 inline uniform storage");
            [_commandQueue createdUniformPage];
            if (reusable && _submission.pages.count < UniformPageLimit) [_submission.pages addObject:_uniformPage];
        }
        _uniformOffset = 0;
#ifdef FLYCAST_METAL_STANDALONE
        ++_inlinePagesForTesting;_inlineBytesForTesting+=_uniformPage.allocatedSize;
#endif
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
    FCMetal4ComputeEncoder *encoder = [[FCMetal4ComputeEncoder alloc] initWithCommands:self fullBoundary:YES];
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
#ifdef FLYCAST_METAL_STANDALONE
    requireCommand(!_commitFailureForTesting,"Injected Metal pre-commit failure");
#endif
    [self finishPendingEncoder:YES];
    MTL4CommitOptions *options = [MTL4CommitOptions new];
    requireCommand(options != nil, "Cannot allocate Metal 4 commit feedback");
    // Metal owns this block until feedback. It keeps the entire submission alive
    // even when the renderer/context releases its reference immediately after commit.
    [options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback) {
        NSError *failure=feedback.error;
#ifdef FLYCAST_METAL_STANDALONE
        if(!failure)failure=self->_feedbackErrorForTesting;
#endif
        self.error = failure;
        self.GPUStartTime = feedback.GPUStartTime;
        self.GPUEndTime = feedback.GPUEndTime;
        self.status = failure ? FCMetal4StatusError : FCMetal4StatusCompleted;
        for (void (^handler)(FCMetal4Commands *) in self->_handlers) {
            @try {
                try { handler(self); }
                catch (const std::exception& failure) { fprintf(stderr, "Metal 4 completion: %s\n", failure.what()); }
                catch (...) { fprintf(stderr, "Metal 4 completion failed\n"); }
            } @catch (NSException *failure) {
                fprintf(stderr, "Metal 4 completion: %s\n", failure.reason.UTF8String);
            }
        }
        // GPU work and client callbacks have ended. Old command wrappers keep
        // their completed state; only the private resource lease is recycled.
        // Presentation/event references belong to the completed submission,
        // not to the diagnostic wrapper a caller may keep afterward.
        self->_drawable = nil; self->_waitEvent = nil; self->_signalEvent = nil;
        self->_uniformPage = nil; self->_residency = nil; self->_native = nil;
        self->_objects = nil; self->_handlers = nil; self->_waitDrawables = nil;
#ifdef FLYCAST_METAL_STANDALONE
        self->_retiredReferencesForTesting.store(true,std::memory_order_release);
#endif
        [self.commandQueue recycleSubmission:self->_submission reusable:failure == nil];
        self->_submission = nil;
        dispatch_group_leave(self->_finished);
    }];
#ifdef FLYCAST_METAL_STANDALONE
    const auto residencyStart=std::chrono::steady_clock::now();
#endif
    [_residency commit];
#ifdef FLYCAST_METAL_STANDALONE
    _residencyCommitUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-residencyStart).count();
#endif
    [_native endCommandBuffer]; _ended = YES;
#ifdef FLYCAST_METAL_STANDALONE
    _encodedAllocatorBytes=_submission.allocator.allocatedSize;
    _encodedResidencyCount=_residency.allocationCount;
    _encodedAllocatorIdentity=reinterpret_cast<uintptr_t>((__bridge void *)_submission.allocator);
#endif
    id<MTL4CommandBuffer> buffers[] = {_native};
    // Feedback can retire the wrapper before commit returns. Keep the
    // post-submit queue operations independent of its retired references.
    id<MTLEvent> signalEvent=_signalEvent;
    id<MTLDrawable> presentedDrawable=_drawable;
    dispatch_group_enter(_finished);
    self.status = FCMetal4StatusCommitted;
    // Acquired drawables need a wait even when waitIdle submits a frame
    // without presentation during resize/backend shutdown.
    // DisplayLink and emulation encode independently. Keep each native
    // wait/commit/signal sequence indivisible on their shared Metal 4 queue.
    BOOL enqueued=NO;
    try { @synchronized (_commandQueue.submissionLock) {
#ifdef FLYCAST_METAL_STANDALONE
    requireCommand(!_queueCommitFailureForTesting,"Injected Metal queue pre-submit failure");
#endif
    if (_waitEvent) [_commandQueue.native waitForEvent:_waitEvent value:_waitValue];
    for (id<MTLDrawable> drawable in [_waitDrawables allObjects]) [_commandQueue.native waitForDrawable:drawable];
#ifdef FLYCAST_METAL_STANDALONE
    const auto nativeStart=std::chrono::steady_clock::now();
#endif
    [_commandQueue.native commit:buffers count:1 options:options];
#ifdef FLYCAST_METAL_STANDALONE
    _nativeCommitUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-nativeStart).count();
#endif
    enqueued=YES;
#ifdef FLYCAST_METAL_STANDALONE
    if(_earlyFeedbackForTesting){
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!_retiredReferencesForTesting.load(std::memory_order_acquire)){
            requireCommand(std::chrono::steady_clock::now()<deadline,"Early feedback test timed out");
            std::this_thread::yield();
        }
    }
#endif
    if (signalEvent) [_commandQueue.native signalEvent:signalEvent value:_signalValue];
    if (presentedDrawable) {
        [_commandQueue.native signalDrawable:presentedDrawable];
#ifdef FLYCAST_METAL_STANDALONE
        requireCommand(!_postCommitFailureForTesting,"Injected Metal post-submit failure");
#endif
        if (_duration > 0) [presentedDrawable presentAfterMinimumDuration:_duration];
        else [presentedDrawable present];
    }
    else {
#ifdef FLYCAST_METAL_STANDALONE
        requireCommand(!_postCommitFailureForTesting,"Injected Metal post-submit failure");
#endif
    }
    }} catch(...) {
        // A queue call that failed before enqueue has no GPU feedback to
        // balance either completion group. Its owner must cancel the buffer.
        if(!enqueued){self.status=FCMetal4StatusEncoding;dispatch_group_leave(_finished);}
        throw;
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
    FCMetal4Table *_vertex, *_fragment;
    BOOL _opened, _aliasAcquired;
    id<MTLBuffer> _vertexBuffers[31], _fragmentBuffers[31];
    metal_sync::PassBindings _bindings;
    BOOL _directBindings,_bindingsDirty;
    __unsafe_unretained id<MTLBuffer> _lastIndex;
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
        _aliasAcquired=commands.needsAliasVisibility;
        if([commands plansPassDependencies]) {
            std::array<metal_sync::Access,20> accesses;size_t count=0;
            const auto add=[&](id<MTLResource> resource){if(resource)accesses[count++]=allocationAccess(resource,true);};
            for(NSUInteger i=0;i<8;++i){add(descriptor.colorAttachments[i].texture);add(descriptor.colorAttachments[i].resolveTexture);}
            add(descriptor.depthAttachment.texture);add(descriptor.depthAttachment.resolveTexture);
            add(descriptor.stencilAttachment.texture);add(descriptor.stencilAttachment.resolveTexture);
            // Depth/stencil can share one allocation; 21 entries is possible
            // with visibility results, so record that buffer separately.
            [commands consumeBefore:_encoder stages:MTLStageAll accesses:accesses.data() count:count opaque:descriptor.rasterizationRateMap!=nil];
            if(descriptor.visibilityResultBuffer){auto access=allocationAccess(descriptor.visibilityResultBuffer,true);[commands consumeBefore:_encoder stages:MTLStageAll accesses:&access count:1 opaque:NO];}
        } else {
        [_encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageAll
            visibilityOptions:MTL4VisibilityOptionDevice | (_aliasAcquired ? MTL4VisibilityOptionResourceAlias : 0)];
#ifdef FLYCAST_METAL_STANDALONE
        [commands recordQueueConsumer];
#endif
        }
        _vertex = [commands newTable:VertexTable]; _fragment = [commands newTable:FragmentTable];
        [_encoder setArgumentTable:_vertex.native atStages:MTLRenderStageVertex];
        [_encoder setArgumentTable:_fragment.native atStages:MTLRenderStageFragment];
    }
    return self;
}
- (NSString *)label { return _encoder.label; }
- (void)setLabel:(NSString *)label { _encoder.label = label; }
- (void)dealloc { [self endEncoding]; }
- (void)endEncoding
{
    if (_encoder) {
        if([_commands keepsEncoderProducerBarriers]) {
            [_encoder barrierAfterStages:MTLStageAll beforeQueueStages:MTLStageAll
                visibilityOptions:MTL4VisibilityOptionDevice | ((_commands.needsAliasVisibility || [_commands plansPassDependencies]) ? MTL4VisibilityOptionResourceAlias : 0)];
            [_commands didPublishEncoder];[_encoder endEncoding];
        } else [_commands deferEncoderEnd:_encoder];
        _encoder = nil;
    }
    if (_opened) {
        [_commands endEncoder]; _opened = NO;
    }
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
{[self setVertexBuffer:buffer offset:offset atIndex:index usage:MTLResourceUsageRead|MTLResourceUsageWrite];}
- (void)setVertexBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index usage:(MTLResourceUsage)usage
{
    requireCommand(index < 31 && (!buffer || offset <= buffer.length), "Invalid Metal 4 vertex buffer range");
    [_commands useResource:buffer]; _vertexBuffers[index] = buffer;
    _bindings.set(index,allocationAccess(buffer,usage&MTLResourceUsageWrite),buffer!=nil);_bindingsDirty=YES;
    [_vertex setAddress:buffer ? buffer.gpuAddress + offset : 0 atIndex:index];
}
- (void)setFragmentBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{[self setFragmentBuffer:buffer offset:offset atIndex:index usage:MTLResourceUsageRead|MTLResourceUsageWrite];}
- (void)setFragmentBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index usage:(MTLResourceUsage)usage
{
    requireCommand(index < 31 && (!buffer || offset <= buffer.length), "Invalid Metal 4 fragment buffer range");
    [_commands useResource:buffer]; _fragmentBuffers[index] = buffer;
    _bindings.set(32+index,allocationAccess(buffer,usage&MTLResourceUsageWrite),buffer!=nil);_bindingsDirty=YES;
    [_fragment setAddress:buffer ? buffer.gpuAddress + offset : 0 atIndex:index];
}
- (void)setVertexBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    requireCommand(_commands.status == FCMetal4StatusEncoding, "Metal 4 vertex buffer offset set after commit");
    requireCommand(index < 31 && _vertexBuffers[index], "Metal 4 vertex buffer offset has no buffer");
    requireCommand(offset <= _vertexBuffers[index].length, "Invalid Metal 4 vertex buffer range");
    [_vertex setAddress:_vertexBuffers[index].gpuAddress + offset atIndex:index];
}
- (void)setFragmentBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    requireCommand(_commands.status == FCMetal4StatusEncoding, "Metal 4 fragment buffer offset set after commit");
    requireCommand(index < 31 && _fragmentBuffers[index], "Metal 4 fragment buffer offset has no buffer");
    requireCommand(offset <= _fragmentBuffers[index].length, "Invalid Metal 4 fragment buffer range");
    [_fragment setAddress:_fragmentBuffers[index].gpuAddress + offset atIndex:index];
}
- (void)setVertexBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
    requireCommand(index < 31, "Invalid Metal 4 vertex uniform binding");
    _vertexBuffers[index] = nil;
    [_vertex setAddress:[_commands copyBytes:bytes length:length] atIndex:index];
    _bindings.set(index,allocationAccess(_commands.inlineUniformBuffer,false));_bindingsDirty=YES;
}
- (void)setFragmentBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
    requireCommand(index < 31, "Invalid Metal 4 fragment uniform binding");
    _fragmentBuffers[index] = nil;
    [_fragment setAddress:[_commands copyBytes:bytes length:length] atIndex:index];
    _bindings.set(32+index,allocationAccess(_commands.inlineUniformBuffer,false));_bindingsDirty=YES;
}
// One immutable upload can be addressed by both shader stages.
- (void)setSharedBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
    requireCommand(index < 31, "Invalid Metal 4 shared uniform binding");
    const MTLGPUAddress address = [_commands copyBytes:bytes length:length];
    _vertexBuffers[index] = _fragmentBuffers[index] = nil;
    [_vertex setAddress:address atIndex:index];
    [_fragment setAddress:address atIndex:index];
    const auto access=allocationAccess(_commands.inlineUniformBuffer,false);_bindings.set(index,access);_bindings.set(32+index,access);_bindingsDirty=YES;
}
- (void)setFragmentTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index
{[self setFragmentTexture:texture atIndex:index usage:MTLResourceUsageRead|MTLResourceUsageWrite];}
- (void)setFragmentTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index usage:(MTLResourceUsage)usage
{
    requireCommand(index < 128, "Invalid Metal 4 fragment texture binding");
    [_commands useResource:texture];
    _bindings.set(64+index,allocationAccess(texture,usage&MTLResourceUsageWrite),texture!=nil);_bindingsDirty=YES;
    [_fragment setTexture:texture ? texture.gpuResourceID : MTLResourceID{} atIndex:index];
}
- (void)setFragmentSamplerState:(id<MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
    requireCommand(index < 16, "Invalid Metal 4 fragment sampler binding");
    [_commands retainObject:sampler];
    [_fragment setSamplerState:sampler ? sampler.gpuResourceID : MTLResourceID{} atIndex:index];
}
- (void)drawPrimitives:(MTLPrimitiveType)type vertexStart:(NSUInteger)start vertexCount:(NSUInteger)count
{ [self prepareDraw]; [_encoder drawPrimitives:type vertexStart:start vertexCount:count]; }
- (void)drawPrimitives:(MTLPrimitiveType)type vertexStart:(NSUInteger)start vertexCount:(NSUInteger)count instanceCount:(NSUInteger)instances
{ [self prepareDraw]; [_encoder drawPrimitives:type vertexStart:start vertexCount:count instanceCount:instances]; }
- (void)prepareDraw
{
    // commit rejects an open encoder; an active encoder therefore also proves
    // the submission is still encoding, without an atomic status read per draw.
    requireCommand(_encoder!=nil,"Metal 4 draw encoded after end/commit");
    if([_commands plansPassDependencies]) {
        if(!_directBindings || _bindings.opaque()){[_commands consumeBefore:_encoder stages:MTLStageAll accesses:nullptr count:0 opaque:YES];return;}
        if(_bindingsDirty) {
            std::array<metal_sync::Access,64> accesses;size_t count=_bindings.collect(accesses.data(),0);
            [_commands consumeBefore:_encoder stages:MTLStageVertex accesses:accesses.data() count:count opaque:NO];count=0;
            count=_bindings.collect(accesses.data(),1);
            [_commands consumeBefore:_encoder stages:MTLStageFragment accesses:accesses.data() count:count opaque:NO];_bindingsDirty=NO;
        }
        return;
    }
    if(!_aliasAcquired && _commands.needsAliasVisibility) {
        [_encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageAll
            visibilityOptions:MTL4VisibilityOptionDevice | MTL4VisibilityOptionResourceAlias];
#ifdef FLYCAST_METAL_STANDALONE
        [_commands recordQueueConsumer];
#endif
        _aliasAcquired=YES;
    }
}
- (void)setDirectShaderBindings:(BOOL)enabled {_directBindings=enabled;_bindingsDirty=YES;}
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type indexCount:(NSUInteger)count indexType:(MTLIndexType)indexType indexBuffer:(id<MTLBuffer>)buffer indexBufferOffset:(NSUInteger)offset
{ [self drawIndexedPrimitives:type indexCount:count indexType:indexType indexBuffer:buffer indexBufferOffset:offset instanceCount:1 baseVertex:0 baseInstance:0]; }
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type indexCount:(NSUInteger)count indexType:(MTLIndexType)indexType indexBuffer:(id<MTLBuffer>)buffer indexBufferOffset:(NSUInteger)offset instanceCount:(NSUInteger)instances baseVertex:(NSInteger)vertex baseInstance:(NSUInteger)instance
{
    const NSUInteger stride = indexType == MTLIndexTypeUInt16 ? 2 : 4;
    requireCommand(buffer && offset <= buffer.length && count <= (buffer.length - offset) / stride,
        "Invalid Metal 4 index buffer range");
    [_commands useResource:buffer];
    [self prepareDraw];
    if(_directBindings && _lastIndex!=buffer && [_commands plansPassDependencies]) {
        auto access=allocationAccess(buffer,false);[_commands consumeBefore:_encoder stages:MTLStageVertex accesses:&access count:1 opaque:NO];_lastIndex=buffer;
    }
    [_encoder drawIndexedPrimitives:type indexCount:count indexType:indexType
        indexBuffer:buffer.gpuAddress + offset indexBufferLength:buffer.length - offset
        instanceCount:instances baseVertex:vertex baseInstance:instance];
}
@end

@implementation FCMetal4ComputeEncoder {
    FCMetal4Commands *_commands;
    id<MTL4ComputeCommandEncoder> _encoder;
    FCMetal4Table *_table;
    BOOL _hasWork, _opened, _fullBoundary, _aliasAcquired;
    metal_sync::TransferHazards *_hazards;
    metal_sync::PassBindings _bindings;
    BOOL _directBindings;
#ifdef FLYCAST_METAL_STANDALONE
    BOOL _ownedResourceBenchmark;
#endif
}
- (instancetype)initWithCommands:(FCMetal4Commands *)commands
{ return [self initWithCommands:commands fullBoundary:NO]; }
- (instancetype)initWithCommands:(FCMetal4Commands *)commands fullBoundary:(BOOL)fullBoundary
{
    if ((self = [super init])) {
        _commands = commands;
        _fullBoundary=fullBoundary;
#ifdef FLYCAST_METAL_STANDALONE
        _fullBoundary|=[commands fullSynchronizationForTesting];
        _ownedResourceBenchmark=[commands ownedResourceBenchmarkForTesting];
#endif
        [commands beginEncoder]; _opened = YES;
        _hazards=[commands resetTransferHazards];
        _encoder = [commands.native computeCommandEncoder];
        requireCommand(_encoder != nil, "Cannot create Metal 4 compute encoder");
        _aliasAcquired=commands.needsAliasVisibility;
        BOOL aliasVisibility=_aliasAcquired;
#ifdef FLYCAST_METAL_STANDALONE
        if(_ownedResourceBenchmark) {aliasVisibility=NO;_aliasAcquired=YES;}
#endif
        if([commands plansPassDependencies]) {
            if(_fullBoundary)[commands consumeBefore:_encoder stages:MTLStageAll accesses:nullptr count:0 opaque:YES];
        } else {
        [_encoder barrierAfterQueueStages:MTLStageAll beforeStages:_fullBoundary ? MTLStageAll : ComputeStages
            visibilityOptions:MTL4VisibilityOptionDevice | (aliasVisibility ? MTL4VisibilityOptionResourceAlias : 0)];
#ifdef FLYCAST_METAL_STANDALONE
        [commands recordQueueConsumer];
#endif
        }
    }
    return self;
}
- (NSString *)label { return _encoder.label; }
- (void)setLabel:(NSString *)label { _encoder.label = label; }
- (void)dealloc { [self endEncoding]; }
- (void)endEncoding
{
    if (_encoder) {
        BOOL aliasVisibility=_commands.needsAliasVisibility;
#ifdef FLYCAST_METAL_STANDALONE
        if(_ownedResourceBenchmark)aliasVisibility=NO;
#endif
        if(_fullBoundary || [_commands keepsEncoderProducerBarriers]) {
            [_encoder barrierAfterStages:(_fullBoundary || [_commands plansPassDependencies]) ? MTLStageAll : ComputeStages beforeQueueStages:MTLStageAll
                visibilityOptions:MTL4VisibilityOptionDevice | ((aliasVisibility || [_commands plansPassDependencies]) ? MTL4VisibilityOptionResourceAlias : 0)];
            [_commands didPublishEncoder];[_encoder endEncoding];
        } else [_commands deferEncoderEnd:_encoder];
        _encoder = nil;
    }
    if (_opened) {
        [_commands endEncoder]; _opened = NO;
    }
}
- (void)beforeWork:(MTLStages)stage accesses:(const metal_sync::Access *)accesses count:(size_t)count opaque:(BOOL)opaque
{
    requireCommand(_encoder!=nil,"Metal 4 compute work encoded after end/commit");
    if([_commands plansPassDependencies]) {
        if(stage==MTLStageDispatch && opaque && _directBindings && !_bindings.opaque()) {
            std::array<metal_sync::Access,64> bound;size_t size=_bindings.collect(bound.data(),2);
            [_commands consumeBefore:_encoder stages:stage accesses:bound.data() count:size opaque:NO];
        } else if(opaque || count)[_commands consumeBefore:_encoder stages:stage accesses:accesses count:count opaque:opaque];
    } else if(!_aliasAcquired && _commands.needsAliasVisibility) {
        [_encoder barrierAfterQueueStages:MTLStageAll beforeStages:_fullBoundary ? MTLStageAll : ComputeStages
            visibilityOptions:MTL4VisibilityOptionDevice | MTL4VisibilityOptionResourceAlias];
#ifdef FLYCAST_METAL_STANDALONE
        [_commands recordQueueConsumer];
#endif
        _aliasAcquired=YES;
    }
    metal_sync::Barrier barrier;
#ifdef FLYCAST_METAL_STANDALONE
    if([_commands fullSynchronizationForTesting]) {barrier.needed=_hasWork;barrier.after=ComputeStages;}
    else
#endif
        barrier=_hazards->before(stage,accesses,count,opaque);
    if(barrier.needed && _commands.needsAliasVisibility)barrier.alias=true;
#ifdef FLYCAST_METAL_STANDALONE
    if(_ownedResourceBenchmark)barrier.alias=false;
    [_commands recordTransferBarrier:barrier.needed afterStages:barrier.after alias:barrier.alias];
#endif
    if(barrier.needed)[_encoder barrierAfterEncoderStages:MTLStages(barrier.after)
        beforeEncoderStages:_fullBoundary ? ComputeStages : stage visibilityOptions:MTL4VisibilityOptionDevice | (barrier.alias ? MTL4VisibilityOptionResourceAlias : 0)];
    _hasWork=YES;
}
- (FCMetal4Table *)table
{
    if (!_table) {
        _table = [_commands newTable:ComputeTable];
        [_encoder setArgumentTable:_table.native];
    }
    return _table;
}
- (void)setComputePipelineState:(id<MTLComputePipelineState>)state { [_commands retainObject:state]; [_encoder setComputePipelineState:state]; }
- (void)setBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{[self setBuffer:buffer offset:offset atIndex:index usage:MTLResourceUsageRead|MTLResourceUsageWrite];}
- (void)setBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index usage:(MTLResourceUsage)usage
{
    requireCommand(index < 31 && (!buffer || offset <= buffer.length), "Invalid Metal 4 compute buffer range");
    [_commands useResource:buffer]; [[self table] setAddress:buffer ? buffer.gpuAddress + offset : 0 atIndex:index];
    _bindings.set(index,allocationAccess(buffer,usage&MTLResourceUsageWrite),buffer!=nil);
}
- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
    requireCommand(index < 31, "Invalid Metal 4 compute uniform binding");
    [[self table] setAddress:[_commands copyBytes:bytes length:length] atIndex:index];
    _bindings.set(index,allocationAccess(_commands.inlineUniformBuffer,false));
}
- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index
{[self setTexture:texture atIndex:index usage:MTLResourceUsageRead|MTLResourceUsageWrite];}
- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index usage:(MTLResourceUsage)usage
{
    requireCommand(index < 128, "Invalid Metal 4 compute texture binding");
    [_commands useResource:texture]; [[self table] setTexture:texture ? texture.gpuResourceID : MTLResourceID{} atIndex:index];
    _bindings.set(64+index,allocationAccess(texture,usage&MTLResourceUsageWrite),texture!=nil);
}
- (void)setDirectShaderBindings:(BOOL)enabled {_directBindings=enabled;}
- (void)dispatchThreads:(MTLSize)threads threadsPerThreadgroup:(MTLSize)group
{
    [self beforeWork:MTLStageDispatch accesses:nullptr count:0 opaque:threads.width && threads.height && threads.depth];
    if(!threads.width || !threads.height || !threads.depth)return;
    [_encoder dispatchThreads:threads threadsPerThreadgroup:group];
}
- (void)fillBuffer:(id<MTLBuffer>)buffer range:(NSRange)range value:(uint8_t)value
{
    requireCommand(range.location <= buffer.length && range.length <= buffer.length - range.location, "Invalid Metal 4 buffer fill range");
    if(!range.length) { [self beforeWork:MTLStageBlit accesses:nullptr count:0 opaque:NO]; return; }
    [_commands useResource:buffer];
    const auto access=allocationAccess(buffer,true,range.location,range.location+range.length);
    [self beforeWork:MTLStageBlit accesses:&access count:1 opaque:NO];[_encoder fillBuffer:buffer range:range value:value];
}
- (void)generateMipmapsForTexture:(id<MTLTexture>)texture
{
    [_commands useResource:texture];const auto access=allocationAccess(texture,true);
    [self beforeWork:MTLStageBlit accesses:&access count:texture.mipmapLevelCount>1 ? 1 : 0 opaque:NO];
    if(texture.mipmapLevelCount>1)[_encoder generateMipmapsForTexture:texture];
}
- (void)copyFromTexture:(id<MTLTexture>)source sourceSlice:(NSUInteger)slice sourceLevel:(NSUInteger)level sourceOrigin:(MTLOrigin)origin sourceSize:(MTLSize)size toTexture:(id<MTLTexture>)destination destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin
{
    [_commands useResource:source]; [_commands useResource:destination];
    const metal_sync::Access accesses[]={textureAccess(source,level,slice,false),textureAccess(destination,destinationLevel,destinationSlice,true)};
    [self beforeWork:MTLStageBlit accesses:accesses count:size.width && size.height && size.depth ? 2 : 0 opaque:NO];
    if(!size.width || !size.height || !size.depth)return;
    [_encoder copyFromTexture:source sourceSlice:slice sourceLevel:level sourceOrigin:origin sourceSize:size toTexture:destination destinationSlice:destinationSlice destinationLevel:destinationLevel destinationOrigin:destinationOrigin];
}
- (void)copyFromTexture:(id<MTLTexture>)source sourceSlice:(NSUInteger)slice sourceLevel:(NSUInteger)level sourceOrigin:(MTLOrigin)origin sourceSize:(MTLSize)size toBuffer:(id<MTLBuffer>)destination destinationOffset:(NSUInteger)offset destinationBytesPerRow:(NSUInteger)row destinationBytesPerImage:(NSUInteger)image
{
    [_commands useResource:source]; [_commands useResource:destination];
    const metal_sync::Access accesses[]={textureAccess(source,level,slice,false),allocationAccess(destination,true)};
    [self beforeWork:MTLStageBlit accesses:accesses count:size.width && size.height && size.depth ? 2 : 0 opaque:NO];
    if(!size.width || !size.height || !size.depth)return;
    [_encoder copyFromTexture:source sourceSlice:slice sourceLevel:level sourceOrigin:origin sourceSize:size toBuffer:destination destinationOffset:offset destinationBytesPerRow:row destinationBytesPerImage:image];
}
- (void)copyFromBuffer:(id<MTLBuffer>)source sourceOffset:(NSUInteger)offset sourceBytesPerRow:(NSUInteger)row sourceBytesPerImage:(NSUInteger)image sourceSize:(MTLSize)size toTexture:(id<MTLTexture>)destination destinationSlice:(NSUInteger)slice destinationLevel:(NSUInteger)level destinationOrigin:(MTLOrigin)origin
{
    [_commands useResource:source]; [_commands useResource:destination];
    const metal_sync::Access accesses[]={allocationAccess(source,false),textureAccess(destination,level,slice,true)};
    [self beforeWork:MTLStageBlit accesses:accesses count:size.width && size.height && size.depth ? 2 : 0 opaque:NO];
    if(!size.width || !size.height || !size.depth)return;
    [_encoder copyFromBuffer:source sourceOffset:offset sourceBytesPerRow:row sourceBytesPerImage:image sourceSize:size toTexture:destination destinationSlice:slice destinationLevel:level destinationOrigin:origin];
}
@end
#endif // Apple Silicon Metal 4 slice
