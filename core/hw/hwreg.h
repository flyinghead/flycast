#pragma once
#include "types.h"

typedef u32  RegReadAddrFP(u32 addr);
typedef void RegWriteAddrFP(u32 addr, u32 data);

/*
	Read Write Const
	D    D     N      -> 0			-> RIO_DATA
	D    F     N      -> WF			-> RIO_WF
	F    F     N      -> RF|WF		-> RIO_FUNC
	D    X     N      -> RO|WF		-> RIO_RO
	F    X     N      -> RF|WF|RO	-> RIO_RO_FUNC
	D    X     Y      -> CONST|RO|WF-> RIO_CONST
	X    F     N      -> RF|WF|WO	-> RIO_WO_FUNC
*/
enum RegStructFlags
{
	REG_RF = 8,
	REG_WF = 16,
	REG_RO = 32,
	REG_WO = 64,
	REG_NO_ACCESS = REG_RO | REG_WO,
};

enum RegIO
{
	RIO_DATA = 0,
	RIO_WF = REG_WF,
	RIO_FUNC = REG_WF | REG_RF,
	RIO_RO = REG_RO | REG_WF,
	RIO_RO_FUNC = REG_RO | REG_RF | REG_WF,
	RIO_CONST = REG_RO | REG_WF,
	RIO_WO_FUNC = REG_WF | REG_RF | REG_WO,
	RIO_NO_ACCESS = REG_WF | REG_RF | REG_NO_ACCESS
};

struct RegisterStruct
{
	union
	{
		u32 data32;					//stores data of reg variable [if used] 32b
		u16 data16;					//stores data of reg variable [if used] 16b
		u8  data8;					//stores data of reg variable [if used]	8b

		RegReadAddrFP* readFunctionAddr; //stored pointer to reg read function
	};

	RegWriteAddrFP* writeFunctionAddr; //stored pointer to reg write function

	u32 flags;					//Access flags !

	void reset()
	{
		if (!(flags & (REG_RO | REG_RF)))
			data32 = 0;
	}
};

template<typename T>
T ReadMemArr(const u8 *array, u32 addr)
{
	return *(const T *)&array[addr];
}

template<typename T>
void WriteMemArr(u8 *array, u32 addr, T data)
{
	*(T *)&array[addr] = data;
}

