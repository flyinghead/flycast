/*
	System bus registers
	This doesn't implement any functionality, only routing
*/

#include "sb.h"
#include "holly_intc.h"
#include "hw/aica/aica_if.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/maple/maple_if.h"
#include "hw/modem/modem.h"
#include "hw/naomi/naomi.h"
#include "hw/pvr/pvr_sb_regs.h"
#include "emulator.h"
#include "hw/bba/bba.h"
#include "serialize.h"

u32 sb_regs[0x540];
HollyRegisters hollyRegs;

//(addr>= 0x005F6800) && (addr<=0x005F7CFF) -> 0x1500 bytes -> 0x540 possible registers , 125 actually exist only
// System Control Reg.   //0x100 bytes
// Maple i/f Control Reg.//0x100 bytes
// GD-ROM                //0x100 bytes
// G1 i/f Control Reg.   //0x100 bytes
// G2 i/f Control Reg.   //0x100 bytes
// PVR i/f Control Reg.  //0x100 bytes
// much empty space

u32 SB_ISTNRM1;

#define SB_REG_NAME(r) { r##_addr, #r },
const std::map<u32, const char *> sb_reg_names = {
		SB_REG_NAME(SB_C2DSTAT)
		SB_REG_NAME(SB_C2DLEN)
		SB_REG_NAME(SB_C2DST)
		SB_REG_NAME(SB_SDSTAW)
		SB_REG_NAME(SB_SDBAAW)
		SB_REG_NAME(SB_SDWLT)
		SB_REG_NAME(SB_SDLAS)
		SB_REG_NAME(SB_SDST)
		SB_REG_NAME(SB_SDDIV)
		SB_REG_NAME(SB_DBREQM)
		SB_REG_NAME(SB_BAVLWC)
		SB_REG_NAME(SB_C2DPRYC)
		SB_REG_NAME(SB_C2DMAXL)
		SB_REG_NAME(SB_TFREM)
		SB_REG_NAME(SB_LMMODE0)
		SB_REG_NAME(SB_LMMODE1)
		SB_REG_NAME(SB_FFST)
		SB_REG_NAME(SB_SFRES)
		SB_REG_NAME(SB_SBREV)
		SB_REG_NAME(SB_RBSPLT)
		SB_REG_NAME(SB_ISTNRM)
		SB_REG_NAME(SB_ISTEXT)
		SB_REG_NAME(SB_ISTERR)
		SB_REG_NAME(SB_IML2NRM)
		SB_REG_NAME(SB_IML2EXT)
		SB_REG_NAME(SB_IML2ERR)
		SB_REG_NAME(SB_IML4NRM)
		SB_REG_NAME(SB_IML4EXT)
		SB_REG_NAME(SB_IML4ERR)
		SB_REG_NAME(SB_IML6NRM)
		SB_REG_NAME(SB_IML6EXT)
		SB_REG_NAME(SB_IML6ERR)
		SB_REG_NAME(SB_PDTNRM)
		SB_REG_NAME(SB_PDTEXT)
		SB_REG_NAME(SB_G2DTNRM)
		SB_REG_NAME(SB_G2DTEXT)
		SB_REG_NAME(SB_MDSTAR)
		SB_REG_NAME(SB_MDTSEL)
		SB_REG_NAME(SB_MDEN)
		SB_REG_NAME(SB_MDST)
		SB_REG_NAME(SB_MSYS)
		SB_REG_NAME(SB_MST)
		SB_REG_NAME(SB_MSHTCL)
		SB_REG_NAME(SB_MDAPRO)
		SB_REG_NAME(SB_MMSEL)
		SB_REG_NAME(SB_MTXDAD)
		SB_REG_NAME(SB_MRXDAD)
		SB_REG_NAME(SB_MRXDBD)
		SB_REG_NAME(SB_GDSTAR)
		SB_REG_NAME(SB_GDLEN)
		SB_REG_NAME(SB_GDDIR)
		SB_REG_NAME(SB_GDEN)
		SB_REG_NAME(SB_GDST)
		SB_REG_NAME(SB_G1RRC)
		SB_REG_NAME(SB_G1RWC)
		SB_REG_NAME(SB_G1FRC)
		SB_REG_NAME(SB_G1FWC)
		SB_REG_NAME(SB_G1CRC)
		SB_REG_NAME(SB_G1CWC)
		SB_REG_NAME(SB_G1GDRC)
		SB_REG_NAME(SB_G1GDWC)
		SB_REG_NAME(SB_G1SYSM)
		SB_REG_NAME(SB_G1CRDYC)
		SB_REG_NAME(SB_GDAPRO)
		SB_REG_NAME(SB_GDSTARD)
		SB_REG_NAME(SB_GDLEND)
		SB_REG_NAME(SB_ADSTAG)
		SB_REG_NAME(SB_ADSTAR)
		SB_REG_NAME(SB_ADLEN)
		SB_REG_NAME(SB_ADDIR)
		SB_REG_NAME(SB_ADTSEL)
		SB_REG_NAME(SB_ADEN)
		SB_REG_NAME(SB_ADST)
		SB_REG_NAME(SB_ADSUSP)
		SB_REG_NAME(SB_E1STAG)
		SB_REG_NAME(SB_E1STAR)
		SB_REG_NAME(SB_E1LEN)
		SB_REG_NAME(SB_E1DIR)
		SB_REG_NAME(SB_E1TSEL)
		SB_REG_NAME(SB_E1EN)
		SB_REG_NAME(SB_E1ST)
		SB_REG_NAME(SB_E1SUSP)
		SB_REG_NAME(SB_E2STAG)
		SB_REG_NAME(SB_E2STAR)
		SB_REG_NAME(SB_E2LEN)
		SB_REG_NAME(SB_E2DIR)
		SB_REG_NAME(SB_E2TSEL)
		SB_REG_NAME(SB_E2EN)
		SB_REG_NAME(SB_E2ST)
		SB_REG_NAME(SB_E2SUSP)
		SB_REG_NAME(SB_DDSTAG)
		SB_REG_NAME(SB_DDSTAR)
		SB_REG_NAME(SB_DDLEN)
		SB_REG_NAME(SB_DDDIR)
		SB_REG_NAME(SB_DDTSEL)
		SB_REG_NAME(SB_DDEN)
		SB_REG_NAME(SB_DDST)
		SB_REG_NAME(SB_DDSUSP)
		SB_REG_NAME(SB_G2ID)
		SB_REG_NAME(SB_G2DSTO)
		SB_REG_NAME(SB_G2TRTO)
		SB_REG_NAME(SB_G2MDMTO)
		SB_REG_NAME(SB_G2MDMW)
		SB_REG_NAME(SB_G2APRO)
		SB_REG_NAME(SB_ADSTAGD)
		SB_REG_NAME(SB_ADSTARD)
		SB_REG_NAME(SB_ADLEND)
		SB_REG_NAME(SB_E1STAGD)
		SB_REG_NAME(SB_E1STARD)
		SB_REG_NAME(SB_E1LEND)
		SB_REG_NAME(SB_E2STAGD)
		SB_REG_NAME(SB_E2STARD)
		SB_REG_NAME(SB_E2LEND)
		SB_REG_NAME(SB_DDSTAGD)
		SB_REG_NAME(SB_DDSTARD)
		SB_REG_NAME(SB_DDLEND)
		SB_REG_NAME(SB_PDSTAP)
		SB_REG_NAME(SB_PDSTAR)
		SB_REG_NAME(SB_PDLEN)
		SB_REG_NAME(SB_PDDIR)
		SB_REG_NAME(SB_PDTSEL)
		SB_REG_NAME(SB_PDEN)
		SB_REG_NAME(SB_PDST)
		SB_REG_NAME(SB_PDAPRO)
		SB_REG_NAME(SB_PDSTAPD)
		SB_REG_NAME(SB_PDSTARD)
		SB_REG_NAME(SB_PDLEND)
};
#undef SB_REG_NAME

