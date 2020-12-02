#pragma once
#include "types.h"

enum PlainJoystickAxisId
{
	PJAI_X1 = 0,
	PJAI_Y1 = 1,
	PJAI_X2 = 2,
	PJAI_Y2 = 3,

	PJAI_Count = 4
};

enum PlainJoystickTriggerId
{
	PJTI_L = 0,
	PJTI_R = 1,

	PJTI_Count = 2
};

struct PlainJoystickState
{
	PlainJoystickState()
	{
		joy[0]=joy[1]=joy[2]=joy[3]=0x80;
		trigger[0]=trigger[1]=0;
	}

	u32 kcode = ~0;

	u8 joy[PJAI_Count];
	u8 trigger[PJTI_Count];
};

struct IMapleConfigMap
{
	virtual void SetVibration(float power, float inclination, u32 duration_ms) = 0;
	virtual void GetInput(PlainJoystickState* pjs)=0;
	virtual void SetImage(void* img)=0;
	virtual ~IMapleConfigMap() {}
};

void mcfg_CreateDevices();
void mcfg_CreateNAOMIJamma();
void mcfg_CreateAtomisWaveControllers();

void mcfg_DestroyDevices();
void mcfg_SerializeDevices(void **data, unsigned int *total_size);
void mcfg_UnserializeDevices(void **data, unsigned int *total_size, bool old_type_numbering);

bool maple_atomiswave_coin_chute(int slot);
