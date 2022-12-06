/*
	Sh4 internal register routing (P4 & 'area 7')
*/
#include <array>
#include "types.h"
#include "sh4_mmr.h"

#include "hw/mem/_vmem.h"
#include "modules/mmu.h"
#include "modules/ccn.h"
#include "modules/modules.h"
#include "sh4_cache.h"

//64bytes of sq // now on context ~

std::array<u8, OnChipRAM_SIZE> OnChipRAM;

//All registers are 4 byte aligned

RegisterStruct CCN[18];
RegisterStruct UBC[9];
RegisterStruct BSC[19];
RegisterStruct DMAC[17];
RegisterStruct CPG[5];
RegisterStruct RTC[16];
RegisterStruct INTC[5];
RegisterStruct TMU[12];
RegisterStruct SCI[8];
RegisterStruct SCIF[10];

static u32 sh4io_read_noacc(u32 addr)
{ 
	INFO_LOG(SH4, "sh4io: Invalid read access @ %08X", addr);
	return 0; 
} 

static void sh4io_write_noacc(u32 addr, u32 data)
{ 
	INFO_LOG(SH4, "sh4io: Invalid write access @ %08X %08X", addr, data);
}

static void sh4io_write_const(u32 addr, u32 data)
{ 
	INFO_LOG(SH4, "sh4io: Const write ignored @ %08X <- %08X", addr, data);
}

void sh4_rio_reg(RegisterStruct *arr, u32 addr, RegIO flags, RegReadAddrFP* rf, RegWriteAddrFP* wf)
{
	u32 idx = (addr & 255) / 4;

	arr[idx].flags = flags;

	if (flags == RIO_NO_ACCESS)
	{
		arr[idx].readFunctionAddr = sh4io_read_noacc;
		arr[idx].writeFunctionAddr = sh4io_write_noacc;
	}
	else if (flags == RIO_RO)
	{
		arr[idx].writeFunctionAddr = sh4io_write_const;
		arr[idx].data32 = 0;
	}
	else
	{
		if (flags & REG_RF)
			arr[idx].readFunctionAddr = rf;
		else
			arr[idx].data32 = 0;

		if (flags & REG_WF)
			arr[idx].writeFunctionAddr = wf == nullptr ? &sh4io_write_noacc : wf;
	}
}

template<typename T>
T sh4_rio_read(RegisterStruct *regs, u32 addr)
{	
	u32 offset = addr & 255;
#ifdef TRACE
	if (offset & 3) //4 is min align size
	{
		WARN_LOG(SH4, "Unaligned System Bus register read @ %x", addr);
	}
#endif

	offset >>= 2;

	if (!(regs[offset].flags & REG_RF))
		return (T)regs[offset].data32;
	else
		return (T)regs[offset].readFunctionAddr(addr);
}

template<typename T>
void sh4_rio_write(RegisterStruct *regs, u32 addr, T data)
{
	u32 offset = addr & 255;
#ifdef TRACE
	if (offset & 3) //4 is min align size
	{
		WARN_LOG(SH4, "Unaligned System bus register write @ %x", addr);
	}
#endif
	offset >>= 2;

	if (!(regs[offset].flags & REG_WF))
		regs[offset].data32 = data;
	else
		regs[offset].writeFunctionAddr(addr, data);
}