static const char *regName(u32 addr)
{
	static char regName[10];
	auto it = sb_reg_names.find(addr & 0x7fffff); // (addr - 0x5f6800) & 0x1fff);
	if (it == sb_reg_names.end())
	{
		sprintf(regName, "?%06x", addr& 0x7fffff);
		return regName;
	}
	else
		return it->second;
}

u32 sb_ReadMem(u32 addr)
{
	// All Holly accesses are 32-bit for now
	u32 rv = hollyRegs.read<u32>(addr);

	if ((addr & 0xffffff) != 0x5f6c18) // SB_MDST
		DEBUG_LOG(HOLLY, "read %s.%c == %x", regName(addr),
				((addr >> 26) & 7) == 2 ? 'b' : (addr & 0x2000000) ? '1' : '0',
						rv);
	return rv;
}

void sb_WriteMem(u32 addr, u32 data)
{
	DEBUG_LOG(HOLLY, "write %s.%c = %x", regName(addr),
			((addr >> 26) & 7) == 2 ? 'b' : (addr & 0x2000000) ? '1' : '0',
					data);
	// All Holly accesses are 32-bit for now
	hollyRegs.write<u32>(addr, data);
}

static void sb_write_zero(u32 addr, u32 data)
{
	if (data != 0)
		INFO_LOG(HOLLY, "ERROR: non-zero write on register; offset=%x, data=%x", addr - SB_BASE, data);
}
static void sb_write_gdrom_unlock(u32 addr, u32 data)
{
	/* CS writes 0x42fe, AtomisWave 0xa677, Naomi Dev BIOS 0x3ff */
	if (data != 0 && data != 0x001fffff && data != 0x42fe && data != 0xa677 && data != 0x3ff)
		WARN_LOG(HOLLY, "ERROR: Unexpected GD-ROM unlock code: %x", data);
}

