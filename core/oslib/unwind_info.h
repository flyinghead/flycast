#pragma once
#include "types.h"
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

class UnwindInfo
{
public:
	virtual ~UnwindInfo() = default;
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

protected:
	virtual void registerFrame(void *frame);
	virtual void deregisterFrame(void *frame);

private:
	u8 *startAddr;
#ifdef _M_X64
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

#if HOST_CPU != CPU_X64 \
	&& (HOST_CPU != CPU_ARM64 || defined(_WIN32))	\
	&& (HOST_CPU != CPU_X86 || defined(_WIN32))		\
	&& (HOST_CPU != CPU_ARM || !defined(__ANDROID__))
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
inline void UnwindInfo::registerFrame(void *frame) {
}
inline void UnwindInfo::deregisterFrame(void *frame) {
}
#endif