#define SH4_REG_NAME(r) { r##_addr, #r },
const std::map<u32, const char *> sh4_reg_names = {
		SH4_REG_NAME(CCN_PTEH)
		SH4_REG_NAME(CCN_PTEL)
		SH4_REG_NAME(CCN_TTB)
		SH4_REG_NAME(CCN_TEA)
		SH4_REG_NAME(CCN_CCR)
		SH4_REG_NAME(CCN_TRA)
		SH4_REG_NAME(CCN_EXPEVT)
		SH4_REG_NAME(CCN_INTEVT)
		SH4_REG_NAME(CPU_VERSION)
		SH4_REG_NAME(CCN_PTEA)
		SH4_REG_NAME(CCN_QACR0)
		SH4_REG_NAME(CCN_QACR1)
		SH4_REG_NAME(CCN_PRR)

		SH4_REG_NAME(UBC_BARA)
		SH4_REG_NAME(UBC_BAMRA)
		SH4_REG_NAME(UBC_BBRA)
		SH4_REG_NAME(UBC_BARB)
		SH4_REG_NAME(UBC_BAMRB)
		SH4_REG_NAME(UBC_BBRB)
		SH4_REG_NAME(UBC_BDRB)
		SH4_REG_NAME(UBC_BDMRB)
		SH4_REG_NAME(UBC_BRCR)

		SH4_REG_NAME(BSC_BCR1)
		SH4_REG_NAME(BSC_BCR2)
		SH4_REG_NAME(BSC_WCR1)
		SH4_REG_NAME(BSC_WCR2)
		SH4_REG_NAME(BSC_WCR3)
		SH4_REG_NAME(BSC_MCR)
		SH4_REG_NAME(BSC_PCR)
		SH4_REG_NAME(BSC_RTCSR)
		SH4_REG_NAME(BSC_RTCNT)
		SH4_REG_NAME(BSC_RTCOR)
		SH4_REG_NAME(BSC_RFCR)
		SH4_REG_NAME(BSC_PCTRA)
		SH4_REG_NAME(BSC_PDTRA)
		SH4_REG_NAME(BSC_PCTRB)
		SH4_REG_NAME(BSC_PDTRB)
		SH4_REG_NAME(BSC_GPIOIC)
		SH4_REG_NAME(BSC_SDMR2)
		SH4_REG_NAME(BSC_SDMR3)

		SH4_REG_NAME(DMAC_SAR0)
		SH4_REG_NAME(DMAC_DAR0)
		SH4_REG_NAME(DMAC_DMATCR0)
		SH4_REG_NAME(DMAC_CHCR0)
		SH4_REG_NAME(DMAC_SAR1)
		SH4_REG_NAME(DMAC_DAR1)
		SH4_REG_NAME(DMAC_DMATCR1)
		SH4_REG_NAME(DMAC_CHCR1)
		SH4_REG_NAME(DMAC_SAR2)
		SH4_REG_NAME(DMAC_DAR2)
		SH4_REG_NAME(DMAC_DMATCR2)
		SH4_REG_NAME(DMAC_CHCR2)
		SH4_REG_NAME(DMAC_SAR3)
		SH4_REG_NAME(DMAC_DAR3)
		SH4_REG_NAME(DMAC_DMATCR3)
		SH4_REG_NAME(DMAC_CHCR3)
		SH4_REG_NAME(DMAC_DMAOR)

		SH4_REG_NAME(CPG_FRQCR)
		SH4_REG_NAME(CPG_STBCR)
		SH4_REG_NAME(CPG_WTCNT)
		SH4_REG_NAME(CPG_WTCSR)
		SH4_REG_NAME(CPG_STBCR2)

		SH4_REG_NAME(RTC_R64CNT)
		SH4_REG_NAME(RTC_RSECCNT)
		SH4_REG_NAME(RTC_RMINCNT)
		SH4_REG_NAME(RTC_RHRCNT)
		SH4_REG_NAME(RTC_RWKCNT)
		SH4_REG_NAME(RTC_RDAYCNT)
		SH4_REG_NAME(RTC_RMONCNT)
		SH4_REG_NAME(RTC_RYRCNT)
		SH4_REG_NAME(RTC_RSECAR)
		SH4_REG_NAME(RTC_RMINAR)
		SH4_REG_NAME(RTC_RHRAR)
		SH4_REG_NAME(RTC_RWKAR)
		SH4_REG_NAME(RTC_RDAYAR)
		SH4_REG_NAME(RTC_RMONAR)
		SH4_REG_NAME(RTC_RCR1)
		SH4_REG_NAME(RTC_RCR2)

		SH4_REG_NAME(INTC_ICR)
		SH4_REG_NAME(INTC_IPRA)
		SH4_REG_NAME(INTC_IPRB)
		SH4_REG_NAME(INTC_IPRC)
		SH4_REG_NAME(INTC_IPRD)

		SH4_REG_NAME(TMU_TOCR)
		SH4_REG_NAME(TMU_TSTR)
		SH4_REG_NAME(TMU_TCOR0)
		SH4_REG_NAME(TMU_TCNT0)
		SH4_REG_NAME(TMU_TCR0)
		SH4_REG_NAME(TMU_TCOR1)
		SH4_REG_NAME(TMU_TCNT1)
		SH4_REG_NAME(TMU_TCR1)
		SH4_REG_NAME(TMU_TCOR2)
		SH4_REG_NAME(TMU_TCNT2)
		SH4_REG_NAME(TMU_TCR2)
		SH4_REG_NAME(TMU_TCPR2)

		SH4_REG_NAME(SCI_SCSMR1)
		SH4_REG_NAME(SCI_SCBRR1)
		SH4_REG_NAME(SCI_SCSCR1)
		SH4_REG_NAME(SCI_SCTDR1)
		SH4_REG_NAME(SCI_SCSSR1)
		SH4_REG_NAME(SCI_SCRDR1)
		SH4_REG_NAME(SCI_SCSCMR1)
		SH4_REG_NAME(SCI_SCSPTR1)

		SH4_REG_NAME(SCIF_SCSMR2)
		SH4_REG_NAME(SCIF_SCBRR2)
		SH4_REG_NAME(SCIF_SCSCR2)
		SH4_REG_NAME(SCIF_SCFTDR2)
		SH4_REG_NAME(SCIF_SCFSR2)
		SH4_REG_NAME(SCIF_SCFRDR2)
		SH4_REG_NAME(SCIF_SCFCR2)
		SH4_REG_NAME(SCIF_SCFDR2)
		SH4_REG_NAME(SCIF_SCSPTR2)
		SH4_REG_NAME(SCIF_SCLSR2)

		SH4_REG_NAME(UDI_SDIR)
		SH4_REG_NAME(UDI_SDDR)
};
#undef SH4_REG_NAME

