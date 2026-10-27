// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the actual Context enqueue path while its serial compiler is held.
@interface FCMetalMaterialLayoutSpy : NSObject <NSCopying> {
@public
	std::atomic<unsigned> copies;
}
@end
@implementation FCMetalMaterialLayoutSpy
- (instancetype)init
{
	if((self=[super init]))copies.store(0);
	return self;
}
- (id)copyWithZone:(NSZone *)zone
{
	++copies;
	return [MTLVertexDescriptor vertexDescriptor];
}
@end

// Fault injection only: exercise the real Context's asynchronous boundary,
// without relying on a driver to fail or throw nondeterministically.
@interface FCMetalMaterialFailureCompiler : NSObject {
@public
	unsigned mode, calls;
	std::shared_ptr<std::atomic<bool>> cancel;
}
@end
@implementation FCMetalMaterialFailureCompiler
- (id<MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTL4RenderPipelineDescriptor *)descriptor
	compilerTaskOptions:(MTL4CompilerTaskOptions *)options error:(NSError **)error
{
	++calls;
	if(cancel)cancel->store(false);
	if(mode==1)throw std::runtime_error("Injected material compiler C++ failure");
	if(mode==2)@throw [NSException exceptionWithName:@"FCMaterialFixture" reason:@"Injected material compiler Objective-C failure" userInfo:nil];
	if(error)*error=[NSError errorWithDomain:@"FCMaterialFixture" code:3
		userInfo:@{NSLocalizedDescriptionKey:@"Injected material compiler NSError failure"}];
	return nil;
}
@end

void MetalContext::withBlockedMaterialCompilerForTesting(const std::function<void()>& producer)
{
	if(impl->view)throw RendererException("Material queue fixture requires an offscreen context");
	drainMaterialCompilerForTesting();
	auto entered=dispatch_semaphore_create(0),gate=dispatch_semaphore_create(0);
	dispatch_async(impl->shaderQueue, ^{
		dispatch_semaphore_signal(entered);dispatch_semaphore_wait(gate,DISPATCH_TIME_FOREVER);
	});
	struct Release {dispatch_semaphore_t gate;~Release(){dispatch_semaphore_signal(gate);}} release{gate};
	if(dispatch_semaphore_wait(entered,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC)))
		throw RendererException("Material compiler gate did not start");
	producer();
}

void MetalContext::validateMaterialQueueForTesting()
{
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	unsigned total=0;
	for(unsigned cycle=0;cycle<4;++cycle) {
		auto wanted=std::make_shared<std::atomic<bool>>(true);
		auto completed=std::make_shared<std::atomic<unsigned>>(0);
		auto layout=[FCMetalMaterialLayoutSpy new];
		withBlockedMaterialCompilerForTesting([&]{
			for(unsigned i=0;i<256;++i)
				makePipelineAsync(@"quad_vertex",@"quad_fragment",(MTLVertexDescriptor *)layout,
					MTLPixelFormatBGRA8Unorm,false,nil,{i,0,0,false},
					[completed](id<MTLRenderPipelineState>,NSError *){++*completed;},
					[wanted]{return wanted->load();});
			wanted->store(false);
			NOTICE_LOG(RENDERER,"Metal material queue fixture burst %u: %u descriptor copies",cycle,layout->copies.load());
			check(layout->copies.load()==16,
				"Material compiler backlog does not retain exactly sixteen available job slots");
			total+=layout->copies;
		});
		drainMaterialCompilerForTesting();
		check(!completed->load(),"Canceled material work called a retired producer");
	}
	struct Result {id<MTLRenderPipelineState> pipeline=nil;std::atomic<bool> done{false};};
	auto result=std::make_shared<Result>();
	makePipelineAsync(@"quad_vertex",@"quad_fragment",nil,MTLPixelFormatBGRA8Unorm,false,nil,{0,0,0,false},
		[result](id<MTLRenderPipelineState> state,NSError *){result->pipeline=state;result->done.store(true);},[]{return true;});
	drainMaterialCompilerForTesting();
	check(result->done.load() && result->pipeline,"Released material capacity did not admit a real pipeline");
	auto input=makeTexture(8,6,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead,false,true);
	std::vector<u8> source(8*6*4),reference(source.size());
	for(size_t i=0;i<source.size();i+=4) {
		source[i]=u8(i*3);source[i+1]=u8(i+19);source[i+2]=u8(i*7);source[i+3]=u8(i);
		reference[i]=source[i+2];reference[i+1]=source[i+1];reference[i+2]=source[i];reference[i+3]=255;
	}
	[input replaceRegion:MTLRegionMake2D(0,0,8,6) mipmapLevel:0 withBytes:source.data() bytesPerRow:32];
	for(id<MTLRenderPipelineState> pipeline : {quadPipeline(true),result->pipeline}) {
		auto output=makeTexture(8,6,MTLPixelFormatBGRA8Unorm);
		auto command=[queue() commandBuffer];auto pass=[MTL4RenderPassDescriptor new];
		pass.colorAttachments[0].texture=output;pass.colorAttachments[0].loadAction=MTLLoadActionDontCare;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
		auto encoder=[command renderCommandEncoderWithDescriptor:pass];
		struct alignas(16) Uniform {float color[4];u32 rotate,ignoreAlpha;} uniform{{1,1,1,1},0,1};
		[encoder setRenderPipelineState:pipeline];[encoder setCullMode:MTLCullModeNone];
		[encoder setViewport:MTLViewport{0,0,8,6,0,1}];[encoder setSharedBytes:&uniform length:sizeof(uniform) atIndex:0];
		[encoder setFragmentTexture:input atIndex:0];[encoder setFragmentSamplerState:impl->nearest atIndex:0];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];[encoder endEncoding];submitCommandBuffer(command);
		std::vector<u8> actual;readTexture(output,actual);check(actual==reference,"Generic/material pipeline differs from complete CPU RGBA oracle");
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual material queue, 1024 requests while compiler held, %u descriptor copies across four bounded/canceled bursts, real pipeline after capacity release, two full images match CPU oracle",total);
}

