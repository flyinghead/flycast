// Production Metal 4 cache with isolated filesystem/config/log services.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_pipeline_cache.h"
#include "rend/metal4/metal4_pipeline_identity.h"
#include "log/Log.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <future>
#include <chrono>
#include <stdexcept>
#include <vector>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

@interface FailingArchiveDescriptor : NSObject <NSCopying>
@property unsigned failure;
@end
@implementation FailingArchiveDescriptor
- (id)copyWithZone:(NSZone *)zone {
	if(self.failure==1)@throw [NSException exceptionWithName:@"ArchiveCopyTest" reason:@"Injected copy failure" userInfo:nil];
	if(self.failure==2)throw std::bad_alloc();
	return nil;
}
@end

@interface BlockingArchiveDescriptor : NSObject <NSCopying>
@property (strong) id<NSCopying> snapshot;
@property (strong) dispatch_semaphore_t entered,gate;
@property unsigned failure;
@end
@implementation BlockingArchiveDescriptor
- (id)copyWithZone:(NSZone *)zone {
	dispatch_semaphore_signal(self.entered);dispatch_semaphore_wait(self.gate,DISPATCH_TIME_FOREVER);
	if(self.failure==1)return nil;
	if(self.failure==2)@throw [NSException exceptionWithName:@"ArchiveBlockedCopy" reason:@"Injected blocked copy failure" userInfo:nil];
	if(self.failure==3)throw std::bad_alloc();
	return [self.snapshot copyWithZone:zone];
}
@end

static std::string cacheRoot;
namespace hostfs {std::string getShaderCachePath(const std::string& name){return cacheRoot+"/"+name;}}
namespace config {bool loadBool(const std::string& section,const std::string& key,bool fallback){return section=="log" && key=="MetalFrameTiming" ? true : fallback;}}
void GenericLog(LogTypes::LOG_LEVELS,LogTypes::LOG_TYPE,const char*,int,const char *format,...)
{
	va_list args;va_start(args,format);std::vfprintf(stderr,format,args);std::fputc('\n',stderr);va_end(args);
}
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void error(NSError *failure){if(failure)throw std::runtime_error(failure.localizedDescription.UTF8String);}
static constexpr const char Source[]=R"MSL(
#include <metal_stdlib>
using namespace metal;
constant uint Flags [[function_constant(0)]];
constant uint Tsp0 [[function_constant(1)]];
constant uint Tsp1 [[function_constant(2)]];
constant bool Sequence [[function_constant(3)]];
struct Out {float4 position [[position]];float4 color;};
vertex Out v(uint id [[vertex_id]]) {
    const float2 xy[]={float2(-1,-1),float2(3,-1),float2(-1,3)};
    return {float4(xy[id],0,1),float4(.25,.5,.75,1)};
}
fragment float4 f(Out in [[stage_in]]) {return in.color;}
fragment float4 f_special(Out in [[stage_in]]) {
    return float4(float(Flags&255)/255,float(Tsp0&255)/255,float(Tsp1&255)/255,Sequence?1:.5);
}
kernel void c0(device uint *out [[buffer(0)]]) {*out=0x12345678;}
kernel void c1(device uint *out [[buffer(0)]]) {*out=0x23456789;}
kernel void c2(device uint *out [[buffer(0)]]) {*out=0x3456789a;}
kernel void c3(device uint *out [[buffer(0)]]) {*out=0x456789ab;}
)MSL";