static const char *regName(u32 paddr)
{
	u32 addr = paddr & 0x1fffffff;
	static char regName[32];
	auto it = sh4_reg_names.find(addr);
	if (it == sh4_reg_names.end()) {
		sprintf(regName, "?%08x", paddr);
		return regName;
	}
	else
		return it->second;
}

//Region P4
//Read P4
template <class T>
T DYNACALL ReadMem_P4(u32 addr)
{
	constexpr size_t sz = sizeof(T);
	switch ((addr >> 24) & 0xFF)
	{

	case 0xE0:
	case 0xE1:
	case 0xE2:
	case 0xE3:
		INFO_LOG(SH4, "Unhandled p4 read [Store queue] 0x%x", addr);
		return 0;

	case 0xF0:
		DEBUG_LOG(SH4, "IC Address read %08x", addr);
		if (sz == 4)
			return icache.ReadAddressArray(addr);
		else
			return 0;

	case 0xF1:
		DEBUG_LOG(SH4, "IC Data read %08x", addr);
		if (sz == 4)
			return icache.ReadDataArray(addr);
		else
			return 0;

	case 0xF2:
		{
			u32 entry = (addr >> 8) & 3;
			return ITLB[entry].Address.reg_data | (ITLB[entry].Data.V << 8);
		}

	case 0xF3:
		{
			u32 entry = (addr >> 8) & 3;
			return ITLB[entry].Data.reg_data;
		}

	case 0xF4:
		DEBUG_LOG(SH4, "OC Address read %08x", addr);
		if (sz == 4)
			return ocache.ReadAddressArray(addr);
		else
			return 0;

	case 0xF5:
		DEBUG_LOG(SH4, "OC Data read %08x", addr);
		if (sz == 4)
			return ocache.ReadDataArray(addr);
		else
			return 0;

	case 0xF6:
		{
			u32 entry = (addr >> 8) & 63;
			u32 rv = UTLB[entry].Address.reg_data;
			rv |= UTLB[entry].Data.D << 9;
			rv |= UTLB[entry].Data.V << 8;
			return rv;
		}

	case 0xF7:
		{
			u32 entry = (addr >> 8) & 63;
			return UTLB[entry].Data.reg_data;
		}

	case 0xFF:
		INFO_LOG(SH4, "Unhandled p4 read [area7] 0x%x", addr);
		break;

	default:
		INFO_LOG(SH4, "Unhandled p4 read [Reserved] 0x%x", addr);
		break;
	}

	return 0;

}

