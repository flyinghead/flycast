// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_METAL
#include "metal4_pipeline_cache.h"
#include "oslib/oslib.h"
#include "cfg/cfg.h"
#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <fcntl.h>
#include <sys/stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr off_t MaxArchiveBytes = 64 * 1024 * 1024;
constexpr off_t MaxCacheBytes = 256 * 1024 * 1024;
constexpr size_t MaxCacheFiles = 16, MaxPendingRecords = 64, MaxNewEntries = 4096;
constexpr const char *CachePrefix = "metal4_pipeline_";
#ifdef FLYCAST_METAL_STANDALONE
std::atomic<bool> identityOptimizationsForTesting{true};
std::atomic<bool> cacheTimingForTesting{false};
#endif

struct PipelineTiming
{
	std::atomic<u64> calls{0}, nanoseconds{0}, maximum{0};
};

class PipelineTimer
{
public:
	explicit PipelineTimer(PipelineTiming *timing) : timing(timing),
		started(timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}) {}
	~PipelineTimer()
	{
		if (!timing) return;
		const u64 elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now() - started).count();
		timing->calls.fetch_add(1, std::memory_order_relaxed);
		timing->nanoseconds.fetch_add(elapsed, std::memory_order_relaxed);
		u64 maximum = timing->maximum.load(std::memory_order_relaxed);
		while (maximum < elapsed && !timing->maximum.compare_exchange_weak(maximum, elapsed,
			std::memory_order_relaxed)) {}
	}
	PipelineTimer(const PipelineTimer&) = delete;
	PipelineTimer& operator=(const PipelineTimer&) = delete;
private:
	PipelineTiming *timing;
	std::chrono::steady_clock::time_point started;
};

void logTiming(const char *operation, const PipelineTiming& timing)
{
	const u64 calls = timing.calls.load(std::memory_order_relaxed);
	if (!calls) return;
	const double totalMs = timing.nanoseconds.load(std::memory_order_relaxed) / 1000000.0;
	NOTICE_LOG(RENDERER, "Metal timing cache %s: %llu calls CPU elapsed total %.3f ms mean %.3f ms max %.3f ms",
		operation, (unsigned long long)calls, totalMs, totalMs / calls,
		timing.maximum.load(std::memory_order_relaxed) / 1000000.0);
}

bool archiveFile(NSURL *url)
{
	struct stat info{};
	return lstat(url.fileSystemRepresentation, &info) == 0 && S_ISREG(info.st_mode)
		&& info.st_size > 0 && info.st_size <= MaxArchiveBytes;
}

