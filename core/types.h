#pragma once
#include "build.h"

#if HOST_CPU == CPU_X86
	#ifdef _MSC_VER
	#define DYNACALL  __fastcall
	#else
	//android defines fastcall as regparm(3), it doesn't work for us
	#undef fastcall
	#define DYNACALL __attribute__((fastcall))
	#endif
#else
	#define DYNACALL
#endif

#include <cstdint>
#include <cstddef>

//basic types
typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef size_t unat;

//intc function pointer and enums
enum HollyInterruptType
{
	holly_nrm = 0x0000,
	holly_ext = 0x0100,
	holly_err = 0x0200,
};

enum HollyInterruptID
{
		// asic9a /sh4 external holly normal [internal]
		holly_RENDER_DONE_vd = holly_nrm | 0,	//bit 0 = End of Render interrupt : Video
		holly_RENDER_DONE_isp = holly_nrm | 1,	//bit 1 = End of Render interrupt : ISP
		holly_RENDER_DONE = holly_nrm | 2,		//bit 2 = End of Render interrupt : TSP

		holly_SCANINT1 = holly_nrm | 3,			//bit 3 = V Blank-in interrupt
		holly_SCANINT2 = holly_nrm | 4,			//bit 4 = V Blank-out interrupt
		holly_HBLank = holly_nrm | 5,			//bit 5 = H Blank-in interrupt

		holly_YUV_DMA = holly_nrm | 6,			//bit 6 = End of Transferring interrupt : YUV
		holly_OPAQUE = holly_nrm | 7,			//bit 7 = End of Transferring interrupt : Opaque List
		holly_OPAQUEMOD = holly_nrm | 8,		//bit 8 = End of Transferring interrupt : Opaque Modifier Volume List

		holly_TRANS = holly_nrm | 9,			//bit 9 = End of Transferring interrupt : Translucent List
		holly_TRANSMOD = holly_nrm | 10,		//bit 10 = End of Transferring interrupt : Translucent Modifier Volume List
		holly_PVR_DMA = holly_nrm | 11,			//bit 11 = End of DMA interrupt : PVR-DMA
		holly_MAPLE_DMA = holly_nrm | 12,		//bit 12 = End of DMA interrupt : Maple-DMA

		holly_MAPLE_VBOI = holly_nrm | 13,		//bit 13 = Maple V blank over interrupt
		holly_GDROM_DMA = holly_nrm | 14,		//bit 14 = End of DMA interrupt : GD-DMA
		holly_SPU_DMA = holly_nrm | 15,			//bit 15 = End of DMA interrupt : AICA-DMA

		holly_EXT_DMA1 = holly_nrm | 16,		//bit 16 = End of DMA interrupt : Ext-DMA1(External 1)
		holly_EXT_DMA2 = holly_nrm | 17,		//bit 17 = End of DMA interrupt : Ext-DMA2(External 2)
		holly_DEV_DMA = holly_nrm | 18,			//bit 18 = End of DMA interrupt : Dev-DMA(Development tool DMA)

		holly_CH2_DMA = holly_nrm | 19,			//bit 19 = End of DMA interrupt : ch2-DMA
		holly_PVR_SortDMA = holly_nrm | 20,		//bit 20 = End of DMA interrupt : Sort-DMA (Transferring for alpha sorting)
		holly_PUNCHTHRU = holly_nrm | 21,		//bit 21 = End of Transferring interrupt : Punch Through List

		// asic9c/sh4 external holly external [EXTERNAL]
		holly_GDROM_CMD = holly_ext | 0x00,	//bit 0 = GD-ROM interrupt
		holly_SPU_IRQ = holly_ext | 0x01,	//bit 1 = AICA interrupt
		holly_EXP_8BIT = holly_ext | 0x02,	//bit 2 = Modem interrupt
		holly_EXP_PCI = holly_ext | 0x03,	//bit 3 = External Device interrupt

