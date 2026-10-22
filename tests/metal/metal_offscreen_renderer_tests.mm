// SPDX-License-Identifier: GPL-2.0-or-later
// Actual MetalContext/MetalRenderer, with offscreen CA surfaces and isolated
// application services. No SDL window, ROM, user configuration or game is used.
#include "rend/metal4/metal4_context.h"
#include "rend/metal4/metal4_texture.h"
#include "rend/metal4/metal4_preload.h"
#include "hw/pvr/Renderer_if.h"
#include "cfg/option.h"
#include "ui/imgui_driver.h"
#include "log/Log.h"
#include "stdclass.h"
#include "metal_offscreen_test_hooks.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <thread>
#include <unistd.h>

void metalValidateRenderer();
std::string metalOffscreenCacheRoot;
bool metalOffscreenSessionsOnly=false;
bool metalOffscreenRecoveryOnly=false;
bool metalOffscreenStorageOnly=false,metalOffscreenStorageBenchmark=false;
bool metalOffscreenStorageLifetimeOnly=false;
bool metalOffscreenIntermediates=false,metalOffscreenIntermediatesBenchmark=false;
bool metalOffscreenBoundaryProfile=false;
bool metalOffscreenPassProfile=false;
bool metalOffscreenProofProfile=false,metalOffscreenProofBenchmark=false;
bool metalOffscreenPlanProfile=false,metalOffscreenPlanBenchmark=false;
bool metalOffscreenPoolProfile=false,metalOffscreenPoolBenchmark=false;
bool metalOffscreenReuseResidency=false;
bool metalOffscreenCacheProfile=false,metalOffscreenCacheOptimized=true;
bool metalOffscreenContextLifecycle=false;
std::string metalOffscreenCachePhase;
static id<MTLTexture> lastDrawableTexture;
id<MTLTexture> metalOffscreenTakeDrawableTexture(){auto image=lastDrawableTexture;lastDrawableTexture=nil;return image;}
static bool nextCommandArmed=false;
static MetalOffscreenFault nextCommandFault=MetalOffscreenFault::None;
static bool nextLegacyBoundaries=false;
static bool nextPassDependencies=true;
static id<MTLSharedEvent> nextCommandGate;
static __weak FCMetal4Commands *lastInstrumentedCommand;
void metalOffscreenArmCommand(MetalOffscreenFault fault,id<MTLSharedEvent> gate,bool legacyBoundaries,bool passDependencies) {
	if(nextCommandArmed)throw std::runtime_error("Offscreen command hook already armed");
	nextCommandFault=fault;nextCommandGate=gate;nextCommandArmed=true;lastInstrumentedCommand=nil;
	nextLegacyBoundaries=legacyBoundaries;
	nextPassDependencies=passDependencies;
}
FCMetal4Commands *metalOffscreenLastCommand(){return lastInstrumentedCommand;}
void metalOffscreenClearCommandHook(){nextCommandArmed=false;nextCommandGate=nil;lastInstrumentedCommand=nil;}
std::string os_PrecomposedString(std::string value) {
	@autoreleasepool {NSString *text=[[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
		if(!text)throw std::runtime_error("Invalid UTF-8 test filename");return std::string(text.precomposedStringWithCanonicalMapping.UTF8String);}
}
void GenericLog(LogTypes::LOG_LEVELS,LogTypes::LOG_TYPE,const char*,int,const char *format,...)
{va_list args;va_start(args,format);std::vfprintf(stderr,format,args);std::fputc('\n',stderr);va_end(args);}

// The native queue still performs all GPU commits/events/feedback. Only the
// CoreAnimation surface wait/signal is omitted for our non-display drawable.
@interface FCOffscreenSurfaceQueue : NSObject
@property(strong) id<MTL4CommandQueue> actual;
@end
@implementation FCOffscreenSurfaceQueue
- (void)waitForDrawable:(id<MTLDrawable>)drawable {}
- (void)signalDrawable:(id<MTLDrawable>)drawable {}
- (id)forwardingTargetForSelector:(SEL)selector {return _actual;}
@end
@interface FCOffscreenQueue : FCMetal4Queue @end
@implementation FCOffscreenQueue {FCOffscreenSurfaceQueue *_surface;}
- (instancetype)initWithDevice:(id<MTLDevice>)device {
 if((self=[super initWithDevice:device])){_surface=[FCOffscreenSurfaceQueue new];_surface.actual=[super native];}return self;
}
- (id<MTL4CommandQueue>)native {return (id<MTL4CommandQueue>)_surface;}
- (FCMetal4Commands *)commandBuffer {
	auto command=[super commandBuffer];
	if(nextCommandArmed) {
		nextCommandArmed=false;lastInstrumentedCommand=command;
		[command setLegacyEncoderBoundariesForTesting:nextLegacyBoundaries];
		[command setPassDependenciesForTesting:nextPassDependencies];
		if(nextCommandGate)[command waitForEvent:nextCommandGate value:1];nextCommandGate=nil;
		switch(nextCommandFault) {
		case MetalOffscreenFault::BeforeCommit:[command setCommitFailureForTesting:YES];break;
		case MetalOffscreenFault::BeforeEnqueue:[command setQueueCommitFailureForTesting:YES];break;
		case MetalOffscreenFault::AfterEnqueue:[command setPostCommitFailureForTesting:YES];break;
		case MetalOffscreenFault::Feedback:[command setFeedbackErrorForTesting:[NSError errorWithDomain:@"org.flycast.test.context-feedback" code:42 userInfo:nil]];break;
		default:break;
		}
	}
	return command;
}
@end
FCMetal4Queue *metalOffscreenMakeQueue(id<MTLDevice> device){return [[FCOffscreenQueue alloc] initWithDevice:device];}
@interface FCOffscreenDrawable : NSObject<CAMetalDrawable>
@property(strong) id<MTLTexture> texture;
@end
@implementation FCOffscreenDrawable
- (CAMetalLayer *)layer{return nil;}
- (NSUInteger)drawableID{return (NSUInteger)(__bridge void *)self;}
- (CFTimeInterval)presentedTime{return 0;}
- (void)present{}
- (void)presentAtTime:(CFTimeInterval)time{}
- (void)presentAfterMinimumDuration:(CFTimeInterval)duration{}
- (void)addPresentedHandler:(MTLDrawablePresentedHandler)handler{}
@end
@interface FCOffscreenLayer : CAMetalLayer @end
@implementation FCOffscreenLayer
- (id<CAMetalDrawable>)nextDrawable {
 auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:self.pixelFormat width:std::max<NSUInteger>(1,self.drawableSize.width) height:std::max<NSUInteger>(1,self.drawableSize.height) mipmapped:NO];
 d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
 auto result=[FCOffscreenDrawable new];result.texture=[self.device newTextureWithDescriptor:d];
 if(!result.texture)throw std::runtime_error("Cannot allocate offscreen drawable");if(metalOffscreenPoolProfile)lastDrawableTexture=result.texture;return result;
}
@end
void metalOffscreenRecreateContext()
{
 const auto require=[](bool value,const char *message){if(!value)throw std::runtime_error(message);};
 require(metalOffscreenContextLifecycle && !renderer,"Context replacement requires the lifecycle CLI and a stopped PVR renderer");
 static unsigned cycle=0;const unsigned current=cycle++;
 auto device=MetalContext::Instance()->device();auto budget=metal_preload::deviceBudget(device);
 require(budget->usage().bytes==0,"PVR session retained preload charges before context replacement");
 @autoreleasepool {
  auto oldQueue=MetalContext::Instance()->queue();
  const auto fault=current%3==1?MetalOffscreenFault::BeforeCommit:current%3==2?MetalOffscreenFault::Feedback:MetalOffscreenFault::None;
  auto event=[device newSharedEvent];auto release=dispatch_group_create();
  auto stopped=std::make_shared<std::atomic<bool>>(false),observed=std::make_shared<std::atomic<bool>>(false);
  struct Release {id<MTLSharedEvent> event;std::shared_ptr<std::atomic<bool>> stopped;~Release(){*stopped=true;event.signaledValue=1;metalOffscreenClearCommandHook();}} releaseOnExit{event,stopped};
  FCMetal4Commands *gate=nil;
  if(fault!=MetalOffscreenFault::BeforeCommit){gate=[oldQueue commandBuffer];[gate waitForEvent:event value:1];MetalContext::Instance()->submitCommandBuffer(gate);}
  const auto makePayload=[&](NativeTextureFormat format) {
   PreparedCustomTexture payload;payload.nativeFormat=format;payload.width=19;payload.height=13;
   u64 offset=0;for(unsigned level=0;level<5;++level){auto mip=computeMipLayout(format,std::max(1u,19u>>level),std::max(1u,13u>>level),offset);payload.levels.push_back(mip);offset+=mip.byteSize;}
   payload.bytes.resize(offset);
   for(unsigned level=0;level<5;++level){const auto& mip=payload.levels[level];const std::array<u8,4> color{u8(31+current*17+level*3),u8(73+current+level*7),u8(149-current-level*11),255};
    for(unsigned by=0;by<mip.blocksY;++by)for(unsigned bx=0;bx<mip.blocksX;++bx){
     auto bytes=payload.bytes.data()+mip.byteOffset+by*mip.rowPitchBytes+bx*(format==NativeTextureFormat::Rgba8Unorm?4:16);
     if(format==NativeTextureFormat::Rgba8Unorm)std::copy(color.begin(),color.end(),bytes);
     else {std::fill_n(bytes,16,255);bytes[0]=0xfc;bytes[1]=0xfd;for(unsigned c=0;c<4;++c){const u16 value=u16(color[c])*257;bytes[8+c*2]=u8(value);bytes[9+c*2]=u8(value>>8);}}
    }
   }return payload;
  };
  auto payload=makePayload(current%3==1?NativeTextureFormat::Rgba8Unorm:NativeTextureFormat::Astc4x4Unorm);
  // Leave the outer upload batch open. The actual context destructor must
  // submit or cancel it, rather than relying on renderer Term to do so.
  MetalContext::Instance()->beginTextureUpdates();metalOffscreenArmCommand(fault,nil);
  bool deferred=false;auto prepared=MetalTexture::preload(payload,deferred);auto command=metalOffscreenLastCommand();
  auto upload=MetalContext::Instance()->textureUploadState();
  require(prepared && !deferred && command && upload->phase()==metal_upload::Phase::Pending && command.status==FCMetal4StatusEncoding,"Context fixture did not retain an unsubmitted tracked preload");
  if(fault!=MetalOffscreenFault::BeforeCommit){
   dispatch_group_enter(release);
   dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(command.status==FCMetal4StatusEncoding && !stopped->load() && std::chrono::steady_clock::now()<end)std::this_thread::yield();
    *observed=command.status==FCMetal4StatusCommitted;event.signaledValue=1;dispatch_group_leave(release);
   });
  }
  GraphicsContext::Term();dispatch_group_wait(release,DISPATCH_TIME_FOREVER);
  require(!GraphicsContext::Instance() && !imguiDriver,"GraphicsContext teardown retained its global context or UI driver");
  if(fault!=MetalOffscreenFault::BeforeCommit)require(observed->load(),"Context destructor failed to submit its pending upload before releasing the GPU gate");
  require(upload->phase()==(fault==MetalOffscreenFault::None?metal_upload::Phase::Ready:fault==MetalOffscreenFault::BeforeCommit?metal_upload::Phase::Canceled:metal_upload::Phase::Failed),"Context teardown left incorrect preload completion state");
  require(budget->usage().bytes>0,"Externally held preload lost its budget charge during context teardown");
  bool rejected=false;
  try{MetalContext::CreateOffscreenForTesting(nil,nil);}catch(const std::exception&){rejected=true;}
  require(rejected && !GraphicsContext::Instance() && !imguiDriver,"Failed Metal construction left a stale global owner");
  auto layer=[[FCOffscreenLayer alloc] init];layer.drawableSize=CGSizeMake(64,48);auto queue=metalOffscreenMakeQueue(device);
  require(queue!=oldQueue,"Context replacement reused its old native queue wrapper");
  MetalContext::CreateOffscreenForTesting(queue,layer);
  require(GraphicsContext::Instance() && imguiDriver && MetalContext::Instance()->device()==device && MetalContext::Instance()->queue()==queue,"Fresh Metal context or UI/queue ownership is incorrect");
  EventManager::event(Event::Pause);EventManager::event(Event::Terminate);
  const auto uploads=MetalContext::Instance()->testTextureUploadCount();MetalTexture probe;
  const bool adopted=probe.useGpuPreloadedTexture(prepared);
  require(adopted==(fault==MetalOffscreenFault::None) && MetalContext::Instance()->testTextureUploadCount()==uploads,"Fresh context accepted a failed preload or reuploaded an existing image");
  const auto checkMips=[&](id<MTLTexture> chain) {
   require(chain && chain.mipmapLevelCount==5,"Context handoff lost its complete mip chain");
   for(unsigned level=0;level<5;++level){const unsigned width=std::max(1u,19u>>level),height=std::max(1u,13u>>level);
    auto view=[chain newTextureViewWithPixelFormat:chain.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(level,1) slices:NSMakeRange(0,1)];
    auto target=MetalContext::Instance()->makeTexture(width,height,MTLPixelFormatBGRA8Unorm);
    auto draw=[queue commandBuffer];auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    auto encoder=[draw renderCommandEncoderWithDescriptor:pass];MetalContext::Instance()->drawQuad(encoder,view,MTLViewport{0,0,double(width),double(height),0,1},false,true,nullptr,false);[encoder endEncoding];MetalContext::Instance()->submitCommandBuffer(draw);
    std::vector<u8> pixels;MetalContext::Instance()->readTexture(target,pixels);require(pixels.size()==width*height*4,"Cross-context mip render size differs");
    const std::array<u8,4> bgra{u8(149-current-level*11),u8(73+current+level*7),u8(31+current*17+level*3),255};
    for(size_t i=0;i<pixels.size();++i)require(pixels[i]==bgra[i%4],"Native or RGBA mip changed pixels across full context recreation");
   }
  };
  if(adopted){require(probe.image.pixelFormat==MTLPixelFormatASTC_4x4_LDR,"Successful native ASTC preload lost its compressed format");checkMips(probe.image);}
  else {
   require(!probe.image,"Rejected cross-context preload changed the binding");
   auto rgba=makePayload(NativeTextureFormat::Rgba8Unorm);
   require(probe.uploadCustomTexture(rgba,true) && probe.image.pixelFormat==MTLPixelFormatRGBA8Unorm,"Fresh context failed its RGBA demand fallback after rejected preload");checkMips(probe.image);
  }
  probe.Delete();prepared.reset();command=nil;gate=nil;
 }
 MetalContext::Instance()->waitIdle();
 require(budget->usage().bytes==0,"Context replacement retained an orphaned preload budget charge");
 std::fprintf(stderr,"Metal validation PASS: actual GraphicsContext recreation cycle=%u, open preload batch %s, old queue drained, failed constructor global owner cleared, new queue/UI/event callbacks, %s, complete budget release\n",current,current%3==0?"completed":current%3==1?"canceled before commit":"failed feedback",current%3==0?"five exact native ASTC mip images without reupload":"failed image rejected; five exact RGBA demand fallback mip images");
}
int main(int argc,char **argv){@autoreleasepool{try {
 metalOffscreenCacheProfile=argc>1 && std::strcmp(argv[1],"--cache-profile")==0;
 std::string ownedCacheRoot;
 char directory[]="/private/tmp/flycast-metal4-renderer-XXXXXX";
 if(metalOffscreenCacheProfile){
  if(argc!=5)throw std::runtime_error("Cache profile requires isolated root, baseline/optimized, cold/warm");
  const auto root=std::filesystem::canonical(argv[2]);
  if(root.string().rfind("/private/tmp/flycast-metal4-todo/cache-transitions-",0)!=0 || !std::filesystem::is_directory(root))throw std::runtime_error("Cache profile root is outside the isolated test workspace");
  metalOffscreenCacheRoot=root.string();metalOffscreenCacheOptimized=std::strcmp(argv[3],"optimized")==0;
  if(!metalOffscreenCacheOptimized && std::strcmp(argv[3],"baseline")!=0)throw std::runtime_error("Unknown cache profile policy");
  metalOffscreenCachePhase=argv[4];if(metalOffscreenCachePhase!="cold" && metalOffscreenCachePhase!="warm"
    && metalOffscreenCachePhase!="seed-even" && metalOffscreenCachePhase!="seed-odd")throw std::runtime_error("Unknown cache profile phase");
  MetalPipelineCache::setIdentityOptimizationsForTesting(metalOffscreenCacheOptimized,true);
 }else{
  if(!mkdtemp(directory))throw std::runtime_error("Cannot create isolated renderer cache");
  metalOffscreenCacheRoot=ownedCacheRoot=directory;
 }
 set_user_config_dir(metalOffscreenCacheRoot+"/config");set_user_data_dir(metalOffscreenCacheRoot+"/userdata");
 struct Cleanup {std::string owned;~Cleanup(){GraphicsContext::Term();if(ImGui::GetCurrentContext())ImGui::DestroyContext();if(!owned.empty()){std::error_code error;std::filesystem::remove_all(owned,error);}}} cleanup{ownedCacheRoot};
 config::VSync=false;config::FramePacing=false;config::DupeFrames=false;config::FloatVMUs=false;
 config::CustomTextures=false;config::DumpTextures=false;
 settings.platform.system=DC_PLATFORM_DREAMCAST;settings.platform.vram_size=8_MB;settings.platform.vram_mask=8_MB-1;
 settings.display.width=64;settings.display.height=48;settings.display.refreshRate=60;
 ImGui::CreateContext();auto layer=[FCOffscreenLayer layer];layer.drawableSize=CGSizeMake(64,48);
 auto device=MTLCreateSystemDefaultDevice();auto queue=[[FCOffscreenQueue alloc] initWithDevice:device];
 const auto contextStart=std::chrono::steady_clock::now();
 MetalContext::CreateOffscreenForTesting(queue,layer);
 if(metalOffscreenCacheProfile)std::fprintf(stderr,"Metal cache context phase=%s optimized=%d cpu_us=%.3f\n",metalOffscreenCachePhase.c_str(),metalOffscreenCacheOptimized,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-contextStart).count());
 metalOffscreenSessionsOnly=argc>1 && std::strcmp(argv[1],"--sessions-only")==0;
 metalOffscreenRecoveryOnly=argc>1 && std::strcmp(argv[1],"--recovery-only")==0;
 metalOffscreenSessionsOnly|=metalOffscreenRecoveryOnly;
 metalOffscreenContextLifecycle=argc>1 && std::strcmp(argv[1],"--context-lifecycle")==0;
 const bool pvrOnly=metalOffscreenContextLifecycle || (argc>1 && std::strcmp(argv[1],"--pvr-only")==0);
 metalOffscreenProofProfile=argc>1 && (std::strcmp(argv[1],"--proofs-validation")==0 || std::strcmp(argv[1],"--proofs-profile")==0);
 metalOffscreenProofBenchmark=argc>1 && std::strcmp(argv[1],"--proofs-profile")==0;
 metalOffscreenPlanProfile=argc>1 && (std::strcmp(argv[1],"--plan-validation")==0 || std::strcmp(argv[1],"--plan-profile")==0);
 metalOffscreenPlanBenchmark=argc>1 && std::strcmp(argv[1],"--plan-profile")==0;
 metalOffscreenPoolProfile=argc>1 && (std::strcmp(argv[1],"--pool-validation")==0 || std::strcmp(argv[1],"--pool-profile")==0);
 metalOffscreenPoolBenchmark=argc>1 && std::strcmp(argv[1],"--pool-profile")==0;
 metalOffscreenReuseResidency=argc>2 && std::strcmp(argv[2],"--reuse-residency")==0;
 metalOffscreenBoundaryProfile=argc>1 && (std::strcmp(argv[1],"--boundaries-profile")==0 || std::strcmp(argv[1],"--boundaries-validation")==0);
 metalOffscreenPassProfile=argc>1 && (std::strcmp(argv[1],"--passes-profile")==0 || std::strcmp(argv[1],"--passes-validation")==0);
 metalOffscreenStorageBenchmark=argc>1 && (std::strcmp(argv[1],"--storage-benchmark")==0 || std::strcmp(argv[1],"--boundaries-profile")==0);
 metalOffscreenStorageBenchmark|=argc>1 && std::strcmp(argv[1],"--passes-profile")==0;
 metalOffscreenBoundaryProfile|=metalOffscreenPassProfile;
 metalOffscreenStorageOnly=metalOffscreenStorageBenchmark || (argc>1 && std::strcmp(argv[1],"--storage-only")==0);
 metalOffscreenStorageLifetimeOnly=argc>1 && std::strcmp(argv[1],"--storage-lifetime")==0;
 metalOffscreenStorageOnly|=metalOffscreenStorageLifetimeOnly;
 metalOffscreenStorageOnly|=metalOffscreenBoundaryProfile;
 metalOffscreenIntermediatesBenchmark=argc>1 && std::strcmp(argv[1],"--intermediates-profile")==0;
 metalOffscreenIntermediates=metalOffscreenIntermediatesBenchmark || (argc>1 && std::strcmp(argv[1],"--intermediates-validation")==0);
 metalOffscreenStorageOnly|=metalOffscreenIntermediates;
 const bool presentationOnly=argc>1 && std::strcmp(argv[1],"--presentation-routing")==0;
 if(presentationOnly)MetalContext::Instance()->validateDeferredPresentationForTesting();
 if(!pvrOnly && !presentationOnly && (argc==1 || std::strcmp(argv[1],"--init-only")!=0))metalValidateRenderer();
 if(argc==1 || pvrOnly)metalOffscreenValidatePvrLifecycle();
 if(metalOffscreenCacheProfile){
  auto s=MetalContext::Instance()->pipelineStatisticsForTesting(true);
  const auto timing=[](MetalPipelineCache::Statistics::Timing t){return @{@"calls":@(t.calls),@"ns":@(t.nanoseconds),@"max_ns":@(t.maximum)};};
  NSMutableDictionary *times=[NSMutableDictionary new];
  for(unsigned i=0;i<3;++i){times[[NSString stringWithFormat:@"lookup_%u",i]]=timing(s.lookup[i]);times[[NSString stringWithFormat:@"creation_%u",i]]=timing(s.creation[i]);}
  for(unsigned i=0;i<2;++i)times[[NSString stringWithFormat:@"writer_%u",i]]=timing(s.writer[i]);
  times[@"serialization"]=timing(s.serialization);times[@"publication"]=timing(s.publication);
  auto data=[NSJSONSerialization dataWithJSONObject:@{@"hits":@(s.hits),@"misses":@(s.misses),@"probes":@(s.probes),@"hinted":@(s.hintedHits),@"queued":@(s.queued),@"recordings":@(s.recordings),@"duplicates":@(s.duplicates),@"copies":@(s.descriptorCopies),@"segments":@(s.savedSegments),@"pending":@(s.pending),@"accepting":@(s.accepting),@"timing":@(s.timing),@"times":times} options:NSJSONWritingSortedKeys error:nil];
  if(!data || s.pending || s.accepting)throw std::runtime_error("Cache profile did not finish its real writer");
  std::fprintf(stderr,"Metal cache summary %s\n",[[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] UTF8String]);
 }
 GraphicsContext::Term();ImGui::DestroyContext();
 std::puts("PASS actual offscreen renderer: no app/window/game, isolated temporary cache");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());GraphicsContext::Term();if(ImGui::GetCurrentContext())ImGui::DestroyContext();return 1;}}}
