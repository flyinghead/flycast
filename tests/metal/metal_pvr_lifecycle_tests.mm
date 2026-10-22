// SPDX-License-Identifier: GPL-2.0-or-later
// Real PVR renderer selection/termination and message dispatch; no emulation.
#include "hw/pvr/Renderer_if.h"
#include "cfg/option.h"
#include "rend/metal4/metal4_context.h"
#include "rend/metal4/metal4_texture.h"
#include "rend/metal4/metal4_preload.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/CustomTexture.h"
#include "log/Log.h"
#include "metal_offscreen_test_hooks.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <stb_image_write.h>

extern std::string metalOffscreenCacheRoot;
Renderer *rend_Metal(bool perPixel);

void metalOffscreenValidatePvrLifecycle()
{
	const auto require=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const auto previous=config::RendererType.get();const bool threaded=config::ThreadedRendering;
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen,oldDelay=config::DelayFrameSwapping;
	const int oldResolution=config::RenderResolution,oldSkip=config::SkipFrame;
	const bool oldCustom=config::CustomTextures;const int oldPreload=config::PreloadCustomTextures;
	const auto oldGame=settings.content.gameId;
	const u32 oldRegion=REGION_BASE,oldFpu=FPU_PARAM_CFG,oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	const auto oldVram=vram;
	std::vector<u8> memory(8_MB);vram.setRegion(memory.data(),memory.size());
	struct Restore {
		RenderType type;bool threaded,emulate,wide,delay;int resolution,skip;u32 region,fpu,read,spg;RamRegion vram;bool custom;int preload;std::string game;
		~Restore(){
			metalOffscreenParsedTa={};if(auto pending=DequeueRender())FinishRender(pending);
			try{rend_term_renderer();while(metalOffscreenPumpPvr()){}}catch(...){}custom_texture.terminate();tactx_Term();
			config::RendererType=type;config::ThreadedRendering=threaded;config::EmulateFramebuffer=emulate;
			config::Widescreen=wide;config::DelayFrameSwapping=delay;config::RenderResolution=resolution;config::SkipFrame=skip;
			REGION_BASE=region;FPU_PARAM_CFG=fpu;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;::vram=vram;
			config::CustomTextures=custom;config::PreloadCustomTextures=preload;settings.content.gameId=game;
		}
	} restore{previous,threaded,oldEmulate,oldWide,oldDelay,oldResolution,oldSkip,oldRegion,oldFpu,oldRead,oldSpg,oldVram,oldCustom,oldPreload,oldGame};
	require(!renderer,"PVR lifecycle fixture requires an idle renderer owner");
	config::ThreadedRendering=true;config::EmulateFramebuffer=false;config::Widescreen=false;
	config::DelayFrameSwapping=false;config::RenderResolution=48;config::SkipFrame=0;
	config::CustomTextures=false;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;
	REGION_BASE=0x200000;FPU_PARAM_CFG=0;
	RegionArrayTile tile{};tile.X=19;tile.Y=14;tile.LastRegion=1;pvr_write32p<u32, true>(REGION_BASE+20,tile.full);
	using Color=std::array<u8,4>;unsigned parsed=0,images=0,switches=0,adoptions=0,contexts=0;
	TSP tsp{};tsp.FogCtrl=2;tsp.UseAlpha=1;tsp.SrcInstr=1;tsp.ClampU=tsp.ClampV=1;
	TCW tcw{};tcw.ScanOrder=1;tcw.PixelFmt=Pixel4444;tcw.TexAddr=0x710000/8;
	for(unsigned i=0;i<64;++i){const u16 value=0xf123;std::memcpy(&memory[tcw.TexAddr*8+i*2],&value,2);}
	metalOffscreenParsedTa=[&](TA_context *context){
		++parsed;
		if(context->rend.global_param_op[0].pcw.Texture){
			const auto uploads=MetalContext::Instance()->testTextureUploadCount();
			auto *texture=static_cast<MetalTexture *>(renderer->GetTexture(tsp,tcw,0));
			require(texture && texture->sharedPreloadImage && uploads==MetalContext::Instance()->testTextureUploadCount(),"PVR Process did not adopt its resident GPU texture without reupload");
			context->rend.global_param_op[0].texture=texture;++adoptions;
		}
	};
	const auto frame=[&](Color color,bool textured=false) {
		auto context=tactx_Alloc();auto& data=context->rend;
		data.clearFramebuffer=true;data.globClip={640,480};data.fbClip=Rect({0,0},{640,480});
		data.scaler_ctl.vscalefactor=1024;data.fog_clamp_max.full=~0u;data.idx={0,1,2,3};data.verts.resize(4);
		for(unsigned i=0;i<4;++i){auto& v=data.verts[i];v={};v.x=i&1?640:0;v.y=i&2?480:0;v.z=1;v.u=i&1?1:0;v.v=i&2?1:0;std::copy(color.begin(),color.end(),v.col);}
		PolyParam poly;poly.init();poly.count=4;poly.pcw.Gouraud=1;poly.isp.DepthMode=7;poly.tsp.SrcInstr=1;poly.tsp.FogCtrl=2;
		if(textured){poly.pcw.Texture=1;poly.tsp=tsp;poly.tcw=tcw;}
		data.global_param_op={poly};RenderPass pass{};pass.op_count=1;data.render_passes={pass};
		metalOffscreenVideoTime+=1./60;require(metalOffscreenQueueRender(context),"PVR rejected fresh synthetic TA frame");
	};
	const auto image=[&](Color expected) {
		auto context=MetalContext::Instance();std::vector<u8> pixels;require(bool(context->lastFrame()),"PVR did not publish the rendered image");
		context->readTexture(context->lastFrame(),pixels);require(pixels.size()==64*48*4,"PVR image size differs from scaled framebuffer");
		for(size_t i=0;i<pixels.size();++i)require(pixels[i]==expected[i%4],"PVR-dispatched renderer produced different pixels");++images;
	};
	for(RenderType type:{RenderType::Metal,RenderType::Metal_OIT,RenderType::Metal}) {
		config::RendererType=type;require(rend_init_renderer(),"PVR Metal initialization failed");
		require(!metalOffscreenPumpPvr() && !MetalContext::Instance()->lastFrame(),"Old PVR work reached a new renderer instance");
		const Color color{u8(23+switches*31),u8(71+switches*17),u8(113+switches*7),255};
		const auto before=parsed;frame(color);require(metalOffscreenPumpPvr(),"PVR render message was not dispatched");
		require(parsed==before+1 && !DequeueRender(),"PVR did not finish and recycle its TA context");
		require(metalOffscreenPumpPvr(),"PVR present message was not dispatched");image(color);
		const std::string game=switches&1?"METAL_TEST_PVR_B":"METAL_TEST_PVR_A";
		u32 hash;
		{MetalTexture original(tsp,tcw,0);require(original.Update(),"Cannot hash the original PVR fixture texture");original.ComputeHash();hash=original.texture_hash;original.Delete();}
		const Color replacement{u8(191-switches*23),u8(43+switches*31),u8(157-switches*17),255};
		const auto directory=std::filesystem::path(metalOffscreenCacheRoot)/"textures"/game;std::filesystem::create_directories(directory);
		char name[24];std::snprintf(name,sizeof(name),"%08x.png",hash);std::vector<u8> png(256);for(size_t i=0;i<png.size();++i)png[i]=replacement[i%4];
		require(stbi_write_png((directory/name).c_str(),8,8,4,png.data(),32),"Cannot write PVR PNG package");
		settings.content.gameId=game;config::CustomTextures=true;config::PreloadCustomTextures=true;
		custom_texture.init();
		const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(20);
		while(custom_texture.isPreloading()) {require(std::chrono::steady_clock::now()<until,"PVR master catalog preload timed out");std::this_thread::yield();}
		frame({255,255,255,255},true);require(metalOffscreenPumpPvr() && metalOffscreenPumpPvr(),"PVR textured render/present not dispatched");image(replacement);
		frame({255,255,255,255},true);rend_term_renderer();
		require(!renderer && !DequeueRender(),"PVR termination left an old TA frame for the next renderer");
		require(metal_preload::deviceBudget(MetalContext::Instance()->device())->usage().bytes==0,"PVR termination retained an old preload budget charge");
		require(!MetalContext::Instance()->lastFrame(),"Metal termination retained its displayed image");
		custom_texture.terminate();config::CustomTextures=false;
		if(metalOffscreenContextLifecycle){metalOffscreenRecreateContext();++contexts;}
		++switches;
	}
	if(metalOffscreenContextLifecycle){require(contexts==3,"Missing complete context handoff");custom_texture.terminate();config::CustomTextures=false;}
	for(RenderType type:{RenderType::Metal,RenderType::Metal_OIT}) {
		config::RendererType=type;require(rend_init_renderer(),"PVR failure fixture initialization failed");frame({3,5,7,255});
		const auto parse=metalOffscreenParsedTa;metalOffscreenParsedTa=[](TA_context *){throw RendererException("Injected PVR Process boundary");};
		bool threw=false;try{rend_term_renderer();}catch(const std::exception& e){threw=std::string(e.what())=="Injected PVR Process boundary";}
		metalOffscreenParsedTa=parse;require(threw && !renderer && !DequeueRender() && !metalOffscreenPumpPvr(),"PVR failure did not recycle TA, delete renderer, empty queue and preserve original exception");
	}
	// The real Metal Term still drains/releases all GPU resources. A thin
	// virtual-boundary adapter throws afterwards to test the shared lifecycle's
	// cleanup and first-exception contract without malformed native GPU work.
	struct FailingTerm final:Renderer {
		std::unique_ptr<Renderer> actual;unsigned& terms;unsigned& destroyed;
		FailingTerm(bool oit,unsigned& terms,unsigned& destroyed):actual(rend_Metal(oit)),terms(terms),destroyed(destroyed){}
		~FailingTerm(){++destroyed;}
		bool Init()override{return actual->Init();}
		void Term()override{actual->Term();++terms;throw RendererException("Injected PVR Term boundary");}
		void Process(TA_context *context)override{actual->Process(context);}
		bool Render()override{return actual->Render();}
		void RenderFramebuffer(const FramebufferInfo& info)override{actual->RenderFramebuffer(info);}
	};
	for(bool oit:{false,true})for(bool processFailure:{false,true}) {
		unsigned terms=0,destroyed=0;renderer=new FailingTerm(oit,terms,destroyed);require(rend_init_renderer(),"PVR Term failure fixture initialization failed");
		frame({13,17,19,255});const auto parse=metalOffscreenParsedTa;
		if(processFailure)metalOffscreenParsedTa=[](TA_context *){throw RendererException("Injected PVR Process boundary");};
		bool threw=false;try{rend_term_renderer();}catch(const std::exception& error){threw=std::string(error.what())==(processFailure?"Injected PVR Process boundary":"Injected PVR Term boundary");}
		metalOffscreenParsedTa=parse;
		require(threw && terms==1 && destroyed==1 && !renderer && !DequeueRender() && !metalOffscreenPumpPvr(),"PVR Term exception lost cleanup or replaced the first Process error");
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual PVR lifecycle, %u package/backend sessions, %u exact dispatched images, %u zero-reupload adoptions; pending frames drained before handoff; six Process/Term exception cases preserve cleanup and first error",switches,images,adoptions);
	if(metalOffscreenContextLifecycle)NOTICE_LOG(RENDERER,"Metal validation PASS: %u full Metal GraphicsContext/queue replacements between actual PVR package sessions; old master PNG source sessions retired before context recreation",contexts);
}