		// asic9b/sh4 external holly err only error [error]
		//missing quite a few ehh ?
		//bit 0 = RENDER : ISP out of Cache(Buffer over flow)
		//bit 1 = RENDER : Hazard Processing of Strip Buffer
		holly_PRIM_NOMEM = holly_err | 0x02,	//bit 2 = TA : ISP/TSP Parameter Overflow
		holly_MATR_NOMEM = holly_err | 0x03,	//bit 3 = TA : Object List Pointer Overflow
		//bit 4 = TA : Illegal Parameter
		//bit 5 = TA : FIFO Overflow
		//bit 6 = PVRIF : Illegal Address set
		//bit 7 = PVRIF : DMA over run
		holly_MAPLE_ILLADDR = holly_err | 0x08,  //bit 8 = MAPLE : Illegal Address set
		holly_MAPLE_OVERRUN = holly_err | 0x09,  //bit 9 = MAPLE : DMA over run
		holly_MAPLE_FIFO = holly_err | 0x0a,     //bit 10 = MAPLE : Write FIFO overflow
		holly_MAPLE_ILLCMD = holly_err | 0x0b,   //bit 11 = MAPLE : Illegal command
		//bit 12 = G1 : Illegal Address set
		//bit 13 = G1 : GD-DMA over run
		//bit 14 = G1 : ROM/FLASH access at GD-DMA
		holly_AICA_ILLADDR = holly_err | 0x0f,   //bit 15 = G2 : AICA-DMA Illegal Address set
		holly_EXT1_ILLADDR = holly_err | 0x10,   //bit 16 = G2 : Ext-DMA1 Illegal Address set
		holly_EXT2_ILLADDR = holly_err | 0x11,   //bit 17 = G2 : Ext-DMA2 Illegal Address set
		holly_DEV_ILLADDR = holly_err | 0x12,    //bit 18 = G2 : Dev-DMA Illegal Address set
		holly_AICA_OVERRUN = holly_err | 0x13,   //bit 19 = G2 : AICA-DMA over run
		holly_EXT1_OVERRUN = holly_err | 0x14,   //bit 20 = G2 : Ext-DMA1 over run
		holly_EXT2_OVERRUN = holly_err | 0x15,   //bit 21 = G2 : Ext-DMA2 over run
		holly_DEV_OVERRUN = holly_err | 0x16,    //bit 22 = G2 : Dev-DMA over run
		//bit 23 = G2 : AICA-DMA Time out
		//bit 24 = G2 : Ext-DMA1 Time out
		//bit 25 = G2 : Ext-DMA2 Time out
		//bit 26 = G2 : Dev-DMA Time out
		//bit 27 = G2 : Time out in CPU accessing
};

#ifndef TARGET_UWP
#include "nowide/cstdlib.hpp"
#include "nowide/cstdio.hpp"
#else
#include "nowide/config.hpp"
#include "nowide/convert.hpp"
#include "nowide/cstdlib.hpp"
#include "nowide/stackstring.hpp"

#include <cstdio>
namespace nowide {
FILE *fopen(char const *file_name, char const *mode);
int remove(const char *pathname);
}
#endif

#if defined(__APPLE__)
int darw_printf(const char* Text,...);
#endif

#ifndef TARGET_IPHONE
#if defined(__APPLE__) && defined(__MACH__) && HOST_CPU == CPU_ARM64
#define TARGET_ARM_MAC
#include "pthread.h"
inline static void JITWriteProtect(bool enabled) {
	if (__builtin_available(macOS 11.0, *))
		pthread_jit_write_protect_np(enabled);
}
#else
inline static void JITWriteProtect(bool enabled) {
}
#endif
#endif

//includes from c++rt
#include <string>
#include <stdexcept>

#ifdef _MSC_VER
#define likely(x) x
#define unlikely(x) x
#define expected(x, y) x
#else
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#define expected(x, y) __builtin_expect((x), (y))
#endif

#include "log/Log.h"

#define VER_EMUNAME		"Flycast"
#define VER_SHORTNAME	VER_EMUNAME

#ifndef _MSC_VER
#define stricmp strcasecmp
#endif

[[noreturn]] void os_DebugBreak();
void fatal_error(const char* text, ...);

#define verify(x) do { if ((x) == false){ fatal_error("Verify Failed  : " #x "\n in %s -> %s : %d", (__FUNCTION__), (__FILE__), __LINE__); os_DebugBreak();}} while (false)
#define die(reason) do { fatal_error("Fatal error : %s\n in %s -> %s : %d", (reason), (__FUNCTION__), (__FILE__), __LINE__); os_DebugBreak();} while (false)

enum class JVS {
	Default,
	FourPlayers,
	RotaryEncoders,
	SegaMarineFishing,
	DualIOBoards4P,
	LightGun,
	Mazan,
	GunSurvivor,
	DogWalking,
	TouchDeUno,
	WorldKicks,
	WorldKicksPCB,
	Keyboard,
	OutTrigger,
	LightGunAsAnalog,
	WaveRunnerGP,
	_18Wheeler,
	F355
};

