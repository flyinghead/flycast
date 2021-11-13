// serialize.cpp : save states
#include "serialize.h"
#include "types.h"
#include "hw/aica/dsp.h"
#include "hw/aica/aica.h"
#include "hw/aica/sgc_if.h"
#include "hw/arm7/arm7.h"
#include "hw/holly/sb.h"
#include "hw/flashrom/flashrom.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/maple/maple_cfg.h"
#include "hw/modem/modem.h"
#include "hw/pvr/pvr.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/modules/mmu.h"
#include "reios/gdrom_hle.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/naomi/naomi.h"
#include "hw/naomi/naomi_cart.h"
#include "hw/sh4/sh4_cache.h"
#include "hw/sh4/sh4_interpreter.h"
#include "hw/bba/bba.h"
#include "cfg/option.h"

//./core/hw/arm7/arm_mem.cpp
extern bool aica_interr;
extern u32 aica_reg_L;
extern bool e68k_out;
extern u32 e68k_reg_L;
extern u32 e68k_reg_M;

//./core/hw/arm7/arm7.cpp
extern bool armIrqEnable;
extern bool armFiqEnable;
extern int armMode;
extern bool Arm7Enabled;

//./core/hw/aica/aica.o
extern AicaTimer timers[3];

//./core/hw/aica/aica_if.o
extern VArray2 aica_ram;
extern u32 VREG;//video reg =P
extern u32 ARMRST;//arm reset reg
extern u32 rtc_EN;
extern int dma_sched_id;
extern u32 RealTimeClock;
extern u32 SB_ADST;

//./core/hw/aica/aica_mem.o
extern u8 aica_reg[0x8000];

//./core/hw/holly/sb.o
extern u32 SB_FFST_rc;
extern u32 SB_FFST;

//./core/hw/holly/sb_mem.o
extern MemChip *sys_rom;
extern WritableChip *sys_nvmem;

//./core/hw/gdrom/gdromv3.o
extern int gdrom_schid;

//./core/hw/maple/maple_if.o
extern int maple_schid;

//./core/hw/modem/modem.cpp
extern int modem_sched;

//./core/hw/pvr/spg.o
extern int render_end_schid;
extern int vblank_schid;

//./core/hw/sh4/sh4_mmr.o
extern std::array<u8, OnChipRAM_SIZE> OnChipRAM;

//./core/hw/sh4/sh4_mem.o
extern VArray2 mem_b;

//./core/hw/sh4/sh4_interrupts.o
alignas(64) extern u16 InterruptEnvId[32];
alignas(64) extern u32 InterruptBit[32];
alignas(64) extern u32 InterruptLevelBit[16];
extern u32 interrupt_vpend; // Vector of pending interrupts
extern u32 interrupt_vmask; // Vector of masked interrupts             (-1 inhibits all interrupts)
extern u32 decoded_srimask; // Vector of interrupts allowed by SR.IMSK (-1 inhibits all interrupts)

//./core/hw/sh4/sh4_core_regs.o
extern Sh4RCB* p_sh4rcb;

//./core/hw/sh4/sh4_sched.o
extern u64 sh4_sched_ffb;
extern std::vector<sched_list> sch_list;
extern int sh4_sched_next_id;

//./core/hw/sh4/interpr/sh4_interpreter.o
extern int aica_schid;
extern int rtc_schid;

//./core/hw/sh4/modules/serial.o
extern SCIF_SCFSR2_type SCIF_SCFSR2;
extern SCIF_SCSCR2_type SCIF_SCSCR2;

//./core/hw/sh4/modules/bsc.o
extern BSC_PDTRA_type BSC_PDTRA;

//./core/hw/sh4/modules/tmu.o
extern u32 tmu_shift[3];
extern u32 tmu_mask[3];
extern u64 tmu_mask64[3];
extern u32 old_mode[3];
extern int tmu_sched[3];
extern u32 tmu_ch_base[3];
extern u64 tmu_ch_base64[3];

//./core/hw/sh4/modules/ccn.o
extern u32 CCN_QACR_TR[2];

//./core/hw/sh4/modules/mmu.o
extern TLB_Entry UTLB[64];
extern TLB_Entry ITLB[4];
extern u32 sq_remap[64];

//./core/imgread/common.o
extern u32 NullDriveDiscType;
extern u8 q_subchannel[96];

