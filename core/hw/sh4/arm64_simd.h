#pragma once
// Simple Arm-NEON helpers used by SH4 interpreter fast paths
#if defined(__aarch64__)
#include <arm_neon.h>

// Multiply-accumulate 4 pairs of signed 16-bit values and return 64-bit sum
static inline int64_t mac_w_4x(const uint16_t* __restrict pA,
                               const uint16_t* __restrict pB)
{
    int16x4_t a = vld1_s16((const int16_t*)pA);   // load 4 halfwords
    int16x4_t b = vld1_s16((const int16_t*)pB);
    int32x4_t prod = vmull_s16(a, b);             // 4 × 32-bit products
    // add horizontally: pairwise add, then widen to 64-bit and add again
    int64x2_t sum2 = vpaddlq_s32(vcombine_s32(vget_low_s32(prod),
                                             vget_high_s32(prod)));
    return vgetq_lane_s64(sum2, 0) + vgetq_lane_s64(sum2, 1);
}
#endif // __aarch64__
