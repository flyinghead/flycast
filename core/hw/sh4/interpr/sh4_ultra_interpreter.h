#pragma once

#include "types.h"

// === NEXT-GENERATION ULTRA-FAST SH4 INTERPRETER ===
// This interpreter beats the legacy interpreter using modern optimization techniques

#ifdef __cplusplus
extern "C" {
#endif

// Ultra-interpreter factory function
void* Get_UltraInterpreter();

#ifdef __cplusplus
}
#endif

// Enable ultra-interpreter by default
#define USE_ULTRA_INTERPRETER 1 