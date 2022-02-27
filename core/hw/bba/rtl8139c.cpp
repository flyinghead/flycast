/**
 * QEMU RTL8139 emulation
 *
 * Copyright (c) 2006 Igor Kovalenko
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * Modifications:
 *  2006-Jan-28  Mark Malakanov :   TSAD and CSCR implementation (for Windows driver)
 *
 *  2006-Apr-28  Juergen Lock   :   EEPROM emulation changes for FreeBSD driver
 *                                  HW revision ID changes for FreeBSD driver
 *
 *  2006-Jul-01  Igor Kovalenko :   Implemented loopback mode for FreeBSD driver
 *                                  Corrected packet transfer reassembly routine for 8139C+ mode
 *                                  Rearranged debugging print statements
 *                                  Implemented PCI timer interrupt (disabled by default)
 *                                  Implemented Tally Counters, increased VM load/save version
 *                                  Implemented IP/TCP/UDP checksum task offloading
 *
 *  2006-Jul-04  Igor Kovalenko :   Implemented TCP segmentation offloading
 *                                  Fixed MTU=1500 for produced ethernet frames
 *
 *  2006-Jul-09  Igor Kovalenko :   Fixed TCP header length calculation while processing
 *                                  segmentation offloading
 *                                  Removed slirp.h dependency
 *                                  Added rx/tx buffer reset when enabling rx/tx operation
 *
 *  2010-Feb-04  Frediano Ziglio:   Rewrote timer support using QEMU timer only
 *                                  when strictly needed (required for
 *                                  Darwin)
 *  2011-Mar-22  Benjamin Poirier:  Implemented VLAN offloading
 */

/* For crc32 */
#include <zlib.h>
#include "rtl8139c.h"
#include "serialize.h"

/* debug RTL8139 card */
//#define DEBUG_RTL8139 1

#define PCI_PERIOD 30    /* 30 ns period = 33.333333 Mhz frequency */

#define SET_MASKED(input, mask, curr) \
    ( ( (input) & ~(mask) ) | ( (curr) & (mask) ) )

/* arg % size for size which is a power of 2 */
#define MOD2(input, size) \
    ( ( input ) & ( size - 1 )  )

#define ETHER_TYPE_LEN 2
#define ETH_MTU     1500

#define VLAN_TCI_LEN 2
#define VLAN_HLEN (ETHER_TYPE_LEN + VLAN_TCI_LEN)

#if defined (DEBUG_RTL8139)
#define DPRINTF(fmt, ...) DEBUG_LOG(NETWORK, fmt, ##__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#define TYPE_RTL8139 "rtl8139"

/* Symbolic offsets to registers. */
enum RTL8139_registers {
    MAC0 = 0,        /* Ethernet hardware address. */
    MAR0 = 8,        /* Multicast filter. */
    TxStatus0 = 0x10,/* Transmit status (Four 32bit registers). C mode only */
                     /* Dump Tally Conter control register(64bit). C+ mode only */
    TxAddr0 = 0x20,  /* Tx descriptors (also four 32bit). */
    RxBuf = 0x30,
    ChipCmd = 0x37,
    RxBufPtr = 0x38,
    RxBufAddr = 0x3A,
    IntrMask = 0x3C,
    IntrStatus = 0x3E,
    TxConfig = 0x40,
    RxConfig = 0x44,
    Timer = 0x48,        /* A general-purpose counter. */
    RxMissed = 0x4C,    /* 24 bits valid, write clears. */
    Cfg9346 = 0x50,
    Config0 = 0x51,
    Config1 = 0x52,
    FlashReg = 0x54,
    MediaStatus = 0x58,
    Config3 = 0x59,
    Config4 = 0x5A,        /* absent on RTL-8139A */
    HltClk = 0x5B,
    MultiIntr = 0x5C,
    PCIRevisionID = 0x5E,
    TxSummary = 0x60, /* TSAD register. Transmit Status of All Descriptors*/
    BasicModeCtrl = 0x62,
    BasicModeStatus = 0x64,
    NWayAdvert = 0x66,
    NWayLPAR = 0x68,
    NWayExpansion = 0x6A,
    /* Undocumented registers, but required for proper operation. */
    FIFOTMS = 0x70,        /* FIFO Control and test. */
    CSCR = 0x74,        /* Chip Status and Configuration Register. */
    PARA78 = 0x78,
    PARA7c = 0x7c,        /* Magic transceiver parameter register. */
    Config5 = 0xD8,        /* absent on RTL-8139A */
    /* C+ mode */
    TxPoll        = 0xD9,    /* Tell chip to check Tx descriptors for work */
    RxMaxSize    = 0xDA, /* Max size of an Rx packet (8169 only) */
    CpCmd        = 0xE0, /* C+ Command register (C+ mode only) */
    IntrMitigate    = 0xE2,    /* rx/tx interrupt mitigation control */
    RxRingAddrLO    = 0xE4, /* 64-bit start addr of Rx ring */
    RxRingAddrHI    = 0xE8, /* 64-bit start addr of Rx ring */
    TxThresh    = 0xEC, /* Early Tx threshold */
};

enum ClearBitMasks {
    MultiIntrClear = 0xF000,
    ChipCmdClear = 0xE2,
    Config1Clear = (1<<7)|(1<<6)|(1<<3)|(1<<2)|(1<<1),
};

enum ChipCmdBits {
    CmdReset = 0x10,
    CmdRxEnb = 0x08,
    CmdTxEnb = 0x04,
    RxBufEmpty = 0x01,
};

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatusBits {
    PCIErr = 0x8000,
    PCSTimeout = 0x4000,
    RxFIFOOver = 0x40,
    RxUnderrun = 0x20, /* Packet Underrun / Link Change */
    RxOverflow = 0x10,
    TxErr = 0x08,
    TxOK = 0x04,
    RxErr = 0x02,
    RxOK = 0x01,

    RxAckBits = RxFIFOOver | RxOverflow | RxOK,
};

enum TxStatusBits {
    TxHostOwns = 0x2000,
    TxUnderrun = 0x4000,
    TxStatOK = 0x8000,
    TxOutOfWindow = 0x20000000,
    TxAborted = 0x40000000,
    TxCarrierLost = 0x80000000,
};
enum RxStatusBits {
    RxMulticast = 0x8000,
    RxPhysical = 0x4000,
    RxBroadcast = 0x2000,
    RxBadSymbol = 0x0020,
    RxRunt = 0x0010,
    RxTooLong = 0x0008,
    RxCRCErr = 0x0004,
    RxBadAlign = 0x0002,
    RxStatusOK = 0x0001,
};

/* Bits in RxConfig. */
enum rx_mode_bits {
    AcceptErr = 0x20,
    AcceptRunt = 0x10,
    AcceptBroadcast = 0x08,
    AcceptMulticast = 0x04,
    AcceptMyPhys = 0x02,
    AcceptAllPhys = 0x01,
};

/* Bits in TxConfig. */
enum tx_config_bits {

        /* Interframe Gap Time. Only TxIFG96 doesn't violate IEEE 802.3 */
        TxIFGShift = 24,
        TxIFG84 = (0 << TxIFGShift),    /* 8.4us / 840ns (10 / 100Mbps) */
        TxIFG88 = (1 << TxIFGShift),    /* 8.8us / 880ns (10 / 100Mbps) */
        TxIFG92 = (2 << TxIFGShift),    /* 9.2us / 920ns (10 / 100Mbps) */
        TxIFG96 = (3 << TxIFGShift),    /* 9.6us / 960ns (10 / 100Mbps) */

    TxLoopBack = (1 << 18) | (1 << 17), /* enable loopback test mode */
    TxCRC = (1 << 16),    /* DISABLE appending CRC to end of Tx packets */
    TxClearAbt = (1 << 0),    /* Clear abort (WO) */
    TxDMAShift = 8,        /* DMA burst value (0-7) is shifted this many bits */
    TxRetryShift = 4,    /* TXRR value (0-15) is shifted this many bits */

    TxVersionMask = 0x7C800000, /* mask out version bits 30-26, 23 */
};


/* Transmit Status of All Descriptors (TSAD) Register */
enum TSAD_bits {
 TSAD_TOK3 = 1<<15, // TOK bit of Descriptor 3
 TSAD_TOK2 = 1<<14, // TOK bit of Descriptor 2
 TSAD_TOK1 = 1<<13, // TOK bit of Descriptor 1
 TSAD_TOK0 = 1<<12, // TOK bit of Descriptor 0
 TSAD_TUN3 = 1<<11, // TUN bit of Descriptor 3
 TSAD_TUN2 = 1<<10, // TUN bit of Descriptor 2
 TSAD_TUN1 = 1<<9, // TUN bit of Descriptor 1
 TSAD_TUN0 = 1<<8, // TUN bit of Descriptor 0
 TSAD_TABT3 = 1<<07, // TABT bit of Descriptor 3
 TSAD_TABT2 = 1<<06, // TABT bit of Descriptor 2
 TSAD_TABT1 = 1<<05, // TABT bit of Descriptor 1
 TSAD_TABT0 = 1<<04, // TABT bit of Descriptor 0
 TSAD_OWN3 = 1<<03, // OWN bit of Descriptor 3
 TSAD_OWN2 = 1<<02, // OWN bit of Descriptor 2
 TSAD_OWN1 = 1<<01, // OWN bit of Descriptor 1
 TSAD_OWN0 = 1<<00, // OWN bit of Descriptor 0
};