template<typename T>
void register_serialize(const T& regs, Serializer& ser)
{
	for (const auto& reg : regs)
		ser << reg.data32;
}
template<typename T>
void register_deserialize(T& regs, Deserializer& deser)
{
	for (auto& reg : regs)
	{
		if (deser.version() < Deserializer::V5)
			deser.skip<u32>(); // regs.data[i].flags
		if (!(reg.flags & REG_RF))
			deser >> reg.data32;
		else
			deser.skip<u32>();
	}
}

static const std::array<int, 11> getSchedulerIds() {
	return { aica_schid, rtc_schid, gdrom_schid, maple_schid, dma_sched_id,
		tmu_sched[0], tmu_sched[1], tmu_sched[2], render_end_schid, vblank_schid,
		modem_sched };
}

void dc_serialize(Serializer& ser)
{
	ser << aica_interr;
	ser << aica_reg_L;
	ser << e68k_out;
	ser << e68k_reg_L;
	ser << e68k_reg_M;

	ser.serialize(arm_Reg, RN_ARM_REG_COUNT - 1);	// Too lazy to create a new version and the scratch register is not used between blocks anyway
	ser << armIrqEnable;
	ser << armFiqEnable;
	ser << armMode;
	ser << Arm7Enabled;
	ser << arm7ClockTicks;

	dsp::state.serialize(ser);

	for (int i = 0 ; i < 3 ; i++)
	{
		ser << timers[i].c_step;
		ser << timers[i].m_step;
	}

	if (!ser.rollback())
		ser.serialize(aica_ram.data, aica_ram.size);
	ser << VREG;
	ser << ARMRST;
	ser << rtc_EN;
	ser << RealTimeClock;

	ser << aica_reg;

	channel_serialize(ser);

	register_serialize(sb_regs, ser);
	ser << SB_ISTNRM;
	ser << SB_FFST_rc;
	ser << SB_FFST;
	ser << SB_ADST;

	sys_rom->Serialize(ser);
	sys_nvmem->Serialize(ser);

	gdrom::serialize(ser);

	mcfg_SerializeDevices(ser);

	pvr::serialize(ser);

	ser << OnChipRAM;

	register_serialize(CCN, ser);
	register_serialize(UBC, ser);
	register_serialize(BSC, ser);
	register_serialize(DMAC, ser);
	register_serialize(CPG, ser);
	register_serialize(RTC, ser);
	register_serialize(INTC, ser);
	register_serialize(TMU, ser);
	register_serialize(SCI, ser);
	register_serialize(SCIF, ser);
	icache.Serialize(ser);
	ocache.Serialize(ser);

	if (!ser.rollback())
		ser.serialize(mem_b.data, mem_b.size);

	ser << InterruptEnvId;
	ser << InterruptBit;
	ser << InterruptLevelBit;
	ser << interrupt_vpend;
	ser << interrupt_vmask;
	ser << decoded_srimask;


	//default to nommu_full
	int i = 3;
	if ( do_sqw_nommu == &do_sqw_nommu_area_3)
		i = 0;
	else if (do_sqw_nommu == &do_sqw_nommu_area_3_nonvmem)
		i = 1;
	else if (do_sqw_nommu == &TAWriteSQ)
		i = 2;
	else if (do_sqw_nommu==&do_sqw_nommu_full)
		i = 3;
	ser << i;

	ser << (*p_sh4rcb).sq_buffer;

	ser << (*p_sh4rcb).cntx;

	ser << sh4_sched_ffb;
	std::array<int, 11> schedIds = getSchedulerIds();
	if (sh4_sched_next_id == -1)
		ser << sh4_sched_next_id;
	else
		for (u32 i = 0; i < schedIds.size(); i++)
			if (schedIds[i] == sh4_sched_next_id)
				ser << i;

	for (u32 i = 0; i < schedIds.size() - 1; i++)
	{
		ser << sch_list[schedIds[i]].tag;
		ser << sch_list[schedIds[i]].start;
		ser << sch_list[schedIds[i]].end;
	}

	ser << config::EmulateBBA.get();
	if (config::EmulateBBA)
	{
		bba_Serialize(ser);
	}
	else
	{
		ser << sch_list[modem_sched].tag;
		ser << sch_list[modem_sched].start;
		ser << sch_list[modem_sched].end;
	}
	ModemSerialize(ser);

	ser << SCIF_SCFSR2;
	ser << SCIF_SCSCR2;
	ser << BSC_PDTRA;

	ser << tmu_shift;
	ser << tmu_mask;
	ser << tmu_mask64;
	ser << old_mode;
	ser << tmu_ch_base;
	ser << tmu_ch_base64;

	ser << CCN_QACR_TR;

	ser << UTLB;
	ser << ITLB;
	ser << sq_remap;

	ser << NullDriveDiscType;
	ser << q_subchannel;

	naomi_Serialize(ser);

	ser << config::Broadcast.get();
	ser << config::Cable.get();
	ser << config::Region.get();

	if (CurrentCartridge != NULL)
		CurrentCartridge->Serialize(ser);

	gd_hle_state.Serialize(ser);

	DEBUG_LOG(SAVESTATE, "Saved %d bytes", (u32)ser.size());
}