static u32 read_SB_FFST(u32 addr)
{
	return 0;
}

static void sb_write_SB_SFRES(u32 addr, u32 data)
{
	if ((u16)data==0x7611)
	{
		NOTICE_LOG(SH4, "SB/HOLLY: System reset requested");
		emu.requestReset();
	}
}

template <u32 reg_addr>
void sb_write_SUSP(u32 addr, u32 data)
{
	SB_REGN_32(reg_addr) = (SB_REGN_32(reg_addr) & 0xfffffffe) | (data & 1);
}

void HollyRegisters::init()
{
	super::init();

	//0x005F6800    SB_C2DSTAT  RW  ch2-DMA destination address
	setRW<SB_C2DSTAT_addr, u32, 0x03ffffe0, 0x10000000>();

	//0x005F6804    SB_C2DLEN   RW  ch2-DMA length
	setRW<SB_C2DLEN_addr, u32, 0x00ffffe0>();

	//0x005F6808    SB_C2DST    RW  ch2-DMA start
	// pvr

	//0x005F6810    SB_SDSTAW   RW  Sort-DMA start link table address
	setRW<SB_SDSTAW_addr, u32, 0x07ffffe0, 0x08000000>();

	//0x005F6814    SB_SDBAAW   RW  Sort-DMA link base address
	setRW<SB_SDBAAW_addr, u32, 0x07ffffe0, 0x08000000>();

	//0x005F6818    SB_SDWLT    RW  Sort-DMA link address bit width
	setRW<SB_SDWLT_addr, u32, 1>();

	//0x005F681C    SB_SDLAS    RW  Sort-DMA link address shift control
	setRW<SB_SDLAS_addr, u32, 1>();

	//0x005F6820    SB_SDST RW  Sort-DMA start
	// pvr

	//0x005F6860 SB_SDDIV R(?) Sort-DMA LAT index (guess)
	setReadOnly<SB_SDDIV_addr>();

	//0x005F6840    SB_DBREQM   RW  DBREQ# signal mask control
	setRW<SB_DBREQM_addr, u32, 1>();

	//0x005F6844    SB_BAVLWC   RW  BAVL# signal wait count
	setRW<SB_BAVLWC_addr, u32, 0x1f>();

	//0x005F6848    SB_C2DPRYC  RW  DMA (TA/Root Bus) priority count
	setRW<SB_C2DPRYC_addr, u32, 0xf>();

	//0x005F684C    SB_C2DMAXL  RW  ch2-DMA maximum burst length
	setRW<SB_C2DMAXL_addr, u32, 3>();

	//0x005F6880    SB_TFREM    R   TA FIFO remaining amount
	setReadOnly<SB_TFREM_addr>();

	//0x005F6884    SB_LMMODE0  RW  Via TA texture memory bus select 0
	setRW<SB_LMMODE0_addr, u32, 1>();

	//0x005F6888    SB_LMMODE1  RW  Via TA texture memory bus select 1
	setRW<SB_LMMODE1_addr, u32, 1>();

	//0x005F688C    SB_FFST     R   FIFO status
	setReadOnly<SB_FFST_addr>(read_SB_FFST);

	//0x005F6890    SB_SFRES    W   System reset
	setWriteOnly<SB_SFRES_addr>(sb_write_SB_SFRES);

	//0x005F689C    SB_SBREV    R   System bus revision number
	setReadOnly<SB_SBREV_addr>();

	//0x005F68A0    SB_RBSPLT   RW  SH4 Root Bus split enable
	setRW<SB_RBSPLT_addr, u32, 0x80000000>();


	//0x005F6900    SB_ISTNRM   RW  Normal interrupt status
	// holly_intc

	//0x005F6904    SB_ISTEXT   R   External interrupt status
	// holly_intc

	//0x005F6908    SB_ISTERR   RW  Error interrupt status
	// holly_intc

	//0x005F6910    SB_IML2NRM  RW  Level 2 normal interrupt mask
	// holly_intc

	//0x005F6914    SB_IML2EXT  RW  Level 2 external interrupt mask
	// holly_intc

	//0x005F6918    SB_IML2ERR  RW  Level 2 error interrupt mask
	// holly_intc

	//0x005F6920    SB_IML4NRM  RW  Level 4 normal interrupt mask
	// holly_intc

	//0x005F6924    SB_IML4EXT  RW  Level 4 external interrupt mask
	// holly_intc

	//0x005F6928    SB_IML4ERR  RW  Level 4 error interrupt mask
	// holly_intc

	//0x005F6930    SB_IML6NRM  RW  Level 6 normal interrupt mask
	// holly_intc

	//0x005F6934    SB_IML6EXT  RW  Level 6 external interrupt mask
	// holly_intc

	//0x005F6938    SB_IML6ERR  RW  Level 6 error interrupt mask
	// holly_intc

	//0x005F6940    SB_PDTNRM   RW  Normal interrupt PVR-DMA startup mask
	setRW<SB_PDTNRM_addr, u32, 0x003fffff>();

	//0x005F6944    SB_PDTEXT   RW  External interrupt PVR-DMA startup mask
	setRW<SB_PDTEXT_addr, u32, 0xf>();

	//0x005F6950    SB_G2DTNRM  RW  Normal interrupt G2-DMA startup mask
	setRW<SB_G2DTNRM_addr, u32, 0x003fffff>();

	//0x005F6954    SB_G2DTEXT  RW  External interrupt G2-DMA startup mask
	setRW<SB_G2DTEXT_addr, u32, 0xf>();


	//0x005F6C04    SB_MDSTAR   RW  Maple-DMA command table address
#ifndef STRICT_MODE
	setRW<SB_MDSTAR_addr, u32, 0x1fffffe0>();
#endif

	//0x005F6C10    SB_MDTSEL   RW  Maple-DMA trigger select
	setRW<SB_MDTSEL_addr, u32, 1>();

	//0x005F6C14    SB_MDEN     RW  Maple-DMA enable
	// maple

	//0x005F6C18    SB_MDST     RW  Maple-DMA start
	// maple

	//0x005F6C80    SB_MSYS     RW  Maple system control
	setRW<SB_MSYS_addr, u32, 0xffff130f>();

	//0x005F6C84    SB_MST      R   Maple status
	setReadOnly<SB_MST_addr>();

	//0x005F6C88    SB_MSHTCL   W   Maple-DMA hard trigger clear
	// maple

	//0x005F6C8C    SB_MDAPRO   W   Maple-DMA address range
	// maple

	//0x005F6CE8    SB_MMSEL    RW  Maple MSB selection
	setRW<SB_MMSEL_addr, u32, 1>();

	//0x005F6CF4    SB_MTXDAD   R   Maple Txd address counter
	setReadOnly<SB_MTXDAD_addr>();

	//0x005F6CF8    SB_MRXDAD   R   Maple Rxd address counter
	setReadOnly<SB_MRXDAD_addr>();

	//0x005F6CFC    SB_MRXDBD   R   Maple Rxd base address
	setReadOnly<SB_MRXDBD_addr>();


	//0x005F7404    SB_GDSTAR   RW  GD-DMA start address
	setRW<SB_GDSTAR_addr, u32, 0x1fffffe0>();

	//0x005F7408    SB_GDLEN    RW  GD-DMA length
	setRW<SB_GDLEN_addr, u32, 0x01ffffff>();

	//0x005F740C    SB_GDDIR    RW  GD-DMA direction
	setRW<SB_GDDIR_addr, u32, 1>();

	//0x005F7414    SB_GDEN     RW  GD-DMA enable
	// gdrom, naomi

	//0x005F7418    SB_GDST     RW  GD-DMA start
	// gdrom, naomi

	//0x005F7480    SB_G1RRC    W   System ROM read access timing
	setWriteOnly<SB_G1RRC_addr>();

	//0x005F7484    SB_G1RWC    W   System ROM write access timing
	setWriteOnly<SB_G1RWC_addr>();

	//0x005F7488    SB_G1FRC    W   Flash ROM read access timing
	setWriteOnly<SB_G1FRC_addr>();

	//0x005F748C    SB_G1FWC    W   Flash ROM write access timing
	setWriteOnly<SB_G1FWC_addr>();

	//0x005F7490    SB_G1CRC    W   GD PIO read access timing
	setWriteOnly<SB_G1CRC_addr>();

	//0x005F7494    SB_G1CWC    W   GD PIO write access timing
	setWriteOnly<SB_G1CWC_addr>();

	//0x005F74A0    SB_G1GDRC   W   GD-DMA read access timing
	setWriteOnly<SB_G1GDRC_addr>();

	//0x005F74A4    SB_G1GDWC   W   GD-DMA write access timing
	setWriteOnly<SB_G1GDWC_addr>();

	//0x005F74B0    SB_G1SYSM   R   System mode
	setRW<SB_G1SYSM_addr>();

	//0x005F74B4    SB_G1CRDYC  W   G1IORDY signal control
	setWriteOnly<SB_G1CRDYC_addr>();

	//0x005F74B8    SB_GDAPRO   W   GD-DMA address range
	setWriteOnly<SB_GDAPRO_addr>();

	//0x005F74F4    SB_GDSTARD  R   GD-DMA address count (on Root Bus)
	setReadOnly<SB_GDSTARD_addr>();

	//0x005F74F8    SB_GDLEND   R   GD-DMA transfer counter
	setReadOnly<SB_GDLEND_addr>();

#ifndef STRICT_MODE
	//0x005F7800    SB_ADSTAG   RW  AICA:G2-DMA G2 start address
	setRW<SB_ADSTAG_addr, u32, 0x1fffffe0>();

	//0x005F7804    SB_ADSTAR   RW  AICA:G2-DMA system memory start address
	setRW<SB_ADSTAR_addr, u32, 0x1fffffe0>();
#endif

	//0x005F7808    SB_ADLEN    RW  AICA:G2-DMA length
	setRW<SB_ADLEN_addr, u32, 0x81FFFFE0>();

	//0x005F780C    SB_ADDIR    RW  AICA:G2-DMA direction
	setRW<SB_ADDIR_addr, u32, 1>();

	//0x005F7810    SB_ADTSEL   RW  AICA:G2-DMA trigger select
	setRW<SB_ADTSEL_addr, u32, 7>();

	//0x005F7814    SB_ADEN     RW  AICA:G2-DMA enable
	setRW<SB_ADEN_addr, u32, 1>();

	//0x005F7818    SB_ADST     RW  AICA:G2-DMA start
	// aica

	//0x005F781C    SB_ADSUSP   RW  AICA:G2-DMA suspend
	setWriteHandler<SB_ADSUSP_addr>(sb_write_SUSP<SB_ADSUSP_addr>);

	//0x005F7820    SB_E1STAG   RW  Ext1:G2-DMA G2 start address
	// aica

	//0x005F7824    SB_E1STAR   RW  Ext1:G2-DMA system memory start address
	// aica

	//0x005F7828    SB_E1LEN    RW  Ext1:G2-DMA length
	setRW<SB_E1LEN_addr, u32, 0x81FFFFE0>();

	//0x005F782C    SB_E1DIR    RW  Ext1:G2-DMA direction
	setRW<SB_E1DIR_addr, u32, 1>();

	//0x005F7830    SB_E1TSEL   RW  Ext1:G2-DMA trigger select
	setRW<SB_E1TSEL_addr, u32, 7>();

	//0x005F7834    SB_E1EN     RW  Ext1:G2-DMA enable
	setRW<SB_E1EN_addr, u32, 1>();

	//0x005F7838    SB_E1ST     RW  Ext1:G2-DMA start
	// aica

	//0x005F783C    SB_E1SUSP   RW  Ext1: G2-DMA suspend
	setWriteHandler<SB_E1SUSP_addr>(sb_write_SUSP<SB_E1SUSP_addr>);

	//0x005F7840    SB_E2STAG   RW  Ext2:G2-DMA G2 start address
	// aica

	//0x005F7844    SB_E2STAR   RW  Ext2:G2-DMA system memory start address
	// aica

	//0x005F7848    SB_E2LEN    RW  Ext2:G2-DMA length
	setRW<SB_E2LEN_addr, u32, 0x81FFFFE0>();

	//0x005F784C    SB_E2DIR    RW  Ext2:G2-DMA direction
	setRW<SB_E2DIR_addr, u32, 1>();

	//0x005F7850    SB_E2TSEL   RW  Ext2:G2-DMA trigger select
	setRW<SB_E2TSEL_addr, u32, 7>();

	//0x005F7854    SB_E2EN     RW  Ext2:G2-DMA enable
	setRW<SB_E2EN_addr, u32, 1>();

	//0x005F7858    SB_E2ST     RW  Ext2:G2-DMA start
	// aica

	//0x005F785C    SB_E2SUSP   RW  Ext2: G2-DMA suspend
	setWriteHandler<SB_E2SUSP_addr>(sb_write_SUSP<SB_E2SUSP_addr>);

	//0x005F7860    SB_DDSTAG   RW  Dev:G2-DMA G2 start address
	// aica

	//0x005F7864    SB_DDSTAR   RW  Dev:G2-DMA system memory start address
	// aica

	//0x005F7868    SB_DDLEN    RW  Dev:G2-DMA length
	setRW<SB_DDLEN_addr, u32, 0x81FFFFE0>();

	//0x005F786C    SB_DDDIR    RW  Dev:G2-DMA direction
	setRW<SB_DDDIR_addr, u32, 1>();

	//0x005F7870    SB_DDTSEL   RW  Dev:G2-DMA trigger select
	setRW<SB_DDTSEL_addr, u32, 7>();

	//0x005F7874    SB_DDEN     RW  Dev:G2-DMA enable
	setRW<SB_DDEN_addr, u32, 1>();

	//0x005F7878    SB_DDST     RW  Dev:G2-DMA start
	// aica

	//0x005F787C    SB_DDSUSP   RW  Dev: G2-DMA suspend
	setWriteHandler<SB_DDSUSP_addr>(sb_write_SUSP<SB_DDSUSP_addr>);

	//0x005F7880    SB_G2ID     R   G2 bus version
	setReadOnly<SB_G2ID_addr>();

	//0x005F7890    SB_G2DSTO   RW  G2/DS timeout
	setRW<SB_G2DSTO_addr>();

	//0x005F7894    SB_G2TRTO   RW  G2/TR timeout
	setRW<SB_G2TRTO_addr>();

	//0x005F7898    SB_G2MDMTO  RW  Modem unit wait timeout
	setRW<SB_G2MDMTO_addr, u32, 8>();

	//0x005F789C    SB_G2MDMW   RW  Modem unit wait time
	setRW<SB_G2MDMW_addr, u32, 8>();

	//0x005F78BC    SB_G2APRO   W   G2-DMA address range
	// aica

	//0x005F78C0    SB_ADSTAGD  R   AICA-DMA address counter (on AICA)
	setReadOnly<SB_ADSTAGD_addr>();

	//0x005F78C4    SB_ADSTARD  R   AICA-DMA address counter (on root bus)
	setReadOnly<SB_ADSTARD_addr>();

	//0x005F78C8    SB_ADLEND   R   AICA-DMA transfer counter
	setReadOnly<SB_ADLEND_addr>();

	//0x005F78D0    SB_E1STAGD  R   Ext-DMA1 address counter (on Ext)
	setReadOnly<SB_E1STAGD_addr>();

	//0x005F78D4    SB_E1STARD  R   Ext-DMA1 address counter (on root bus)
	setReadOnly<SB_E1STARD_addr>();

	//0x005F78D8    SB_E1LEND   R   Ext-DMA1 transfer counter
	setReadOnly<SB_E1LEND_addr>();

	//0x005F78E0    SB_E2STAGD  R   Ext-DMA2 address counter (on Ext)
	setReadOnly<SB_E2STAGD_addr>();

	//0x005F78E4    SB_E2STARD  R   Ext-DMA2 address counter (on root bus)
	setReadOnly<SB_E2STARD_addr>();

	//0x005F78E8    SB_E2LEND   R   Ext-DMA2 transfer counter
	setReadOnly<SB_E2LEND_addr>();

	//0x005F78F0    SB_DDSTAGD  R   Dev-DMA address counter (on Ext)
	setReadOnly<SB_DDSTAGD_addr>();

	//0x005F78F4    SB_DDSTARD  R   Dev-DMA address counter (on root bus)
	setReadOnly<SB_DDSTARD_addr>();

	//0x005F78F8    SB_DDLEND   R   Dev-DMA transfer counter
	setReadOnly<SB_DDLEND_addr>();


	//0x005F7C00    SB_PDSTAP   RW  PVR-DMA PVR start address
	setRW<SB_PDSTAP_addr, u32, 0x1fffffe0>();

	//0x005F7C04    SB_PDSTAR   RW  PVR-DMA system memory start address
	setRW<SB_PDSTAR_addr, u32, 0x1fffffe0>();

	//0x005F7C08    SB_PDLEN    RW  PVR-DMA length
	setRW<SB_PDLEN_addr, u32, 0x00ffffe0>();

	//0x005F7C0C    SB_PDDIR    RW  PVR-DMA direction
	setRW<SB_PDDIR_addr, u32, 1>();

	//0x005F7C10    SB_PDTSEL   RW  PVR-DMA trigger select
	setRW<SB_PDTSEL_addr, u32, 1>();

	//0x005F7C14    SB_PDEN     RW  PVR-DMA enable
	setRW<SB_PDEN_addr, u32, 1>();

	//0x005F7C18    SB_PDST     RW  PVR-DMA start
	// pvr

	//0x005F7C80    SB_PDAPRO   W   PVR-DMA address range
	setWriteOnly<SB_PDAPRO_addr>();

	//0x005F7CF0    SB_PDSTAPD  R   PVR-DMA address counter (on Ext)
	setReadOnly<SB_PDSTAPD_addr>();

	//0x005F7CF4    SB_PDSTARD  R   PVR-DMA address counter (on root bus)
	setReadOnly<SB_PDSTARD_addr>();

	//0x005F7CF8    SB_PDLEND   R   PVR-DMA transfer counter
	setReadOnly<SB_PDLEND_addr>();

	//GDROM unlock register (bios checksumming, etc)
	//0x005f74e4
	setWriteOnly<0x005f74e4>(sb_write_gdrom_unlock);

	//0x005f68a4, 0x005f68ac, 0x005f78a0,0x005f78a4, 0x005f78a8, 0x005f78b0, 0x005f78b4, 0x005f78b8
	setWriteOnly<0x005f68a4>(sb_write_zero);
	setWriteOnly<0x005f68ac>(sb_write_zero);
	setWriteOnly<0x005f78a0>(sb_write_zero);
	setWriteOnly<0x005f78a4>(sb_write_zero);
	setWriteOnly<0x005f78a8>(sb_write_zero);
	setWriteOnly<0x005f78ac>(sb_write_zero);
	setWriteOnly<0x005f78b0>(sb_write_zero);
	setWriteOnly<0x005f78b4>(sb_write_zero);
	setWriteOnly<0x005f78b8>(sb_write_zero);

	reset();
}

