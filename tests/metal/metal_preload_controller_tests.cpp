// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/CustomTexture.h"
#include "log/Log.h"
#include "util/worker_thread.h"
#include "rend/metal4/metal4_preload_lookup.h"
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
void GenericLog(LogTypes::LOG_LEVELS,LogTypes::LOG_TYPE,const char*,int,const char *format,...)
{va_list args;va_start(args,format);std::vfprintf(stderr,format,args);std::fputc('\n',stderr);va_end(args);}
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
struct CustomTexturePreloadTest {
	static auto payload(){auto p=std::make_shared<PreparedCustomTexture>();p->width=p->height=1;p->levels.push_back(computeMipLayout(NativeTextureFormat::Rgba8Unorm,1,1,0));p->bytes={31,97,173,255};return p;}
	static void run() {
		CustomTexture state;using Result=CustomTexture::GpuPreloadResult;std::vector<std::weak_ptr<PreparedCustomTexture>> weak;
		for(unsigned i=0;i<10;++i){auto p=payload();weak.push_back(p);state.pendingGpuPreloads.emplace_back(i,std::move(p));}
		unsigned calls=0;state.processGpuPreloadsWithAdmission([&](u32 hash,const PreparedCustomTexture&){++calls;return hash&1?Result::OnDemand:Result::Uploaded;});
		require(calls==8 && state.pendingGpuPreloads.size()==2 && state.preloadLoaded==8 && state.preloadLoadedSize==16,"Bounded batch/progress/loaded byte accounting changed");
		require(!state.pendingError && state.preloadedTextures.empty(),"Budget denial reported upload error or retained a CPU payload cache");
		for(unsigned i=0;i<8;++i)require(weak[i].expired(),"Completed/deferred payload retained");
		state.processGpuPreloadsWithAdmission([](u32,const PreparedCustomTexture&){return Result::OnDemand;});
		require(!state.pendingError && state.pendingGpuPreloads.empty() && state.preloadLoaded==10 && state.preloadLoadedSize==16,"On-demand result changed successful-byte totals");
		for(auto& p:weak)require(p.expired(),"Deferred source payload retained");
		// Exercise the old bool contract unchanged, including empty uploaders.
		state.pendingGpuPreloads.emplace_back(1,payload());state.processGpuPreloads([](u32,const PreparedCustomTexture&){return true;});
		require(state.preloadLoaded==11 && state.preloadLoadedSize==20 && !state.pendingError,"Legacy bool success changed");
		state.pendingGpuPreloads.emplace_back(2,payload());state.processGpuPreloads([](u32,const PreparedCustomTexture&){return false;});
		require(state.pendingError==CustomTextureException::Error::Upload,"Legacy bool failure no longer reported");state.pendingError.reset();
		state.pendingGpuPreloads.emplace_back(3,payload());state.processGpuPreloads({});require(state.pendingError.has_value(),"Empty bool uploader no longer reports failure");state.pendingError.reset();
		state.pendingGpuPreloads.emplace_back(4,nullptr);state.processGpuPreloadsWithAdmission([](u32,const PreparedCustomTexture&)->Result{throw std::runtime_error("Null payload invoked uploader");});require(state.pendingError.has_value(),"Null payload no longer fails");state.pendingError.reset();
		for(bool allocation:{false,true}) {
			state.pendingGpuPreloads.emplace_back(5,payload());state.processGpuPreloadsWithAdmission([&](u32,const PreparedCustomTexture&)->Result{if(allocation)throw std::bad_alloc();throw FlycastException("expected upload failure");});
			require(state.pendingError.has_value(),"Upload exception did not report failure");state.pendingError.reset();
		}
		struct Source final:BaseCustomTextureSource {
			PreparedCustomTexture::Ptr value;u32 target;
			explicit Source(u32 marker,u32 target=77):target(target){auto p=payload();p->replacementHash=marker;value=std::move(p);}
			bool shouldReplace()const override{return true;}bool loadMap(const CustomTextureCapabilities&)override{return true;}
			bool isTextureReplaced(u32 hash)const override{return hash==target;}
			PreparedCustomTexture::Ptr loadCustomTexture(u32 hash,const CustomTextureCapabilities&,const CancellationCheck& cancelled)override{return hash==target && !(cancelled && cancelled())?value:nullptr;}
		};
		state.sources.push_back(std::make_unique<Source>(101));state.sources.push_back(std::make_unique<Source>(202));
		auto loaded=state.loadTexture(77,0,0,CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Metal4),{});
		require(loaded && loaded->replacementHash==101,"On-demand source loader did not retain source precedence");
		require(!state.loadTexture(77,0,0,CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Metal4),[]{return true;}),"On-demand source ignored cancellation");
		std::unordered_map<u32,PreparedCustomTexture::Ptr> resident{{78,payload()},{79,payload()}};
		auto lookup=[&]{return metal_preload::findResident(resident,77,78,79,[&](u32 hash){return state.hasReplacement(hash);});};
		require(!lookup(),"Resident old-hash replacement bypassed deferred current hash");
		state.sources.clear();require(lookup()==resident.at(78),"Resident VQ alias was needlessly reloaded with no preferred source");
		state.sources.push_back(std::make_unique<Source>(303,78));resident.erase(78);
		require(!lookup(),"Resident old hash bypassed deferred VQ hash");
		loaded=state.loadTexture(77,78,79,CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Metal4),{});
		require(loaded && loaded->replacementHash==303,"On-demand VQ hash precedence changed");
		resident.emplace(77,payload());require(lookup()==resident.at(77),"Matching resident current image not reused");
		unsigned queries=0;require(metal_preload::findResident(resident,77,77,77,[&](u32){++queries;return true;})==resident.at(77) && queries==0,"Exact resident hit queried the source or lost hash identity");
		state.pendingGpuPreloads.emplace_back(6,payload());state.clearSessionResources();
		require(state.pendingGpuPreloads.empty() && state.sources.empty() && !state.preloadLoaded && !state.preloadLoadedSize && !state.pendingError,"Session cleanup retained pending payloads/progress/errors/sources");
		std::puts("PASS actual preload controller: eight-item batch, on-demand denial without error/cache, exact progress/byte totals, bool compatibility, null/exception failures, source precedence, current/VQ/old-hash GPU lookup versus deferred preferred sources, direct resident reuse, cancellation and session cleanup");
	}
};
int main(){try{CustomTexturePreloadTest::run();return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