// Verify bytes before handing an optional disk cache to the driver. A malformed
// MTL4 archive can cause a native fault rather than an Objective-C exception.
// Digest-bearing filenames avoid a second sidecar transaction or payload copy.
NSString *fileDigest(NSURL *url)
{
	const int fd=open(url.fileSystemRepresentation,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
	if(fd<0)return nil;
	struct Close {int fd;~Close(){close(fd);}} guard{fd};
	struct stat before{},after{};
	if(fstat(fd,&before)!=0 || !S_ISREG(before.st_mode) || before.st_size<=0 || before.st_size>MaxArchiveBytes)return nil;
	CC_SHA256_CTX hash;CC_SHA256_Init(&hash);
	std::array<unsigned char,64*1024> bytes;off_t total=0;
	for(;;) {
		const ssize_t count=read(fd,bytes.data(),bytes.size());
		if(count<0) {if(errno==EINTR)continue;return nil;}
		if(count==0)break;
		if(count>before.st_size-total)return nil;
		CC_SHA256_Update(&hash,bytes.data(),CC_LONG(count));total+=count;
	}
	if(total!=before.st_size || fstat(fd,&after)!=0 || before.st_size!=after.st_size
		|| before.st_mtimespec.tv_sec!=after.st_mtimespec.tv_sec || before.st_mtimespec.tv_nsec!=after.st_mtimespec.tv_nsec
		|| before.st_ctimespec.tv_sec!=after.st_ctimespec.tv_sec || before.st_ctimespec.tv_nsec!=after.st_ctimespec.tv_nsec)return nil;
	unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256_Final(digest,&hash);
	char hex[65];for(size_t i=0;i<sizeof(digest);++i){hex[i*2]="0123456789abcdef"[digest[i]>>4];hex[i*2+1]="0123456789abcdef"[digest[i]&15];}hex[64]=0;
	return [NSString stringWithUTF8String:hex];
}

const char *errorText(NSError *error)
{
	return error ? error.localizedDescription.UTF8String : "operation failed";
}

// Each serializer flush creates a new immutable segment. In particular it
// must never overwrite the archive of pipelines captured by an earlier flush.
NSString *archiveIdentity(NSString *name)
{
	const NSUInteger prefix = strlen(CachePrefix);
	const NSUInteger oldLength=prefix+64+1+36+6;
	const bool checked=name.length==oldLength+65;
	if (![name hasPrefix:@"metal4_pipeline_"] || (name.length!=oldLength && !checked)
			|| ![name hasSuffix:@".cache"] || [name characterAtIndex:prefix + 64] != '-') return nil;
	NSCharacterSet *invalid = [[NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdef"] invertedSet];
	if ([[name substringWithRange:NSMakeRange(prefix, 64)] rangeOfCharacterFromSet:invalid].location != NSNotFound
			|| ![[NSUUID alloc] initWithUUIDString:[name substringWithRange:NSMakeRange(prefix + 65, 36)]]) return nil;
	if(checked && ([name characterAtIndex:prefix+101]!='-'
		|| [[name substringWithRange:NSMakeRange(prefix+102,64)] rangeOfCharacterFromSet:invalid].location!=NSNotFound))return nil;
	return [name substringToIndex:prefix + 64];
}

NSString *archiveDigest(NSString *name)
{
	const NSUInteger prefix=strlen(CachePrefix);
	return archiveIdentity(name) && name.length==prefix+64+1+36+1+64+6
		? [name substringWithRange:NSMakeRange(prefix+102,64)] : nil;
}

NSArray<NSURL *> *cacheFiles(NSURL *directory)
{
	NSArray<NSURL *> *files = [NSFileManager.defaultManager contentsOfDirectoryAtURL:directory
		includingPropertiesForKeys:@[NSURLContentModificationDateKey] options:NSDirectoryEnumerationSkipsHiddenFiles error:nil];
	return [files sortedArrayUsingComparator:^NSComparisonResult(NSURL *a, NSURL *b) {
		NSDate *left = nil, *right = nil;
		[a getResourceValue:&left forKey:NSURLContentModificationDateKey error:nil];
		[b getResourceValue:&right forKey:NSURLContentModificationDateKey error:nil];
		NSComparisonResult order = [right compare:left];
		return order == NSOrderedSame ? [a.lastPathComponent compare:b.lastPathComponent] : order;
	}];
}

API_AVAILABLE(macos(26.0))
id<MTL4Archive> openArchive(id<MTLDevice> device, NSURL *url)
{
	NSError *error = nil;
	id<MTL4Archive> archive = nil;
	@try { archive = [device newArchiveWithURL:url error:&error]; }
	@catch (NSException *exception) {
		WARN_LOG(RENDERER, "Metal pipeline cache ignored: %s", exception.reason.UTF8String);
		return nil;
	}
	if (!archive) WARN_LOG(RENDERER, "Metal pipeline cache unavailable: %s", errorText(error));
	return archive;
}
}

struct MetalPipelineCache::Impl
{
	id<MTLDevice> device;
	id<MTL4Compiler> foregroundCompiler, materialCompiler;
	// Read-only segments and independent archive instances per caller queue.
	NSArray<id<MTL4Archive>> *foreground, *material;
	// Only writerQueue uses this compiler and its serializer after creation.
	id<MTL4Compiler> writer;
	id<MTL4PipelineDataSetSerializer> serializer;
	dispatch_queue_t writerQueue;
	NSURL *url;
	int lock = -1;
	std::atomic<bool> writable{false};
	bool accepting = true; // Protected by recordsMutex through queue admission.
	std::mutex recordsMutex,hintsMutex;
	// Empty segment name means queued/captured but not serialized yet.
	std::unordered_map<std::string,std::string> records;
	std::vector<std::string> unsavedRecords; // Writer queue only.
	std::array<std::unordered_map<std::string,NSUInteger>,3> hints;
	std::atomic<size_t> pending{0};
	std::atomic<u64> hits{0}, misses{0};
	std::atomic<u64> probes{0},hintedHits{0},queued{0},recordings{0},duplicates{0},savedSegments{0};
	std::atomic<u64> descriptorCopies{0};
	std::atomic<bool> reported{false};
	std::atomic<size_t> newEntries{0};
	bool timing = false;
	// Lookup/creation categories: foreground render, background material,
	// foreground compute. Writer timings are separate and can overlap them.
	std::array<PipelineTiming, 3> lookupTimings, creationTimings;
	std::array<PipelineTiming, 2> recordTimings;
	PipelineTiming serializeTiming;
	PipelineTiming publicationTiming;
#ifdef FLYCAST_METAL_STANDALONE
	bool optimizeIdentitiesForTesting=true;
#endif
	bool identitiesEnabled() const {
#ifdef FLYCAST_METAL_STANDALONE
		return optimizeIdentitiesForTesting;
#else
		return true;
#endif
	}
	bool dirty = false, saveScheduled = false;
	std::array<NSUInteger,MaxCacheFiles> lookupOrder(size_t category,const std::string& identity,NSUInteger count,NSUInteger& hint)
	{
		hint=NSNotFound;
		try { if(identitiesEnabled() && !identity.empty()) {
			std::lock_guard<std::mutex> lock(hintsMutex);
			auto found=hints[category].find(identity);
			if(found!=hints[category].end() && found->second<count)hint=found->second;
		} } catch(...) { hint=NSNotFound; }
		std::array<NSUInteger,MaxCacheFiles> order{};size_t used=0;
		if(hint!=NSNotFound)order[used++]=hint;
		for(NSUInteger i=0;i<count;++i)if(i!=hint)order[used++]=i;
		return order;
	}
	void remember(size_t category,const std::string& identity,NSUInteger index) noexcept
	{
		if(!identitiesEnabled() || identity.empty())return;
		try {
		std::lock_guard<std::mutex> lock(hintsMutex);
		auto& map=hints[category];
		if(map.size()<MaxNewEntries || map.find(identity)!=map.end())map[identity]=index;
		} catch(...) { /* A lookup hint is optional, including under memory pressure. */ }
	}
	~Impl() { if (lock >= 0) close(lock); }
};

MetalPipelineCache::MetalPipelineCache() : impl(std::make_unique<Impl>()) {}
MetalPipelineCache::~MetalPipelineCache() = default;
#ifdef FLYCAST_METAL_STANDALONE
void MetalPipelineCache::setIdentityOptimizationsForTesting(bool enabled,bool timing){identityOptimizationsForTesting=enabled;cacheTimingForTesting=timing;}
#endif

std::shared_ptr<MetalPipelineCache> MetalPipelineCache::create(id<MTL4Compiler> foreground, id<MTL4Compiler> material,
	const char *source, size_t sourceSize, MTLCompileOptions *options)
{
	id<MTLDevice> device = foreground.device;
	if (!foreground || !material || material.device != device) return nullptr;
	try {
		@try {
			// Never reuse uninstrumented binaries in a GPU shader-validation run.
			NSDictionary<NSString *, NSString *> *environment = NSProcessInfo.processInfo.environment;
			for (NSString *name in environment)
				if ([name hasPrefix:@"MTL_SHADER_VALIDATION"] && environment[name].length
						&& ![environment[name] isEqualToString:@"0"]) {
					INFO_LOG(RENDERER, "Metal pipeline archive disabled for shader validation");
					return nullptr;
				}
			NSString *identity = [NSString stringWithFormat:@"Flycast Metal4 archive v2; checked segments; arm64; MSL %lu; math %ld functions %ld; invariant %d optimization %ld; GPU %llu %@; %@;",
				(unsigned long)options.languageVersion, (long)options.mathMode, (long)options.mathFloatingPointFunctions,
				options.preserveInvariance, (long)options.optimizationLevel,
				(unsigned long long)device.registryID, device.name, NSProcessInfo.processInfo.operatingSystemVersionString];
			CC_SHA256_CTX hash;
			CC_SHA256_Init(&hash);
			CC_SHA256_Update(&hash, identity.UTF8String, (CC_LONG)[identity lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
			// The same shader source may select a different scoped arithmetic
			// policy. Include its actual compiler macros, in deterministic order.
			for (NSString *key in [options.preprocessorMacros.allKeys sortedArrayUsingSelector:@selector(compare:)]) {
				NSString *macro = [NSString stringWithFormat:@"%@=%@;", key, options.preprocessorMacros[key]];
				CC_SHA256_Update(&hash, macro.UTF8String, (CC_LONG)[macro lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
			}
			CC_SHA256_Update(&hash, source, (CC_LONG)sourceSize);
			unsigned char digest[CC_SHA256_DIGEST_LENGTH];
			CC_SHA256_Final(digest, &hash);
			std::string name = CachePrefix;
			for (unsigned char byte : digest) {
				name += "0123456789abcdef"[byte >> 4];
				name += "0123456789abcdef"[byte & 15];
			}
			std::string path = hostfs::getShaderCachePath(name);
			NSString *nativePath = [NSString stringWithUTF8String:path.c_str()];
			if (!nativePath) return nullptr;
			NSURL *url = [NSURL fileURLWithPath:nativePath];
			NSError *error = nil;
			if (![NSFileManager.defaultManager createDirectoryAtURL:url.URLByDeletingLastPathComponent
					withIntermediateDirectories:YES attributes:nil error:&error]) {
				WARN_LOG(RENDERER, "Metal pipeline cache directory unavailable: %s", errorText(error));
				return nullptr;
			}
			auto cache = std::shared_ptr<MetalPipelineCache>(new MetalPipelineCache());
			auto& state = *cache->impl;
#ifdef FLYCAST_METAL_STANDALONE
			state.optimizeIdentitiesForTesting=identityOptimizationsForTesting.load();
#endif
			state.device = device;
			state.foregroundCompiler = foreground;
			state.materialCompiler = material;
			state.url = url;
			state.timing = config::loadBool("log", "MetalFrameTiming", false);
#ifdef FLYCAST_METAL_STANDALONE
			state.timing |= cacheTimingForTesting.load();
#endif
			state.writerQueue = dispatch_queue_create("org.flycast.metal.archive",
				dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0));
			if (!state.writerQueue) return nullptr;
			std::string lockPath = path + ".lock";
			state.lock = open(lockPath.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
			if (state.lock >= 0 && flock(state.lock, LOCK_EX | LOCK_NB) == 0)
				state.writable.store(true);
			else {
				if (state.lock >= 0) close(state.lock);
				state.lock = -1;
				INFO_LOG(RENDERER, "Metal pipeline cache is read-only: %s", path.c_str());
			}
			NSMutableArray<id<MTL4Archive>> *foregroundArchives = [NSMutableArray array];
			NSMutableArray<id<MTL4Archive>> *materialArchives = [NSMutableArray array];
			off_t loadedBytes = 0;
			for (NSURL *file in cacheFiles(url.URLByDeletingLastPathComponent)) {
				if (![archiveIdentity(file.lastPathComponent) isEqualToString:url.lastPathComponent]
						|| !archiveFile(file)) continue;
				struct stat info{};
				if (lstat(file.fileSystemRepresentation, &info) != 0 || !S_ISREG(info.st_mode)
						|| info.st_size <= 0 || info.st_size > MaxArchiveBytes
						|| info.st_size > MaxCacheBytes - loadedBytes) continue;
				if (foregroundArchives.count >= MaxCacheFiles) break;
				NSString *expected=archiveDigest(file.lastPathComponent);
				NSString *actual=expected ? fileDigest(file) : nil;
				if(!expected || ![expected isEqualToString:actual]) {
					WARN_LOG(RENDERER,"Metal pipeline cache ignored: missing or mismatched archive digest");
					// Only remove a known-invalid segment while owning its identity
					// lock. A transient read failure (nil digest) merely skips it.
					if(state.writable.load() && (!expected || actual))unlink(file.fileSystemRepresentation);
					continue;
				}
				// Immutable segment: verify once, then open separate driver lookup
				// instances without reading/hash-checking the same payload twice.
				id<MTL4Archive> archive = openArchive(device, file);
				if (!archive) continue;
				[foregroundArchives addObject:archive];
				// Never share a driver's mutable lookup state between queues.
				id<MTL4Archive> materialArchive = openArchive(device, file);
				if (materialArchive) [materialArchives addObject:materialArchive];
				loadedBytes += info.st_size;
			}
			state.foreground = [foregroundArchives copy];
			state.material = [materialArchives copy];
			if (state.foreground.count) NOTICE_LOG(RENDERER, "Metal 4 pipeline cache loaded: %lu segments", (unsigned long)state.foreground.count);
			if (state.writable.load()) {
				MTL4PipelineDataSetSerializerDescriptor *serializerDescriptor = [[MTL4PipelineDataSetSerializerDescriptor alloc] init];
				// Binary archives need binary capture only. Combining descriptor
				// capture currently makes Apple's serializer return NO without an
				// NSError, even after successful pipeline creation.
				serializerDescriptor.configuration = MTL4PipelineDataSetSerializerConfigurationCaptureBinaries;
				state.serializer = [device newPipelineDataSetSerializerWithDescriptor:serializerDescriptor];
				if (state.serializer) {
					MTL4CompilerDescriptor *writerDescriptor = [[MTL4CompilerDescriptor alloc] init];
					writerDescriptor.label = @"Flycast Metal 4 archive writer";
					writerDescriptor.pipelineDataSetSerializer = state.serializer;
					state.writer = [device newCompilerWithDescriptor:writerDescriptor error:&error];
				}
				if (!state.writer) state.writable.store(false);
				dispatch_async(state.writerQueue, ^{ @autoreleasepool { cache->prune(); } });
			}
			return cache;
		} @catch (NSException *exception) {
			WARN_LOG(RENDERER, "Metal pipeline cache disabled: %s", exception.reason.UTF8String);
		}
	} catch (const std::exception& error) {
		WARN_LOG(RENDERER, "Metal pipeline cache disabled: %s", error.what());
	}
	return nullptr;
}

id<MTLRenderPipelineState> MetalPipelineCache::makeRenderPipeline(MTL4RenderPipelineDescriptor *descriptor,
	NSError **error, bool material, const std::string& identity)
{
	const auto archives=material ? impl->material : impl->foreground;
	const size_t category=material ? 1 : 0;NSUInteger hint;
	const auto order=impl->lookupOrder(category,identity,archives.count,hint);
	for (NSUInteger slot=0;slot<archives.count;++slot) {
		const NSUInteger index=order[slot];id<MTL4Archive> archive=archives[index];
		++impl->probes;
		PipelineTimer timer(impl->timing ? &impl->lookupTimings[material ? 1 : 0] : nullptr);
		id<MTLRenderPipelineState> pipeline = nil;
		NSError *lookupError = nil;
		@try {
			pipeline = [archive newRenderPipelineStateWithDescriptor:descriptor error:&lookupError];
		} @catch (NSException *) { /* Optional archive lookup; compile normally below. */ }
		if (pipeline) {
			++impl->hits;if(index==hint)++impl->hintedHits;
			impl->remember(category,identity,index);
			if (error) *error = nil; return pipeline;
		}
	}
	++impl->misses;
	// A damaged or missing optional archive never changes shader correctness.
	id<MTLRenderPipelineState> pipeline = nil;
	{
		PipelineTimer timer(impl->timing ? &impl->creationTimings[material ? 1 : 0] : nullptr);
		id<MTL4Compiler> compiler = material ? impl->materialCompiler : impl->foregroundCompiler;
		pipeline = [compiler newRenderPipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:error];
	}
	if (pipeline) record(descriptor, false, identity);
	return pipeline;
}

id<MTLComputePipelineState> MetalPipelineCache::makeComputePipeline(MTL4ComputePipelineDescriptor *descriptor,
	NSError **error, const std::string& identity)
{
	NSUInteger hint;const auto order=impl->lookupOrder(2,identity,impl->foreground.count,hint);
	for (NSUInteger slot=0;slot<impl->foreground.count;++slot) {
		const NSUInteger index=order[slot];id<MTL4Archive> archive=impl->foreground[index];
		++impl->probes;
		PipelineTimer timer(impl->timing ? &impl->lookupTimings[2] : nullptr);
		id<MTLComputePipelineState> pipeline = nil;
		NSError *lookupError = nil;
		@try {
			pipeline = [archive newComputePipelineStateWithDescriptor:descriptor error:&lookupError];
		} @catch (NSException *) { /* Optional archive lookup; compile normally below. */ }
		if (pipeline) {
			++impl->hits;if(index==hint)++impl->hintedHits;
			impl->remember(2,identity,index);
			if (error) *error = nil; return pipeline;
		}
	}
	++impl->misses;
	id<MTLComputePipelineState> pipeline = nil;
	{
		PipelineTimer timer(impl->timing ? &impl->creationTimings[2] : nullptr);
		pipeline = [impl->foregroundCompiler newComputePipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:error];
	}
	if (pipeline) record(descriptor, true, identity);
	return pipeline;
}

void MetalPipelineCache::record(id descriptor, bool compute, const std::string& identity) noexcept
{
	if (!impl->writable.load() || impl->newEntries.load() >= MaxNewEntries) return;
	bool admitted=false,counted=false;
	try {
		@try {
			auto cache = shared_from_this();
			const std::string key=impl->identitiesEnabled()?identity:std::string{};
			std::lock_guard<std::mutex> lock(impl->recordsMutex);
			if(!impl->accepting || !impl->writable.load()) return;
			if(!key.empty()) {
				if(impl->records.find(key)!=impl->records.end()) {++impl->duplicates;return;}
				if(impl->records.size()>=MaxNewEntries) return;
			}
			if(impl->pending.fetch_add(1)>=MaxPendingRecords) {--impl->pending;return;}
			counted=true;
			// Reject duplicates before copying the entire descriptor tree.
			// Keep copying and dispatch under the admission lock, so finish
			// cannot flush between reservation and publication to the queue.
			++impl->descriptorCopies;
			id copy = [descriptor copy];
			if(!copy) {--impl->pending;return;}
			if(!key.empty()) {
				impl->records.emplace(key,std::string{});admitted=true;
			}
			// Queue admission and finish's barrier are ordered by recordsMutex.
			dispatch_async(impl->writerQueue, ^{
				@autoreleasepool { cache->recordOnWriter(copy, compute, key); }
			});
			++impl->queued;
		} @catch (NSException *exception) {
			if(counted)--impl->pending;
			if(admitted) {std::lock_guard<std::mutex> lock(impl->recordsMutex);impl->records.erase(identity);}
			WARN_LOG(RENDERER, "Metal pipeline cache record skipped: %s", exception.reason.UTF8String);
		}
	} catch (const std::exception& error) {
		if(counted)--impl->pending;
		if(admitted) {std::lock_guard<std::mutex> lock(impl->recordsMutex);impl->records.erase(identity);}
		WARN_LOG(RENDERER, "Metal pipeline cache record skipped: %s", error.what());
	}
}

void MetalPipelineCache::recordOnWriter(id descriptor, bool compute, const std::string& identity) noexcept
{
	--impl->pending;
	if (!impl->writable.load() || impl->newEntries.load() >= MaxNewEntries) return;
	try {
		@try {
			auto& state = *impl;
			NSError *error = nil;
			id pipeline = nil;
			{
				PipelineTimer timer(state.timing ? &state.recordTimings[compute ? 1 : 0] : nullptr);
				++state.recordings;
				if (compute) pipeline = [state.writer newComputePipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:&error];
				else pipeline = [state.writer newRenderPipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:&error];
			}
			if (!pipeline) {
				WARN_LOG(RENDERER, "Metal pipeline cache recording stopped: %s", errorText(error));
				state.writable.store(false); return;
			}
			++state.newEntries;
			if(!identity.empty())state.unsavedRecords.push_back(identity);
			state.dirty = true;
			if (!state.saveScheduled) {
				state.saveScheduled = true;
				std::weak_ptr<MetalPipelineCache> weak = shared_from_this();
				dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), state.writerQueue, ^{
					@autoreleasepool {
						if (auto active = weak.lock()) {
							active->impl->saveScheduled = false;
							active->save();
							active->prune();
						}
					}
				});
			}
		} @catch (NSException *exception) {
			impl->writable.store(false);
			WARN_LOG(RENDERER, "Metal pipeline cache recording stopped: %s", exception.reason.UTF8String);
		}
	} catch (const std::exception& error) {
		impl->writable.store(false);
		WARN_LOG(RENDERER, "Metal pipeline cache recording stopped: %s", error.what());
	}
}

void MetalPipelineCache::save() noexcept
{
	if (!impl->dirty || !impl->writable.load()) return;
	PipelineTimer publication(impl->timing?&impl->publicationTiming:nullptr);
	NSURL *temporary = nil;
	try {
		@try {
			NSString *base=[impl->url.path stringByAppendingFormat:@"-%@",NSUUID.UUID.UUIDString];
			temporary = [NSURL fileURLWithPath:[base stringByAppendingFormat:@".cache.tmp-%@", NSUUID.UUID.UUIDString]];
			NSError *error = nil;
			BOOL serialized = NO;
			{
				PipelineTimer timer(impl->timing ? &impl->serializeTiming : nullptr);
				serialized = [impl->serializer serializeAsArchiveAndFlushToURL:temporary error:&error];
			}
			const char *reason = errorText(error);
			bool saved = serialized && archiveFile(temporary);
			if (serialized && !saved) reason = "archive empty, invalid or larger than 64 MiB";
			NSString *digest=saved ? fileDigest(temporary) : nil;
			if(saved && !digest){saved=false;reason="cannot hash serialized archive";}
			NSURL *segment=digest ? [NSURL fileURLWithPath:[base stringByAppendingFormat:@"-%@.cache",digest]] : nil;
			if (saved && renamex_np(temporary.fileSystemRepresentation, segment.fileSystemRepresentation, RENAME_EXCL) != 0) {
				reason = strerror(errno); saved = false;
			}
			if (saved) {
				impl->dirty = false;
				++impl->savedSegments;
				{
					std::lock_guard<std::mutex> lock(impl->recordsMutex);
					for(const auto& key:impl->unsavedRecords) {
						auto found=impl->records.find(key);
						if(found!=impl->records.end())found->second=segment.lastPathComponent.UTF8String;
					}
					impl->unsavedRecords.clear();
				}
				NOTICE_LOG(RENDERER, "Metal 4 pipeline cache segment saved: %s", segment.fileSystemRepresentation);
			} else {
				// The serializer may have flushed; stop recording rather than
				// pretend those entries were saved. Earlier segments stay valid.
				WARN_LOG(RENDERER, "Metal pipeline cache save stopped: %s", reason);
				impl->writable.store(false);
			}
		} @catch (NSException *exception) {
			impl->writable.store(false);
			WARN_LOG(RENDERER, "Metal pipeline cache save stopped: %s", exception.reason.UTF8String);
		}
	} catch (const std::exception& error) {
		impl->writable.store(false);
		WARN_LOG(RENDERER, "Metal pipeline cache save stopped: %s", error.what());
	}
	if (temporary) unlink(temporary.fileSystemRepresentation);
}

void MetalPipelineCache::prune() noexcept
{
	// Best effort across our versioned segments only. Never remove a live
	// writer's files or old Metal3/user caches. Identity lock files stay reusable.
	if (!impl->writable.load()) return;
	try {
		@try {
			size_t kept = 0;
			off_t bytes = 0;
			for (NSURL *file in cacheFiles(impl->url.URLByDeletingLastPathComponent)) {
				NSString *name = file.lastPathComponent;
				NSString *identity = archiveIdentity(name);
				bool temporary = false;
				if (!identity && name.length > 41
						&& [[name substringWithRange:NSMakeRange(name.length - 41, 5)] isEqualToString:@".tmp-"]
						&& [[NSUUID alloc] initWithUUIDString:[name substringFromIndex:name.length - 36]]) {
					identity = archiveIdentity([name substringToIndex:name.length - 41]);
					temporary = true;
				}
				if (!identity) continue;
				struct stat info{};
				if (lstat(file.fileSystemRepresentation, &info) != 0 || !S_ISREG(info.st_mode)) continue;
				bool remove = temporary || info.st_size <= 0 || info.st_size > MaxArchiveBytes
					|| kept >= MaxCacheFiles || bytes + info.st_size > MaxCacheBytes;
				if (!remove) {
					++kept; bytes += info.st_size; continue;
				}
				bool removed = false;
				if ([identity isEqualToString:impl->url.lastPathComponent]) {
					// writable implies we already own this identity's lock.
					removed = unlink(file.fileSystemRepresentation) == 0;
					if(removed) {
						std::lock_guard<std::mutex> lock(impl->recordsMutex);
						const std::string segment=name.UTF8String;
						for(auto i=impl->records.begin();i!=impl->records.end();) {
							if(i->second==segment)i=impl->records.erase(i);else ++i;
						}
					}
				} else {
					NSURL *lockURL = [impl->url.URLByDeletingLastPathComponent URLByAppendingPathComponent:[identity stringByAppendingString:@".lock"]];
					int lock = open(lockURL.fileSystemRepresentation, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
					if (lock >= 0) {
						if (flock(lock, LOCK_EX | LOCK_NB) == 0) removed = unlink(file.fileSystemRepresentation) == 0;
						close(lock);
					}
				}
				if (!removed && !temporary) {
					++kept;
					// Protected oversized files still consume the whole budget;
					// saturate so malformed file sizes cannot overflow the sum.
					bytes += std::min(std::max(off_t(0), info.st_size), MaxCacheBytes - bytes);
				}
			}
		} @catch (NSException *exception) {
			WARN_LOG(RENDERER, "Metal pipeline cache pruning skipped: %s", exception.reason.UTF8String);
		}
	} catch (const std::exception& error) {
		WARN_LOG(RENDERER, "Metal pipeline cache pruning skipped: %s", error.what());
	}
}

void MetalPipelineCache::finish() noexcept
{
	@autoreleasepool {
		{
			std::lock_guard<std::mutex> lock(impl->recordsMutex);
			impl->accepting=false;
		}
		dispatch_sync(impl->writerQueue, ^{
			@autoreleasepool {
				save();
				prune();
				impl->writable.store(false);
				impl->writer = nil;
				impl->serializer = nil;
				if (impl->lock >= 0) { close(impl->lock); impl->lock = -1; }
			}
		});
		if(impl->reported.exchange(true))return;
		if (impl->timing)
			NOTICE_LOG(RENDERER, "Metal pipeline cache: %llu archive hits, %llu archive misses",
				(unsigned long long)impl->hits.load(), (unsigned long long)impl->misses.load());
		else
			INFO_LOG(RENDERER, "Metal pipeline cache: %llu archive hits, %llu archive misses",
				(unsigned long long)impl->hits.load(), (unsigned long long)impl->misses.load());
		if (impl->timing) {
			NOTICE_LOG(RENDERER,"Metal pipeline cache work: %llu archive probes, %llu hinted hits, %llu queued, %llu writer calls, %llu duplicates suppressed, %llu descriptor copies",
				(unsigned long long)impl->probes.load(),(unsigned long long)impl->hintedHits.load(),(unsigned long long)impl->queued.load(),
				(unsigned long long)impl->recordings.load(),(unsigned long long)impl->duplicates.load(),(unsigned long long)impl->descriptorCopies.load());
			// CPU elapsed intervals can overlap on different queues; their sum
			// is not frame time or GPU time. Creation may hit Metal's system cache.
			logTiming("foreground render lookup", impl->lookupTimings[0]);
			logTiming("material lookup", impl->lookupTimings[1]);
			logTiming("compute lookup", impl->lookupTimings[2]);
			logTiming("foreground render creation", impl->creationTimings[0]);
			logTiming("material creation", impl->creationTimings[1]);
			logTiming("compute creation", impl->creationTimings[2]);
			logTiming("writer render addition", impl->recordTimings[0]);
			logTiming("writer compute addition", impl->recordTimings[1]);
			logTiming("archive serialization", impl->serializeTiming);
			logTiming("archive publication", impl->publicationTiming);
		}
	}
}
MetalPipelineCache::Statistics MetalPipelineCache::statistics() const
{
	std::lock_guard<std::mutex> lock(impl->recordsMutex);
	Statistics result{impl->hits.load(),impl->misses.load(),impl->probes.load(),impl->hintedHits.load(),impl->queued.load(),
		impl->recordings.load(),impl->duplicates.load(),impl->savedSegments.load(),impl->pending.load(),impl->writable.load(),impl->accepting};
	const auto snapshot=[](const PipelineTiming& t){return Statistics::Timing{t.calls.load(),t.nanoseconds.load(),t.maximum.load()};};
	for(size_t i=0;i<3;++i){result.lookup[i]=snapshot(impl->lookupTimings[i]);result.creation[i]=snapshot(impl->creationTimings[i]);}
	for(size_t i=0;i<2;++i)result.writer[i]=snapshot(impl->recordTimings[i]);
	result.serialization=snapshot(impl->serializeTiming);result.publication=snapshot(impl->publicationTiming);result.timing=impl->timing;
	result.descriptorCopies=impl->descriptorCopies.load();
	return result;
}
#endif // Apple Silicon Metal 4 slice