//Write P4
template <class T>
void DYNACALL WriteMem_P4(u32 addr,T data)
{
	constexpr size_t sz = sizeof(T);
	switch ((addr >> 24) & 0xFF)
	{
	case 0xE0:
	case 0xE1:
	case 0xE2:
	case 0xE3:
		INFO_LOG(SH4, "Unhandled p4 Write [Store queue] 0x%x", addr);
		break;

	case 0xF0:
		DEBUG_LOG(SH4, "IC Address write %08x = %x", addr, data);
		if (sz == 4)
			icache.WriteAddressArray(addr, data);
		return;

	case 0xF1:
		DEBUG_LOG(SH4, "IC Data write %08x = %x", addr, data);
		if (sz == 4)
			icache.WriteDataArray(addr, data);
		return;

	case 0xF2:
		{
			u32 entry = (addr >> 8) & 3;
			ITLB[entry].Address.reg_data = data & 0xFFFFFCFF;
			ITLB[entry].Data.V = (data >> 8) & 1;
			ITLB_Sync(entry);
		}
		return;

	case 0xF3:
		{
			u32 entry = (addr >> 8) & 3;
			if (addr & 0x800000)
				ITLB[entry].Assistance.reg_data = data & 0xf;
			else
				ITLB[entry].Data.reg_data=data;
			ITLB_Sync(entry);
		}
		return;

	case 0xF4:
//		DEBUG_LOG(SH4, "OC Address write %08x = %x", addr, data);
		if (sz == 4)
			ocache.WriteAddressArray(addr, data);
		return;

	case 0xF5:
		DEBUG_LOG(SH4, "OC Data write %08x = %x", addr, data);
		if (sz == 4)
			ocache.WriteDataArray(addr, data);
		return;

	case 0xF6:
		if (addr & 0x80)
		{
			CCN_PTEH_type t;
			t.reg_data = data;

			u32 va = t.VPN << 10;

			for (int i = 0; i < 64; i++)
			{
				if (mmu_match(va, UTLB[i].Address, UTLB[i].Data))
				{
					UTLB[i].Data.V = ((u32)data >> 8) & 1;
					UTLB[i].Data.D = ((u32)data >> 9) & 1;
					UTLB_Sync(i);
				}
			}

			for (int i = 0; i < 4; i++)
			{
				if (mmu_match(va, ITLB[i].Address, ITLB[i].Data))
				{
					ITLB[i].Data.V = ((u32)data >> 8) & 1;
					ITLB[i].Data.D = ((u32)data >> 9) & 1;
					ITLB_Sync(i);
				}
			}
		}
		else
		{
			u32 entry = (addr >> 8) & 63;
			UTLB[entry].Address.reg_data = data & 0xFFFFFCFF;
			UTLB[entry].Data.D = (data >> 9) & 1;
			UTLB[entry].Data.V = (data >> 8) & 1;
			UTLB_Sync(entry);
		}
		return;

	case 0xF7:
		{
			u32 entry = (addr >> 8) & 63;
			if (addr & 0x800000)
				UTLB[entry].Assistance.reg_data = data & 0xf;
			else
				UTLB[entry].Data.reg_data = data;
			UTLB_Sync(entry);
		}
		return;

	case 0xFF:
		INFO_LOG(SH4, "Unhandled p4 Write [area7] 0x%x = %x", addr, data);
		break;

	default:
		INFO_LOG(SH4, "Unhandled p4 Write [Reserved] 0x%x", addr);
		break;
	}
}

//***********
//**Area  7**
//***********

#define OUT_OF_RANGE(reg) INFO_LOG(SH4, "Out of range on register %s index %x", reg, addr)
#define A7_REG_HASH(addr) (((addr) >> 16) & 0x1FFF)