static void dc_deserialize_libretro(Deserializer& deser)
{
	deser >> aica_interr;
	deser >> aica_reg_L;
	deser >> e68k_out;
	deser >> e68k_reg_L;
	deser >> e68k_reg_M;

	deser.deserialize(arm_Reg, RN_ARM_REG_COUNT - 1);
	deser >> armIrqEnable;
	deser >> armFiqEnable;
	deser >> armMode;
	deser >> Arm7Enabled;
	if (deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip(256);		// cpuBitsSet
		deser.skip(1);			// intState
		deser.skip(1);			// stopState
		deser.skip(1);			// holdState
	}
	arm7ClockTicks = 0;

	dsp::state.deserialize(deser);

	for (int i = 0 ; i < 3 ; i++)
	{
		deser >> timers[i].c_step;
		deser >> timers[i].m_step;
	}

	deser.deserialize(aica_ram.data, aica_ram.size);
	deser >> VREG;
	deser >> ARMRST;
	deser >> rtc_EN;

	deser >> aica_reg;

	channel_deserialize(deser);

	register_deserialize(sb_regs, deser);
	deser >> SB_ISTNRM;
	deser >> SB_FFST_rc;
	deser >> SB_FFST;
	SB_ADST = 0;

	deser.skip<u32>(); // sys_nvmem->size
	deser.skip<u32>(); // sys_nvmem->mask
	if (settings.platform.system == DC_PLATFORM_NAOMI || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		sys_nvmem->Deserialize(deser);

	deser.skip<u32>(); // sys_nvmem/sys_rom->size
	deser.skip<u32>(); // sys_nvmem/sys_rom->mask
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		sys_nvmem->Deserialize(deser);
	}
	else if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		deser >> static_cast<DCFlashChip*>(sys_rom)->state;
		deser.deserialize(sys_rom->data, sys_rom->size);
	}
	else
	{
		deser.skip<u32>();
	}

	gdrom::deserialize(deser);

	mcfg_DeserializeDevices(deser);

	pvr::deserialize(deser);

	deser >> OnChipRAM;

	register_deserialize(CCN, deser);
	register_deserialize(UBC, deser);
	register_deserialize(BSC, deser);
	register_deserialize(DMAC, deser);
	register_deserialize(CPG, deser);
	register_deserialize(RTC, deser);
	register_deserialize(INTC, deser);
	register_deserialize(TMU, deser);
	register_deserialize(SCI, deser);
	register_deserialize(SCIF, deser);
	if (deser.version() >= Deserializer::V11_LIBRETRO)	// FIXME was added in V11 fa49de29 24/12/2020 but ver not updated until V12 (13/4/2021)
		icache.Deserialize(deser);
	else
		icache.Reset(true);
	if (deser.version() >= Deserializer::V11_LIBRETRO)	// FIXME was added in V11 2eb66879 27/12/2020 but ver not updated until V12 (13/4/2021)
		ocache.Deserialize(deser);
	else
		ocache.Reset(true);

	deser.deserialize(mem_b.data, mem_b.size);
	if (deser.version() < Deserializer::V9_LIBRETRO)
		deser.skip<u16>();
	deser >> InterruptEnvId;
	deser >> InterruptBit;
	deser >> InterruptLevelBit;
	deser >> interrupt_vpend;
	deser >> interrupt_vmask;
	deser >> decoded_srimask;

	int i;
	deser >> i;
	if (i == 0)
		do_sqw_nommu = &do_sqw_nommu_area_3;
	else if (i == 1)
		do_sqw_nommu = &do_sqw_nommu_area_3_nonvmem;
	else if (i == 2)
		do_sqw_nommu = &TAWriteSQ ;
	else if (i == 3)
		do_sqw_nommu = &do_sqw_nommu_full;

	deser >> (*p_sh4rcb).sq_buffer;

	deser >> (*p_sh4rcb).cntx;
	p_sh4rcb->cntx.cycle_counter = SH4_TIMESLICE;

	if (deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip<u32>();		// old_rm
		deser.skip<u32>();		// old_dn
	}

	deser >> sh4_sched_ffb;
	if (deser.version() < Deserializer::V9_LIBRETRO)
		deser.skip<u32>();		// sh4_sched_intr

	deser >> sch_list[aica_schid].tag;
	deser >> sch_list[aica_schid].start;
	deser >> sch_list[aica_schid].end;

	deser >> sch_list[rtc_schid].tag;
	deser >> sch_list[rtc_schid].start;
	deser >> sch_list[rtc_schid].end;

	deser >> sch_list[gdrom_schid].tag;
	deser >> sch_list[gdrom_schid].start;
	deser >> sch_list[gdrom_schid].end;

	deser >> sch_list[maple_schid].tag;
	deser >> sch_list[maple_schid].start;
	deser >> sch_list[maple_schid].end;

	deser >> sch_list[dma_sched_id].tag;
	deser >> sch_list[dma_sched_id].start;
	deser >> sch_list[dma_sched_id].end;

	for (int i = 0; i < 3; i++)
	{
		deser >> sch_list[tmu_sched[i]].tag;
		deser >> sch_list[tmu_sched[i]].start;
		deser >> sch_list[tmu_sched[i]].end;
	}

	deser >> sch_list[render_end_schid].tag;
	deser >> sch_list[render_end_schid].start;
	deser >> sch_list[render_end_schid].end;

	deser >> sch_list[vblank_schid].tag;
	deser >> sch_list[vblank_schid].start;
	deser >> sch_list[vblank_schid].end;

	if (deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip<u32>();		// sch_list[time_sync].tag
		deser.skip<u32>();		// sch_list[time_sync].start
		deser.skip<u32>();		// sch_list[time_sync].end
	}

	if (deser.version() >= Deserializer::V13_LIBRETRO)
		deser.skip<bool>();		// settings.network.EmulateBBA

	deser >> sch_list[modem_sched].tag;
    deser >> sch_list[modem_sched].start;
    deser >> sch_list[modem_sched].end;

	deser >> SCIF_SCFSR2;
	if (deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip(1);		// SCIF_SCFRDR2
		deser.skip(4);		// SCIF_SCFDR2
	}
	else if (deser.version() >= Deserializer::V11_LIBRETRO)
		deser >> SCIF_SCSCR2;
	deser >> BSC_PDTRA;

	deser >> tmu_shift;
	deser >> tmu_mask;
	deser >> tmu_mask64;
	deser >> old_mode;
	deser >> tmu_ch_base;
	deser >> tmu_ch_base64;

	deser >> CCN_QACR_TR;

	if (deser.version() < Deserializer::V6_LIBRETRO)
	{
		for (int i = 0; i < 64; i++)
		{
			deser >> UTLB[i].Address;
			deser >> UTLB[i].Data;
		}
		for (int i = 0; i < 4; i++)
		{
			deser >> ITLB[i].Address;
			deser >> ITLB[i].Data;
		}
	}
	else
	{
		deser >> UTLB;
		deser >> ITLB;
	}
	if (deser.version() >= Deserializer::V11_LIBRETRO)
		deser >> sq_remap;
	deser.skip(64 * 4); // ITLB_LRU_USE

	deser >> NullDriveDiscType;
	deser >> q_subchannel;

	deser.skip<u32>();	// FLASH_SIZE
	deser.skip<u32>();	// BBSRAM_SIZE
	deser.skip<u32>();	// BIOS_SIZE
	deser.skip<u32>();	// RAM_SIZE
	deser.skip<u32>();	// ARAM_SIZE
	deser.skip<u32>();	// VRAM_SIZE
	deser.skip<u32>();	// RAM_MASK
	deser.skip<u32>();	// ARAM_MASK
	deser.skip<u32>();	// VRAM_MASK

	naomi_Deserialize(deser);

	if (deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip<u32>();		// cycle_counter
		deser.skip<u32>();		// idxnxx
		deser.skip(44); 		// sizeof(state_t)
		deser.skip<u32>();		// div_som_reg1
		deser.skip<u32>();		// div_som_reg2
		deser.skip<u32>();		// div_som_reg3

		deser.skip<u32>();		// LastAddr
		deser.skip<u32>();		// LastAddr_min
		deser.skip(1024);		// block_hash

		// RegisterRead, RegisterWrite
		for (int i = 0; i < 74; i++)	// sh4_reg_count (changed to 75 on 9/6/2020 (V9), V10 on 22/6/2020)
		{
			deser.skip(4);
			deser.skip(4);
		}
		deser.skip<u32>(); // fallback_blocks
		deser.skip<u32>(); // total_blocks
		deser.skip<u32>(); // REMOVED_OPS
	}
	deser >> config::Broadcast.get();
	deser >> config::Cable.get();
	deser >> config::Region.get();

	if (CurrentCartridge != nullptr && (settings.platform.system != DC_PLATFORM_ATOMISWAVE || deser.version() >= Deserializer::V10_LIBRETRO))
		CurrentCartridge->Deserialize(deser);
	if (deser.version() >= Deserializer::V7_LIBRETRO)
		gd_hle_state.Deserialize(deser);
	config::EmulateBBA.override(false);

	DEBUG_LOG(SAVESTATE, "Loaded %d bytes (libretro compat)", (u32)deser.size());
}

