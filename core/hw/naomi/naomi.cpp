/*
	This file is a mix of my code, Zezu's, and duno wtf-else (most likely ElSemi's ?)
*/
#include "types.h"
#include "cfg/cfg.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/holly/holly_intc.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/modules/dmac.h"

#include "naomi.h"
#include "naomi_cart.h"
#include "naomi_regs.h"
#include "naomi_m3comm.h"
#include "network/naomi_network.h"
#include "serialize.h"

//#define NAOMI_COMM

static NaomiM3Comm m3comm;

static const u32 BoardID = 0x980055AA;
static u32 GSerialBuffer, BSerialBuffer;
static int GBufPos, BBufPos;
static int GState, BState;
static int GOldClk, BOldClk;
static int BControl, BCmd, BLastCmd;
static int GControl, GCmd, GLastCmd;
static int SerStep, SerStep2;

#ifdef NAOMI_COMM
	u32 CommOffset;
	u32* CommSharedMem;
	HANDLE CommMapFile=INVALID_HANDLE_VALUE;
#endif

/*
El numero de serie solo puede contener:
0-9		(0x30-0x39)
A-H		(0x41-0x48)
J-N		(0x4A-0x4E)
P-Z		(0x50-0x5A)
*/
static u8 BSerial[]="\xB7"/*CRC1*/"\x19"/*CRC2*/"0123234437897584372973927387463782196719782697849162342198671923649";
static u8 GSerial[]="\xB7"/*CRC1*/"\x19"/*CRC2*/"0123234437897584372973927387463782196719782697849162342198671923649";

static unsigned int ShiftCRC(unsigned int CRC,unsigned int rounds)
{
	const unsigned int Magic=0x10210000;
	unsigned int i;
	for(i=0;i<rounds;++i)
	{
		if(CRC&0x80000000)
			CRC=(CRC<<1)+Magic;
		else
			CRC=(CRC<<1);
	}
	return CRC;
}

static unsigned short CRCSerial(const u8 *Serial,unsigned int len)
{
	unsigned int CRC=0xDEBDEB00;
	unsigned int i;

	for(i=0;i<len;++i)
	{
		unsigned char c=Serial[i];
		//CRC&=0xFFFFFF00;
		CRC|=c;
		CRC=ShiftCRC(CRC,8);
	}
	CRC=ShiftCRC(CRC,8);
	return (u16)(CRC>>16);
}

void NaomiInit()
{
	u16 CRC;
	CRC=CRCSerial(BSerial+2,0x2E);
	BSerial[0]=(u8)(CRC>>8);
	BSerial[1]=(u8)(CRC);

	CRC=CRCSerial(GSerial+2,0x2E);
	GSerial[0]=(u8)(CRC>>8);
	GSerial[1]=(u8)(CRC);
}

void NaomiBoardIDWrite(const u16 Data)
{
	int Dat=Data&8;
	int Clk=Data&4;
	int Rst=Data&0x20;
	int Sta=Data&0x10;
	
	if(Rst)
	{
		BState=0;
		BBufPos=0;
	}
	
	if(Clk!=BOldClk && !Clk)	//Falling Edge clock
	{
		//State change
		if(BState==0 && Sta) 
			BState=1;		
		if(BState==1 && !Sta)
			BState=2;

		if((BControl&0xfff)==0xFF0)	//Command mode
		{
			BCmd<<=1;
			if(Dat)
				BCmd|=1;
			else
				BCmd&=0xfffffffe;
		}

		//State processing
		if(BState==1)		//LoadBoardID
		{
			BSerialBuffer=BoardID;
			BBufPos=0;		//??
		}
		if(BState==2)		//ShiftBoardID
		{
			BBufPos++;
		}
	}
	BOldClk=Clk;
}

u16 NaomiBoardIDRead()
{
	if((BControl&0xff)==0xFE)
		return 0xffff;
	return (BSerialBuffer&(1<<(31-BBufPos)))?8:0;
}

static u32 AdaptByte(u8 val)
{
	return val<<24;
}