enum class RenderType {
	OpenGL = 0,
	OpenGL_OIT = 3,
	Vulkan = 4,
	Vulkan_OIT = 5,
	DirectX9 = 1,
	DirectX11 = 2,
	DirectX11_OIT = 6,
};

static inline bool isOpenGL(RenderType renderType)  {
	return renderType == RenderType::OpenGL || renderType == RenderType::OpenGL_OIT;
}
static inline bool isVulkan(RenderType renderType) {
	return renderType == RenderType::Vulkan || renderType == RenderType::Vulkan_OIT;
}
static inline bool isDirectX(RenderType renderType) {
	return renderType == RenderType::DirectX9 || renderType == RenderType::DirectX11 || renderType == RenderType::DirectX11_OIT;
}

enum class KeyboardLayout {
	JP = 1,
	US,
	UK,
	GE,
	FR,
	IT,
	SP,
	SW,
	CH,
	NL,
	PT,
	LATAM,
	FR_CA,
	RU,
	CN,
	KO
};

struct settings_t
{
	struct {
		int system;
		u32 ram_size;
		u32 ram_mask;
		u32 vram_size;
		u32 vram_mask;
		u32 aram_size;
		u32 aram_mask;
		u32 bios_size;
		u32 flash_size;

		bool isNaomi1() const { return system == DC_PLATFORM_NAOMI; }
		bool isNaomi2() const { return system == DC_PLATFORM_NAOMI2; }
		bool isNaomi() const { return isNaomi1() || isNaomi2(); }
		bool isAtomiswave() const { return system == DC_PLATFORM_ATOMISWAVE; }
		bool isArcade() const { return !isConsole(); }
		bool isConsole() const { return system == DC_PLATFORM_DREAMCAST; }
		bool isSystemSP() const { return system == DC_PLATFORM_SYSTEMSP; }
	} platform;

	struct {
		int width = 640;
		int height = 480;
		float pointScale = 1.f;
		float refreshRate = 0;
		float dpi = 96.f;
		float uiScale = 1.f;
	} display;

	struct
	{
		bool disable_nvmem;
	} dynarec;

	struct
	{
		bool muteAudio;
	} aica;

	struct
	{
		std::string path;
		std::string gameId;
		std::string fileName;
	} content;

	struct {
		JVS JammaSetup;
		KeyboardLayout keyboardLangId = KeyboardLayout::US;
		bool fastForwardMode;
		bool lightgunGame; // or touchscreen
	} input;

	struct
	{
		bool online;
		struct
		{
			u8 game[16];
			u8 bios[16];
			u8 savestate[16];
			u8 nvmem[16];
			u8 nvmem2[16];
			u8 eeprom[16];
			u8 vmu[16];
		} md5;
	} network;

	struct
	{
		bool multiboard;
		bool slave;
		int drivingSimSlave;
	} naomi;

	bool disableRenderer;
};

extern settings_t settings;

#define RAM_SIZE settings.platform.ram_size
#define RAM_MASK settings.platform.ram_mask
#define ARAM_SIZE settings.platform.aram_size
#define ARAM_MASK settings.platform.aram_mask
#define VRAM_SIZE settings.platform.vram_size
#define VRAM_MASK settings.platform.vram_mask
#define BIOS_SIZE settings.platform.bios_size

struct OnLoad
{
	typedef void OnLoadFP();
	OnLoad(OnLoadFP* fp) { fp(); }
};

class FlycastException : public std::runtime_error
{
public:
	FlycastException(const std::string& reason) : std::runtime_error(reason) {}
};

class LoadCancelledException : public FlycastException
{
public:
	LoadCancelledException() : FlycastException("") {}
};

class Serializer;
class Deserializer;

constexpr size_t operator""_KB(unsigned long long  x)
{
	return 1024 * x;
}

constexpr size_t operator""_MB(unsigned long long  x)
{
	return 1024 * 1024 * x;
}

constexpr size_t operator""_GB(unsigned long long  x)
{
	return 1024 * 1024 * 1024 * x;
}

constexpr u32 RAM_SIZE_MAX = 32_MB;
constexpr u32 VRAM_SIZE_MAX = 16_MB;
constexpr u32 ARAM_SIZE_MAX = 8_MB;