struct GPU {
	id<MTLDevice> device=MTLCreateSystemDefaultDevice();
	id<MTL4Compiler> foreground,material;
	id<MTLLibrary> library;
	MTLCompileOptions *options=[MTLCompileOptions new];
	FCMetal4Queue *queue;
	GPU() {
		require(device!=nil,"No Metal GPU");NSError *failure=nil;
		options.languageVersion=MTLLanguageVersion4_0;
		foreground=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&failure];error(failure);
		material=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&failure];error(failure);
		MTL4LibraryDescriptor *source=[MTL4LibraryDescriptor new];source.source=[NSString stringWithUTF8String:Source];source.options=options;
		library=[foreground newLibraryWithDescriptor:source error:&failure];error(failure);
		queue=[[FCMetal4Queue alloc] initWithDevice:device];
	}
	MTL4FunctionDescriptor *function(NSString *name,const std::array<uint32_t,4> *constants=nullptr) {
		auto base=[MTL4LibraryFunctionDescriptor new];base.library=library;base.name=name;
		if(!constants)return base;
		auto values=[MTLFunctionConstantValues new];
		for(NSUInteger i=0;i<3;++i)[values setConstantValue:&(*constants)[i] type:MTLDataTypeUInt atIndex:i];
		const bool sequence=(*constants)[3]!=0;[values setConstantValue:&sequence type:MTLDataTypeBool atIndex:3];
		auto specialized=[MTL4SpecializedFunctionDescriptor new];specialized.functionDescriptor=base;specialized.constantValues=values;return specialized;
	}
	MTL4ComputePipelineDescriptor *compute(NSString *name) {
		auto d=[MTL4ComputePipelineDescriptor new];d.computeFunctionDescriptor=function(name);return d;
	}
	MTL4RenderPipelineDescriptor *render(const std::array<uint32_t,4> *constants=nullptr) {
		auto d=[MTL4RenderPipelineDescriptor new];d.vertexFunctionDescriptor=function(@"v");d.fragmentFunctionDescriptor=function(constants?@"f_special":@"f",constants);
		d.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;d.rasterSampleCount=1;return d;
	}
	uint32_t dispatch(id<MTLComputePipelineState> pipeline) {
		auto output=[device newBufferWithLength:4 options:MTLResourceStorageModeShared];
		auto cb=[queue commandBuffer];auto e=[cb computeCommandEncoder];[e setComputePipelineState:pipeline];[e setBuffer:output offset:0 atIndex:0];
		[e dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[e endEncoding];[cb commit];[cb waitUntilCompleted];error(cb.error);
		return *static_cast<uint32_t *>(output.contents);
	}
	std::array<uint8_t,4> draw(id<MTLRenderPipelineState> pipeline) {
		auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];desc.usage=MTLTextureUsageRenderTarget;
		auto texture=[device newTextureWithDescriptor:desc];auto cb=[queue commandBuffer];auto p=[MTL4RenderPassDescriptor new];
		p.colorAttachments[0].texture=texture;p.colorAttachments[0].loadAction=MTLLoadActionClear;p.colorAttachments[0].storeAction=MTLStoreActionStore;
		auto e=[cb renderCommandEncoderWithDescriptor:p];[e setRenderPipelineState:pipeline];[e setViewport:MTLViewport{0,0,1,1,0,1}];
		[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[e endEncoding];
		auto data=[device newBufferWithLength:256 options:MTLResourceStorageModeShared];auto copy=[cb transferEncoder];
		[copy copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(1,1,1)
			toBuffer:data destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:256];[copy endEncoding];[cb commit];[cb waitUntilCompleted];error(cb.error);
		std::array<uint8_t,4> result;std::memcpy(result.data(),data.contents,4);return result;
	}
};
struct Session {
	std::shared_ptr<MetalPipelineCache> cache;
	explicit Session(GPU& gpu):cache(MetalPipelineCache::create(gpu.foreground,gpu.material,Source,sizeof(Source)-1,gpu.options)) {require(cache!=nullptr,"Archive disabled/unavailable");}
	~Session(){if(cache)cache->finish();}
};
static std::string computeKey(MTL4ComputePipelineDescriptor *d,NSString *name) {
	return metal_pipeline::computeIdentity(d,metal_pipeline::functionIdentity(name));
}
static std::string renderKey(MTL4RenderPipelineDescriptor *d,const std::array<uint32_t,4> *constants=nullptr) {
	return metal_pipeline::renderIdentity(d,metal_pipeline::functionIdentity(@"v"),metal_pipeline::functionIdentity(constants?@"f_special":@"f",constants));
}
static void archiveAdmission(GPU& gpu)
{
	const auto originalRoot=cacheRoot;
	struct Restore {std::string root;~Restore(){cacheRoot=root;}} restore{originalRoot};
	cacheRoot=originalRoot+"/admission";
	Session session(gpu);auto cache=session.cache;
	auto blocked=[BlockingArchiveDescriptor new];blocked.snapshot=gpu.compute(@"c2");
	blocked.entered=dispatch_semaphore_create(0);blocked.gate=dispatch_semaphore_create(0);
	auto key=computeKey((MTL4ComputePipelineDescriptor *)blocked.snapshot,@"c2");
	auto first=std::async(std::launch::async,[cache,blocked,key]{@autoreleasepool{cache->recordForTesting(blocked,true,key);}});
	struct Release {dispatch_semaphore_t gate;void open(){if(gate){dispatch_semaphore_signal(gate);gate=nil;}}~Release(){open();}} release{blocked.gate};
	require(dispatch_semaphore_wait(blocked.entered,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC))==0,"Blocked descriptor copy did not start");
	auto descriptor=gpu.compute(@"c3");const auto otherKey=computeKey(descriptor,@"c3");
	auto entered=dispatch_semaphore_create(0);
	auto foreground=std::async(std::launch::async,[cache,descriptor,otherKey,entered]{@autoreleasepool{
		dispatch_semaphore_signal(entered);cache->recordForTesting(descriptor,true,otherKey);
	}});
	const bool started=dispatch_semaphore_wait(entered,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC))==0;
	const bool independent=started && foreground.wait_for(std::chrono::milliseconds(200))==std::future_status::ready;
	release.open();first.get();foreground.get();
	require(independent,"Foreground archive admission waited for another descriptor copy");
	cache->finish();const auto stats=cache->statistics();
	require(stats.queued==2 && stats.recordings==2 && !stats.pending && stats.savedSegments==1,"Parallel descriptor publication lost an accepted pipeline");
	std::puts("PASS archive admission: blocked descriptor copy does not stall another request; both accepted pipelines serialized");
	for(unsigned failure=0;failure<4;++failure) {
		cacheRoot=originalRoot+"/finish-copy-"+std::to_string(failure);
		Session closing(gpu);auto target=closing.cache;
		auto delayed=[BlockingArchiveDescriptor new];delayed.snapshot=gpu.compute(@"c2");delayed.failure=failure;
		delayed.entered=dispatch_semaphore_create(0);delayed.gate=dispatch_semaphore_create(0);
		auto producer=std::async(std::launch::async,[target,delayed,key]{@autoreleasepool{target->recordForTesting(delayed,true,key);}});
		Release cancel{delayed.gate};
		const bool copying=dispatch_semaphore_wait(delayed.entered,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC))==0;
		auto finishA=std::async(std::launch::async,[target]{target->finish();});
		const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
		while(target->statistics().accepting && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
		const bool closed=!target->statistics().accepting;
		auto finishB=std::async(std::launch::async,[target]{target->finish();});
		const bool held=finishA.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout
			&& finishB.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout;
		target->recordForTesting(descriptor,true,otherKey); // Already closed: no new copy or queue entry.
		cancel.open();producer.get();finishA.get();finishB.get();
		const auto done=target->statistics();const unsigned expected=failure?0:1;
		require(copying && closed && held,"Finish failed to hold its barrier behind an accepted descriptor copy");
		require(!done.accepting && !done.pending && done.queued==expected && done.recordings==expected
			&& done.descriptorCopies==1 && done.savedSegments==expected,"Copy completion/cancellation crossed the finish barrier or leaked admission");
	}
	std::puts("PASS archive finish barrier: two concurrent finishers wait for accepted copies; valid/nil/Objective-C/C++ copy outcomes, late admission rejected and reservations drained");
}
static void backgroundCompute(GPU& gpu)
{
	const auto originalRoot=cacheRoot;
	struct Restore{std::string root;~Restore(){cacheRoot=root;}} restore{originalRoot};
	cacheRoot=originalRoot+"/background-compute";
	{
		Session session(gpu);const auto cache=session.cache;
		auto descriptor=gpu.compute(@"c0");const auto key=computeKey(descriptor,@"c0");
		auto background=std::async(std::launch::async,[cache,descriptor,key]() -> id<MTLComputePipelineState> {
			@autoreleasepool{ id<MTLComputePipelineState> pipeline=nil;
				for(unsigned i=0;i<16;++i){NSError *e=nil;pipeline=cache->makeComputePipeline(descriptor,&e,key,true);error(e);require(pipeline!=nil,"Background compute unavailable");}
				return pipeline;
			}
		});
		auto foreground=gpu.compute(@"c1");const auto foregroundKey=computeKey(foreground,@"c1");
		id<MTLComputePipelineState> pipeline=nil;
		for(unsigned i=0;i<16;++i){NSError *e=nil;pipeline=cache->makeComputePipeline(foreground,&e,foregroundKey);error(e);require(pipeline!=nil,"Foreground compute unavailable");}
		require(gpu.dispatch(background.get())==0x12345678 && gpu.dispatch(pipeline)==0x23456789,"Concurrent compute compilers changed GPU output");
		cache->finish();const auto stats=cache->statistics();
		require(stats.creation[2].calls==16 && stats.creation[3].calls==16 && stats.recordings==2 && stats.duplicates==30,
			"Background compute mixed foreground timing or duplicated writer work");
	}
	{
		Session session(gpu);NSError *failure=nil;
		for(bool background:{false,true}) {
			auto name=background?@"c1":@"c0";auto d=gpu.compute(name);
			auto pipeline=session.cache->makeComputePipeline(d,&failure,computeKey(d,name),background);error(failure);
			require(gpu.dispatch(pipeline)==(background?0x23456789u:0x12345678u),"Warm compute archive changed output");
		}
		session.cache->finish();const auto stats=session.cache->statistics();
		require(stats.hits==2 && stats.misses==0 && stats.queued==0 && stats.lookup[2].calls==1 && stats.lookup[3].calls==1,
			"Foreground/background compute did not share persistent identities with separate reader timing");
	}
	std::puts("PASS background compute archive: concurrent separate compilers/readers, 32 requests / 2 writer entries, exact GPU results, warm cross-category hits and separate foreground/background timings");
}
static NSArray<NSString *> *segments() {
	NSMutableArray<NSString *> *files=[NSMutableArray new];
	for(NSString *name in [NSFileManager.defaultManager contentsOfDirectoryAtPath:[NSString stringWithUTF8String:cacheRoot.c_str()] error:nil])
		if([name hasSuffix:@".cache"])[files addObject:name];return files;
}
static size_t verifyIdentities(GPU& gpu)
{
	size_t checks=0;auto d=gpu.render();const auto original=renderKey(d);require(!original.empty(),"Render identity unavailable");
	require(original==renderKey([d copy]),"Copied descriptor identity changed");++checks;
	const auto changed=[&](MTL4RenderPipelineDescriptor *copy) {require(original!=renderKey(copy),"Render state missing from identity");require(original==renderKey(d),"Descriptor copy mutated the baseline");++checks;};
	for(unsigned field=0;field<12;++field) {
		MTL4RenderPipelineDescriptor *c=[d copy];switch(field) {
		case 0:c.rasterSampleCount=2;break;case 1:c.alphaToCoverageState=MTL4AlphaToCoverageStateEnabled;break;
		case 2:c.alphaToOneState=MTL4AlphaToOneStateEnabled;break;case 3:c.rasterizationEnabled=NO;break;
		case 4:c.maxVertexAmplificationCount=2;break;case 5:c.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;break;
		case 6:c.supportVertexBinaryLinking=YES;break;case 7:c.supportFragmentBinaryLinking=YES;break;
		case 8:c.supportIndirectCommandBuffers=MTL4IndirectCommandBufferSupportStateEnabled;break;
		case 9:c.label=@"different label";break;case 10:c.options=[MTL4PipelineOptions new];c.options.shaderReflection=MTL4ShaderReflectionBindingInfo;break;
		case 11:c.colorAttachmentMappingState=MTL4LogicalToPhysicalColorAttachmentMappingStateInherited;break;
		}changed(c);
	}
	{
		MTL4RenderPipelineDescriptor *base=[d copy];base.options=[MTL4PipelineOptions new];
		const auto key=renderKey(base);
		for(unsigned field=0;field<2;++field) {
			MTL4RenderPipelineDescriptor *copy=[base copy];
			// Pipeline descriptors retain their options object when copied.
			copy.options=[base.options copy];
			if(field==0)copy.options.shaderValidation=MTLShaderValidationEnabled;
			else copy.options.shaderReflection=MTL4ShaderReflectionBufferTypeInfo;
			require(key!=renderKey(copy),"Pipeline option value missing from identity");
			require(key==renderKey(base),"Copied pipeline options mutated baseline");++checks;
		}
	}
	for(NSUInteger attachment=0;attachment<8;++attachment)for(unsigned field=0;field<9;++field) {
		MTL4RenderPipelineDescriptor *c=[d copy];auto a=c.colorAttachments[attachment];
		switch(field) {
		case 0:a.pixelFormat=attachment==0?MTLPixelFormatBGRA8Unorm:MTLPixelFormatRGBA8Unorm;break;
		case 1:a.blendingState=MTL4BlendStateEnabled;break;case 2:a.sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;break;
		case 3:a.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;break;case 4:a.rgbBlendOperation=MTLBlendOperationSubtract;break;
		case 5:a.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;break;case 6:a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;break;
		case 7:a.alphaBlendOperation=MTLBlendOperationSubtract;break;case 8:a.writeMask=a.writeMask==MTLColorWriteMaskNone?MTLColorWriteMaskAll:MTLColorWriteMaskNone;break;
		}changed(c);
	}
	d.vertexDescriptor=[MTLVertexDescriptor new];const auto layoutKey=renderKey(d);
	for(NSUInteger index=0;index<31;++index)for(unsigned field=0;field<6;++field) {
		MTL4RenderPipelineDescriptor *c=[d copy];auto a=c.vertexDescriptor.attributes[index];auto b=c.vertexDescriptor.layouts[index];
		switch(field) {case 0:a.format=MTLVertexFormatFloat3;break;case 1:a.offset=4;break;case 2:a.bufferIndex=1;break;
		case 3:b.stride=16;break;case 4:b.stepFunction=MTLVertexStepFunctionPerInstance;break;case 5:b.stepRate=2;break;}
		require(layoutKey!=renderKey(c),"Vertex layout state missing from identity");++checks;
		require(layoutKey==renderKey(d),"Vertex descriptor copy mutated baseline");
	}
	std::array<uint32_t,4> values{1,2,3,0};const auto specialized=renderKey(gpu.render(&values),&values);
	for(size_t i=0;i<4;++i) {auto c=values;++c[i];require(specialized!=renderKey(gpu.render(&c),&c),"Function constants missing from identity");++checks;}
	require(specialized==renderKey(gpu.render(&values),&values),"Equivalent constants produced unstable identity");++checks;
	auto c=gpu.compute(@"c0");const auto base=computeKey(c,@"c0");require(!base.empty(),"Compute identity unavailable");
	for(unsigned field=0;field<6;++field) {
		MTL4ComputePipelineDescriptor *copy=[c copy];switch(field) {
		case 0:copy.threadGroupSizeIsMultipleOfThreadExecutionWidth=YES;break;case 1:copy.maxTotalThreadsPerThreadgroup=32;break;
		case 2:copy.requiredThreadsPerThreadgroup=MTLSizeMake(2,1,1);break;case 3:copy.supportBinaryLinking=YES;break;
		case 4:copy.supportIndirectCommandBuffers=MTL4IndirectCommandBufferSupportStateEnabled;break;case 5:copy.label=@"compute label";break;}
		require(base!=computeKey(copy,@"c0"),"Compute state missing from identity");++checks;
	}
	require(base!=computeKey(gpu.compute(@"c1"),@"c1"),"Function name missing from identity");++checks;
	auto linking=[MTL4StaticLinkingDescriptor new];linking.functionDescriptors=@[gpu.function(@"v")];d.vertexStaticLinkingDescriptor=linking;
	require(renderKey(d).empty(),"Unsupported linking identity was accepted");++checks;
	require(metal_pipeline::renderIdentity(d,{},{}).empty(),"Missing function identity accepted");++checks;
	require(metal_pipeline::optionalIdentity([]()->std::string{throw std::bad_alloc();}).empty(),"Optional identity failure escaped");++checks;
	return checks;
}
static int reader(const char *root)
{
	cacheRoot=root;GPU gpu;Session session(gpu);require(!session.cache->statistics().writable,"Concurrent process obtained writer lock");
	NSError *failure=nil;auto d=gpu.compute(@"c0");auto p=session.cache->makeComputePipeline(d,&failure,computeKey(d,@"c0"));error(failure);
	require(gpu.dispatch(p)==0x12345678,"Read-only archive compute differs");
	d=gpu.compute(@"c3");p=session.cache->makeComputePipeline(d,&failure,computeKey(d,@"c3"));error(failure);require(gpu.dispatch(p)==0x456789ab,"Read-only fallback differs");
	session.cache->finish();require(session.cache->statistics().queued==0,"Read-only process queued writes");std::puts("PASS concurrent read-only process");return 0;
}
static int abortWriter(const char *root)
{
	cacheRoot=root;GPU gpu;Session session(gpu);require(session.cache->statistics().writable,"Abort test failed to obtain writer lock");
	NSError *failure=nil;auto d=gpu.compute(@"c3");auto p=session.cache->makeComputePipeline(d,&failure,computeKey(d,@"c3"));error(failure);
	require(p!=nil && session.cache->statistics().queued==1,"Abort test did not admit a new pipeline");
	std::fputs("PASS admitted pipeline followed by abrupt process exit without finish\n",stderr);
	_exit(0); // Deliberately bypass RAII/finish and any delayed serialization.
}
int main(int argc,char **argv)
{
	@autoreleasepool {
	NSString *temporary=nil;
	try {
		if(argc==3 && std::strcmp(argv[1],"--reader")==0)return reader(argv[2]);
		if(argc==3 && std::strcmp(argv[1],"--abort")==0)return abortWriter(argv[2]);
		char path[]="/private/tmp/flycast-metal-cache-tests-XXXXXX";require(mkdtemp(path)!=nullptr,"Cannot create isolated test cache");
		cacheRoot=path;temporary=[NSString stringWithUTF8String:path];
		GPU gpu;
		if(argc==2 && std::strcmp(argv[1],"--admission-only")==0){archiveAdmission(gpu);[NSFileManager.defaultManager removeItemAtPath:temporary error:nil];temporary=nil;return 0;}
		backgroundCompute(gpu);archiveAdmission(gpu);const auto identityChecks=verifyIdentities(gpu);NSError *failure=nil;
		std::vector<NSString *> files;
		for(unsigned i=0;i<2;++i) {
			Session session(gpu);auto name=i==0?@"c0":@"c1";auto d=gpu.compute(name);
			auto p=session.cache->makeComputePipeline(d,&failure,computeKey(d,name));error(failure);require(gpu.dispatch(p)==(i?0x23456789u:0x12345678u),"Cold compute output differs");
			session.cache->finish();auto stats=session.cache->statistics();require(stats.recordings==1 && stats.savedSegments==1,"Cold cache failed to capture/serialize pipeline");
			for(NSString *name in segments())if(std::find_if(files.begin(),files.end(),[&](NSString *old){return [old isEqualToString:name];})==files.end())files.push_back(name);
			require(files.size()==i+1,"Unexpected segment count");
			[NSFileManager.defaultManager setAttributes:@{NSFileModificationDate:[NSDate dateWithTimeIntervalSince1970:100+100*i]}
				ofItemAtPath:[temporary stringByAppendingPathComponent:files.back()] error:nil];
		}
		{
			Session session(gpu);auto d=gpu.compute(@"c0");auto key=computeKey(d,@"c0");
			auto p=session.cache->makeComputePipeline(d,&failure,key);error(failure);require(gpu.dispatch(p)==0x12345678,"Warm archive output differs");
			const auto first=session.cache->statistics();require(first.probes==2 && first.hits==1,"Warm fixture did not search the second segment");
			for(unsigned i=0;i<15;++i){p=session.cache->makeComputePipeline([d copy],&failure,key);error(failure);require(p!=nil,"Hinted pipeline unavailable");}
			const auto hints=session.cache->statistics();require(hints.probes==17 && hints.hintedHits==15,"Positive segment hint did not eliminate repeated searches");
			pid_t child;char *args[]={argv[0],const_cast<char *>("--reader"),const_cast<char *>(cacheRoot.c_str()),nullptr};
			extern char **environ;require(posix_spawn(&child,argv[0],nullptr,nullptr,args,environ)==0,"Cannot spawn read-only test process");
			int status=0;require(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"Read-only process failed");
			const std::array<uint32_t,4> constants{31,97,173,1};auto render=gpu.render(&constants);const auto renderID=renderKey(render,&constants);
			MTL4RenderPipelineDescriptor *background=[render copy];auto cache=session.cache;
			auto material=std::async(std::launch::async,[cache,background,renderID] {
				@autoreleasepool {for(unsigned i=0;i<16;++i){NSError *e=nil;auto p=cache->makeRenderPipeline(background,&e,true,renderID);error(e);require(p!=nil,"Material pipeline unavailable");}}
			});
			id<MTLRenderPipelineState> rendered=nil;
			for(unsigned i=0;i<16;++i){rendered=cache->makeRenderPipeline(render,&failure,false,renderID);error(failure);require(rendered!=nil,"Foreground render unavailable");}
			material.get();require(gpu.draw(rendered)==std::array<uint8_t,4>{31,97,173,255},"Material constants/render cache output differs");
			d=gpu.compute(@"c2");key=computeKey(d,@"c2");
			for(unsigned i=0;i<16;++i){p=cache->makeComputePipeline(d,&failure,key);error(failure);require(p!=nil,"Foreground compute unavailable");}
			require(gpu.dispatch(p)==0x3456789a,"New compute material transition differs");
			auto finishA=std::async(std::launch::async,[cache]{cache->finish();});auto finishB=std::async(std::launch::async,[cache]{cache->finish();});finishA.get();finishB.get();
			const auto done=cache->statistics();require(done.queued==2 && done.recordings==2 && done.duplicates==46 && done.pending==0 && !done.accepting,"Archive deduplication/admission/drain counters differ");
			require(done.descriptorCopies==2,"Duplicate writer jobs copied descriptors before rejection");
			d=gpu.compute(@"c3");p=cache->makeComputePipeline(d,&failure,computeKey(d,@"c3"));error(failure);require(gpu.dispatch(p)==0x456789ab,"Finished cache fallback differs");
			require(cache->statistics().queued==done.queued,"Finished cache accepted a late writer job");
			std::printf("PASS pipeline cache work: probes 32 -> %llu, 48 requests/2 identities -> %llu writer calls, suppressed=%llu\n",
				(unsigned long long)hints.probes,(unsigned long long)done.recordings,(unsigned long long)done.duplicates);
			std::printf("PASS archive descriptor copies: 48 requests/2 identities -> %llu copies, no copies for duplicate or finished jobs\n",(unsigned long long)cache->statistics().descriptorCopies);
			require(cache->statistics().descriptorCopies==done.descriptorCopies,"Finished cache copied a late descriptor");
		}
		{
			Session session(gpu);const std::array<uint32_t,4> constants{31,97,173,1};
			auto d=gpu.render(&constants);const auto key=renderKey(d,&constants);
			for(bool material:{false,true}) {
				auto p=session.cache->makeRenderPipeline([d copy],&failure,material,key);error(failure);
				require(gpu.draw(p)==std::array<uint8_t,4>{31,97,173,255},"Restored material archive changed specialized output");
			}
			auto compute=gpu.compute(@"c2");auto p=session.cache->makeComputePipeline(compute,&failure,computeKey(compute,@"c2"));error(failure);
			require(gpu.dispatch(p)==0x3456789a,"Restored compute archive output differs");session.cache->finish();
			const auto restored=session.cache->statistics();
			require(restored.hits==3 && restored.misses==0 && restored.queued==0,"Warm render/material/compute archive was not reused");
			std::puts("PASS restored specialized render/material and compute: 3 archive hits, 0 misses/writer jobs");
		}
		{
			const NSUInteger previous=segments().count;
			pid_t child;char *args[]={argv[0],const_cast<char *>("--abort"),const_cast<char *>(cacheRoot.c_str()),nullptr};
			extern char **environ;require(posix_spawn(&child,argv[0],nullptr,nullptr,args,environ)==0,"Cannot spawn abrupt-exit cache test");
			int status=0;require(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"Abrupt-exit process failed");
			require(segments().count==previous,"Aborted process changed published segments");
			NSString *orphan=[NSString stringWithFormat:@"%@.tmp-%@",segments().firstObject,NSUUID.UUID.UUIDString];
			NSString *orphanPath=[temporary stringByAppendingPathComponent:orphan];
			[[NSData dataWithBytes:"partial" length:7] writeToFile:orphanPath atomically:NO];
			Session session(gpu);auto d=gpu.compute(@"c0");auto p=session.cache->makeComputePipeline(d,&failure,computeKey(d,@"c0"));error(failure);
			require(gpu.dispatch(p)==0x12345678,"Abrupt-exit recovery lost an earlier valid pipeline");session.cache->finish();
			require(![NSFileManager.defaultManager fileExistsAtPath:orphanPath],"Orphaned temporary archive was not removed");
		}
		// Corruption is optional-cache failure, never a substitute PSO.
		NSArray<NSString *> *published=segments();NSUInteger corruption=0;
		NSString *unsignedName=[[published.firstObject substringToIndex:published.firstObject.length-71] stringByAppendingString:@".cache"];
		[[NSData dataWithBytes:"unsigned cache" length:14] writeToFile:[temporary stringByAppendingPathComponent:unsignedName] atomically:NO];
		for(NSString *name in segments()) {
			if([name isEqualToString:unsignedName])continue;
			NSString *file=[temporary stringByAppendingPathComponent:name];
			NSMutableData *data=[[NSData dataWithContentsOfFile:file] mutableCopy];require(data.length>0,"Published archive is empty");
			if(corruption%3==0) {const char bad[]="not a Metal archive";data=[NSMutableData dataWithBytes:bad length:sizeof(bad)];}
			else if(corruption%3==1)static_cast<uint8_t *>(data.mutableBytes)[data.length/2]^=0x80;
			else std::memset(data.mutableBytes,0,data.length);
			[data writeToFile:file atomically:NO];++corruption;
		}
		{
			Session session(gpu);auto d=gpu.compute(@"c0");auto p=session.cache->makeComputePipeline(d,&failure,computeKey(d,@"c0"));error(failure);
			require(gpu.dispatch(p)==0x12345678,"Corrupt archive changed fallback output");session.cache->finish();require(session.cache->statistics().misses==1,"Corrupt archive did not fall back to compiler");
			require(segments().count==1,"Invalid/unsigned segments were not retired");
		}
		{
			Session session(gpu);auto fake=[FailingArchiveDescriptor new];
			auto descriptor=gpu.compute(@"c3");const auto key=computeKey(descriptor,@"c3");
			for(unsigned failure=0;failure<3;++failure) {
				fake.failure=failure;session.cache->recordForTesting(fake,true,key);
				const auto stats=session.cache->statistics();
				require(stats.pending==0 && stats.queued==0 && stats.descriptorCopies==failure+1,"Copy failure leaked pending work or identity reservation");
			}
			auto pipeline=session.cache->makeComputePipeline(descriptor,&failure,key);error(failure);
			require(gpu.dispatch(pipeline)==0x456789ab,"Retry after descriptor-copy failure changed output");
			session.cache->finish();const auto done=session.cache->statistics();
			require(done.pending==0 && done.queued==1 && done.recordings==1 && done.descriptorCopies==4,"Copy failures prevented successful retry or final drain");
			std::puts("PASS archive copy failures: nil, Objective-C exception, C++ allocation exception, then exact GPU retry and drain");
		}
		[NSFileManager.defaultManager removeItemAtPath:temporary error:nil];temporary=nil;
		std::printf("PASS pipeline identity: %zu state/constant/copy/fallback checks; cold/warm, concurrent writer lock, dedup, hinting, finish/late admission, abrupt exit/orphan recovery, truncated/same-size corruption\n",identityChecks);
		return 0;
	} catch(const std::exception& e) {
		std::fprintf(stderr,"FAIL: %s\n",e.what());
		if(temporary)[NSFileManager.defaultManager removeItemAtPath:temporary error:nil];return 1;
	}
	}
}