/* Bits in Config1 */
enum Config1Bits {
    Cfg1_PM_Enable = 0x01,
    Cfg1_VPD_Enable = 0x02,
    Cfg1_PIO = 0x04,
    Cfg1_MMIO = 0x08,
    LWAKE = 0x10,        /* not on 8139, 8139A */
    Cfg1_Driver_Load = 0x20,
    Cfg1_LED0 = 0x40,
    Cfg1_LED1 = 0x80,
    SLEEP = (1 << 1),    /* only on 8139, 8139A */
    PWRDN = (1 << 0),    /* only on 8139, 8139A */
};

/* Bits in Config3 */
enum Config3Bits {
    Cfg3_FBtBEn    = (1 << 0), /* 1 = Fast Back to Back */
    Cfg3_FuncRegEn = (1 << 1), /* 1 = enable CardBus Function registers */
    Cfg3_CLKRUN_En = (1 << 2), /* 1 = enable CLKRUN */
    Cfg3_CardB_En  = (1 << 3), /* 1 = enable CardBus registers */
    Cfg3_LinkUp    = (1 << 4), /* 1 = wake up on link up */
    Cfg3_Magic     = (1 << 5), /* 1 = wake up on Magic Packet (tm) */
    Cfg3_PARM_En   = (1 << 6), /* 0 = software can set twister parameters */
    Cfg3_GNTSel    = (1 << 7), /* 1 = delay 1 clock from PCI GNT signal */
};

/* Bits in Config4 */
enum Config4Bits {
    LWPTN = (1 << 2),    /* not on 8139, 8139A */
};

/* Bits in Config5 */
enum Config5Bits {
    Cfg5_PME_STS     = (1 << 0), /* 1 = PCI reset resets PME_Status */
    Cfg5_LANWake     = (1 << 1), /* 1 = enable LANWake signal */
    Cfg5_LDPS        = (1 << 2), /* 0 = save power when link is down */
    Cfg5_FIFOAddrPtr = (1 << 3), /* Realtek internal SRAM testing */
    Cfg5_UWF         = (1 << 4), /* 1 = accept unicast wakeup frame */
    Cfg5_MWF         = (1 << 5), /* 1 = accept multicast wakeup frame */
    Cfg5_BWF         = (1 << 6), /* 1 = accept broadcast wakeup frame */
};

enum RxConfigBits {
    /* rx fifo threshold */
    RxCfgFIFOShift = 13,
    RxCfgFIFONone = (7 << RxCfgFIFOShift),

    /* Max DMA burst */
    RxCfgDMAShift = 8,
    RxCfgDMAUnlimited = (7 << RxCfgDMAShift),

    /* rx ring buffer length */
    RxCfgRcv8K = 0,
    RxCfgRcv16K = (1 << 11),
    RxCfgRcv32K = (1 << 12),
    RxCfgRcv64K = (1 << 11) | (1 << 12),

    /* Disable packet wrap at end of Rx buffer. (not possible with 64k) */
    RxNoWrap = (1 << 7),
};

/* Twister tuning parameters from RealTek.
   Completely undocumented, but required to tune bad links on some boards. */
/*
enum CSCRBits {
    CSCR_LinkOKBit = 0x0400,
    CSCR_LinkChangeBit = 0x0800,
    CSCR_LinkStatusBits = 0x0f000,
    CSCR_LinkDownOffCmd = 0x003c0,
    CSCR_LinkDownCmd = 0x0f3c0,
*/
enum CSCRBits {
    CSCR_Testfun = 1<<15, /* 1 = Auto-neg speeds up internal timer, WO, def 0 */
    CSCR_LD  = 1<<9,  /* Active low TPI link disable signal. When low, TPI still transmits link pulses and TPI stays in good link state. def 1*/
    CSCR_HEART_BIT = 1<<8,  /* 1 = HEART BEAT enable, 0 = HEART BEAT disable. HEART BEAT function is only valid in 10Mbps mode. def 1*/
    CSCR_JBEN = 1<<7,  /* 1 = enable jabber function. 0 = disable jabber function, def 1*/
    CSCR_F_LINK_100 = 1<<6, /* Used to login force good link in 100Mbps for diagnostic purposes. 1 = DISABLE, 0 = ENABLE. def 1*/
    CSCR_F_Connect  = 1<<5,  /* Assertion of this bit forces the disconnect function to be bypassed. def 0*/
    CSCR_Con_status = 1<<3, /* This bit indicates the status of the connection. 1 = valid connected link detected; 0 = disconnected link detected. RO def 0*/
    CSCR_Con_status_En = 1<<2, /* Assertion of this bit configures LED1 pin to indicate connection status. def 0*/
    CSCR_PASS_SCR = 1<<0, /* Bypass Scramble, def 0*/
};

enum Cfg9346Bits {
    Cfg9346_Normal = 0x00,
    Cfg9346_Autoload = 0x40,
    Cfg9346_Programming = 0x80,
    Cfg9346_ConfigWrite = 0xC0,
};

typedef enum {
    CH_8139 = 0,
    CH_8139_K,
    CH_8139A,
    CH_8139A_G,
    CH_8139B,
    CH_8130,
    CH_8139C,
    CH_8100,
    CH_8100B_8139D,
    CH_8101,
} chip_t;

enum chip_flags {
    HasHltClk = (1 << 0),
    HasLWake = (1 << 1),
};

#define HW_REVID(b30, b29, b28, b27, b26, b23, b22) \
    (b30<<30 | b29<<29 | b28<<28 | b27<<27 | b26<<26 | b23<<23 | b22<<22)
#define HW_REVID_MASK    HW_REVID(1, 1, 1, 1, 1, 1, 1)

#define RTL8139_PCI_REVID_8139      0x10
#define RTL8139_PCI_REVID_8139CPLUS 0x20

#define RTL8139_PCI_REVID           RTL8139_PCI_REVID_8139

/* Size is 64 * 16bit words */
#define EEPROM_9346_ADDR_BITS 6
#define EEPROM_9346_SIZE  (1 << EEPROM_9346_ADDR_BITS)
#define EEPROM_9346_ADDR_MASK (EEPROM_9346_SIZE - 1)

enum Chip9346Operation
{
    Chip9346_op_mask = 0xc0,          /* 10 zzzzzz */
    Chip9346_op_read = 0x80,          /* 10 AAAAAA */
    Chip9346_op_write = 0x40,         /* 01 AAAAAA D(15)..D(0) */
    Chip9346_op_ext_mask = 0xf0,      /* 11 zzzzzz */
    Chip9346_op_write_enable = 0x30,  /* 00 11zzzz */
    Chip9346_op_write_all = 0x10,     /* 00 01zzzz */
    Chip9346_op_write_disable = 0x00, /* 00 00zzzz */
};

enum Chip9346Mode
{
    Chip9346_none = 0,
    Chip9346_enter_command_mode,
    Chip9346_read_command,
    Chip9346_data_read,      /* from output register */
    Chip9346_data_write,     /* to input register, then to contents at specified address */
    Chip9346_data_write_all, /* to input register, then filling contents */
};

typedef struct EEprom9346
{
    uint16_t contents[EEPROM_9346_SIZE];
    int      mode;
    uint32_t tick;
    uint8_t  address;
    uint16_t input;
    uint16_t output;

    uint8_t eecs;
    uint8_t eesk;
    uint8_t eedi;
    uint8_t eedo;
} EEprom9346;

struct RTL8139State {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    uint8_t phys[8]; /* mac address */
    uint8_t mult[8]; /* multicast mask array */

    uint32_t TxStatus[4]; /* TxStatus0 in C mode*/ /* also DTCCR[0] and DTCCR[1] in C+ mode */
    uint32_t TxAddr[4];   /* TxAddr0 */
    uint32_t RxBuf;       /* Receive buffer */
    uint32_t RxBufferSize;/* internal variable, receive ring buffer size in C mode */
    uint32_t RxBufPtr;
    uint32_t RxBufAddr;

    uint16_t IntrStatus;
    uint16_t IntrMask;

    uint32_t TxConfig;
    uint32_t RxConfig;
    uint32_t RxMissed;

    uint16_t CSCR;

    uint8_t  Cfg9346;
    uint8_t  Config0;
    uint8_t  Config1;
    uint8_t  Config3;
    uint8_t  Config4;
    uint8_t  Config5;

    uint8_t  clock_enabled;
    uint8_t  bChipCmdState;

    uint16_t MultiIntr;

    uint16_t BasicModeCtrl;
    uint16_t BasicModeStatus;
    uint16_t NWayAdvert;
    uint16_t NWayLPAR;
    uint16_t NWayExpansion;

    NICConf conf;

    /* C ring mode */
    uint32_t   currTxDesc;

    EEprom9346 eeprom;

    uint32_t   TCTR;
    uint32_t   TimerInt;
    int64_t    TCTR_base;

    /* PCI interrupt timer */
#ifdef RTL8139_TIMER
    QEMUTimer *timer;
#endif

    MemoryRegion bar_io;
    MemoryRegion bar_mem;
};

static void rtl8139_set_next_tctr_time(RTL8139State *s);

