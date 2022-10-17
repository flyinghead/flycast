#include <cstdio>
#include <chrono>
#include <thread>
#if _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <time.h>
#endif

#include "sleep.h"

#if _WIN32
static NTSTATUS(__stdcall* NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval) = (NTSTATUS(__stdcall*)(BOOL, PLARGE_INTEGER)) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtDelayExecution");
static NTSTATUS(__stdcall* ZwSetTimerResolution)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution) = (NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG)) GetProcAddress(GetModuleHandle("ntdll.dll"), "ZwSetTimerResolution");
#endif

void init_timer_resolution()
{
#if _WIN32
	ULONG actual_resolution;
	ZwSetTimerResolution(1, true, &actual_resolution);
#endif
	// FIXME: Optimize for other platforms
}

void sleep_us(int64_t us)
{
#if _WIN32
	LARGE_INTEGER interval;
	interval.QuadPart = -us * 10;
	NtDelayExecution(false, &interval);
#else
	timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = us * 1000;
	while (nanosleep(&ts, &ts));
#endif
}

int64_t sleep_and_spinlock(int64_t us)
{
	const auto t1 = std::chrono::steady_clock::now();
#if _WIN32
	if (1200 < us) sleep_us(us - 1200);
#else
	if (4000 < us) sleep_us(us - 4000);
#endif

	if (0 < us) {
		for (;;) {
			const auto t2 = std::chrono::steady_clock::now();
			const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
			if (us <= duration) return duration - us;
		}
	}

	return 0;
}
