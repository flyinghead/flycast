#pragma once

#include "build.h"

#ifndef _MSC_VER
#ifndef __forceinline
#define __forceinline inline
#endif
#ifndef _WIN32
#define __debugbreak
#endif
#endif


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
#include <cstring>

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

#ifdef _M_X64
#undef X86
#define X64
#endif

typedef size_t unat;

#ifdef X64
typedef u64 unat;
#endif

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


#include "nowide/cstdlib.hpp"
#include "nowide/cstdio.hpp"

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
#include <vector>
#include <string>
#include <map>

#define INLINE __forceinline

//no inline -- fixme
#ifdef _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__ ((noinline))
#endif

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
#define VER_FULLNAME	VER_EMUNAME " (built " __DATE__ "@" __TIME__ ")"
#define VER_SHORTNAME	VER_EMUNAME

void os_DebugBreak();
#define dbgbreak os_DebugBreak()

#ifndef _MSC_VER
#define stricmp strcasecmp
#endif

int msgboxf(const char* text, unsigned int type, ...);

#define MBX_OK                       0
#define MBX_ICONEXCLAMATION          0
#define MBX_ICONERROR                0

#define verify(x) do { if ((x) == false){ msgboxf("Verify Failed  : " #x "\n in %s -> %s : %d", MBX_ICONERROR, (__FUNCTION__), (__FILE__), __LINE__); dbgbreak;}} while (false)
#define die(reason) do { msgboxf("Fatal error : %s\n in %s -> %s : %d", MBX_ICONERROR,(reason), (__FUNCTION__), (__FILE__), __LINE__); dbgbreak;} while (false)


//will be removed sometime soon
//This shit needs to be moved to proper headers
typedef u32  RegReadAddrFP(u32 addr);
typedef void RegWriteAddrFP(u32 addr, u32 data);

/*
	Read Write Const
	D    D     N      -> 0			-> RIO_DATA
	D    F     N      -> WF			-> RIO_WF
	F    F     N      -> RF|WF		-> RIO_FUNC
	D    X     N      -> RO|WF		-> RIO_RO
	F    X     N      -> RF|WF|RO	-> RIO_RO_FUNC
	D    X     Y      -> CONST|RO|WF-> RIO_CONST
	X    F     N      -> RF|WF|WO	-> RIO_WO_FUNC
*/
enum RegStructFlags
{
	REG_RF=8,
	REG_WF=16,
	REG_RO=32,
	REG_WO=64,
	REG_NO_ACCESS=REG_RO|REG_WO,
};

enum RegIO
{
	RIO_DATA = 0,
	RIO_WF = REG_WF,
	RIO_FUNC = REG_WF | REG_RF,
	RIO_RO = REG_RO | REG_WF,
	RIO_RO_FUNC = REG_RO | REG_RF | REG_WF,
	RIO_CONST = REG_RO | REG_WF,
	RIO_WO_FUNC = REG_WF | REG_RF | REG_WO,
	RIO_NO_ACCESS = REG_WF | REG_RF | REG_NO_ACCESS
};

struct RegisterStruct
{
	union
	{
		u32 data32;					//stores data of reg variable [if used] 32b
		u16 data16;					//stores data of reg variable [if used] 16b
		u8  data8;					//stores data of reg variable [if used]	8b

		RegReadAddrFP* readFunctionAddr; //stored pointer to reg read function
	};

	RegWriteAddrFP* writeFunctionAddr; //stored pointer to reg write function

	u32 flags;					//Access flags !

	void reset()
	{
		if (!(flags & (REG_RO | REG_RF)))
			data32 = 0;
	}
};

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
};

enum class RenderType {
	OpenGL = 0,
	OpenGL_OIT = 3,
	Vulkan = 4,
	Vulkan_OIT = 5,
	DirectX9 = 1,
};

static inline bool isOpenGL(RenderType renderType)  {
	return renderType == RenderType::OpenGL || renderType == RenderType::OpenGL_OIT;
}
static inline bool isVulkan(RenderType renderType) {
	return renderType == RenderType::Vulkan || renderType == RenderType::Vulkan_OIT;
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
	} platform;

	struct {
		int width = 640;
		int height = 480;
		float pointScale = 1.f;
		float refreshRate = 0;
	} display;

	struct
	{
		bool disable_nvmem;
	} dynarec;

	struct
	{
		bool NoBatch;
		bool muteAudio;
	} aica;

	struct
	{
		std::string path;
		std::string gameId;
	} content;

	struct {
		JVS JammaSetup;
		KeyboardLayout keyboardLangId = KeyboardLayout::US;
		bool fastForwardMode;
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

inline bool is_s8(u32 v) { return (s8)v==(s32)v; }
inline bool is_u8(u32 v) { return (u8)v==(s32)v; }
inline bool is_s16(u32 v) { return (s16)v==(s32)v; }
inline bool is_u16(u32 v) { return (u16)v==(u32)v; }

// 0x00600000 - 0x006007FF [NAOMI] (modem area for dreamcast)
u32  libExtDevice_ReadMem_A0_006(u32 addr,u32 size);
void libExtDevice_WriteMem_A0_006(u32 addr,u32 data,u32 size);

//Area 0 , 0x01000000- 0x01FFFFFF	[Ext. Device]
static inline u32 libExtDevice_ReadMem_A0_010(u32 addr,u32 size) { return 0; }
static inline void libExtDevice_WriteMem_A0_010(u32 addr,u32 data,u32 size) { }

//Area 5
static inline u32 libExtDevice_ReadMem_A5(u32 addr,u32 size){ return 0; }
static inline void libExtDevice_WriteMem_A5(u32 addr,u32 data,u32 size) { }

//ARM
s32 libARM_Init();
void libARM_Reset(bool hard);
void libARM_Term();

template<u32 sz>
u32 ReadMemArr(const u8 *array, u32 addr)
{
	switch(sz)
	{
	case 1:
		return array[addr];
	case 2:
		return *(const u16 *)&array[addr];
	case 4:
		return *(const u32 *)&array[addr];
	default:
		die("invalid size");
		return 0;
	}
}

template<u32 sz>
void WriteMemArr(u8 *array, u32 addr, u32 data)
{
	switch(sz)
	{
	case 1:
		array[addr] = data;
		break;
	case 2:
		*(u16 *)&array[addr] = data;
		break;
	case 4:
		*(u32 *)&array[addr] = data;
		break;
	default:
		die("invalid size");
		break;
	}
}

struct OnLoad
{
	typedef void OnLoadFP();
	OnLoad(OnLoadFP* fp) { fp(); }
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

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