//Read P4 memory-mapped registers
template <class T>
T DYNACALL ReadMem_p4mmr(u32 addr)
{
	DEBUG_LOG(SH4, "read %s", regName(addr));

	/*
	if (likely(addr == 0xffd80024))
		return TMU_TCNT(2);
	if (likely(addr == 0xFFD8000C))
		return TMU_TCNT(0);
	*/
	if (likely(addr == 0xFF000028))
		return (T)CCN_INTEVT;
	if (likely(addr == 0xFFA0002C))
		return (T)DMAC_CHCR(2).full;

	addr &= 0x1FFFFFFF;
	u32 map_base = addr >> 16;
	switch (expected(map_base, A7_REG_HASH(TMU_BASE_addr)))
	{
	case A7_REG_HASH(CCN_BASE_addr):
		if (addr <= 0x1F000044)
		{
			return sh4_rio_read<T>(CCN, addr);
		}
		else
		{
			OUT_OF_RANGE("CCN");
			return 0;
		}
		break;

	case A7_REG_HASH(UBC_BASE_addr):
		if (addr <= 0x1F200020)
		{
			return sh4_rio_read<T>(UBC, addr);
		}
		else
		{
			OUT_OF_RANGE("UBC");
			return 0;
		}
		break;

	case A7_REG_HASH(BSC_BASE_addr):
		if (addr <= 0x1F800048)
		{
			return sh4_rio_read<T>(BSC, addr);
		}
		else
		{
			OUT_OF_RANGE("BSC");
			return 0;
		}
		break;

	case A7_REG_HASH(BSC_SDMR2_addr):
		//dram settings 2 / write only
		INFO_LOG(SH4, "Read from write-only registers [dram settings 2]");
		return 0;
	case A7_REG_HASH(BSC_SDMR3_addr):
		//dram settings 3 / write only
		INFO_LOG(SH4, "Read from write-only registers [dram settings 3]");
		return 0;

	case A7_REG_HASH(DMAC_BASE_addr):
		if (addr <= 0x1FA00040)
		{
			return sh4_rio_read<T>(DMAC, addr);
		}
		else
		{
			OUT_OF_RANGE("DMAC");
			return 0;
		}
		break;

	case A7_REG_HASH(CPG_BASE_addr):
		if (addr <= 0x1FC00010)
		{
			return sh4_rio_read<T>(CPG, addr);
		}
		else
		{
			OUT_OF_RANGE("CPG");
			return 0;
		}
		break;

	case A7_REG_HASH(RTC_BASE_addr):
		if (addr <= 0x1FC8003C)
		{
			return sh4_rio_read<T>(RTC, addr);
		}
		else
		{
			OUT_OF_RANGE("RTC");
			return 0;
		}
		break;

	case A7_REG_HASH(INTC_BASE_addr):
		if (addr <= 0x1FD00010)
		{
			return sh4_rio_read<T>(INTC, addr);
		}
		else
		{
			OUT_OF_RANGE("INTC");
			return 0;
		}
		break;

	case A7_REG_HASH(TMU_BASE_addr):
		if (addr <= 0x1FD8002C)
		{
			return sh4_rio_read<T>(TMU, addr);
		}
		else
		{
			OUT_OF_RANGE("TMU");
			return 0;
		}
		break;

	case A7_REG_HASH(SCI_BASE_addr):
		if (addr <= 0x1FE0001C)
		{
			return sh4_rio_read<T>(SCI, addr);
		}
		else
		{
			OUT_OF_RANGE("SCI");
			return 0;
		}
		break;

	case A7_REG_HASH(SCIF_BASE_addr):
		if (addr <= 0x1FE80024)
		{
			return sh4_rio_read<T>(SCIF, addr);
		}
		else
		{
			OUT_OF_RANGE("SCIF");
			return 0;
		}
		break;

		// Who really cares about ht-UDI? it's not existent on the Dreamcast IIRC
	case A7_REG_HASH(UDI_BASE_addr):
		switch(addr)
		{
			//UDI SDIR 0x1FF00000 0x1FF00000 16 0xFFFF Held Held Held Pclk
		case UDI_SDIR_addr :
			break;


			//UDI SDDR 0x1FF00008 0x1FF00008 32 Held Held Held Held Pclk
		case UDI_SDDR_addr :
			break;
		}
		break;
	}

	INFO_LOG(SH4, "Unknown Read from P4 mmr - addr=%x", addr);
	return 0;
}

