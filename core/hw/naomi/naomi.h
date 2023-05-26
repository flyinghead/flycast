/*
**	naomi.h
*/
#pragma once
#include "types.h"
#include "multiboard.h"

void naomi_reg_Init();
void naomi_reg_Term();
void naomi_reg_Reset(bool hard);
void naomi_Serialize(Serializer& ser);
void naomi_Deserialize(Deserializer& deser);

u32  ReadMem_naomi(u32 Addr, u32 size);
void WriteMem_naomi(u32 Addr, u32 data, u32 size);

void NaomiBoardIDWrite(u16 Data);
void NaomiBoardIDWriteControl(u16 Data);
u16 NaomiBoardIDRead();
u16 NaomiGameIDRead();
void NaomiGameIDWrite(u16 Data);

void initMidiForceFeedback();
void initDriveSimSerialPipe();

u32  libExtDevice_ReadMem_A0_006(u32 addr, u32 size);
void libExtDevice_WriteMem_A0_006(u32 addr, u32 data, u32 size);

class G2PrinterConnection
{
public:
	u32 read(u32 addr, u32 size);
	void write(u32 addr, u32 size, u32 data);

	static constexpr u32 STATUS_REG_ADDR = 0x1018000;
	static constexpr u32 DATA_REG_ADDR = 0x1010000;

private:
	u32 printerStat = 0xf;
};
extern G2PrinterConnection g2PrinterConnection;

extern Multiboard *multiboard;

//Area 0 , 0x01000000- 0x01FFFFFF      [G2 Ext. Device]
static inline u32 g2ext_readMem(u32 addr, u32 size)
{
	if (addr == G2PrinterConnection::STATUS_REG_ADDR || addr == G2PrinterConnection::DATA_REG_ADDR)
		return g2PrinterConnection.read(addr, size);
	if (multiboard != nullptr)
		return multiboard->readG2Ext(addr, size);

	INFO_LOG(NAOMI, "Unhandled G2 Ext read<%d> at %x", size, addr);
	return 0;
}

static inline void g2ext_writeMem(u32 addr, u32 data, u32 size)
{
	if (addr == G2PrinterConnection::STATUS_REG_ADDR || addr == G2PrinterConnection::DATA_REG_ADDR)
		g2PrinterConnection.write(addr, size, data);
	else if (multiboard != nullptr)
		multiboard->writeG2Ext(addr, size, data);
	else
		INFO_LOG(NAOMI, "Unhandled G2 Ext write<%d> at %x: %x", size, addr, data);
}
