#pragma once
#include "types.h"
#include "ccn.h"
#include "mmu.h"


//Do a full lookup on the UTLB entry's
//Return Values
//Translation was sucessfull , rv contains return
#define MMU_ERROR_NONE	   0
//TLB miss
#define MMU_ERROR_TLB_MISS 1
//TLB Multihit
#define MMU_ERROR_TLB_MHIT 2
//Mem is read/write protected (depends on translation type)
#define MMU_ERROR_PROTECTED 3
//Mem is write protected , firstwrite
#define MMU_ERROR_FIRSTWRITE 4
//data-Opcode read/write missasligned
#define MMU_ERROR_BADADDR 5
//Can't Execute
#define MMU_ERROR_EXECPROT 6

extern u32 mmu_error_TT;

void MMU_Init();
void MMU_Reset(bool Manual);
void MMU_Term();
