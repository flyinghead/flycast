//gah , ccn emulation
//CCN: Cache and TLB controller

#include "ccn.h"
#include "mmu.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_cache.h"

CCNRegisters ccn;

template<u32 idx>
void CCN_QACR_write(u32 addr, u32 value)
{
	if (idx == 0)
		CCN_QACR0.reg_data = value & 0x1c;
	else
		CCN_QACR1.reg_data = value & 0x1c;

	setSqwHandler();
}

static void CCN_PTEH_write(u32 addr, u32 value)
{
	CCN_PTEH_type temp;
	temp.reg_data = value & 0xfffffcff;
#ifdef FAST_MMU
	if (temp.ASID != CCN_PTEH.ASID)
		mmuAddressLUTFlush(false);
#endif

	CCN_PTEH = temp;
}

static void CCN_MMUCR_write(u32 addr, u32 value)
{
	CCN_MMUCR_type temp;
	temp.reg_data = value & 0xfcfcff05;

	bool mmu_changed_state = temp.AT != CCN_MMUCR.AT;

	if (temp.TI != 0)
	{
		DEBUG_LOG(SH4, "Full MMU flush");
		mmu_flush_table();

		temp.TI = 0;
	}
	CCN_MMUCR = temp;

	if (mmu_changed_state)
	{
		//printf("<*******>MMU Enabled , ONLY SQ remaps work<*******>\n");
		mmu_set_state();
		sh4_cpu.ResetCache();
	}
}

static void CCN_CCR_write(u32 addr, u32 value)
{
	CCN_CCR_type temp;
	temp.reg_data = value & 0x89AF;

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
void CCNRegisters::init()
{
	super::init();

	//CCN PTEH 0xFF000000 0x1F000000 32 Undefined Undefined Held Held Iclk
	setWriteHandler<CCN_PTEH_addr>(CCN_PTEH_write);

	//CCN PTEL 0xFF000004 0x1F000004 32 Undefined Undefined Held Held Iclk
	setRW<CCN_PTEL_addr, u32, 0x1ffffdff>();

	//CCN TTB 0xFF000008 0x1F000008 32 Undefined Undefined Held Held Iclk
	setRW<CCN_TTB_addr>();

	//CCN TEA 0xFF00000C 0x1F00000C 32 Undefined Held Held Held Iclk
	setRW<CCN_TEA_addr>();

	//CCN MMUCR 0xFF000010 0x1F000010 32 0x00000000 0x00000000 Held Held Iclk
	setWriteHandler<CCN_MMUCR_addr>(CCN_MMUCR_write);

	//CCN BASRA 0xFF000014 0x1F000014 8 Undefined Held Held Held Iclk
	setRW<CCN_BASRA_addr, u8>();

	//CCN BASRB 0xFF000018 0x1F000018 8 Undefined Held Held Held Iclk
	setRW<CCN_BASRB_addr, u8>();

	//CCN CCR 0xFF00001C 0x1F00001C 32 0x00000000 0x00000000 Held Held Iclk
	setWriteHandler<CCN_CCR_addr>(CCN_CCR_write);

	//CCN TRA 0xFF000020 0x1F000020 32 Undefined Undefined Held Held Iclk
	setRW<CCN_TRA_addr, u32, 0x000003fc>();

	//CCN EXPEVT 0xFF000024 0x1F000024 32 0x00000000 0x00000020 Held Held Iclk
	setRW<CCN_EXPEVT_addr, u32, 0x00000fff>();

	//CCN INTEVT 0xFF000028 0x1F000028 32 Undefined Undefined Held Held Iclk
	setRW<CCN_INTEVT_addr, u32, 0x00000fff>();

	// CPU VERSION 0xFF000030 0x1F000030 (undocumented)
	setReadOnly<CPU_VERSION_addr>(CPU_VERSION_read);

	//CCN PTEA 0xFF000034 0x1F000034 32 Undefined Undefined Held Held Iclk
	setRW<CCN_PTEA_addr, u32, 0x0000000f>();

	//CCN QACR0 0xFF000038 0x1F000038 32 Undefined Undefined Held Held Iclk
	setWriteHandler<CCN_QACR0_addr>(CCN_QACR_write<0>);

	//CCN QACR1 0xFF00003C 0x1F00003C 32 Undefined Undefined Held Held Iclk
	setWriteHandler<CCN_QACR1_addr>(CCN_QACR_write<1>);

	// CCN PRR 0xFF000044 0x1F000044 (undocumented)
	setReadOnly<CCN_PRR_addr>(CCN_PRR_read);

	reset();
}
