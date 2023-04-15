#pragma once
//#define STRICT_MODE
#ifndef STRICT_MODE
#define FAST_MMU
#define USE_WINCE_HACK
#endif

#define DC_PLATFORM_DREAMCAST   0
#define DC_PLATFORM_DEV_UNIT    1
#define DC_PLATFORM_NAOMI       2
#define DC_PLATFORM_NAOMI2      3
#define DC_PLATFORM_ATOMISWAVE  4

//HOST_CPU
#define CPU_X86      0x20000001
#define CPU_ARM      0x20000002
#define CPU_MIPS     0x20000003
#define CPU_X64      0x20000004
#define CPU_GENERIC  0x20000005 //used for pnacl, emscripten, etc
#define CPU_PPC      0x20000006
#define CPU_PPC64    0x20000007
#define CPU_ARM64    0x20000008

//FEAT_SHREC, FEAT_AREC, FEAT_DSPREC
#define DYNAREC_NONE	0x40000001
#define DYNAREC_JIT		0x40000002

//automatic

#if defined(__x86_64__) || defined(_M_X64)
	#define HOST_CPU CPU_X64
#elif defined(__i386__) || defined(_M_IX86)
	#define HOST_CPU CPU_X86
#elif defined(__arm__) || defined (_M_ARM)
	#define HOST_CPU CPU_ARM
#elif defined(__aarch64__) || defined(_M_ARM64)
	#define HOST_CPU CPU_ARM64
#elif defined(__mips__)
	#define HOST_CPU CPU_MIPS
#else
	#define HOST_CPU CPU_GENERIC
#endif

#if defined(TARGET_NO_REC)
#define FEAT_SHREC DYNAREC_NONE
#define FEAT_AREC DYNAREC_NONE
#define FEAT_DSPREC DYNAREC_NONE
#endif

#if defined(TARGET_NO_AREC)
#define FEAT_SHREC DYNAREC_JIT
#define FEAT_AREC DYNAREC_NONE
#define FEAT_DSPREC DYNAREC_NONE
#endif

#ifdef __SWITCH__
#define FEAT_NO_RWX_PAGES
#endif

//defaults

#ifndef FEAT_SHREC
	#if HOST_CPU == CPU_ARM || HOST_CPU == CPU_ARM64 || HOST_CPU == CPU_X86 || HOST_CPU == CPU_X64
		#define FEAT_SHREC DYNAREC_JIT
	#else
		#define FEAT_SHREC DYNAREC_NONE
	#endif
#endif

#ifndef FEAT_AREC
	#if HOST_CPU == CPU_ARM || HOST_CPU == CPU_ARM64 || HOST_CPU == CPU_X64
		#define FEAT_AREC DYNAREC_JIT
	#else
		#define FEAT_AREC DYNAREC_NONE
	#endif
#endif

#ifndef FEAT_DSPREC
	#if HOST_CPU == CPU_ARM || HOST_CPU == CPU_ARM64 || HOST_CPU == CPU_X86 || HOST_CPU == CPU_X64
		#define FEAT_DSPREC DYNAREC_JIT
	#else
		#define FEAT_DSPREC DYNAREC_NONE
	#endif
#endif

// Some restrictions on FEAT_NO_RWX_PAGES
#if defined(FEAT_NO_RWX_PAGES) && FEAT_SHREC == DYNAREC_JIT
#if HOST_CPU != CPU_X64 && HOST_CPU != CPU_ARM64
#error "FEAT_NO_RWX_PAGES Only implemented for X64 and ARMv8"
#endif
#endif

#ifdef _WIN32
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#define TARGET_UWP
#endif
#ifdef HAVE_D3D11
#define USE_DX11
#endif
#endif

#if !defined(LIBRETRO) && !defined(TARGET_NO_EXCEPTIONS)
#define USE_GGPO
#endif

#if !defined(__ANDROID__) && !defined(TARGET_IPHONE) && !defined(TARGET_UWP) \
	&& !defined(__SWITCH__) && !defined(LIBRETRO) && !defined(__NetBSD__) && !defined(__OpenBSD__)
#define NAOMI_MULTIBOARD
#endif

// TARGET PLATFORM

#define RAM_SIZE_MAX (32*1024*1024)
#define VRAM_SIZE_MAX (16*1024*1024)
#define ARAM_SIZE_MAX (8*1024*1024)

#define GD_CLOCK 33868800						//GDROM XTAL -- 768fs
#define AICA_CORE_CLOCK (GD_CLOCK * 4 / 3)		//[45158400]  GD->PLL 3:4 -> AICA CORE	 -- 1024fs
#define AICA_ARM_CLOCK (AICA_CORE_CLOCK / 2)	//[22579200]  AICA CORE -> PLL 2:1 -> ARM
#define SH4_MAIN_CLOCK (200 * 1000 * 1000)		//[200000000] XTal(13.5) -> PLL (33.3) -> PLL 1:6 (200)
#define G2_BUS_CLOCK (25 * 1000 * 1000)			//[25000000]  from Holly, from SH4_RAM_CLOCK w/ 2 2:1 plls

#if defined(GLES) && !defined(GLES3) && !defined(GLES2)
// Only use GL ES 2.0 API functions
#define GLES2
#endif
