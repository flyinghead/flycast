/*
	This file is a mix of my code, Zezu's, and duno wtf-else (most likely ElSemi's ?)
*/
#include "types.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/holly/holly_intc.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/aica/aica_if.h"
#include "hw/hwreg.h"

#include "naomi.h"
#include "naomi_cart.h"
#include "naomi_regs.h"
#include "naomi_m3comm.h"
#include "serialize.h"
#include "network/output.h"
#include "hw/sh4/modules/modules.h"
#include "rend/gui.h"
#include "printer.h"

#include <algorithm>

static NaomiM3Comm m3comm;
Multiboard *multiboard;

static const u32 BoardID = 0x980055AA;
static u32 GSerialBuffer, BSerialBuffer;
static int GBufPos, BBufPos;
static int GState, BState;
static int GOldClk, BOldClk;
static int BControl, BCmd, BLastCmd;
static int GControl, GCmd, GLastCmd;
static int SerStep, SerStep2;

/*
El numero de serie solo puede contener:
0-9		(0x30-0x39)
A-H		(0x41-0x48)
J-N		(0x4A-0x4E)
P-Z		(0x50-0x5A)
*/
static u8 BSerial[]="\xB7"/*CRC1*/"\x19"/*CRC2*/"0123234437897584372973927387463782196719782697849162342198671923649";
//static u8 BSerial[]="\x09\xa1                              0000000000000000\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"; // default from mame
static u8 GSerial[]="\xB7"/*CRC1*/"\x19"/*CRC2*/"0123234437897584372973927387463782196719782697849162342198671923649";

static u8 midiTxBuf[4];
static u32 midiTxBufIndex;

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
	int Dat=Data&0x01;	// mame: SDA
	int Clk=Data&0x02;	// mame: SCL
	int Rst=Data&0x04;	// mame: CS
	int Sta=Data&0x08;	// mame: RST
	int Cmd=Data&0x10;	// mame: unused...
	
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

static bool aw_ram_test_skipped = false;


u32 ReadMem_naomi(u32 address, u32 size)
{
//	verify(size != 1);
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
	if (address >= NAOMI_COMM2_CTRL_addr && address <= NAOMI_COMM2_STATUS1_addr
			&& settings.platform.isNaomi())
		m3comm.WriteMem(address, data, size);
	else
		CurrentCartridge->WriteMem(address, data, size);
}

//Dma Start
static void Naomi_DmaStart(u32 addr, u32 data)
{
	if ((data & 1) == 0)
		return;
	if (SB_GDEN == 0)
	{
		INFO_LOG(NAOMI, "Invalid (NAOMI)GD-DMA start, SB_GDEN=0. Ignoring it.");
		return;
	}
	
	if (multiboard != nullptr && multiboard->dmaStart())
	{
	}
	else if (!m3comm.DmaStart(addr, data) && CurrentCartridge != NULL)
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
	asic_RaiseInterrupt(holly_GDROM_DMA);
}


static void Naomi_DmaEnable(u32 addr, u32 data)
{
	SB_GDEN = data & 1;
	if (SB_GDEN == 0 && SB_GDST == 1)
	{
		INFO_LOG(NAOMI, "(NAOMI)GD-DMA aborted");
		SB_GDST = 0;
	}
}

void naomi_reg_Init()
{
	NaomiInit();
	networkOutput.init();
}

void naomi_reg_Term()
{
	if (multiboard != nullptr)
		delete multiboard;
	multiboard = nullptr;
	m3comm.closeNetwork();
	networkOutput.term();
}

void naomi_reg_Reset(bool hard)
{
	hollyRegs.setWriteHandler<SB_GDST_addr>(Naomi_DmaStart);
	hollyRegs.setWriteHandler<SB_GDEN_addr>(Naomi_DmaEnable);
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
	m3comm.closeNetwork();
	if (hard)
	{
		naomi_cart_Close();
		if (multiboard != nullptr)
		{
			delete multiboard;
			multiboard = nullptr;
		}
		if (settings.naomi.multiboard)
			multiboard = new Multiboard();
		networkOutput.reset();
	}
	else if (multiboard != nullptr)
		multiboard->reset();
}

static u8 aw_maple_devs;
static u64 coin_chute_time[4];
static u8 awDigitalOuput;

u32 libExtDevice_ReadMem_A0_006(u32 addr, u32 size)
{
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
	case 0x28c:
		return awDigitalOuput;
	}
	INFO_LOG(NAOMI, "Unhandled read @ %x sz %d", addr, size);
	return 0xFF;
}

void libExtDevice_WriteMem_A0_006(u32 addr, u32 data, u32 size)
{
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
	case 0x28C:		// Digital output
		if ((u8)data != awDigitalOuput)
		{
			if (atomiswaveForceFeedback)
				// Wheel force feedback:
				// bit 0    direction (0 pos, 1 neg)
				// bit 1-4  strength
				networkOutput.output("awffb", (u8)data);
			else
			{
				u8 changes = data ^ awDigitalOuput;
				for (int i = 0; i < 8; i++)
					if (changes & (1 << i))
					{
						std::string name = "lamp" + std::to_string(i);
						networkOutput.output(name.c_str(), (data >> i) & 1);
					}
			}
			awDigitalOuput = data;
			DEBUG_LOG(NAOMI, "AW output %02x", data);
		}
		return;
	default:
		break;
	}
	INFO_LOG(NAOMI, "Unhandled write @ %x (%d): %x", addr, size, data);
}

