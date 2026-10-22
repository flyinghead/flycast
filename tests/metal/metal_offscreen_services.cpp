// SPDX-License-Identifier: GPL-2.0-or-later
// Application boundaries for the ROM-free offscreen renderer target. Rendering,
// transforms, decoding, framebuffer conversion and VRAM invalidation are real
// production units. Configuration I/O and unavailable UI/parser entrypoints
// fail closed rather than reading user data or starting a window/emulator.
#include "types.h"
#include "emulator.h"
#include "cfg/option.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/CustomTexture.h"
#include "rend/osd.h"
#include "ui/imgui_driver.h"
#include "oslib/oslib.h"
#include "sdl/sdl.h"
#include "hw/sh4/sh4_if.h"
#include <SDL.h>
#include <SDL_metal.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <pthread.h>
#include <stdexcept>
#include "metal_offscreen_test_hooks.h"
std::function<void(TA_context *)> metalOffscreenParsedTa;
bool metalOffscreenAllowExpectedNotification=false;
unsigned metalOffscreenExpectedNotificationCount=0;
settings_t settings;
u8 pvr_regs[pvr_RegSize]{};
bool pal_needs_update=true;
bool SH4FastEnough=true;
u32 fskip=0;
Emulator emu;
Sh4RCB *p_sh4rcb=nullptr;
double metalOffscreenVideoTime=0;
GuiState gui_state=GuiState::Closed;
std::unique_ptr<ImGuiDriver> imguiDriver;
extern std::string metalOffscreenCacheRoot;
[[noreturn]] static void unavailable(const char *name){throw std::runtime_error(std::string("Offscreen test attempted unavailable service: ")+name);}
Sh4Executor *Emulator::getSh4Executor(){unavailable("SH4 executor");}
void Sh4Context::restoreHostRoundingMode(){unavailable("SH4 rounding mode");}
void setDefaultRoundingMode(){unavailable("nonthreaded emulation");}
namespace ggpo {bool active(){unavailable("GGPO session");}}
namespace hostfs {
std::string getShaderCachePath(const std::string& name){return metalOffscreenCacheRoot+"/"+name;}
std::string getTextureDumpPath(){unavailable("texture dump path");}
std::string getTextureLoadPath(const std::string& gameId){
	if(gameId.rfind("METAL_TEST_",0)!=0 || gameId.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789")!=std::string::npos)
		unavailable("non-test texture pack path");
	return metalOffscreenCacheRoot+"/textures/"+gameId;
}
}
void fatal_error(const char *format,...){char message[2048];va_list args;va_start(args,format);std::vsnprintf(message,sizeof(message),format,args);va_end(args);throw std::runtime_error(message);}
void os_DebugBreak(){unavailable("debug break");}
void os_SetThreadName(const char *name){pthread_setname_np(name);}
u16 defaultNaomiServerPort(){return 0;}
void os_notify(const char *message,int,const char *){
	if(metalOffscreenAllowExpectedNotification && std::string(message)=="Custom texture could not be uploaded to the GPU") {
		++metalOffscreenExpectedNotificationCount;return;
	}
	throw std::runtime_error(std::string("Unexpected renderer notification: ")+message);
}
namespace i18n {const char *T(const char *message){return message;}}
namespace config {
int loadInt(const std::string&,const std::string&,int value){return value;}
int64_t loadInt64(const std::string&,const std::string&,int64_t value){return value;}
bool loadBool(const std::string&,const std::string&,bool value){return value;}
float loadFloat(const std::string&,const std::string&,float value){return value;}
std::string loadStr(const std::string&,const std::string&,const std::string& value){return value;}
bool isTransient(const std::string&,const std::string&){return false;}
void saveInt(const std::string&,const std::string&,int){unavailable("saveInt");}
void saveInt64(const std::string&,const std::string&,int64_t){unavailable("saveInt64");}
void saveBool(const std::string&,const std::string&,bool){unavailable("saveBool");}
void saveFloat(const std::string&,const std::string&,float){unavailable("saveFloat");}
void saveStr(const std::string&,const std::string&,const std::string&){unavailable("saveStr");}
void deleteEntry(const std::string&,const std::string&){unavailable("deleteEntry");}
}
void EventManager::registerEvent(Event event,Callback cb,void *p){unregisterEvent(event,cb,p);callbacks[size_t(event)].emplace_back(cb,p);}
void EventManager::unregisterEvent(Event event,Callback cb,void *p){auto& list=callbacks[size_t(event)];auto i=std::find(list.begin(),list.end(),std::make_pair(cb,p));if(i!=list.end())list.erase(i);}
void EventManager::broadcastEvent(Event event){auto copy=callbacks[size_t(event)];for(auto& cb:copy)cb.first(event,cb.second);}
void gui_updateStyle(){}
void gui_display_osd(){}
void ta_parse(TA_context *context,bool){if(metalOffscreenParsedTa){metalOffscreenParsedTa(context);return;}unavailable("raw TA parser");}
bool sdl_recreate_window(u32){unavailable("SDL window");return false;}
bool sdl_update_display_metrics(SDL_Window *window,u32){if(window)unavailable("SDL display metrics");return false;}
extern "C" void SDL_Metal_DestroyView(SDL_MetalView view){if(view)unavailable("SDL Metal view");}
namespace addrspace {
// Test VRAM is an ordinary vector, not a page-protected emulator mapping.
// Tests invoke the real VramLockedWriteOffset notification explicitly.
void protectVram(u32,u32){}
void unprotectVram(u32,u32){}
}
double spg_GetFrameDuration(){return 1./60;}
double spg_GetVideoTime(){return metalOffscreenVideoTime;}
u32 vmu_lcd_data[8][48*32]{};bool vmu_lcd_status[8]{};u64 vmuLastChanged[8]{};
void push_vmu_screen(int,int,u8*){unavailable("VMU upload");}
bool getVmuScreenSnapshot(int,u64&,u32*){return false;}
void resetVmuScreens(){}
void blankVmus(){}
const u32 *getCrosshairTextureData(){unavailable("crosshair texture");return nullptr;}
std::pair<float,float> getCrosshairPosition(int){unavailable("crosshair position");return {};}
