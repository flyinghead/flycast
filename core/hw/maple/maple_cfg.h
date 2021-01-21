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

struct maple_device;

class MapleConfigMap
{
public:
	MapleConfigMap(maple_device* dev, s32 player_num = -1) : dev(dev), player_num(player_num) {}
	void SetVibration(float power, float inclination, u32 duration_ms);
	void GetInput(PlainJoystickState* pjs);
	void GetAbsCoordinates(int& x, int& y);
	void GetMouseInput(u32& buttons, int& x, int& y, int& wheel);
	void SetImage(u8 *img);

private:
	u32 playerNum();

	maple_device* dev;
	s32 player_num;
};

void mcfg_CreateDevices();
void mcfg_CreateNAOMIJamma();
void mcfg_CreateAtomisWaveControllers();

void mcfg_DestroyDevices();
void mcfg_SerializeDevices(void **data, unsigned int *total_size);
void mcfg_UnserializeDevices(void **data, unsigned int *total_size, bool old_type_numbering);

bool maple_atomiswave_coin_chute(int slot);
void push_vmu_screen(int bus_id, int bus_port, u8* buffer);