void NaomiBoardIDWriteControl(const u16 Data)
{
	if((Data&0xfff)==0xF30 && BCmd!=BLastCmd)
	{
		if((BCmd&0x81)==0x81)
		{
			SerStep2=(BCmd>>1)&0x3f;

			BSerialBuffer=0x00000000;	//First block contains CRC
			BBufPos=0;
		}
		if((BCmd&0xff)==0x55)	//Load Offset 0
		{
			BState=2;
			BBufPos=0;
			BSerialBuffer=AdaptByte(BSerial[8*SerStep2])>>1;
		}
		if((BCmd&0xff)==0xAA)	//Load Offset 1
		{
			BState=2;
			BBufPos=0;
			BSerialBuffer=AdaptByte(BSerial[8*SerStep2+1]);
		}
		if((BCmd&0xff)==0x54)
		{
			BState=2;
			BBufPos=0;
			BSerialBuffer=AdaptByte(BSerial[8*SerStep2+2]);
		}
		if((BCmd&0xff)==0xA8)
		{
			BState=2;
			BBufPos=0;
			BSerialBuffer=AdaptByte(BSerial[8*SerStep2+3]);
		}
		if((BCmd&0xff)==0x50)
		{
			BState=2;
			BBufPos=0;
			BSerialBuffer=AdaptByte(BSerial[8*SerStep2+4]);
		}
		if((BCmd&0xff)==0xA0)
		{
			BState=2;
			BBufPos=0;
			BSerialBuffer=AdaptByte(BSerial[8*SerStep2+5]);
		}
		if((BCmd&0xff)==0x40)
		{
			BState=2;
			BBufPos=0;
			BSerialBuffer=AdaptByte(BSerial[8*SerStep2+6]);
		}
		if((BCmd&0xff)==0x80)
		{
			BState=2;
			BBufPos=0;
			BSerialBuffer=AdaptByte(BSerial[8*SerStep2+7]);
		}
		BLastCmd=BCmd;
	}
	BControl=Data;
}

static void NaomiGameIDProcessCmd()
{
	if(GCmd!=GLastCmd)
	{
		if((GCmd&0x81)==0x81)
		{
			SerStep=(GCmd>>1)&0x3f;

			GSerialBuffer=0x00000000;	//First block contains CRC
			GBufPos=0;
		}
		if((GCmd&0xff)==0x55)	//Load Offset 0
		{
			GState=2;
			GBufPos=0;
			GSerialBuffer=AdaptByte(GSerial[8*SerStep])>>0;
		}
		if((GCmd&0xff)==0xAA)	//Load Offset 1
		{
			GState=2;
			GBufPos=0;
			GSerialBuffer=AdaptByte(GSerial[8*SerStep+1]);
		}
		if((GCmd&0xff)==0x54)
		{
			GState=2;
			GBufPos=0;
			GSerialBuffer=AdaptByte(GSerial[8*SerStep+2]);
		}
		if((GCmd&0xff)==0xA8)
		{
			GState=2;
			GBufPos=0;
			GSerialBuffer=AdaptByte(GSerial[8*SerStep+3]);
		}
		if((GCmd&0xff)==0x50)
		{
			GState=2;
			GBufPos=0;
			GSerialBuffer=AdaptByte(GSerial[8*SerStep+4]);
		}
		if((GCmd&0xff)==0xA0)
		{
			GState=2;
			GBufPos=0;
			GSerialBuffer=AdaptByte(GSerial[8*SerStep+5]);
		}
		if((GCmd&0xff)==0x40)
		{
			GState=2;
			GBufPos=0;
			GSerialBuffer=AdaptByte(GSerial[8*SerStep+6]);
		}
		if((GCmd&0xff)==0x80)
		{
			GState=2;
			GBufPos=0;
			GSerialBuffer=AdaptByte(GSerial[8*SerStep+7]);
		}
		GLastCmd=GCmd;
	}
}


void NaomiGameIDWrite(const u16 Data)
{
	int Dat=Data&0x01;
	int Clk=Data&0x02;
	int Rst=Data&0x04;
	int Sta=Data&0x08;
	int Cmd=Data&0x10;
	
	if(Rst)
	{
		GState=0;
		GBufPos=0;
	}
	
	if(Clk!=GOldClk && !Clk)	//Falling Edge clock
	{
		//State change
		if(GState==0 && Sta) 
			GState=1;		
		if(GState==1 && !Sta)
			GState=2;

		//State processing
		if(GState==1)		//LoadBoardID
		{
			GSerialBuffer=BoardID;
			GBufPos=0;		//??
		}
		if(GState==2)		//ShiftBoardID
			GBufPos++;

		if(GControl!=Cmd && !Cmd)
		{
			NaomiGameIDProcessCmd();
		}
		GControl=Cmd;
	}
	if(Clk!=GOldClk && Clk)	//Rising Edge clock
	{
		if(Cmd)	//Command mode
		{
			GCmd<<=1;
			if(Dat)
				GCmd|=1;
			else
				GCmd&=0xfffffffe;
			GControl=Cmd;
		}
	}
	GOldClk=Clk;
}