static void prom9346_decode_command(EEprom9346 *eeprom, uint8_t command)
{
    DPRINTF("eeprom command 0x%02x", command);

    switch (command & Chip9346_op_mask)
    {
        case Chip9346_op_read:
        {
            eeprom->address = command & EEPROM_9346_ADDR_MASK;
            eeprom->output = eeprom->contents[eeprom->address];
            eeprom->eedo = 0;
            eeprom->tick = 0;
            eeprom->mode = Chip9346_data_read;
            DPRINTF("eeprom read from address 0x%02x data=0x%04x",
                eeprom->address, eeprom->output);
        }
        break;

        case Chip9346_op_write:
        {
            eeprom->address = command & EEPROM_9346_ADDR_MASK;
            eeprom->input = 0;
            eeprom->tick = 0;
            eeprom->mode = Chip9346_none; /* Chip9346_data_write */
            DPRINTF("eeprom begin write to address 0x%02x",
                eeprom->address);
        }
        break;
        default:
            eeprom->mode = Chip9346_none;
            switch (command & Chip9346_op_ext_mask)
            {
                case Chip9346_op_write_enable:
                    DPRINTF("eeprom write enabled");
                    break;
                case Chip9346_op_write_all:
                    DPRINTF("eeprom begin write all");
                    break;
                case Chip9346_op_write_disable:
                    DPRINTF("eeprom write disabled");
                    break;
            }
            break;
    }
}

static void prom9346_shift_clock(EEprom9346 *eeprom)
{
    int bit = eeprom->eedi?1:0;

    ++ eeprom->tick;

    DPRINTF("eeprom: tick %d eedi=%d eedo=%d", eeprom->tick, eeprom->eedi,
        eeprom->eedo);

    switch (eeprom->mode)
    {
        case Chip9346_enter_command_mode:
            if (bit)
            {
                eeprom->mode = Chip9346_read_command;
                eeprom->tick = 0;
                eeprom->input = 0;
                DPRINTF("eeprom: +++ synchronized, begin command read");
            }
            break;

        case Chip9346_read_command:
            eeprom->input = (eeprom->input << 1) | (bit & 1);
            if (eeprom->tick == 8)
            {
                prom9346_decode_command(eeprom, eeprom->input & 0xff);
            }
            break;

        case Chip9346_data_read:
            eeprom->eedo = (eeprom->output & 0x8000)?1:0;
            eeprom->output <<= 1;
            if (eeprom->tick == 16)
            {
#if 1
        // the FreeBSD drivers (rl and re) don't explicitly toggle
        // CS between reads (or does setting Cfg9346 to 0 count too?),
        // so we need to enter wait-for-command state here
                eeprom->mode = Chip9346_enter_command_mode;
                eeprom->input = 0;
                eeprom->tick = 0;

                DPRINTF("eeprom: +++ end of read, awaiting next command");
#else
        // original behaviour
                ++eeprom->address;
                eeprom->address &= EEPROM_9346_ADDR_MASK;
                eeprom->output = eeprom->contents[eeprom->address];
                eeprom->tick = 0;
                DPRINTF("eeprom: +++ read next address 0x%02x data=0x%04x",
                    eeprom->address, eeprom->output);
#endif
            }
            break;

        case Chip9346_data_write:
            eeprom->input = (eeprom->input << 1) | (bit & 1);
            if (eeprom->tick == 16)
            {
                DPRINTF("eeprom write to address 0x%02x data=0x%04x",
                    eeprom->address, eeprom->input);

                eeprom->contents[eeprom->address] = eeprom->input;
                eeprom->mode = Chip9346_none; /* waiting for next command after CS cycle */
                eeprom->tick = 0;
                eeprom->input = 0;
            }
            break;

        case Chip9346_data_write_all:
            eeprom->input = (eeprom->input << 1) | (bit & 1);
            if (eeprom->tick == 16)
            {
                int i;
                for (i = 0; i < EEPROM_9346_SIZE; i++)
                {
                    eeprom->contents[i] = eeprom->input;
                }
                DPRINTF("eeprom filled with data=0x%04x", eeprom->input);

                eeprom->mode = Chip9346_enter_command_mode;
                eeprom->tick = 0;
                eeprom->input = 0;
            }
            break;

        default:
            break;
    }
}

static int prom9346_get_wire(RTL8139State *s)
{
    EEprom9346 *eeprom = &s->eeprom;
    if (!eeprom->eecs)
        return 0;

    return eeprom->eedo;
}

static void prom9346_set_wire(RTL8139State *s, int eecs, int eesk, int eedi)
{
    EEprom9346 *eeprom = &s->eeprom;
    uint8_t old_eecs = eeprom->eecs;
    uint8_t old_eesk = eeprom->eesk;

    eeprom->eecs = eecs;
    eeprom->eesk = eesk;
    eeprom->eedi = eedi;

    DPRINTF("eeprom: +++ wires CS=%d SK=%d DI=%d DO=%d", eeprom->eecs,
        eeprom->eesk, eeprom->eedi, eeprom->eedo);

    if (!old_eecs && eecs)
    {
        /* Synchronize start */
        eeprom->tick = 0;
        eeprom->input = 0;
        eeprom->output = 0;
        eeprom->mode = Chip9346_enter_command_mode;

        DPRINTF("=== eeprom: begin access, enter command mode");
    }

    if (!eecs)
    {
        DPRINTF("=== eeprom: end access");
        return;
    }

    if (!old_eesk && eesk)
    {
        /* SK front rules */
        prom9346_shift_clock(eeprom);
    }
}

static void rtl8139_update_irq(RTL8139State *s)
{
    PCIDevice *d = PCI_DEVICE(s);
    int isr;
    isr = (s->IntrStatus & s->IntrMask) & 0xffff;

    DPRINTF("Set IRQ to %d (%04x %04x)", isr ? 1 : 0, s->IntrStatus,
        s->IntrMask);

    pci_set_irq(d, (isr != 0));
}

static int rtl8139_RxWrap(RTL8139State *s)
{
    /* wrapping enabled; assume 1.5k more buffer space if size < 65536 */
    return (s->RxConfig & (1 << 7));
}

static int rtl8139_receiver_enabled(RTL8139State *s)
{
    return s->bChipCmdState & CmdRxEnb;
}

static int rtl8139_transmitter_enabled(RTL8139State *s)
{
    return s->bChipCmdState & CmdTxEnb;
}

static void rtl8139_write_buffer(RTL8139State *s, const void *buf, int size)
{
    PCIDevice *d = PCI_DEVICE(s);

    if (s->RxBufAddr + size > s->RxBufferSize)
    {
        int wrapped = MOD2(s->RxBufAddr + size, s->RxBufferSize);

        /* write packet data */
        if (wrapped && !(s->RxBufferSize < 65536 && rtl8139_RxWrap(s)))
        {
            DPRINTF(">>> rx packet wrapped in buffer at %d", size - wrapped);

            if (size > wrapped)
            {
                pci_dma_write(d, s->RxBuf + s->RxBufAddr,
                              buf, size-wrapped);
            }

            /* reset buffer pointer */
            s->RxBufAddr = 0;

            pci_dma_write(d, s->RxBuf + s->RxBufAddr,
                          (uint8_t*)buf + (size-wrapped), wrapped);

            s->RxBufAddr = wrapped;

            return;
        }
    }

    /* non-wrapping path or overwrapping enabled */
    pci_dma_write(d, s->RxBuf + s->RxBufAddr, buf, size);

    s->RxBufAddr += size;
}

#define MIN_BUF_SIZE 60

bool rtl8139_can_receive(RTL8139State *s)
{
    int avail;

    /* Receive (drop) packets if card is disabled.  */
    if (!s->clock_enabled) {
        return true;
    }
    if (!rtl8139_receiver_enabled(s)) {
        return true;
    }

    avail = MOD2(s->RxBufferSize + s->RxBufPtr - s->RxBufAddr,
                 s->RxBufferSize);
    return avail == 0 || avail >= 1514 || (s->IntrMask & RxOverflow);
}