void sb_Init()
{
	hollyRegs.init();
	asic_reg_Init();

	gdrom_reg_Init();
	naomi_reg_Init();

	pvr_sb_Init();
	maple_Init();
	aica::sbInit();

	bba_Init();
	ModemInit();
}

void sb_Reset(bool hard)
{
	if (hard)
	{
		hollyRegs.reset();

		SB_SBREV = 0xB;
		SB_G2ID = 0x12;
		SB_G1SYSM = ((0 << 4) | 1);
		SB_TFREM = 8;
		SB_PDAPRO = 0x7f00;
		SB_GDAPRO = 0x7f00;
	}
	SB_ISTNRM = 0;
	SB_ISTNRM1 = 0;

	bba_Reset(hard);
	ModemReset();

	asic_reg_Reset(hard);
	if (settings.platform.isConsole())
		gdrom_reg_Reset(hard);
	else
		naomi_reg_Reset(hard);
	pvr_sb_Reset(hard);
	maple_Reset(hard);
	aica::sbReset(hard);
}

void sb_Term()
{
	bba_Term();
	ModemTerm();
	aica::sbTerm();
	maple_Term();
	pvr_sb_Term();
	gdrom_reg_Term();
	naomi_reg_Term();
	asic_reg_Term();
	hollyRegs.term();
}

void sb_serialize(Serializer& ser)
{
	ser << sb_regs;
	ser << SB_ISTNRM1;
}

void sb_deserialize(Deserializer& deser)
{
	if (deser.version() <= Deserializer::VLAST_LIBRETRO)
	{
		for (u32& reg : sb_regs)
		{
			deser.skip<u32>(); // regs.data[i].flags
			deser >> reg;
		}
	}
	else
	{
		deser >> sb_regs;
	}
	if (deser.version() < Deserializer::V33)
		deser >> SB_ISTNRM;
	if (deser.version() >= Deserializer::V24)
		deser >> SB_ISTNRM1;
	else
		SB_ISTNRM1 = 0;
	if (deser.version() < Deserializer::V33)
	{
		if (deser.version() < Deserializer::V30)
		{
			deser.skip<u32>(); // SB_FFST_rc;
			deser.skip<u32>(); // SB_FFST;
		}
		if (deser.version() >= Deserializer::V15)
			deser >> SB_ADST;
		else
			SB_ADST = 0;
	}
}
