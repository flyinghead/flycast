#include "sleep.h"

#include <cstdio>
#include <chrono>
#include <thread>
#if _WIN32
#include <windows.h>
#elif __APPLE__
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#else
#include <sched.h>
#include <time.h>
#endif

#include "log/Log.h"

#if _WIN32
static NTSTATUS(__stdcall* NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval) = (NTSTATUS(__stdcall*)(BOOL, PLARGE_INTEGER)) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtDelayExecution");
static NTSTATUS(__stdcall* ZwSetTimerResolution)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution) = (NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG)) GetProcAddress(GetModuleHandle("ntdll.dll"), "ZwSetTimerResolution");
#endif

void set_timer_resolution()
{
#if _WIN32
	ULONG actual_resolution;
	ZwSetTimerResolution(1, true, &actual_resolution);
#elif __APPLE__
	thread_port_t mach_thread_id = pthread_mach_thread_np(pthread_self());

	// Make thread fixed priority
	thread_extended_policy_data_t extended_policy;
	extended_policy.timeshare = 0;
	kern_return_t kr = thread_policy_set(mach_thread_id, THREAD_EXTENDED_POLICY,
						   (thread_policy_t)&extended_policy,
						   THREAD_EXTENDED_POLICY_COUNT);
	if (kr != KERN_SUCCESS) {
		ERROR_LOG(COMMON, "Cannot make thread fixed priority: %d", kr);
		return;
	}

	// Set to relatively high priority.
	thread_precedence_policy_data_t precedence_policy;
	precedence_policy.importance = 63;
	kr = thread_policy_set(mach_thread_id, THREAD_PRECEDENCE_POLICY,
						   (thread_policy_t)&precedence_policy,
							 THREAD_PRECEDENCE_POLICY_COUNT);
	if (kr != KERN_SUCCESS) {
		ERROR_LOG(COMMON, "Cannot set high priority: %d", kr);
		return;
	}

	mach_timebase_info_data_t timebase;
	kr = mach_timebase_info(&timebase);
	if (kr != KERN_SUCCESS)
	{
		ERROR_LOG(COMMON, "Couldn't get timebase: %d", kr);
		return;
	}
	static double clock2abs = ((double)timebase.denom / (double)timebase.numer) * USEC_PER_SEC;

	// Set the thread priority.
	thread_time_constraint_policy tc_policy;
	tc_policy.period = 0;
	tc_policy.computation = 50 * clock2abs;
	tc_policy.constraint = 100 * clock2abs;
	tc_policy.preemptible = FALSE;

	kr = thread_policy_set(mach_thread_id, THREAD_TIME_CONSTRAINT_POLICY,
						   (thread_policy_t)&tc_policy,
						   THREAD_TIME_CONSTRAINT_POLICY_COUNT);
	if (kr != KERN_SUCCESS)
	{
		ERROR_LOG(COMMON, "Could not set thread policy: %d", kr);
	}
#endif
}

void reset_timer_resolution()
{
#if _WIN32
	ULONG actual_resolution;
	ZwSetTimerResolution(1, false, &actual_resolution);
#endif
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

int64_t sleep_and_busy_wait(int64_t us)
{
	const auto t1 = std::chrono::steady_clock::now();
#if defined(_WIN32) || defined(__APPLE__)
	if (1200 < us) sleep_us(us - 1200);
#else
	// FIXME: Optimize for other platforms
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
