/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _TYPES_H
#define _TYPES_H
/*
 * Keep the compiler happy
 */

#ifdef _MSC_VER
/*
 * Disable specific compiler warnings
 *   4018 - '<' : signed/unsigned mismatch
 *   4100 - 'xxx' : unreferenced formal parameter
 *   4127 - conditional expression is constant
 *   4201 - nonstandard extension used : nameless struct/union
 *   4389 - '!=' : signed/unsigned mismatch
 *   4800 - 'int' : forcing value to bool 'true' or 'false' (performance warning)
 */
#pragma warning(disable: 4018 4100 4127 4201 4389 4800)
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#define TARGET_UWP
#endif

/*
 * Simple types
 */
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned char byte;
typedef char int8;
typedef short int16;
typedef int int32;

#include "ggponet.h"
#include "log/Log.h"
#include <stdexcept>

class GGPOException : public std::runtime_error {
public:
	GGPOException(const char *what, GGPOErrorCode ggpoError)
		: std::runtime_error(what), ggpoError(ggpoError) {}

	GGPOErrorCode ggpoError;
};

/*
 * Additional headers
 */
#if defined(_WIN32)
#  include "platform_windows.h"
#elif defined(__unix__) || defined(__APPLE__) || defined(__SWITCH__) || defined(__vita__)
#  include "platform_linux.h"
#else
#  error Unsupported platform
#endif

/*
 * Macros
 */
#define ASSERT(x)                                           \
   do {                                                     \
      if (!(x)) {                                           \
         char assert_buf[1024];                             \
         snprintf(assert_buf, sizeof(assert_buf) - 1, "Assertion: %s @ %s:%d", #x, __FILE__, __LINE__); \
         GGPOPlatform::AssertFailed(assert_buf);            \
         throw GGPOException(assert_buf, GGPO_ERRORCODE_GENERAL_FAILURE); \
      }                                                     \
   } while (false)

#define Log(...) DEBUG_LOG(NETWORK, __VA_ARGS__)

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef MAX_INT
#  define MAX_INT          0xEFFFFFF
#endif

#ifndef MAX
#  define MAX(x, y)        (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#  define MIN(x, y)        (((x) < (y)) ? (x) : (y))
#endif

#endif // _TYPES_H
