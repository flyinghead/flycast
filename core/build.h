/*
	reicast build options

		Reicast can support a lot of stuff, and this is an attempt
		to organize the build time options

		Option categories

			BUILD_* - BUILD_COMPILER, etc...
				definitions about the build machine

			HOST_*	
				definitions about the host machine

			FEAT_*
				definitions about the features that this build targets
				This is higly related to HOST_*, but it's for options that might
				or might not be avaiable depending on the target host, or that 
				features that are irrelevant of	the host

				Eg, Alsa, Pulse Audio and OSS might sense as HOST dedinitions
				but it usually makes more sense to detect them as runtime. In
				that context, HOST_ALSA makes no sense because the host might
				or might not have alsa installed/ running

				MMU makes no sense as a HOST definition at all, so it should
				be FEAT_HAS_MMU

			TARGET_*
				A preconfigured default. Eg TARGET_WIN86. 

		Naming of options, option values, and how to use them
			
			for options that makes sense to have a list of values
				{CATEGORY}_{OPTION} 
				{OPTION}_{VALUE}

				eg.
				BUILD_COMPILER == COMPILER_GCC, HOST_CPU != CPU_X64, ...

			for options that are boolean
				{CATEGORY}_IS_{OPTION} or {CATEGORY}_HAS_{OPTION} 
				
				Evaluates to 0 or 1

			If an configuration cannot be neatly split into a set of
			of orthogonal options, then it makes sense to break things
			to "sets" or have a hierarchy of options. 

			Example
			-------
			
			In the beggining it made sense to have an audio backend
			per operating system. It made sense to have it depend 
			on HOST_OS and seleect DirectSound or alsa. 

				// no option needed

			Then, as android was introduced, which also uses OS_LINUX
			atm, the audio could have been made an option. It could be
			a HOST_* option, or FEAT_* one. I'd prefer FEAT_*. 
			FEAT_* makes more sense as future wise we might want
			to support multiple backends.
				
				FEAT_AUDIO_BACKEND
					AUDIO_BACKEND_NONE
					AUDIO_BACKEND_DS
					AUDIO_BACKEND_ALSA
					AUDIO_BACKEND_ANDROID

				Used like
					#if FEAT_AUDIO_BACKEND == AUDIO_BACKEND_DS ....
			
			At some point, we might have multiple audio backends that
			can be compiled in and autodetected/selected at runtime.
			In that case, it might make sense to have the options like

					FEAT_HAS_ALSA
					FEAT_HAS_DIRECTSOUND
					FEAT_HAS_ANDROID_AUDIO

				or 
					FEAT_HAS_AUDIO_ALSA
					FEAT_HAS_AUDIO_DS
					FEAT_HAS_AUDIO_ANDROID

				The none option might or might not make sense. In this
				case it can be removed, as it should always be avaiable.

			Guidelines
			----------

			General rule of thumb, don't overcomplicate things. Start
			with a simple option, and then make it more complicated
			as new uses apply (see the example above)

			Don't use too long names, don't use too cryptic names. 
			Most team developers should be able to understand or
			figure out most of the acronyms used. 

			Try to be consistent on the acronyms across all definitions

			Code shouldn't depend on build level options whenever possible

			Generally, the file should compile even if the option/module is
			disabled. This makes makefiles etc much easier to write

			TARGET_* options should generally only be used in this file

			The current source is *not* good example of these guidelines

		We'll try to be smart and figure out some options/defaults on this file
		but this shouldn't get too complicated


*/

//#define STRICT_MODE
#ifndef STRICT_MODE
#define FAST_MMU
#define USE_WINCE_HACK
#endif

#define DC_PLATFORM_MASK        7
#define DC_PLATFORM_DREAMCAST   0   /* Works, for the most part */
#define DC_PLATFORM_DEV_UNIT    1   /* This is missing hardware */
#define DC_PLATFORM_NAOMI       2   /* Works, for the most part */ 
#define DC_PLATFORM_NAOMI2      3   /* Needs to be done, 2xsh4 + 2xpvr + custom TNL */
#define DC_PLATFORM_ATOMISWAVE  4   /* Works, for the most part */
#define DC_PLATFORM_HIKARU      5   /* Needs to be done, 2xsh4, 2x aica , custom vpu */
#define DC_PLATFORM_AURORA      6   /* Needs to be done, Uses newer 300 mhz sh4 + 150 mhz pvr mbx SoC */

//HOST_CPU
#define CPU_X86      0x20000001
#define CPU_ARM      0x20000002
#define CPU_MIPS     0x20000003
#define CPU_X64      0x20000004
#define CPU_GENERIC  0x20000005 //used for pnacl, emscripten, etc
#define CPU_PPC      0x20000006
#define CPU_PPC64    0x20000007
#define CPU_ARM64    0x20000008
#define CPU_MIPS64   0x20000009

//FEAT_SHREC, FEAT_AREC, FEAT_DSPREC
#define DYNAREC_NONE	0x40000001
#define DYNAREC_JIT		0x40000002
#define DYNAREC_CPP		0x40000003


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

#if defined(TARGET_NO_JIT)
#define FEAT_SHREC DYNAREC_CPP
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
#if !defined(LIBRETRO) && !defined(TARGET_UWP)
#define USE_DX9
#endif
#endif


#if !defined(LIBRETRO) && !defined(TARGET_NO_EXCEPTIONS)
#define USE_GGPO
#endif

// TARGET PLATFORM

#define RAM_SIZE_MAX (32*1024*1024)
#define VRAM_SIZE_MAX (16*1024*1024)
#define ARAM_SIZE_MAX (8*1024*1024)

#define GD_CLOCK 33868800				//GDROM XTAL -- 768fs

#define AICA_CORE_CLOCK (GD_CLOCK*4/3)		//[45158400]  GD->PLL 3:4 -> AICA CORE	 -- 1024fs
#define ADAC_CLOCK (AICA_CORE_CLOCK/4)		//[11289600]  44100*256, AICA CORE -> PLL 4:1 -> ADAC -- 256fs
#define AICA_ARM_CLOCK (AICA_CORE_CLOCK/2)	//[22579200]  AICA CORE -> PLL 2:1 -> ARM
#define AICA_SDRAM_CLOCK (GD_CLOCK*2)		//[67737600]  GD-> PLL 2 -> SDRAM
#define SH4_MAIN_CLOCK (200*1000*1000)		//[200000000] XTal(13.5) -> PLL (33.3) -> PLL 1:6 (200)
#define SH4_RAM_CLOCK (100*1000*1000)		//[100000000] XTal(13.5) -> PLL (33.3) -> PLL 1:3 (100)	, also suplied to HOLLY chip
#define G2_BUS_CLOCK (25*1000*1000)			//[25000000]  from Holly, from SH4_RAM_CLOCK w/ 2 2:1 plls

#if defined(GLES) && !defined(GLES3) && !defined(GLES2)
// Only use GL ES 2.0 API functions
#define GLES2
#endif