void dc_deserialize(Deserializer& deser)
{
	if (deser.version() >= Deserializer::V5_LIBRETRO && deser.version() <= Deserializer::VLAST_LIBRETRO)
	{
		dc_deserialize_libretro(deser);
		return;
	}
	DEBUG_LOG(SAVESTATE, "Loading state version %d", deser.version());

	deser >> aica_interr;
	deser >> aica_reg_L;
	deser >> e68k_out;
	deser >> e68k_reg_L;
	deser >> e68k_reg_M;

	deser.deserialize(arm_Reg, RN_ARM_REG_COUNT - 1);
	deser >> armIrqEnable;
	deser >> armFiqEnable;
	deser >> armMode;
	deser >> Arm7Enabled;
	if (deser.version() < Deserializer::V5)
		deser.skip(256 + 3);
	if (deser.version() >= Deserializer::V19)
		deser >> arm7ClockTicks;
	else
		arm7ClockTicks = 0;

	dsp::state.deserialize(deser);

	for (int i = 0 ; i < 3 ; i++)
	{
		deser >> timers[i].c_step;
		deser >> timers[i].m_step;
	}

	if (!deser.rollback())
		deser.deserialize(aica_ram.data, aica_ram.size);
	deser >> VREG;
	deser >> ARMRST;
	deser >> rtc_EN;
	if (deser.version() >= Deserializer::V9)
		deser >> RealTimeClock;

	deser >> aica_reg;

	channel_deserialize(deser);

	register_deserialize(sb_regs, deser);
	deser >> SB_ISTNRM;
	deser >> SB_FFST_rc;
	deser >> SB_FFST;
	if (deser.version() >= Deserializer::V15)
		deser >> SB_ADST;
	else
		SB_ADST = 0;

	if (deser.version() < Deserializer::V5)
	{
		deser.skip<u32>();	// size
		deser.skip<u32>();	// mask
	}
	sys_rom->Deserialize(deser);
	sys_nvmem->Deserialize(deser);

	gdrom::deserialize(deser);

	mcfg_DeserializeDevices(deser);

	pvr::deserialize(deser);

	deser >> OnChipRAM;

	register_deserialize(CCN, deser);
	register_deserialize(UBC, deser);
	register_deserialize(BSC, deser);
	register_deserialize(DMAC, deser);
	register_deserialize(CPG, deser);
	register_deserialize(RTC, deser);
	register_deserialize(INTC, deser);
	register_deserialize(TMU, deser);
	register_deserialize(SCI, deser);
	register_deserialize(SCIF, deser);
	if (deser.version() >= Deserializer::V9)
		icache.Deserialize(deser);
	else
		icache.Reset(true);
	if (deser.version() >= Deserializer::V10)
		ocache.Deserialize(deser);
	else
		ocache.Reset(true);

	if (!deser.rollback())
		deser.deserialize(mem_b.data, mem_b.size);

	if (deser.version() < Deserializer::V5)
		deser.skip(2);
	deser >> InterruptEnvId;
	deser >> InterruptBit;
	deser >> InterruptLevelBit;
	deser >> interrupt_vpend;
	deser >> interrupt_vmask;
	deser >> decoded_srimask;

	int i;
	deser >> i;
	if (i == 0)
		do_sqw_nommu = &do_sqw_nommu_area_3;
	else if (i == 1)
		do_sqw_nommu = &do_sqw_nommu_area_3_nonvmem;
	else if (i == 2)
		do_sqw_nommu = &TAWriteSQ;
	else if (i == 3)
		do_sqw_nommu = &do_sqw_nommu_full;

	deser >> (*p_sh4rcb).sq_buffer;

	deser >> (*p_sh4rcb).cntx;
	if (deser.version() < Deserializer::V5)
	{
		deser.skip(4);
		deser.skip(4);
	}
	if (deser.version() >= Deserializer::V19 && deser.version() < Deserializer::V21)
		deser.skip<u32>(); // sh4InterpCycles
	if (deser.version() < Deserializer::V21)
		p_sh4rcb->cntx.cycle_counter = SH4_TIMESLICE;

	deser >> sh4_sched_ffb;
	std::array<int, 11> schedIds = getSchedulerIds();

	if (deser.version() >= Deserializer::V19)
	{
		deser >> sh4_sched_next_id;
		if (sh4_sched_next_id != -1)
			sh4_sched_next_id = schedIds[sh4_sched_next_id];
	}
	if (deser.version() < Deserializer::V8)
		deser.skip<u32>();		// sh4_sched_intr

	for (u32 i = 0; i < schedIds.size() - 1; i++)
	{
		deser >> sch_list[schedIds[i]].tag;
		deser >> sch_list[schedIds[i]].start;
		deser >> sch_list[schedIds[i]].end;
	}

	if (deser.version() < Deserializer::V8)
	{
		deser.skip<u32>(); // sch_list[time_sync].tag
		deser.skip<u32>(); // sch_list[time_sync].start
		deser.skip<u32>(); // sch_list[time_sync].end
	}

	if (deser.version() >= Deserializer::V13)
		deser >> config::EmulateBBA.get();
	else
		config::EmulateBBA.override(false);
	if (config::EmulateBBA)
	{
		bba_Deserialize(deser);
	}
	else
	{
		deser >> sch_list[modem_sched].tag;
		deser >> sch_list[modem_sched].start;
		deser >> sch_list[modem_sched].end;
	}
	if (deser.version() < Deserializer::V19)
		sh4_sched_ffts();
	ModemDeserialize(deser);

	deser >> SCIF_SCFSR2;
	if (deser.version() < Deserializer::V8)
	{
		deser.skip<bool>();	// SCIF_SCFRDR2
		deser.skip<u32>();	// SCIF_SCFDR2
	}
	else if (deser.version() >= Deserializer::V11)
		deser >> SCIF_SCSCR2;
	deser >> BSC_PDTRA;

	deser >> tmu_shift;
	deser >> tmu_mask;
	deser >> tmu_mask64;
	deser >> old_mode;
	deser >> tmu_ch_base;
	deser >> tmu_ch_base64;

	deser >> CCN_QACR_TR;

	deser >> UTLB;
	deser >> ITLB;
	if (deser.version() >= Deserializer::V11)
		deser >> sq_remap;
	deser.skip(64 * 4, Deserializer::V23); // ITLB_LRU_USE

	deser >> NullDriveDiscType;
	deser >> q_subchannel;

	naomi_Deserialize(deser);

	if (deser.version() < Deserializer::V5)
	{
		deser.skip<u32>();	// idxnxx
		deser.skip(44);		// sizeof(state_t)
		deser.skip(4);
		deser.skip(4);
		deser.skip(4);
		deser.skip(4);
		deser.skip(4);
		deser.skip(1024);

		deser.skip(8 * 74);	// sh4_reg_count
		deser.skip(4);
		deser.skip(4);
		deser.skip(4);

		deser.skip(2 * 4);
		deser.skip(4);
		deser.skip(4);
		deser.skip(4 * 4);
		deser.skip(4);
		deser.skip(4);
	}
	deser >> config::Broadcast.get();
	verify(config::Broadcast <= 4);
	deser >> config::Cable.get();
	verify(config::Cable <= 3);
	deser >> config::Region.get();
	verify(config::Region <= 3);

	if (CurrentCartridge != NULL)
		CurrentCartridge->Deserialize(deser);
	if (deser.version() >= Deserializer::V6)
		gd_hle_state.Deserialize(deser);

	DEBUG_LOG(SAVESTATE, "Loaded %d bytes", (u32)deser.size());
}