static ssize_t rtl8139_do_receive(RTL8139State *s, const uint8_t *buf, size_t size_, int do_interrupt)
{
    /* size is the length of the buffer passed to the driver */
    size_t size = size_;

    uint32_t packet_header = 0;

    uint8_t buf1[MIN_BUF_SIZE + VLAN_HLEN];
    static const uint8_t broadcast_macaddr[6] =
        { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    DPRINTF(">>> received len=%zu", size);

    /* test if board clock is stopped */
    if (!s->clock_enabled)
    {
        DPRINTF("stopped ==========================");
        return -1;
    }

    /* first check if receiver is enabled */

    if (!rtl8139_receiver_enabled(s))
    {
        DPRINTF("receiver disabled ================");
        return -1;
    }

    /* XXX: check this */
    if (s->RxConfig & AcceptAllPhys) {
        /* promiscuous: receive all */
        DPRINTF(">>> packet received in promiscuous mode");

    } else {
        if (!memcmp(buf,  broadcast_macaddr, 6)) {
            /* broadcast address */
            if (!(s->RxConfig & AcceptBroadcast))
            {
                DPRINTF(">>> broadcast packet rejected");

                return size;
            }

            packet_header |= RxBroadcast;

            DPRINTF(">>> broadcast packet received");

        } else if (buf[0] & 0x01) {
            /* multicast */
            if (!(s->RxConfig & AcceptMulticast))
            {
                DPRINTF(">>> multicast packet rejected");

                return size;
            }

            int mcast_idx = net_crc32(buf, ETH_ALEN) >> 26;

            if (!(s->mult[mcast_idx >> 3] & (1 << (mcast_idx & 7))))
            {
                DPRINTF(">>> multicast address mismatch");

                return size;
            }

            packet_header |= RxMulticast;

            DPRINTF(">>> multicast packet received");

        } else if (s->phys[0] == buf[0] &&
                   s->phys[1] == buf[1] &&
                   s->phys[2] == buf[2] &&
                   s->phys[3] == buf[3] &&
                   s->phys[4] == buf[4] &&
                   s->phys[5] == buf[5]) {
            /* match */
            if (!(s->RxConfig & AcceptMyPhys))
            {
                DPRINTF(">>> rejecting physical address matching packet");

                return size;
            }

            packet_header |= RxPhysical;

            DPRINTF(">>> physical address matching packet received");

        } else {

            DPRINTF(">>> unknown packet");

            return size;
        }
    }

    /* if too small buffer, then expand it
     * Include some tailroom in case a vlan tag is later removed. */
    if (size < MIN_BUF_SIZE + VLAN_HLEN) {
        memcpy(buf1, buf, size);
        memset(buf1 + size, 0, MIN_BUF_SIZE + VLAN_HLEN - size);
        buf = buf1;
        if (size < MIN_BUF_SIZE) {
            size = MIN_BUF_SIZE;
        }
    }

	DPRINTF("in ring Rx mode ================");

	/* begin ring receiver mode */
	uint32_t avail = MOD2(s->RxBufferSize + s->RxBufPtr - s->RxBufAddr, s->RxBufferSize);

	/* if receiver buffer is empty then avail == 0 */

#define RX_ALIGN(x) (((x) + 3) & ~0x3)

	if (avail != 0 && RX_ALIGN(size + 8) >= avail)
	{
		DPRINTF("rx overflow: rx buffer length %d head 0x%04x "
			"read 0x%04x === available 0x%04x need 0x%04zx",
			s->RxBufferSize, s->RxBufAddr, s->RxBufPtr, avail, size + 8);

		s->IntrStatus |= RxOverflow;
		++s->RxMissed;
		rtl8139_update_irq(s);
		return 0;
	}

	packet_header |= RxStatusOK;

	packet_header |= (((size+4) << 16) & 0xffff0000);

	/* write header */
	uint32_t val = cpu_to_le32(packet_header);

	rtl8139_write_buffer(s, (uint8_t *)&val, 4);

	rtl8139_write_buffer(s, buf, size);

	/* write checksum */
	val = cpu_to_le32(crc32(0, buf, size));
	rtl8139_write_buffer(s, (uint8_t *)&val, 4);

	/* correct buffer write pointer */
	s->RxBufAddr = MOD2(RX_ALIGN(s->RxBufAddr), s->RxBufferSize);

	/* now we can signal we have received something */

	DPRINTF("received: rx buffer length %d head 0x%04x read 0x%04x",
		s->RxBufferSize, s->RxBufAddr, s->RxBufPtr);

    s->IntrStatus |= RxOK;

    if (do_interrupt)
    {
        rtl8139_update_irq(s);
    }

    return size_;
}

ssize_t rtl8139_receive(RTL8139State *s, const uint8_t *buf, size_t size)
{
    return rtl8139_do_receive(s, buf, size, 1);
}

static void rtl8139_reset_rxring(RTL8139State *s, uint32_t bufferSize)
{
    s->RxBufferSize = bufferSize;
    s->RxBufPtr  = 0;
    s->RxBufAddr = 0;
}

static void rtl8139_reset_phy(RTL8139State *s)
{
    s->BasicModeStatus  = 0x7809;
    s->BasicModeStatus |= 0x0020; /* autonegotiation completed */
    /* preserve link state */
	s->BasicModeStatus |= 0x04; // Link up;

    s->NWayAdvert    = 0x05e1; /* all modes, full duplex */
    s->NWayLPAR      = 0x05e1; /* all modes, full duplex */
    s->NWayExpansion = 0x0001; /* autonegotiation supported */

    s->CSCR = CSCR_F_LINK_100 | CSCR_HEART_BIT | CSCR_LD;
}

void rtl8139_reset(RTL8139State *s)
{
    int i;

    /* restore MAC address */
    memcpy(s->phys, s->conf.macaddr.a, 6);
    //qemu_format_nic_info_str(qemu_get_queue(s->nic), s->phys);

    /* reset interrupt mask */
    s->IntrStatus = 0;
    s->IntrMask = 0;

    rtl8139_update_irq(s);

    /* mark all status registers as owned by host */
    for (i = 0; i < 4; ++i)
    {
        s->TxStatus[i] = TxHostOwns;
    }

    s->currTxDesc = 0;

    s->RxBuf = 0;

    rtl8139_reset_rxring(s, 8192);

    /* ACK the reset */
    s->TxConfig = 0;

#if 0
//    s->TxConfig |= HW_REVID(1, 0, 0, 0, 0, 0, 0); // RTL-8139  HasHltClk
    s->clock_enabled = 0;
#else
    s->TxConfig |= HW_REVID(1, 1, 1, 0, 1, 1, 0); // RTL-8139C+ HasLWake
    s->clock_enabled = 1;
#endif

    s->bChipCmdState = CmdReset; /* RxBufEmpty bit is calculated on read from ChipCmd */;

    /* set initial state data */
    s->Config0 = 0x0; /* No boot ROM */
    s->Config1 = 0xC; /* IO mapped and MEM mapped registers available */
    s->Config3 = 0x1; /* fast back-to-back compatible */
    s->Config5 = 0x0;

//    s->BasicModeCtrl = 0x3100; // 100Mbps, full duplex, autonegotiation
//    s->BasicModeCtrl = 0x2100; // 100Mbps, full duplex
    s->BasicModeCtrl = 0x1000; // autonegotiation

    rtl8139_reset_phy(s);

    /* also reset timer and disable timer interrupt */
    s->TCTR = 0;
    s->TimerInt = 0;
    s->TCTR_base = 0;
    rtl8139_set_next_tctr_time(s);
}

static void rtl8139_ChipCmd_write(RTL8139State *s, uint32_t val)
{
    val &= 0xff;

    DPRINTF("ChipCmd write val=0x%08x", val);

    if (val & CmdReset)
    {
        DPRINTF("ChipCmd reset");
        rtl8139_reset(s);
    }
    if (val & CmdRxEnb)
    {
        DPRINTF("ChipCmd enable receiver");
    }
    if (val & CmdTxEnb)
    {
        DPRINTF("ChipCmd enable transmitter");
    }

    /* mask unwritable bits */
    val = SET_MASKED(val, 0xe3, s->bChipCmdState);

    /* Deassert reset pin before next read */
    val &= ~CmdReset;

    s->bChipCmdState = val;
}

static int rtl8139_RxBufferEmpty(RTL8139State *s)
{
    int unread = MOD2(s->RxBufferSize + s->RxBufAddr - s->RxBufPtr, s->RxBufferSize);

    if (unread != 0)
    {
        DPRINTF("receiver buffer data available 0x%04x", unread);
        return 0;
    }

    DPRINTF("receiver buffer is empty");

    return 1;
}

static uint32_t rtl8139_ChipCmd_read(RTL8139State *s)
{
    uint32_t ret = s->bChipCmdState;

    if (rtl8139_RxBufferEmpty(s))
        ret |= RxBufEmpty;

    DPRINTF("ChipCmd read val=0x%04x", ret);

    return ret;
}

static int rtl8139_config_writable(RTL8139State *s)
{
    if ((s->Cfg9346 & Chip9346_op_mask) == Cfg9346_ConfigWrite)
    {
        return 1;
    }

    DPRINTF("Configuration registers are write-protected");

    return 0;
}

static void rtl8139_BasicModeCtrl_write(RTL8139State *s, uint32_t val)
{
    val &= 0xffff;

    DPRINTF("BasicModeCtrl register write(w) val=0x%04x", val);

    /* mask unwritable bits */
    uint32_t mask = 0xccff;

    if (true || !rtl8139_config_writable(s))
    {
        /* Speed setting and autonegotiation enable bits are read-only */
        mask |= 0x3000;
        /* Duplex mode setting is read-only */
        mask |= 0x0100;
    }

    if (val & 0x8000) {
        /* Reset PHY */
        rtl8139_reset_phy(s);
    }

    val = SET_MASKED(val, mask, s->BasicModeCtrl);

    s->BasicModeCtrl = val;
}

static uint32_t rtl8139_BasicModeCtrl_read(RTL8139State *s)
{
    uint32_t ret = s->BasicModeCtrl;

    DPRINTF("BasicModeCtrl register read(w) val=0x%04x", ret);

    return ret;
}

static void rtl8139_BasicModeStatus_write(RTL8139State *s, uint32_t val)
{
    val &= 0xffff;

    DPRINTF("BasicModeStatus register write(w) val=0x%04x", val);

    /* mask unwritable bits */
    val = SET_MASKED(val, 0xff3f, s->BasicModeStatus);

    s->BasicModeStatus = val;
}

static uint32_t rtl8139_BasicModeStatus_read(RTL8139State *s)
{
    uint32_t ret = s->BasicModeStatus;

    DPRINTF("BasicModeStatus register read(w) val=0x%04x", ret);

    return ret;
}

static void rtl8139_Cfg9346_write(RTL8139State *s, uint32_t val)
{
    val &= 0xff;

    DPRINTF("Cfg9346 write val=0x%02x", val);

    /* mask unwritable bits */
    val = SET_MASKED(val, 0x31, s->Cfg9346);

    uint32_t opmode = val & 0xc0;
    uint32_t eeprom_val = val & 0xf;

    if (opmode == 0x80) {
        /* eeprom access */
        int eecs = (eeprom_val & 0x08)?1:0;
        int eesk = (eeprom_val & 0x04)?1:0;
        int eedi = (eeprom_val & 0x02)?1:0;
        prom9346_set_wire(s, eecs, eesk, eedi);
    } else if (opmode == 0x40) {
        /* Reset.  */
        val = 0;
        rtl8139_reset(s);
    }

    s->Cfg9346 = val;
}

static uint32_t rtl8139_Cfg9346_read(RTL8139State *s)
{
    uint32_t ret = s->Cfg9346;

    uint32_t opmode = ret & 0xc0;

    if (opmode == 0x80)
    {
        /* eeprom access */
        int eedo = prom9346_get_wire(s);
        if (eedo)
        {
            ret |=  0x01;
        }
        else
        {
            ret &= ~0x01;
        }
    }

    DPRINTF("Cfg9346 read val=0x%02x", ret);

    return ret;
}

static void rtl8139_Config0_write(RTL8139State *s, uint32_t val)
{
    val &= 0xff;

    DPRINTF("Config0 write val=0x%02x", val);

    if (!rtl8139_config_writable(s)) {
        return;
    }

    /* mask unwritable bits */
    val = SET_MASKED(val, 0xf8, s->Config0);

    s->Config0 = val;
}

static uint32_t rtl8139_Config0_read(RTL8139State *s)
{
    uint32_t ret = s->Config0;

    DPRINTF("Config0 read val=0x%02x", ret);

    return ret;
}

static void rtl8139_Config1_write(RTL8139State *s, uint32_t val)
{
    val &= 0xff;

    DPRINTF("Config1 write val=0x%02x", val);

    if (!rtl8139_config_writable(s)) {
        return;
    }

    /* mask unwritable bits */
    val = SET_MASKED(val, 0xC, s->Config1);

    s->Config1 = val;
}

static uint32_t rtl8139_Config1_read(RTL8139State *s)
{
    uint32_t ret = s->Config1;

    DPRINTF("Config1 read val=0x%02x", ret);

    return ret;
}

static void rtl8139_Config3_write(RTL8139State *s, uint32_t val)
{
    val &= 0xff;

    DPRINTF("Config3 write val=0x%02x", val);

    if (!rtl8139_config_writable(s)) {
        return;
    }

    /* mask unwritable bits */
    val = SET_MASKED(val, 0x8F, s->Config3);

    s->Config3 = val;
}

static uint32_t rtl8139_Config3_read(RTL8139State *s)
{
    uint32_t ret = s->Config3;

    DPRINTF("Config3 read val=0x%02x", ret);

    return ret;
}

static void rtl8139_Config4_write(RTL8139State *s, uint32_t val)
{
    val &= 0xff;

    DPRINTF("Config4 write val=0x%02x", val);

    if (!rtl8139_config_writable(s)) {
        return;
    }

    /* mask unwritable bits */
    val = SET_MASKED(val, 0x0a, s->Config4);

    s->Config4 = val;
}

static uint32_t rtl8139_Config4_read(RTL8139State *s)
{
    uint32_t ret = s->Config4;

    DPRINTF("Config4 read val=0x%02x", ret);

    return ret;
}

static void rtl8139_Config5_write(RTL8139State *s, uint32_t val)
{
    val &= 0xff;

    DPRINTF("Config5 write val=0x%02x", val);

    /* mask unwritable bits */
    val = SET_MASKED(val, 0x80, s->Config5);

    s->Config5 = val;
}

static uint32_t rtl8139_Config5_read(RTL8139State *s)
{
    uint32_t ret = s->Config5;

    DPRINTF("Config5 read val=0x%02x", ret);

    return ret;
}

static void rtl8139_TxConfig_write(RTL8139State *s, uint32_t val)
{
    if (!rtl8139_transmitter_enabled(s))
    {
        DPRINTF("transmitter disabled; no TxConfig write val=0x%08x", val);
        return;
    }

    DPRINTF("TxConfig write val=0x%08x", val);

    val = SET_MASKED(val, TxVersionMask | 0x8070f80f, s->TxConfig);

    s->TxConfig = val;
}

static void rtl8139_TxConfig_writeb(RTL8139State *s, uint32_t val)
{
    DPRINTF("RTL8139C TxConfig via write(b) val=0x%02x", val);

    uint32_t tc = s->TxConfig;
    tc &= 0xFFFFFF00;
    tc |= (val & 0x000000FF);
    rtl8139_TxConfig_write(s, tc);
}

static uint32_t rtl8139_TxConfig_read(RTL8139State *s)
{
    uint32_t ret = s->TxConfig;

    DPRINTF("TxConfig read val=0x%04x", ret);

    return ret;
}

static void rtl8139_RxConfig_write(RTL8139State *s, uint32_t val)
{
    DPRINTF("RxConfig write val=0x%08x", val);

    /* mask unwritable bits */
    val = SET_MASKED(val, 0xf0fc0040, s->RxConfig);

    s->RxConfig = val;

    /* reset buffer size and read/write pointers */
    rtl8139_reset_rxring(s, 8192 << ((s->RxConfig >> 11) & 0x3));

    DPRINTF("RxConfig write reset buffer size to %d", s->RxBufferSize);
}

static uint32_t rtl8139_RxConfig_read(RTL8139State *s)
{
    uint32_t ret = s->RxConfig;

    DPRINTF("RxConfig read val=0x%08x", ret);

    return ret;
}

static void rtl8139_transfer_frame(RTL8139State *s, uint8_t *buf, int size,
    int do_interrupt)
{
    if (!size)
    {
        DPRINTF("+++ empty ethernet frame");
        return;
    }

    if (TxLoopBack == (s->TxConfig & TxLoopBack))
    {
        DPRINTF("+++ transmit loopback mode");
        rtl8139_do_receive(s, buf, size, do_interrupt);
    }
    else
    {
        qemu_send_packet(s, buf, size);
    }
}

static int rtl8139_transmit_one(RTL8139State *s, int descriptor)
{
    if (!rtl8139_transmitter_enabled(s))
    {
        DPRINTF("+++ cannot transmit from descriptor %d: transmitter "
            "disabled", descriptor);
        return 0;
    }

    if (s->TxStatus[descriptor] & TxHostOwns)
    {
        DPRINTF("+++ cannot transmit from descriptor %d: owned by host "
            "(%08x)", descriptor, s->TxStatus[descriptor]);
        return 0;
    }

    DPRINTF("+++ transmitting from descriptor %d", descriptor);

    PCIDevice *d = PCI_DEVICE(s);
    int txsize = s->TxStatus[descriptor] & 0x1fff;
    uint8_t txbuffer[0x2000];

    DPRINTF("+++ transmit reading %d bytes from host memory at 0x%08x",
        txsize, s->TxAddr[descriptor]);

    pci_dma_read(d, s->TxAddr[descriptor], txbuffer, txsize);

    /* Mark descriptor as transferred */
    s->TxStatus[descriptor] |= TxHostOwns;
    s->TxStatus[descriptor] |= TxStatOK;

    rtl8139_transfer_frame(s, txbuffer, txsize, 0);

    DPRINTF("+++ transmitted %d bytes from descriptor %d", txsize,
        descriptor);

    /* update interrupt */
    s->IntrStatus |= TxOK;
    rtl8139_update_irq(s);

    return 1;
}

static void rtl8139_transmit(RTL8139State *s)
{
    int descriptor = s->currTxDesc, txcount = 0;

    /*while*/
    if (rtl8139_transmit_one(s, descriptor))
    {
        ++s->currTxDesc;
        s->currTxDesc %= 4;
        ++txcount;
    }

    /* Mark transfer completed */
    if (!txcount)
    {
        DPRINTF("transmitter queue stalled, current TxDesc = %d",
            s->currTxDesc);
    }
}

static void rtl8139_TxStatus_write(RTL8139State *s, uint32_t txRegOffset, uint32_t val)
{

    int descriptor = txRegOffset/4;

    DPRINTF("TxStatus write offset=0x%x val=0x%08x descriptor=%d",
        txRegOffset, val, descriptor);

    /* mask only reserved bits */
    val &= ~0xff00c000; /* these bits are reset on write */
    val = SET_MASKED(val, 0x00c00000, s->TxStatus[descriptor]);

    s->TxStatus[descriptor] = val;

    /* attempt to start transmission */
    rtl8139_transmit(s);
}

static uint32_t rtl8139_TxStatus_TxAddr_read(RTL8139State *s, const uint32_t regs[],
                                             uint32_t base, uint8_t addr,
                                             int size)
{
    uint32_t reg = (addr - base) / 4;
    uint32_t offset = addr & 0x3;
    uint32_t ret = 0;

    if (addr & (size - 1)) {
        DPRINTF("not implemented read for TxStatus/TxAddr "
                "addr=0x%x size=0x%x", addr, size);
        return ret;
    }

    switch (size) {
    case 1: /* fall through */
    case 2: /* fall through */
    case 4:
        ret = (regs[reg] >> offset * 8) & (((uint64_t)1 << (size * 8)) - 1);
        DPRINTF("TxStatus/TxAddr[%d] read addr=0x%x size=0x%x val=0x%08x",
                reg, addr, size, ret);
        break;
    default:
        DPRINTF("unsupported size 0x%x of TxStatus/TxAddr reading", size);
        break;
    }

    return ret;
}

static uint16_t rtl8139_TSAD_read(RTL8139State *s)
{
    uint16_t ret = 0;

    /* Simulate TSAD, it is read only anyway */

    ret = ((s->TxStatus[3] & TxStatOK  )?TSAD_TOK3:0)
         |((s->TxStatus[2] & TxStatOK  )?TSAD_TOK2:0)
         |((s->TxStatus[1] & TxStatOK  )?TSAD_TOK1:0)
         |((s->TxStatus[0] & TxStatOK  )?TSAD_TOK0:0)

         |((s->TxStatus[3] & TxUnderrun)?TSAD_TUN3:0)
         |((s->TxStatus[2] & TxUnderrun)?TSAD_TUN2:0)
         |((s->TxStatus[1] & TxUnderrun)?TSAD_TUN1:0)
         |((s->TxStatus[0] & TxUnderrun)?TSAD_TUN0:0)

         |((s->TxStatus[3] & TxAborted )?TSAD_TABT3:0)
         |((s->TxStatus[2] & TxAborted )?TSAD_TABT2:0)
         |((s->TxStatus[1] & TxAborted )?TSAD_TABT1:0)
         |((s->TxStatus[0] & TxAborted )?TSAD_TABT0:0)

         |((s->TxStatus[3] & TxHostOwns )?TSAD_OWN3:0)
         |((s->TxStatus[2] & TxHostOwns )?TSAD_OWN2:0)
         |((s->TxStatus[1] & TxHostOwns )?TSAD_OWN1:0)
         |((s->TxStatus[0] & TxHostOwns )?TSAD_OWN0:0) ;


    DPRINTF("TSAD read val=0x%04x", ret);

    return ret;
}

static uint16_t rtl8139_CSCR_read(RTL8139State *s)
{
    uint16_t ret = s->CSCR;

    DPRINTF("CSCR read val=0x%04x", ret);

    return ret;
}

static void rtl8139_TxAddr_write(RTL8139State *s, uint32_t txAddrOffset, uint32_t val)
{
    DPRINTF("TxAddr write offset=0x%x val=0x%08x", txAddrOffset, val);

    s->TxAddr[txAddrOffset/4] = val;
}

static uint32_t rtl8139_TxAddr_read(RTL8139State *s, uint32_t txAddrOffset)
{
    uint32_t ret = s->TxAddr[txAddrOffset/4];

    DPRINTF("TxAddr read offset=0x%x val=0x%08x", txAddrOffset, ret);

    return ret;
}

static void rtl8139_RxBufPtr_write(RTL8139State *s, uint32_t val)
{
    DPRINTF("RxBufPtr write val=0x%04x", val);

    /* this value is off by 16 */
    s->RxBufPtr = MOD2(val + 0x10, s->RxBufferSize);

    /* more buffer space may be available so try to receive */
    // qemu_flush_queued_packets(qemu_get_queue(s->nic));

    DPRINTF(" CAPR write: rx buffer length %d head 0x%04x read 0x%04x",
        s->RxBufferSize, s->RxBufAddr, s->RxBufPtr);
}

static uint32_t rtl8139_RxBufPtr_read(RTL8139State *s)
{
    /* this value is off by 16 */
    uint32_t ret = s->RxBufPtr - 0x10;

    DPRINTF("RxBufPtr read val=0x%04x", ret);

    return ret;
}

static uint32_t rtl8139_RxBufAddr_read(RTL8139State *s)
{
    /* this value is NOT off by 16 */
    uint32_t ret = s->RxBufAddr;

    DPRINTF("RxBufAddr read val=0x%04x", ret);

    return ret;
}

static void rtl8139_RxBuf_write(RTL8139State *s, uint32_t val)
{
    DPRINTF("RxBuf write val=0x%08x", val);

    s->RxBuf = val;

    /* may need to reset rxring here */
}

static uint32_t rtl8139_RxBuf_read(RTL8139State *s)
{
    uint32_t ret = s->RxBuf;

    DPRINTF("RxBuf read val=0x%08x", ret);

    return ret;
}

static void rtl8139_IntrMask_write(RTL8139State *s, uint32_t val)
{
    DPRINTF("IntrMask write(w) val=0x%04x", val);

    /* mask unwritable bits */
    val = SET_MASKED(val, 0x1e00, s->IntrMask);

    s->IntrMask = val;

    rtl8139_update_irq(s);

}

static uint32_t rtl8139_IntrMask_read(RTL8139State *s)
{
    uint32_t ret = s->IntrMask;

    DPRINTF("IntrMask read(w) val=0x%04x", ret);

    return ret;
}

static void rtl8139_IntrStatus_write(RTL8139State *s, uint32_t val)
{
    DPRINTF("IntrStatus write(w) val=0x%04x", val);

#if 0

    /* writing to ISR has no effect */

    return;

#else
    uint16_t newStatus = s->IntrStatus & ~val;

    /* mask unwritable bits */
    newStatus = SET_MASKED(newStatus, 0x1e00, s->IntrStatus);

    /* writing 1 to interrupt status register bit clears it */
    s->IntrStatus = 0;
    rtl8139_update_irq(s);

    s->IntrStatus = newStatus;
    rtl8139_set_next_tctr_time(s);
    rtl8139_update_irq(s);

#endif
}

static uint32_t rtl8139_IntrStatus_read(RTL8139State *s)
{
    uint32_t ret = s->IntrStatus;

    DPRINTF("IntrStatus read(w) val=0x%04x", ret);

#if 0

    /* reading ISR clears all interrupts */
    s->IntrStatus = 0;

    rtl8139_update_irq(s);

#endif

    return ret;
}

static void rtl8139_MultiIntr_write(RTL8139State *s, uint32_t val)
{
    DPRINTF("MultiIntr write(w) val=0x%04x", val);

    /* mask unwritable bits */
    val = SET_MASKED(val, 0xf000, s->MultiIntr);

    s->MultiIntr = val;
}

static uint32_t rtl8139_MultiIntr_read(RTL8139State *s)
{
    uint32_t ret = s->MultiIntr;

    DPRINTF("MultiIntr read(w) val=0x%04x", ret);

    return ret;
}

static void rtl8139_io_writeb(void *opaque, uint8_t addr, uint32_t val)
{
    RTL8139State *s = (RTL8139State *)opaque;

    switch (addr)
    {
        case MAC0:
        case MAC0 + 1:
        case MAC0 + 2:
        case MAC0 + 3:
        case MAC0 + 4:
            s->phys[addr - MAC0] = val;
            break;
        case MAC0+5:
            s->phys[addr - MAC0] = val;
            //qemu_format_nic_info_str(qemu_get_queue(s->nic), s->phys);
            break;
        case MAC0+6:
        case MAC0+7:
            /* reserved */
            break;
        case MAR0:
        case MAR0 + 1:
        case MAR0 + 2:
        case MAR0 + 3:
        case MAR0 + 4:
        case MAR0 + 5:
        case MAR0 + 6:
        case MAR0 + 7:
            s->mult[addr - MAR0] = val;
            break;
        case ChipCmd:
            rtl8139_ChipCmd_write(s, val);
            break;
        case Cfg9346:
            rtl8139_Cfg9346_write(s, val);
            break;
        case TxConfig: /* windows driver sometimes writes using byte-lenth call */
            rtl8139_TxConfig_writeb(s, val);
            break;
        case Config0:
            rtl8139_Config0_write(s, val);
            break;
        case Config1:
            rtl8139_Config1_write(s, val);
            break;
        case Config3:
            rtl8139_Config3_write(s, val);
            break;
        case Config4:
            rtl8139_Config4_write(s, val);
            break;
        case Config5:
            rtl8139_Config5_write(s, val);
            break;
        case MediaStatus:
            /* ignore */
            DPRINTF("not implemented write(b) to MediaStatus val=0x%02x",
                val);
            break;

        case HltClk:
            DPRINTF("HltClk write val=0x%08x", val);
            if (val == 'R')
            {
                s->clock_enabled = 1;
            }
            else if (val == 'H')
            {
                s->clock_enabled = 0;
            }
            break;

        case TxThresh:
            DPRINTF("not implemented write(b) to TxThresh val=0x%02x", val);
            break;

        case TxPoll:
            DPRINTF("not implemented write(b) to TxPoll val=0x%02x", val);
            break;

        default:
            INFO_LOG(NETWORK, "not implemented write(b) addr=0x%x val=0x%02x", addr,
                val);
            break;
    }
}

static void rtl8139_io_writew(void *opaque, uint8_t addr, uint32_t val)
{
    RTL8139State *s = (RTL8139State *)opaque;

    switch (addr)
    {
        case IntrMask:
            rtl8139_IntrMask_write(s, val);
            break;

        case IntrStatus:
            rtl8139_IntrStatus_write(s, val);
            break;

        case MultiIntr:
            rtl8139_MultiIntr_write(s, val);
            break;

        case RxBufPtr:
            rtl8139_RxBufPtr_write(s, val);
            break;

        case BasicModeCtrl:
            rtl8139_BasicModeCtrl_write(s, val);
            break;
        case BasicModeStatus:
            rtl8139_BasicModeStatus_write(s, val);
            break;
        case NWayAdvert:
            DPRINTF("NWayAdvert write(w) val=0x%04x", val);
            s->NWayAdvert = val;
            break;
        case NWayLPAR:
            DPRINTF("forbidden NWayLPAR write(w) val=0x%04x", val);
            break;
        case NWayExpansion:
            DPRINTF("NWayExpansion write(w) val=0x%04x", val);
            s->NWayExpansion = val;
            break;

        case CpCmd:
            DPRINTF("ioport write(w) CpCmd unimplemented");
            break;

        case IntrMitigate:
            DPRINTF("ioport write(w) IntrMitigate unimplemented");
            break;

        default:
            DPRINTF("ioport write(w) addr=0x%x val=0x%04x via write(b)",
                addr, val);

            rtl8139_io_writeb(opaque, addr, val & 0xff);
            rtl8139_io_writeb(opaque, addr + 1, (val >> 8) & 0xff);
            break;
    }
}

static void rtl8139_set_next_tctr_time(RTL8139State *s)
{
#ifdef RTL8139_TIMER
    const uint64_t ns_per_period = (uint64_t)PCI_PERIOD << 32;

    DPRINTF("entered rtl8139_set_next_tctr_time");

    /* This function is called at least once per period, so it is a good
     * place to update the timer base.
     *
     * After one iteration of this loop the value in the Timer register does
     * not change, but the device model is counting up by 2^32 ticks (approx.
     * 130 seconds).
     */
    while (s->TCTR_base + ns_per_period <= qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)) {
        s->TCTR_base += ns_per_period;
    }

    if (!s->TimerInt) {
        timer_del(s->timer);
    } else {
        uint64_t delta = (uint64_t)s->TimerInt * PCI_PERIOD;
        if (s->TCTR_base + delta <= qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)) {
            delta += ns_per_period;
        }
        timer_mod(s->timer, s->TCTR_base + delta);
    }
