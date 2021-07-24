#pragma once
#include "types.h"

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
struct _RUNTIME_FUNCTION;
#endif
class UnwindInfo
{
public:
	void start(void *address);
	void pushReg(u32 offset, int reg);
	void pushFPReg(u32 offset, int reg);
	void allocStack(u32 offset, int size);
	void endProlog(u32 offset);
	size_t end(u32 offset);

	void clear();

private:
	u8 *startAddr;
#ifdef _WIN64
	std::vector<_RUNTIME_FUNCTION *> tables;
	std::vector<u16> codes;
#endif
#if defined(__unix__) || defined(__APPLE__)
	int stackOffset = 0;
	u64 lastOffset = 0;
	std::vector<u8> cieInstructions;
	std::vector<u8> fdeInstructions;
	std::vector<u8 *> registeredFrames;
#endif
};

#if !defined(_WIN64) && !defined(__unix__) && !defined(__APPLE__)
inline void UnwindInfo::start(void *address) {
}
inline void UnwindInfo::pushReg(u32 offset, int reg) {
}
inline void UnwindInfo::pushFPReg(u32 offset, int reg) {
}
inline void UnwindInfo::allocStack(u32 offset, int size) {
}
inline void UnwindInfo::endProlog(u32 offset) {
}
inline size_t UnwindInfo::end(u32 offset) {
	return 0;
}
inline void UnwindInfo::clear() {
}
#endif