//Write P4 memory-mapped registers
template <class T>
void DYNACALL WriteMem_p4mmr(u32 addr, T data)
{
	DEBUG_LOG(SH4, "write %s = %x", regName(addr), (int)data);

	if (likely(addr == 0xFF000038))
	{
		CCN_QACR_write<0>(addr, data);
		return;
	}
	if (likely(addr == 0xFF00003C))
	{
		CCN_QACR_write<1>(addr, data);
		return;
	}	

	addr &= 0x1FFFFFFF;
	u32 map_base = addr >> 16;
	switch (map_base)
	{

	case A7_REG_HASH(CCN_BASE_addr):
		if (addr <= 0x1F00003C)
			sh4_rio_write(CCN, addr, data);
		else
			OUT_OF_RANGE("CCN");
		return;

	case A7_REG_HASH(UBC_BASE_addr):
		if (addr <= 0x1F200020)
			sh4_rio_write(UBC, addr, data);
		else
			OUT_OF_RANGE("UBC");
		return;

	case A7_REG_HASH(BSC_BASE_addr):
		if (addr <= 0x1F800048)
			sh4_rio_write(BSC, addr, data);
		else
			OUT_OF_RANGE("BSC");
		return;
	case A7_REG_HASH(BSC_SDMR2_addr):
		//dram settings 2 / write only
		return;

	case A7_REG_HASH(BSC_SDMR3_addr):
		//dram settings 3 / write only
		return;

	case A7_REG_HASH(DMAC_BASE_addr):
		if (addr <= 0x1FA00040)
			sh4_rio_write(DMAC, addr, data);
		else
			OUT_OF_RANGE("DMAC");
		return;

	case A7_REG_HASH(CPG_BASE_addr):
		if (addr <= 0x1FC00010)
			sh4_rio_write(CPG, addr, data);
		else
			OUT_OF_RANGE("CPG");
		return;

	case A7_REG_HASH(RTC_BASE_addr):
		if (addr <= 0x1FC8003C)
			sh4_rio_write(RTC, addr, data);
		else
			OUT_OF_RANGE("RTC");
		return;

	case A7_REG_HASH(INTC_BASE_addr):
		if (addr <= 0x1FD00010)
			sh4_rio_write(INTC, addr, data);
		else
			OUT_OF_RANGE("INTC");
		return;

	case A7_REG_HASH(TMU_BASE_addr):
		if (addr <= 0x1FD8002C)
			sh4_rio_write(TMU, addr, data);
		else
			OUT_OF_RANGE("TMU");
		return;

	case A7_REG_HASH(SCI_BASE_addr):
		if (addr <= 0x1FE0001C)
			sh4_rio_write(SCI, addr, data);
		else
			OUT_OF_RANGE("SCI");
		return;

	case A7_REG_HASH(SCIF_BASE_addr):
		if (addr <= 0x1FE80024)
			sh4_rio_write(SCIF, addr, data);
		else
			OUT_OF_RANGE("SCIF");
		return;

		//who really cares about ht-udi ? it's not existent on dc iirc ..
	case A7_REG_HASH(UDI_BASE_addr):
		switch(addr)
		{
			//UDI SDIR 0xFFF00000 0x1FF00000 16 0xFFFF Held Held Held Pclk
		case UDI_SDIR_addr :
			break;


			//UDI SDDR 0xFFF00008 0x1FF00008 32 Held Held Held Held Pclk
		case UDI_SDDR_addr :
			break;
		}
		break;
	}

	INFO_LOG(SH4, "Write to P4 mmr not implemented, addr=%x, data=%x", addr, data);
}


