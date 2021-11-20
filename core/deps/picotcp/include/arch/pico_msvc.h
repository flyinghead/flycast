#ifndef PICO_SUPPORT_MSVC
#define PICO_SUPPORT_MSVC

#pragma pack(push, 8)
#include <stdio.h>
#include <time.h>
#include <windows.h>
#pragma pack(pop)

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

#define dbg printf

#define stack_fill_pattern(...) do {} while(0)
#define stack_count_free_words(...) do {} while(0)
#define stack_get_free_words() (0)

#define pico_zalloc(x) calloc(x, 1)
#define pico_free(x) free(x)

static inline uint32_t PICO_TIME_MS(void)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;

	GetSystemTimeAsFileTime(&ft);

	tmpres |= ft.dwHighDateTime;
	tmpres <<= 32;
	tmpres |= ft.dwLowDateTime;

	tmpres /= 10;  /*convert into microseconds*/
	/*converting file time to unix epoch*/
	tmpres -= DELTA_EPOCH_IN_MICROSECS;

	return (uint32_t)(tmpres / 1000);	// milliseconds
}

static inline uint32_t PICO_TIME(void)
{
	return PICO_TIME_MS() / 1000;
}

static inline void PICO_IDLE(void)
{
    // Not used anyway usleep(5000);
}

#endif  /* PICO_SUPPORT_MSVC */