#endif
}

static void rtl8139_io_writel(void *opaque, uint8_t addr, uint32_t val)
{
    RTL8139State *s = (RTL8139State *)opaque;

    switch (addr)
    {
        case RxMissed:
            DPRINTF("RxMissed clearing on write");
            s->RxMissed = 0;
            break;

        case TxConfig:
            rtl8139_TxConfig_write(s, val);
            break;

        case RxConfig:
            rtl8139_RxConfig_write(s, val);
            break;

        case TxStatus0:
        case TxStatus0 + 1:
        case TxStatus0 + 2:
        case TxStatus0 + 3:
        case TxStatus0 + 4:
        case TxStatus0 + 5:
        case TxStatus0 + 6:
        case TxStatus0 + 7:
        case TxStatus0 + 8:
        case TxStatus0 + 9:
        case TxStatus0 + 10:
        case TxStatus0 + 11:
        case TxStatus0 + 12:
        case TxStatus0 + 13:
        case TxStatus0 + 14:
        case TxStatus0 + 15:
            rtl8139_TxStatus_write(s, addr-TxStatus0, val);
            break;

        case TxAddr0:
        case TxAddr0 + 1:
        case TxAddr0 + 2:
        case TxAddr0 + 3:
        case TxAddr0 + 4:
        case TxAddr0 + 5:
        case TxAddr0 + 6:
        case TxAddr0 + 7:
        case TxAddr0 + 8:
        case TxAddr0 + 9:
        case TxAddr0 + 10:
        case TxAddr0 + 11:
        case TxAddr0 + 12:
        case TxAddr0 + 13:
        case TxAddr0 + 14:
        case TxAddr0 + 15:
            rtl8139_TxAddr_write(s, addr-TxAddr0, val);
            break;

        case RxBuf:
            rtl8139_RxBuf_write(s, val);
            break;

        case RxRingAddrLO:
            DPRINTF("ioport write(l) to C+ RxRingLO not implemented");
            break;

        case RxRingAddrHI:
            DPRINTF("ioport write(l) to C+ RxRingHI not implemented");
            break;

        case Timer:
            DPRINTF("TCTR Timer reset on write");
#ifdef RTL8139_TIMER
            s->TCTR_base = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
#endif
            rtl8139_set_next_tctr_time(s);
            break;

        case FlashReg:
            DPRINTF("FlashReg TimerInt write val=0x%08x", val);
            if (s->TimerInt != val) {
                s->TimerInt = val;
                rtl8139_set_next_tctr_time(s);
            }
            break;

        default:
            DPRINTF("ioport write(l) addr=0x%x val=0x%08x via write(w)",
                addr, val);
            rtl8139_io_writew(opaque, addr, val & 0xffff);
            rtl8139_io_writew(opaque, addr + 2, (val >> 16) & 0xffff);
            break;
    }
}

