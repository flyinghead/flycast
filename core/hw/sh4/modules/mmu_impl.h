#pragma once
#include "types.h"
#include "ccn.h"
#include "mmu.h"

void MMU_Init();
void MMU_Reset(bool Manual);
void MMU_Term();

template<u32 translation_type> u32 mmu_full_SQ(u32 va, u32& rv);