u16 NaomiGameIDRead()
{
	return (GSerialBuffer&(1<<(31-GBufPos)))?1:0;
}

//DIMM board
//Uses interrupt ext#3  (holly_EXT_PCI)

//status/flags ? 0x1 is some completion/init flag(?), 0x100 is the interrupt disable flag (?)
//n1 bios rev g (n2/epr-23605b has similar behavior of not same):
//3c=0x1E03
//40=0
//44=0
//48=0
//read 4c
//wait for 4c not 0
//4c=[4c]-1

//Naomi 2 bios epr-23609
//read 3c
//wait 4c to be non 0
//

//SO the writes to 3c/stuff are not relaced with 4c '1'
//If the dimm board has some internal cpu/pic logic 
//4c '1' seems to be the init done bit (?)
//n1/n2 clears it after getting a non 0 value
//n1 bios writes the value -1, meaning it expects the bit 0 to be set
//.//

u32 reg_dimm_command;		// command, written, 0x1E03 some flag ?
u32 reg_dimm_offsetl;
u32 reg_dimm_parameterl;
u32 reg_dimm_parameterh;
u32 reg_dimm_status = 0x11;

static bool aw_ram_test_skipped = false;

void naomi_process(u32 command, u32 offsetl, u32 parameterl, u32 parameterh)
{
	DEBUG_LOG(NAOMI, "Naomi process 0x%04X 0x%04X 0x%04X 0x%04X", command, offsetl, parameterl, parameterh);
	DEBUG_LOG(NAOMI, "Possible format 0 %d 0x%02X 0x%04X",command >> 15,(command & 0x7e00) >> 9, command & 0x1FF);
	DEBUG_LOG(NAOMI, "Possible format 1 0x%02X 0x%02X", (command & 0xFF00) >> 8,command & 0xFF);

	u32 param=(command&0xFF);
	if (param==0xFF)
	{
		DEBUG_LOG(NAOMI, "invalid opcode or smth ?");
	}
	static int opcd=0;
	//else if (param!=3)
	if (opcd<255)
	{
		reg_dimm_command=0x8000 | (opcd%12<<9) | (0x0);
		DEBUG_LOG(NAOMI, "new reg is 0x%X", reg_dimm_command);
		asic_RaiseInterrupt(holly_EXP_PCI);
		DEBUG_LOG(NAOMI, "Interrupt raised");
		opcd++;
	}
}

u32 ReadMem_naomi(u32 address, u32 size)
{
	verify(size != 1);
	if (unlikely(CurrentCartridge == NULL))
	{
		INFO_LOG(NAOMI, "called without cartridge");
		return 0xFFFF;
	}
	if (address >= NAOMI_COMM2_CTRL_addr && address <= NAOMI_COMM2_STATUS1_addr)
		return m3comm.ReadMem(address, size);
	else
		return CurrentCartridge->ReadMem(address, size);
}

void WriteMem_naomi(u32 address, u32 data, u32 size)
{
	if (unlikely(CurrentCartridge == NULL))
	{
		INFO_LOG(NAOMI, "called without cartridge");
		return;
	}
	if (address >= NAOMI_COMM2_CTRL_addr && address <= NAOMI_COMM2_STATUS1_addr && settings.platform.system == DC_PLATFORM_NAOMI)
		m3comm.WriteMem(address, data, size);
	else
		CurrentCartridge->WriteMem(address, data, size);
}

//Dma Start
void Naomi_DmaStart(u32 addr, u32 data)
{
	if (SB_GDEN==0)
	{
		INFO_LOG(NAOMI, "Invalid (NAOMI)GD-DMA start, SB_GDEN=0. Ignoring it.");
		return;
	}
	
	SB_GDST |= data & 1;

	if (SB_GDST == 0)
		return;

	if (!m3comm.DmaStart(addr, data) && CurrentCartridge != NULL)
	{
		DEBUG_LOG(NAOMI, "NAOMI-DMA start addr %08X len %d", SB_GDSTAR, SB_GDLEN);
		verify(1 == SB_GDDIR);
		u32 start = SB_GDSTAR & 0x1FFFFFE0;
		u32 len = (SB_GDLEN + 31) & ~31;
		SB_GDLEND = 0;
		while (len > 0)
		{
			u32 block_len = len;
			void* ptr = CurrentCartridge->GetDmaPtr(block_len);
			if (block_len == 0)
			{
				INFO_LOG(NAOMI, "Aborted DMA transfer. Read past end of cart?");
				break;
			}
			WriteMemBlock_nommu_ptr(start, (u32*)ptr, block_len);
			CurrentCartridge->AdvancePtr(block_len);
			len -= block_len;
			start += block_len;
			SB_GDLEND += block_len;
		}
		SB_GDSTARD = start;
	}
	else
	{
		SB_GDSTARD = SB_GDSTAR + SB_GDLEN;
		SB_GDLEND = SB_GDLEN;
	}
	SB_GDST = 0;
	asic_RaiseInterrupt(holly_GDROM_DMA);
}