static uint32_t rtl8139_io_readb(void *opaque, uint8_t addr)
{
    RTL8139State *s = (RTL8139State *)opaque;
    int ret;

    switch (addr)
    {
        case MAC0:
        case MAC0 + 1:
        case MAC0 + 2:
        case MAC0 + 3:
        case MAC0 + 4:
        case MAC0 + 5:
            ret = s->phys[addr - MAC0];
            break;
        case MAC0+6:
        case MAC0+7:
            ret = 0;
            break;
        case MAR0:
        case MAR0 + 1:
        case MAR0 + 2:
        case MAR0 + 3:
        case MAR0 + 4:
        case MAR0 + 5:
        case MAR0 + 6:
        case MAR0 + 7:
            ret = s->mult[addr - MAR0];
            break;
        case TxStatus0:
        case TxStatus0 + 1:
        case TxStatus0 + 2:
        case TxStatus0 + 3:
        case TxStatus0 + 4:
        case TxStatus0 + 5:
        case TxStatus0 + 6:
        case TxStatus0 + 7:
        case TxStatus0 + 8:
        case TxStatus0 + 9:
        case TxStatus0 + 10:
        case TxStatus0 + 11:
        case TxStatus0 + 12:
        case TxStatus0 + 13:
        case TxStatus0 + 14:
        case TxStatus0 + 15:
            ret = rtl8139_TxStatus_TxAddr_read(s, s->TxStatus, TxStatus0,
                                               addr, 1);
            break;
        case ChipCmd:
            ret = rtl8139_ChipCmd_read(s);
            break;
        case Cfg9346:
            ret = rtl8139_Cfg9346_read(s);
            break;
        case Config0:
            ret = rtl8139_Config0_read(s);
            break;
        case Config1:
            ret = rtl8139_Config1_read(s);
            break;
        case Config3:
            ret = rtl8139_Config3_read(s);
            break;
        case Config4:
            ret = rtl8139_Config4_read(s);
            break;
        case Config5:
            ret = rtl8139_Config5_read(s);
            break;

        case MediaStatus:
            /* The LinkDown bit of MediaStatus is inverse with link status */
            ret = 0xd0 | (~s->BasicModeStatus & 0x04);
            DPRINTF("MediaStatus read 0x%x", ret);
            break;

        case HltClk:
            ret = s->clock_enabled;
            DPRINTF("HltClk read 0x%x", ret);
            break;

        case PCIRevisionID:
            ret = RTL8139_PCI_REVID;
            DPRINTF("PCI Revision ID read 0x%x", ret);
            break;

        case TxThresh:
            DPRINTF("not implemented read(b) TxThresh");
            ret = -1;
            break;

        case 0x43: /* Part of TxConfig register. Windows driver tries to read it */
            ret = s->TxConfig >> 24;
            DPRINTF("RTL8139C TxConfig at 0x43 read(b) val=0x%02x", ret);
            break;

        default:
        	INFO_LOG(NETWORK, "not implemented read(b) addr=0x%x", addr);
            ret = 0;
            break;
    }

    return ret;
}

