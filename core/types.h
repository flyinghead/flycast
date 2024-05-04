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
#include <pthread.h>
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

#ifndef _MSC_VER
#define stricmp strcasecmp
#endif

[[noreturn]] void os_DebugBreak();
void fatal_error(const char* text, ...);

#define verify(x) do { if ((x) == false){ fatal_error("Verify Failed  : " #x "\n in %s -> %s : %d", (__FUNCTION__), (__FILE__), __LINE__); os_DebugBreak();}} while (false)
#define die(reason) do { fatal_error("Fatal error : %s\n in %s -> %s : %d", (reason), (__FUNCTION__), (__FILE__), __LINE__); os_DebugBreak();} while (false)

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
		std::string title;
	} content;

	struct {
		KeyboardLayout keyboardLangId = KeyboardLayout::US;
		bool fastForwardMode;
		// The following flags are only set for arcade games
		bool lightgunGame; // or touchscreen
		bool keyboardGame;
		bool mouseGame; // or rotary encoders
		bool fourPlayerGames;
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

	bool raHardcoreMode;
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