//***********
//On Chip Ram
//***********
template <class T>
T DYNACALL ReadMem_area7_OCR(u32 addr)
{
	if (CCN_CCR.ORA == 1)
		return *(T *)&OnChipRAM[addr & OnChipRAM_MASK];

	INFO_LOG(SH4, "On Chip Ram Read, but OCR is disabled. addr %x", addr);
	return 0;
}

template <class T>
void DYNACALL WriteMem_area7_OCR(u32 addr, T data)
{
	if (CCN_CCR.ORA == 1)
		*(T *)&OnChipRAM[addr & OnChipRAM_MASK] = data;
	else
		INFO_LOG(SH4, "On Chip Ram Write, but OCR is disabled. addr %x", addr);
}

template <class T>
static void init_regs(T& regs)
{
	for (auto& reg : regs)
	{
		reg.flags = RIO_NO_ACCESS;
		reg.readFunctionAddr = &sh4io_read_noacc;
		reg.writeFunctionAddr = &sh4io_write_noacc;
	}
}

//Init/Res/Term
void sh4_mmr_init()
{
	init_regs(CCN);
	init_regs(UBC);
	init_regs(BSC);
	init_regs(DMAC);
	init_regs(CPG);
	init_regs(RTC);
	init_regs(INTC);
	init_regs(TMU);
	init_regs(SCI);
	init_regs(SCIF);

	//initialise Register structs
	bsc_init();
	ccn_init();
	cpg_init();
	dmac_init();
	intc_init();
	rtc_init();
	serial_init();
	tmu_init();
	ubc_init();

	MMU_init();
}

void sh4_mmr_reset(bool hard)
{
	for (auto& reg : CCN)
		reg.reset();
	for (auto& reg : UBC)
		reg.reset();
	for (auto& reg : BSC)
		reg.reset();
	for (auto& reg : DMAC)
		reg.reset();
	for (auto& reg : CPG)
		reg.reset();
	for (auto& reg : RTC)
		reg.reset();
	for (auto& reg : INTC)
		reg.reset();
	for (auto& reg : TMU)
		reg.reset();
	for (auto& reg : SCI)
		reg.reset();
	for (auto& reg : SCIF)
		reg.reset();

	OnChipRAM = {};
	//Reset register values
	bsc_reset(true);
	ccn_reset(true);
	cpg_reset();
	dmac_reset();
	intc_reset();
	rtc_reset();
	serial_reset(hard);
	tmu_reset(true);
	ubc_reset();

	MMU_reset();
}

void sh4_mmr_term()
{
	MMU_term();

	ubc_term();
	tmu_term();
	serial_term();
	rtc_term();
	intc_term();
	dmac_term();
	cpg_term();
	ccn_term();
	bsc_term();
}

// AREA 7--Sh4 Regs
static _vmem_handler p4mmr_handler;
static _vmem_handler area7_ocr_handler;

void map_area7_init()
{
	p4mmr_handler = _vmem_register_handler_Template(ReadMem_p4mmr, WriteMem_p4mmr);
	area7_ocr_handler = _vmem_register_handler_Template(ReadMem_area7_OCR, WriteMem_area7_OCR);
}

void map_area7(u32 base)
{
	// on-chip RAM: 7C000000-7FFFFFFF
	if (base == 0x60)
		_vmem_map_handler(area7_ocr_handler, 0x7C, 0x7F);
}

//P4
void map_p4()
{
	//P4 Region :
	_vmem_handler p4_handler = _vmem_register_handler_Template(ReadMem_P4, WriteMem_P4);

	//register this before mmr and SQ so they overwrite it and handle em
	//default P4 handler
	//0xE0000000-0xFFFFFFFF
	_vmem_map_handler(p4_handler, 0xE0, 0xFF);

	//Store Queues -- Write only 32bit
	_vmem_map_block(sq_both, 0xE0, 0xE0, 63);
	_vmem_map_block(sq_both, 0xE1, 0xE1, 63);
	_vmem_map_block(sq_both, 0xE2, 0xE2, 63);
	_vmem_map_block(sq_both, 0xE3, 0xE3, 63);

	_vmem_map_handler(p4mmr_handler, 0xFF, 0xFF);
}
