//gah , ccn emulation
//CCN: Cache and TLB controller

#include "ccn.h"
#include "mmu.h"
#include "hw/mem/_vmem.h"
#include "hw/mem/vmem32.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_cache.h"

//Types

u32 CCN_QACR_TR[2];

template<u32 idx>
void CCN_QACR_write(u32 addr, u32 value)
{
	if (idx == 0)
		CCN_QACR0.reg_data = value;
	else
		CCN_QACR1.reg_data = value;

	u32 area = ((CCN_QACR_type&)value).Area;

	CCN_QACR_TR[idx] = (area << 26) - 0xE0000000; //-0xE0000000 because 0xE0000000 is added on the translation again ...

	switch (area)
	{
		case 3: 
			if (_nvmem_enabled())
				do_sqw_nommu = &do_sqw_nommu_area_3;
			else
				do_sqw_nommu = &do_sqw_nommu_area_3_nonvmem;
		break;

		case 4:
			do_sqw_nommu = &TAWriteSQ;
			break;

		default:
			do_sqw_nommu = &do_sqw_nommu_full;
			break;
	}
}

void CCN_PTEH_write(u32 addr, u32 value)
{
	CCN_PTEH_type temp;
	temp.reg_data = value;
	if (temp.ASID != CCN_PTEH.ASID && vmem32_enabled())
		vmem32_flush_mmu();

	CCN_PTEH = temp;
}

void CCN_MMUCR_write(u32 addr, u32 value)
{
	CCN_MMUCR_type temp;
	temp.reg_data=value;

	bool mmu_changed_state = temp.AT != CCN_MMUCR.AT;

	if (temp.TI != 0)
	{
		//sh4_cpu.ResetCache();
		mmu_flush_table();
		if (vmem32_enabled())
			vmem32_flush_mmu();

		temp.TI = 0;
	}
	CCN_MMUCR=temp;

	if (mmu_changed_state)
	{
		//printf("<*******>MMU Enabled , ONLY SQ remaps work<*******>\n");
		sh4_cpu.ResetCache();
		mmu_set_state();
	}
}
void CCN_CCR_write(u32 addr, u32 value)
{
	CCN_CCR_type temp;
	temp.reg_data=value;

	if (temp.ICI) {
		DEBUG_LOG(SH4, "Sh4: i-cache invalidation %08X", curr_pc);
		//Shikigami No Shiro II uses ICI frequently
		if (!config::DynarecEnabled)
			icache.Invalidate();
		temp.ICI = 0;
	}
	if (temp.OCI) {
		DEBUG_LOG(SH4, "Sh4: o-cache invalidation %08X", curr_pc);
		if (!config::DynarecEnabled)
			ocache.Invalidate();
		temp.OCI = 0;
	}

	CCN_CCR=temp;
}

static u32 CPU_VERSION_read(u32 addr)
{
	return 0x040205c1;	// this is what a real SH7091 in a Dreamcast returns - the later Naomi BIOSes check and care!
}

static u32 CCN_PRR_read(u32 addr)
{
	return 0;
}

//Init/Res/Term
void ccn_init()
{
	//CCN PTEH 0xFF000000 0x1F000000 32 Undefined Undefined Held Held Iclk
	sh4_rio_reg(CCN,CCN_PTEH_addr,RIO_WF,32,0,&CCN_PTEH_write);

	//CCN PTEL 0xFF000004 0x1F000004 32 Undefined Undefined Held Held Iclk
	sh4_rio_reg(CCN,CCN_PTEL_addr,RIO_DATA,32);

	//CCN TTB 0xFF000008 0x1F000008 32 Undefined Undefined Held Held Iclk
	sh4_rio_reg(CCN,CCN_TTB_addr,RIO_DATA,32);

	//CCN TEA 0xFF00000C 0x1F00000C 32 Undefined Held Held Held Iclk
	sh4_rio_reg(CCN,CCN_TEA_addr,RIO_DATA,32);

	//CCN MMUCR 0xFF000010 0x1F000010 32 0x00000000 0x00000000 Held Held Iclk
	sh4_rio_reg(CCN,CCN_MMUCR_addr,RIO_WF,32,0,&CCN_MMUCR_write);

	//CCN BASRA 0xFF000014 0x1F000014 8 Undefined Held Held Held Iclk
	sh4_rio_reg(CCN,CCN_BASRA_addr,RIO_DATA,8);

	//CCN BASRB 0xFF000018 0x1F000018 8 Undefined Held Held Held Iclk
	sh4_rio_reg(CCN,CCN_BASRB_addr,RIO_DATA,8);

	//CCN CCR 0xFF00001C 0x1F00001C 32 0x00000000 0x00000000 Held Held Iclk
	sh4_rio_reg(CCN,CCN_CCR_addr,RIO_WF,32,0,&CCN_CCR_write);

	//CCN TRA 0xFF000020 0x1F000020 32 Undefined Undefined Held Held Iclk
	sh4_rio_reg(CCN,CCN_TRA_addr,RIO_DATA,32);

	//CCN EXPEVT 0xFF000024 0x1F000024 32 0x00000000 0x00000020 Held Held Iclk
	sh4_rio_reg(CCN,CCN_EXPEVT_addr,RIO_DATA,32);

	//CCN INTEVT 0xFF000028 0x1F000028 32 Undefined Undefined Held Held Iclk
	sh4_rio_reg(CCN,CCN_INTEVT_addr,RIO_DATA,32);

	// CPU VERSION 0xFF000030 0x1F000030 (undocumented)
	sh4_rio_reg(CCN,CPU_VERSION_addr, RIO_RO_FUNC, 32, &CPU_VERSION_read, 0);

	//CCN PTEA 0xFF000034 0x1F000034 32 Undefined Undefined Held Held Iclk
	sh4_rio_reg(CCN,CCN_PTEA_addr,RIO_DATA,32);

	//CCN QACR0 0xFF000038 0x1F000038 32 Undefined Undefined Held Held Iclk
	sh4_rio_reg(CCN,CCN_QACR0_addr,RIO_WF,32,0,&CCN_QACR_write<0>);

	//CCN QACR1 0xFF00003C 0x1F00003C 32 Undefined Undefined Held Held Iclk
	sh4_rio_reg(CCN,CCN_QACR1_addr,RIO_WF,32,0,&CCN_QACR_write<1>);

	// CCN PRR 0xFF000044 0x1F000044 (undocumented)
	sh4_rio_reg(CCN,CCN_PRR_addr, RIO_RO_FUNC, 32, &CCN_PRR_read, 0);

}

void ccn_reset(bool hard)
{
	CCN_TRA            = 0x0;
	CCN_EXPEVT         = hard ? 0 : 0x20;
	CCN_MMUCR.reg_data = 0x0;
	CCN_CCR.reg_data   = 0x0;
}

void ccn_term()
{
}