void Naomi_DmaEnable(u32 addr, u32 data)
{
	SB_GDEN=data&1;
	if (SB_GDEN==0 && SB_GDST==1)
	{
		INFO_LOG(NAOMI, "(NAOMI)GD-DMA aborted");
		SB_GDST=0;
	}
}
void naomi_reg_Init()
{
	#ifdef NAOMI_COMM
	CommMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security 
		PAGE_READWRITE,          // read/write access
		0,                       // max. object size 
		0x1000*4,                // buffer size  
		L"Global\\nullDC_103_naomi_comm");                 // name of mapping object

	if (CommMapFile == NULL || CommMapFile==INVALID_HANDLE_VALUE) 
	{ 
		_tprintf(TEXT("Could not create file mapping object (%d).\nTrying to open existing one\n"), 	GetLastError());
		
		CommMapFile=OpenFileMapping(
                   FILE_MAP_ALL_ACCESS,   // read/write access
                   FALSE,                 // do not inherit the name
                   L"Global\\nullDC_103_naomi_comm");               // name of mapping object 
	}
	
	if (CommMapFile == NULL || CommMapFile==INVALID_HANDLE_VALUE) 
	{ 
		_tprintf(TEXT("Could not open existing file either\n"), 	GetLastError());
		CommMapFile=INVALID_HANDLE_VALUE;
	}
	else
	{
		printf("NAOMI: Created \"Global\\nullDC_103_naomi_comm\"\n");
		CommSharedMem = (u32*) MapViewOfFile(CommMapFile,   // handle to map object
			FILE_MAP_ALL_ACCESS, // read/write permission
			0,                   
			0,                   
			0x1000*4);           

		if (CommSharedMem == NULL) 
		{ 
			_tprintf(TEXT("Could not map view of file (%d).\n"), 
				GetLastError()); 

			CloseHandle(CommMapFile);
			CommMapFile=INVALID_HANDLE_VALUE;
		}
		else
			printf("NAOMI: Mapped CommSharedMem\n");
	}
	#endif
	NaomiInit();
}

void naomi_reg_Term()
{
#ifdef NAOMI_COMM
	if (CommSharedMem)
	{
		UnmapViewOfFile(CommSharedMem);
	}
	if (CommMapFile!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(CommMapFile);
	}
#endif
	m3comm.closeNetwork();
	naomiNetwork.terminate();
}

void naomi_reg_Reset(bool hard)
{
	sb_rio_register(SB_GDST_addr, RIO_WF, 0, &Naomi_DmaStart);
	sb_rio_register(SB_GDEN_addr, RIO_WF, 0, &Naomi_DmaEnable);
	SB_GDST = 0;
	SB_GDEN = 0;

	aw_ram_test_skipped = false;
	GSerialBuffer = 0;
	BSerialBuffer = 0;
	GBufPos = 0;
	BBufPos = 0;
	GState = 0;
	BState = 0;
	GOldClk = 0;
	BOldClk = 0;
	BControl = 0;
	BCmd = 0;
	BLastCmd = 0;
	GControl = 0;
	GCmd = 0;
	GLastCmd = 0;
	SerStep = 0;
	SerStep2 = 0;
	reg_dimm_command = 0;
	reg_dimm_offsetl = 0;
	reg_dimm_parameterl = 0;
	reg_dimm_parameterh = 0;
	reg_dimm_status = 0x11;
	m3comm.closeNetwork();
	naomiNetwork.terminate();
	if (hard)
		naomi_cart_Close();
}

static u8 aw_maple_devs;
static u64 coin_chute_time[4];

