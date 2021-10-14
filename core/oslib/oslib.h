#pragma once
#include "types.h"
#if defined(__SWITCH__)
#include <malloc.h>
#endif

void os_SetWindowText(const char* text);
double os_GetSeconds();

void os_DoEvents();
void os_CreateWindow();
void os_SetupInput();
void os_InstallFaultHandler();
void os_UninstallFaultHandler();

#ifdef _MSC_VER
#include <intrin.h>
#endif

u32 static INLINE bitscanrev(u32 v)
{
#ifdef __GNUC__
	return 31-__builtin_clz(v);
#else
	unsigned long rv;
	_BitScanReverse(&rv,v);
	return rv;
#endif
}

namespace hostfs
{
	std::string getVmuPath(const std::string& port);

	std::string getArcadeFlashPath();

	std::string findFlash(const std::string& prefix, const std::string& names);
	std::string getFlashSavePath(const std::string& prefix, const std::string& name);
	std::string findNaomiBios(const std::string& name);

	std::string getSavestatePath(int index, bool writable);

	std::string getTextureLoadPath(const std::string& gameId);
	std::string getTextureDumpPath();

	std::string getVulkanCachePath();

	std::string getBiosFontPath();
}

#ifdef _WIN64
#ifdef __MINGW64__
struct _RUNTIME_FUNCTION;
typedef struct _RUNTIME_FUNCTION RUNTIME_FUNCTION;
#else
struct _IMAGE_RUNTIME_FUNCTION_ENTRY;
typedef struct _IMAGE_RUNTIME_FUNCTION_ENTRY RUNTIME_FUNCTION;
#endif
#endif

class UnwindInfo
{
public:
	void start(void *address);
	void pushReg(u32 offset, int reg);
	void saveReg(u32 offset, int reg, int stackOffset);
	void saveExtReg(u32 offset, int reg, int stackOffset);
	void allocStack(u32 offset, int size);
	void endProlog(u32 offset);
	size_t end(u32 offset, ptrdiff_t rwRxOffset = 0);

	void clear();
	void allocStackPtr(const void *address, int size) {
		allocStack((u32)((const u8 *)address - startAddr), size);
	}

private:
	u8 *startAddr;
#ifdef _WIN64
	std::vector<RUNTIME_FUNCTION *> tables;
	std::vector<u16> codes;
#endif
#if defined(__unix__) || defined(__APPLE__) || defined(__SWITCH__)
	int stackOffset = 0;
	uintptr_t lastOffset = 0;
	std::vector<u8> cieInstructions;
	std::vector<u8> fdeInstructions;
	std::vector<u8 *> registeredFrames;
#endif
};

#if HOST_CPU != CPU_X64 && HOST_CPU != CPU_ARM64 && (HOST_CPU != CPU_X86 || defined(_WIN32))
inline void UnwindInfo::start(void *address) {
}
inline void UnwindInfo::pushReg(u32 offset, int reg) {
}
inline void UnwindInfo::saveReg(u32 offset, int reg, int stackOffset) {
}
inline void UnwindInfo::saveExtReg(u32 offset, int reg, int stackOffset) {
}
inline void UnwindInfo::allocStack(u32 offset, int size) {
}
inline void UnwindInfo::endProlog(u32 offset) {
}
inline size_t UnwindInfo::end(u32 offset, ptrdiff_t rwRxOffset) {
	return 0;
}
inline void UnwindInfo::clear() {
}
#endif


static inline void *allocAligned(size_t alignment, size_t size)
{
#ifdef _WIN32
	return _aligned_malloc(size, alignment);
#elif defined(__SWITCH__)
   return memalign(alignment, size);
#else
	void *data;
	if (posix_memalign(&data, alignment, size) != 0)
		return nullptr;
	else
		return data;
#endif
}

static inline void freeAligned(void *p)
{
#ifdef _WIN32
	_aligned_free(p);
#else
	free(p);
#endif
}