void MetalContext::validateMaterialFailuresForTesting()
{
	drainMaterialCompilerForTesting();
	auto callbacks=std::make_shared<std::atomic<unsigned>>(0);
	auto valid=std::make_shared<std::atomic<bool>>(false);
	const bool admitted=makePipelineAsync(@"quad_vertex",@"quad_fragment",nil,
		MTLPixelFormatBGRA8Unorm,false,nil,{0,0,0,false},
		[callbacks,valid](id<MTLRenderPipelineState> state,NSError *error) {
			valid->store(state!=nil && error==nil);
			if(callbacks->fetch_add(1)==0) {
				throw std::runtime_error("Injected material result handoff failure");
			}
		},[]{return true;});
	drainMaterialCompilerForTesting();
	if(!admitted || callbacks->load()!=1 || !valid->load())
		throw RendererException("Material callback invoked twice after result handoff failure");
	callbacks->store(0);valid->store(false);
	if(!makePipelineAsync(@"quad_vertex",@"quad_fragment",nil,MTLPixelFormatBGRA8Unorm,false,nil,{0,0,0,false},
		[callbacks,valid](id<MTLRenderPipelineState> state,NSError *error) {
			valid->store(state!=nil && error==nil);++*callbacks;
			@throw [NSException exceptionWithName:@"FCMaterialFixture" reason:@"Injected result Objective-C failure" userInfo:nil];
		},[]{return true;}))throw RendererException("Material result failure fixture was rejected");
	drainMaterialCompilerForTesting();
	if(callbacks->load()!=1 || !valid->load())throw RendererException("Objective-C material callback failed its single real result");
	{
		struct Restore {
			Impl *owner;
			id<MTL4Compiler> compiler;
			std::shared_ptr<MetalPipelineCache> cache;
			~Restore(){owner->materialCompiler=compiler;owner->pipelineCache=cache;}
		} restore{impl.get(),impl->materialCompiler,impl->pipelineCache};
		impl->pipelineCache.reset();
		for(unsigned mode : {1u,2u,3u,0u}) {
			auto fixture=[FCMetalMaterialFailureCompiler new];fixture->mode=mode;
			auto wanted=std::make_shared<std::atomic<bool>>(true);
			if(mode==0)fixture->cancel=wanted;
			impl->materialCompiler=(id<MTL4Compiler>)fixture;
			callbacks->store(0);valid->store(false);
			if(!makePipelineAsync(@"quad_vertex",@"quad_fragment",nil,MTLPixelFormatBGRA8Unorm,false,nil,{0,0,0,false},
				[callbacks,valid](id<MTLRenderPipelineState> state,NSError *error) {
					valid->store(!state && error && [error.localizedDescription hasPrefix:@"Injected material compiler"]);
					++*callbacks;
				},[wanted]{return wanted->load();}))throw RendererException("Material compiler failure fixture was rejected");
			drainMaterialCompilerForTesting();
			if(fixture->calls!=1 || callbacks->load()!=(mode ? 1u : 0u) || (mode && !valid->load()))
				throw RendererException("Material compiler failure/cancellation did not preserve its single result boundary");
		}
	}
	// Prove all admission tickets retired on the failed/canceled paths in this
	// same Context, then compare real generic/material output to the CPU oracle.
	validateMaterialQueueForTesting();
	NOTICE_LOG(RENDERER,"Metal validation PASS: real material C++/Objective-C result handoff failures call their producer once; injected compiler C++/Objective-C/NSError failures each report once; cancellation during compilation reports no retired result");
}