u32 libExtDevice_ReadMem_A0_006(u32 addr,u32 size) {
	addr &= 0x7ff;
	//printf("libExtDevice_ReadMem_A0_006 %d@%08x: %x\n", size, addr, mem600[addr]);
	switch (addr)
	{
//	case 0:
//		return 0;
//	case 4:
//		return 1;
	case 0x280:
		// 0x00600280 r  0000dcba
		//	a/b - 1P/2P coin inputs (JAMMA), active low
		//	c/d - 3P/4P coin inputs (EX. IO board), active low
		//
		//	(ab == 0) -> BIOS skip RAM test
		if (!aw_ram_test_skipped)
		{
			// Skip RAM test at startup
			aw_ram_test_skipped = true;
			return 0;
		}
		{
			u8 coin_input = 0xF;
			u64 now = sh4_sched_now64();
			for (int slot = 0; slot < 4; slot++)
			{
				if (maple_atomiswave_coin_chute(slot))
				{
					// ggx15 needs 4 or 5 reads to register the coin but it needs to be limited to avoid coin errors
					// 1 s of cpu time is too much, 1/2 s seems to work, let's use 100 ms
					if (coin_chute_time[slot] == 0 || now - coin_chute_time[slot] < SH4_MAIN_CLOCK / 10)
					{
						if (coin_chute_time[slot] == 0)
							coin_chute_time[slot] = now;
						coin_input &= ~(1 << slot);
					}
				}
				else
				{
					coin_chute_time[slot] = 0;
				}
			}
			return coin_input;
		}

	case 0x284:		// Atomiswave maple devices
		// ddcc0000 where cc/dd are the types of devices on maple bus 2 and 3:
		// 0: regular AtomisWave controller
		// 1: light gun
		// 2,3: mouse/trackball
		//printf("NAOMI 600284 read %x\n", aw_maple_devs);
		return aw_maple_devs;
	case 0x288:
		// ??? Dolphin Blue
		return 0;

	}
	INFO_LOG(NAOMI, "Unhandled read @ %x sz %d", addr, size);
	return 0xFF;
}

void libExtDevice_WriteMem_A0_006(u32 addr,u32 data,u32 size) {
	addr &= 0x7ff;
	//printf("libExtDevice_WriteMem_A0_006 %d@%08x: %x\n", size, addr, data);
	switch (addr)
	{
	case 0x284:		// Atomiswave maple devices
		DEBUG_LOG(NAOMI, "NAOMI 600284 write %x", data);
		aw_maple_devs = data & 0xF0;
		return;
	case 0x288:
		// ??? Dolphin Blue
		return;
	//case 0x28C:		// Wheel force feedback?
	default:
		break;
	}
	INFO_LOG(NAOMI, "Unhandled write @ %x (%d): %x", addr, size, data);
}

void naomi_Serialize(Serializer& ser)
{
	ser << GSerialBuffer;
	ser << BSerialBuffer;
	ser << GBufPos;
	ser << BBufPos;
	ser << GState;
	ser << BState;
	ser << GOldClk;
	ser << BOldClk;
	ser << BControl;
	ser << BCmd;
	ser << BLastCmd;
	ser << GControl;
	ser << GCmd;
	ser << GLastCmd;
	ser << SerStep;
	ser << SerStep2;
	ser.serialize(BSerial, 69);
	ser.serialize(GSerial, 69);
	ser << reg_dimm_command;
	ser << reg_dimm_offsetl;
	ser << reg_dimm_parameterl;
	ser << reg_dimm_parameterh;
	ser << reg_dimm_status;
	ser << aw_maple_devs;
	ser << coin_chute_time;
	ser << aw_ram_test_skipped;
	// TODO serialize m3comm?
}
void naomi_Deserialize(Deserializer& deser)
{
	if (deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip<u32>();		// naomi_updates
		deser.skip<u32>();		// BoardID
	}
	deser >> GSerialBuffer;
	deser >> BSerialBuffer;
	deser >> GBufPos;
	deser >> BBufPos;
	deser >> GState;
	deser >> BState;
	deser >> GOldClk;
	deser >> BOldClk;
	deser >> BControl;
	deser >> BCmd;
	deser >> BLastCmd;
	deser >> GControl;
	deser >> GCmd;
	deser >> GLastCmd;
	deser >> SerStep;
	deser >> SerStep2;
	deser.deserialize(BSerial, 69);
	deser.deserialize(GSerial, 69);
	deser >> reg_dimm_command;
	deser >> reg_dimm_offsetl;
	deser >> reg_dimm_parameterl;
	deser >> reg_dimm_parameterh;
	deser >> reg_dimm_status;
	if (deser.version() < Deserializer::V11)
		deser.skip<u8>();
	else if (deser.version() >= Deserializer::V14)
		deser >> aw_maple_devs;
	if (deser.version() >= Deserializer::V20)
	{
		deser >> coin_chute_time;
		deser >> aw_ram_test_skipped;
	}
}