static uint32_t rtl8139_io_readw(void *opaque, uint8_t addr)
{
    RTL8139State *s = (RTL8139State *)opaque;
    uint32_t ret;

    switch (addr)
    {
        case TxAddr0:
        case TxAddr0 + 1:
        case TxAddr0 + 2:
        case TxAddr0 + 3:
        case TxAddr0 + 4:
        case TxAddr0 + 5:
        case TxAddr0 + 6:
        case TxAddr0 + 7:
        case TxAddr0 + 8:
        case TxAddr0 + 9:
        case TxAddr0 + 10:
        case TxAddr0 + 11:
        case TxAddr0 + 12:
        case TxAddr0 + 13:
        case TxAddr0 + 14:
        case TxAddr0 + 15:
            ret = rtl8139_TxStatus_TxAddr_read(s, s->TxAddr, TxAddr0, addr, 2);
            break;
        case IntrMask:
            ret = rtl8139_IntrMask_read(s);
            break;

        case IntrStatus:
            ret = rtl8139_IntrStatus_read(s);
            break;

        case MultiIntr:
            ret = rtl8139_MultiIntr_read(s);
            break;

        case RxBufPtr:
            ret = rtl8139_RxBufPtr_read(s);
            break;

        case RxBufAddr:
            ret = rtl8139_RxBufAddr_read(s);
            break;

        case BasicModeCtrl:
            ret = rtl8139_BasicModeCtrl_read(s);
            break;
        case BasicModeStatus:
            ret = rtl8139_BasicModeStatus_read(s);
            break;
        case NWayAdvert:
            ret = s->NWayAdvert;
            DPRINTF("NWayAdvert read(w) val=0x%04x", ret);
            break;
        case NWayLPAR:
            ret = s->NWayLPAR;
            DPRINTF("NWayLPAR read(w) val=0x%04x", ret);
            break;
        case NWayExpansion:
            ret = s->NWayExpansion;
            DPRINTF("NWayExpansion read(w) val=0x%04x", ret);
            break;

        case TxSummary:
            ret = rtl8139_TSAD_read(s);
            break;

        case CSCR:
            ret = rtl8139_CSCR_read(s);
            break;

        case CpCmd:
        	DPRINTF("ioport read(w) CpCmd unimplemented");
        	ret = -1;
        	break;

        case IntrMitigate:
        	DPRINTF("ioport read(w) IntrMitigate unimplemented");
        	ret = -1;
        	break;

        default:
            DPRINTF("ioport read(w) addr=0x%x via read(b)", addr);

            ret  = rtl8139_io_readb(opaque, addr);
            ret |= rtl8139_io_readb(opaque, addr + 1) << 8;

            DPRINTF("ioport read(w) addr=0x%x val=0x%04x", addr, ret);
            break;
    }

    return ret;
}