void MetalContext::validateMaterialFunctionsForTesting()
{
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	check(!impl->view,"Material function fixture requires an offscreen context");drainMaterialCompilerForTesting();
	struct Result {id<MTLRenderPipelineState> pipeline=nil;size_t retained=0;bool retired=false,hot=false;};
	auto result=std::make_shared<Result>();auto failure=std::make_shared<std::exception_ptr>();
	dispatch_sync(impl->shaderQueue, ^{
		try { @try {
			auto cache=std::make_shared<MetalMaterialFunctions>();
			const auto count=[&]{size_t n=0;for(const auto& group:cache->functions)n+=group.second.size();return n;};
			for(u32 i=0;i<4096;++i) {
				@autoreleasepool {
					cache->get(impl->library,i&1?@"quad_vertex":@"quad_fragment",{i/2,i*17,i*31,bool(i&2)});
				}
				check(count()<=1024,"Completed material function descriptors remain unbounded across unique variants");
			}
			result->retained=count();check(count()==1024 && cache->recent.size()==count(),"Function cache failed its exact global retention bound");
			cache=std::make_shared<MetalMaterialFunctions>();
			const MaterialConstants hotKey{0,7,11,false},coldKey{1,7,11,false};
			__weak MTL4FunctionDescriptor *hot=nil;
			__weak MTL4FunctionDescriptor *cold=nil;
			@autoreleasepool {
				hot=cache->get(impl->library,@"quad_fragment",hotKey);cold=cache->get(impl->library,@"quad_fragment",coldKey);
				for(u32 i=2;i<1024;++i)cache->get(impl->library,@"quad_fragment",{i,7,11,false});
				for(u32 i=1024;i<4096;++i){
					cache->get(impl->library,@"quad_fragment",hotKey);
					cache->get(impl->library,@"quad_fragment",{i,7,11,false});
				}
			}
			result->retired=cold==nil;result->hot=hot!=nil && cache->get(impl->library,@"quad_fragment",hotKey)==hot;
			check(result->retired && result->hot && count()==1024,"Function LRU retained cold descriptors or replaced a repeatedly used hot identity");
			// The SDK pipeline properties copy their function descriptors. Hold
			// those inputs across cache eviction, then compile from the copies.
			auto descriptor=[MTL4RenderPipelineDescriptor new];descriptor.rasterSampleCount=1;
			descriptor.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
			descriptor.vertexFunctionDescriptor=cache->get(impl->library,@"quad_vertex",{});
			descriptor.fragmentFunctionDescriptor=cache->get(impl->library,@"quad_fragment",{});
			for(u32 i=4096;i<8192;++i){@autoreleasepool{cache->get(impl->library,@"quad_fragment",{i,7,11,false});}}
			const auto has=[&](const std::string& name){auto g=cache->functions.find(name);return g!=cache->functions.end() && g->second.count(MaterialConstants{})!=0;};
			check(!has("quad_vertex") && !has("quad_fragment") && count()==1024,"Held pipeline inputs were not evicted before the lifetime check");
			cache.reset();NSError *error=nil;
			result->pipeline=[impl->materialCompiler newRenderPipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:&error];
			check(result->pipeline && !error,"Eviction or cache destruction invalidated copied pipeline inputs");
		} @catch(NSException *error) {
			*failure=std::make_exception_ptr(RendererException(error.reason.UTF8String ?: "Material function fixture exception"));
		}}catch(...){*failure=std::current_exception();}
	});
	if(*failure)std::rethrow_exception(*failure);
	auto input=makeTexture(8,6,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead,false,true);
	std::vector<u8> source(8*6*4),oraclePixels(source.size());
	for(size_t i=0;i<source.size();i+=4){source[i]=u8(i*3);source[i+1]=u8(i+19);source[i+2]=u8(i*7);source[i+3]=u8(i);oraclePixels[i]=source[i+2];oraclePixels[i+1]=source[i+1];oraclePixels[i+2]=source[i];oraclePixels[i+3]=255;}
	[input replaceRegion:MTLRegionMake2D(0,0,8,6) mipmapLevel:0 withBytes:source.data() bytesPerRow:32];
	for(unsigned repeat=0;repeat<2;++repeat){
		auto output=makeTexture(8,6,MTLPixelFormatBGRA8Unorm);auto command=[queue() commandBuffer];auto pass=[MTL4RenderPassDescriptor new];
		pass.colorAttachments[0].texture=output;pass.colorAttachments[0].loadAction=MTLLoadActionDontCare;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
		auto encoder=[command renderCommandEncoderWithDescriptor:pass];
		struct alignas(16) Uniform {float color[4];u32 rotate,ignoreAlpha;} uniform{{1,1,1,1},0,1};
		[encoder setRenderPipelineState:result->pipeline];[encoder setCullMode:MTLCullModeNone];[encoder setViewport:MTLViewport{0,0,8,6,0,1}];
		[encoder setSharedBytes:&uniform length:sizeof(uniform) atIndex:0];[encoder setFragmentTexture:input atIndex:0];[encoder setFragmentSamplerState:impl->nearest atIndex:0];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];[encoder endEncoding];submitCommandBuffer(command);
		std::vector<u8> actual;readTexture(output,actual);check(actual==oraclePixels,"Compiled pipeline changed full CPU-oracle pixels after function-cache eviction/destruction");
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: material function cache, 4096 distinct descriptors retain %zu globally, cold weak reference released and repeatedly used hot identity preserved across 3072 insertions; both held pipeline inputs evicted and cache destroyed before real compilation, two full CPU-oracle GPU images; no app/total-memory/FPS claim",result->retained);
}