static bool ffbCalibrating;

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
	ser << aw_maple_devs;
	ser << coin_chute_time;
	ser << aw_ram_test_skipped;
	ser << midiTxBuf;
	ser << midiTxBufIndex;
	// TODO serialize m3comm?
	ser << ffbCalibrating;
}
void naomi_Deserialize(Deserializer& deser)
{
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
	if (deser.version() < Deserializer::V36)
	{
		deser.skip<u32>(); // reg_dimm_command;
		deser.skip<u32>(); // reg_dimm_offsetl;
		deser.skip<u32>(); // reg_dimm_parameterl;
		deser.skip<u32>(); // reg_dimm_parameterh;
		deser.skip<u32>(); // reg_dimm_status;
	}
	if (deser.version() < Deserializer::V11)
		deser.skip<u8>();
	else if (deser.version() >= Deserializer::V14)
		deser >> aw_maple_devs;
	if (deser.version() >= Deserializer::V20)
	{
		deser >> coin_chute_time;
		deser >> aw_ram_test_skipped;
	}
	if (deser.version() >= Deserializer::V27)
	{
		deser >> midiTxBuf;
		deser >> midiTxBufIndex;
	}
	else
	{
		midiTxBufIndex = 0;
	}
	if (deser.version() >= Deserializer::V34)
		deser >> ffbCalibrating;
	else
		ffbCalibrating = false;
}

static void midiSend(u8 b1, u8 b2, u8 b3)
{
	aica::midiSend(b1);
	aica::midiSend(b2);
	aica::midiSend(b3);
	aica::midiSend((b1 ^ b2 ^ b3) & 0x7f);
}

static void forceFeedbackMidiReceiver(u8 data)
{
	static float position = 8192.f;
	static float torque;
	position = std::min(16383.f, std::max(0.f, position + torque));
	if (data & 0x80)
		midiTxBufIndex = 0;
	midiTxBuf[midiTxBufIndex] = data;
	if (midiTxBufIndex == 3 && ((midiTxBuf[0] ^ midiTxBuf[1] ^ midiTxBuf[2]) & 0x7f) == midiTxBuf[3])
	{
		if (midiTxBuf[0] == 0x84)
			torque = ((midiTxBuf[1] << 7) | midiTxBuf[2]) - 0x80;
		else if (midiTxBuf[0] == 0xff)
			ffbCalibrating = true;
		else if (midiTxBuf[0] == 0xf0)
			ffbCalibrating = false;

		if (!ffbCalibrating)
		{
			int direction = -1;
			if (NaomiGameInputs != nullptr)
				direction = NaomiGameInputs->axes[0].inverted ? 1 : -1;

			position = std::clamp(mapleInputState[0].fullAxes[0] * direction * 64.f + 8192.f, 0.f, 16383.f);
		}
		// required: b1 & 0x1f == 0x10 && b1 & 0x40 == 0
		midiSend(0x90, ((int)position >> 7) & 0x7f, (int)position & 0x7f);

		// decoding from FFB Arcade Plugin (by Boomslangnz)
		// https://github.com/Boomslangnz/FFBArcadePlugin/blob/master/Game%20Files/Demul.cpp
		if (midiTxBuf[0] == 0x85)
			MapleConfigMap::UpdateVibration(0, std::max(0.f, (float)(midiTxBuf[2] - 1) / 24.f), 0.f, 5);
		if (midiTxBuf[0] != 0xfd)
			networkOutput.output("midiffb", (midiTxBuf[0] << 16) | (midiTxBuf[1]) << 8 | midiTxBuf[2]);
	}
	midiTxBufIndex = (midiTxBufIndex + 1) % std::size(midiTxBuf);
}

void initMidiForceFeedback()
{
	aica::setMidiReceiver(forceFeedbackMidiReceiver);
}

struct DriveSimPipe : public SerialPipe
{
	void write(u8 data) override
	{
		if (buffer.empty() && data != 2)
			return;
		if (buffer.size() == 7)
		{
			u8 checksum = 0;
			for (u8 b : buffer)
				checksum += b;
			if (checksum == data)
			{
				int newTacho = (buffer[2] - 1) * 100;
				if (newTacho != tacho)
				{
					tacho = newTacho;
					networkOutput.output("tachometer", tacho);
				}
				int newSpeed = buffer[3] - 1;
				if (newSpeed != speed)
				{
					speed = newSpeed;
					networkOutput.output("speedometer", speed);
				}
				if (!config::NetworkOutput)
				{
					char message[16];
					sprintf(message, "Speed: %3d", speed);
					gui_display_notification(message, 1000);
				}
			}
			buffer.clear();
		}
		else
		{
			buffer.push_back(data);
		}
	}

	void reset()
	{
		buffer.clear();
		tacho = -1;
		speed = -1;
	}
private:
	std::vector<u8> buffer;
	int tacho = -1;
	int speed = -1;
};

void initDriveSimSerialPipe()
{
	static DriveSimPipe pipe;

	pipe.reset();
	serial_setPipe(&pipe);
}

G2PrinterConnection g2PrinterConnection;

u32 G2PrinterConnection::read(u32 addr, u32 size)
{
	if (addr == STATUS_REG_ADDR)
	{
		u32 ret = printerStat;
		printerStat |= 1;
		DEBUG_LOG(NAOMI, "Printer status == %x", ret);
		return ret;
	}
	else
	{
		INFO_LOG(NAOMI, "Unhandled G2 Ext read<%d> at %x", size, addr);
		return 0;
	}
}

void G2PrinterConnection::write(u32 addr, u32 size, u32 data)
{
	switch (addr)
	{
	case DATA_REG_ADDR:
		for (u32 i = 0; i < size; i++)
			printer::print((char)(data >> (i * 8)));
		break;

	case STATUS_REG_ADDR:
		DEBUG_LOG(NAOMI, "Printer status = %x", data);
		printerStat &= ~1;
		break;

	default:
		INFO_LOG(NAOMI, "Unhandled G2 Ext write<%d> at %x: %x", size, addr, data);
		break;
	}
}