static uint32_t rtl8139_io_readl(void *opaque, uint8_t addr)
{
    RTL8139State *s = (RTL8139State *)opaque;
    uint32_t ret;

    switch (addr)
    {
        case RxMissed:
            ret = s->RxMissed;

            DPRINTF("RxMissed read val=0x%08x", ret);
            break;

        case TxConfig:
            ret = rtl8139_TxConfig_read(s);
            break;

        case RxConfig:
            ret = rtl8139_RxConfig_read(s);
            break;

        case TxStatus0:
        case TxStatus0 + 1:
        case TxStatus0 + 2:
        case TxStatus0 + 3:
        case TxStatus0 + 4:
        case TxStatus0 + 5:
        case TxStatus0 + 6:
        case TxStatus0 + 7:
        case TxStatus0 + 8:
        case TxStatus0 + 9:
        case TxStatus0 + 10:
        case TxStatus0 + 11:
        case TxStatus0 + 12:
        case TxStatus0 + 13:
        case TxStatus0 + 14:
        case TxStatus0 + 15:
            ret = rtl8139_TxStatus_TxAddr_read(s, s->TxStatus, TxStatus0,
                                               addr, 4);
            break;

        case TxAddr0:
        case TxAddr0 + 1:
        case TxAddr0 + 2:
        case TxAddr0 + 3:
        case TxAddr0 + 4:
        case TxAddr0 + 5:
        case TxAddr0 + 6:
        case TxAddr0 + 7:
        case TxAddr0 + 8:
        case TxAddr0 + 9:
        case TxAddr0 + 10:
        case TxAddr0 + 11:
        case TxAddr0 + 12:
        case TxAddr0 + 13:
        case TxAddr0 + 14:
        case TxAddr0 + 15:
            ret = rtl8139_TxAddr_read(s, addr-TxAddr0);
            break;

        case RxBuf:
            ret = rtl8139_RxBuf_read(s);
            break;

        case Timer:
#ifdef RTL8139_TIMER
            ret = (qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->TCTR_base) /
                  PCI_PERIOD;
#else
			ret = 0;
#endif
            DPRINTF("TCTR Timer read val=0x%08x", ret);
            break;

        case FlashReg:
            ret = s->TimerInt;
            DPRINTF("FlashReg TimerInt read val=0x%08x", ret);
            break;

        case RxRingAddrLO:
        	DPRINTF("ioport read(l) RxRingAddrLO unimplemented");
        	ret = -1;
        	break;

        case RxRingAddrHI:
        	DPRINTF("ioport read(l) RxRingAddrHI unimplemented");
        	ret = -1;
        	break;

        default:
            DPRINTF("ioport read(l) addr=0x%x via read(w)", addr);

            ret  = rtl8139_io_readw(opaque, addr);
            ret |= rtl8139_io_readw(opaque, addr + 2) << 16;

            DPRINTF("read(l) addr=0x%x val=%08x", addr, ret);
            break;
    }

    return ret;
}

/***********************************************************/
/* PCI RTL8139 definitions */

void rtl8139_ioport_write(void *opaque, hwaddr addr,
						  uint64_t val, unsigned size)
{
    switch (size) {
    case 1:
        rtl8139_io_writeb(opaque, addr, val);
        break;
    case 2:
        rtl8139_io_writew(opaque, addr, val);
        break;
    case 4:
        rtl8139_io_writel(opaque, addr, val);
        break;
    }
}

uint64_t rtl8139_ioport_read(void *opaque, hwaddr addr, unsigned size)
{
    switch (size) {
    case 1:
        return rtl8139_io_readb(opaque, addr);
    case 2:
        return rtl8139_io_readw(opaque, addr);
    case 4:
        return rtl8139_io_readl(opaque, addr);
    }

    return -1;
}

#ifdef RTL8139_TIMER
static void rtl8139_timer(void *opaque)
{
    RTL8139State *s = (RTL8139State *)opaque;

    if (!s->clock_enabled)
    {
        DPRINTF(">>> timer: clock is not running");
        return;
    }

    s->IntrStatus |= PCSTimeout;
    rtl8139_update_irq(s);
    rtl8139_set_next_tctr_time(s);
}
#endif

static void pci_rtl8139_uninit(PCIDevice *dev)
{
#ifdef RTL8139_TIMER
    RTL8139State *s = RTL8139(dev);

    timer_del(s->timer);
    timer_free(s->timer);
#endif
}

void pci_rtl8139_realize(PCIDevice *dev)
{
    RTL8139State *s = RTL8139(dev);
    uint8_t *pci_conf;

    pci_conf = dev->config;
    pci_conf[PCI_INTERRUPT_PIN] = 1;    /* interrupt pin A */
    /* TODO: start of capability list, but no capability
     * list bit in status register, and offset 0xdc seems unused. */
    pci_conf[PCI_CAPABILITY_LIST] = 0xdc;

    s->bar_io.size = 0x100;
    s->bar_mem.size = 0x100;

	// write to pci config
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &s->bar_io);
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->bar_mem);

    /* prepare eeprom */
    s->eeprom.contents[0] = 0x8129;
#if 1
    /* PCI vendor and device ID should be mirrored here */
    s->eeprom.contents[1] = PCI_VENDOR_ID_REALTEK;
    s->eeprom.contents[2] = PCI_DEVICE_ID_REALTEK_8139;
#endif
    s->eeprom.contents[7] = s->conf.macaddr.a[0] | s->conf.macaddr.a[1] << 8;
    s->eeprom.contents[8] = s->conf.macaddr.a[2] | s->conf.macaddr.a[3] << 8;
    s->eeprom.contents[9] = s->conf.macaddr.a[4] | s->conf.macaddr.a[5] << 8;

#ifdef RTL8139_TIMER
    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, rtl8139_timer, s);
#endif
}

RTL8139State *rtl8139_init(NICConf *conf)
{
	RTL8139State *state = (RTL8139State *)calloc(1, sizeof(RTL8139State));
	memcpy(&state->conf, conf, sizeof(state->conf));
	// pci config
	state->parent_obj.config = (uint8_t *)calloc(256, 1);
	state->parent_obj.wmask = (uint8_t *)calloc(256, 1);
	state->parent_obj.cmask = (uint8_t *)calloc(256, 1);
	
	return state;
}

void rtl8139_destroy(RTL8139State *state)
{
	pci_rtl8139_uninit(PCI_DEVICE(state));
	free(state->parent_obj.config);
	free(state->parent_obj.wmask);
	free(state->parent_obj.cmask);
	free(state);
}

void rtl8139_serialize(RTL8139State *s, Serializer& ser)
{
	ser.serialize(s->parent_obj.config, 256);
	ser.serialize(s->parent_obj.cmask, 256);
	ser.serialize(s->parent_obj.wmask, 256);
	ser << s->parent_obj.io_regions;

	ser << s->phys;
	ser << s->mult;
	ser << s->TxStatus;
	ser << s->TxAddr;

	ser << s->RxBuf;
	ser << s->RxBufferSize;
	ser << s->RxBufPtr;
	ser << s->RxBufAddr;

	ser << s->IntrStatus;
	ser << s->IntrMask;

	ser << s->TxConfig;
	ser << s->RxConfig;
	ser << s->RxMissed;
	ser << s->CSCR;

	ser << s->Cfg9346;
	ser << s->Config0;
	ser << s->Config1;
	ser << s->Config3;
	ser << s->Config4;
	ser << s->Config5;

	ser << s->clock_enabled;
	ser << s->bChipCmdState;

	ser << s->MultiIntr;

	ser << s->BasicModeCtrl;
	ser << s->BasicModeStatus;
	ser << s->NWayAdvert;
	ser << s->NWayLPAR;
	ser << s->NWayExpansion;

	ser << s->conf.macaddr;

	ser << s->currTxDesc;

	ser << s->eeprom.contents;
	ser << s->eeprom.mode;
	ser << s->eeprom.tick;
	ser << s->eeprom.address;
	ser << s->eeprom.input;
	ser << s->eeprom.output;

	ser << s->eeprom.eecs;
	ser << s->eeprom.eesk;
	ser << s->eeprom.eedi;
	ser << s->eeprom.eedo;

	ser << s->TCTR;
	ser << s->TimerInt;
	ser << s->TCTR_base;
}

bool rtl8139_deserialize(RTL8139State *s, Deserializer& deser)
{
	deser.deserialize(s->parent_obj.config, 256);
	deser.deserialize(s->parent_obj.cmask, 256);
	deser.deserialize(s->parent_obj.wmask, 256);
	deser >> s->parent_obj.io_regions;

	deser >> s->phys;
	deser >> s->mult;
	deser >> s->TxStatus;
	deser >> s->TxAddr;

	deser >> s->RxBuf;
	deser >> s->RxBufferSize;
	deser >> s->RxBufPtr;
	deser >> s->RxBufAddr;

	deser >> s->IntrStatus;
	deser >> s->IntrMask;

	deser >> s->TxConfig;
	deser >> s->RxConfig;
	deser >> s->RxMissed;
	deser >> s->CSCR;

	deser >> s->Cfg9346;
	deser >> s->Config0;
	deser >> s->Config1;
	deser >> s->Config3;
	deser >> s->Config4;
	deser >> s->Config5;

	deser >> s->clock_enabled;
	deser >> s->bChipCmdState;

	deser >> s->MultiIntr;

	deser >> s->BasicModeCtrl;
	deser >> s->BasicModeStatus;
	deser >> s->NWayAdvert;
	deser >> s->NWayLPAR;
	deser >> s->NWayExpansion;

	deser >> s->conf.macaddr;

	deser >> s->currTxDesc;

	deser >> s->eeprom.contents;
	deser >> s->eeprom.mode;
	deser >> s->eeprom.tick;
	deser >> s->eeprom.address;
	deser >> s->eeprom.input;
	deser >> s->eeprom.output;

	deser >> s->eeprom.eecs;
	deser >> s->eeprom.eesk;
	deser >> s->eeprom.eedi;
	deser >> s->eeprom.eedo;

	deser >> s->TCTR;
	deser >> s->TimerInt;
	deser >> s->TCTR_base;

	return s->bChipCmdState & CmdRxEnb;
}
