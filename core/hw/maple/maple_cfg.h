#pragma once
#include "types.h"

enum MapleDeviceType
{
	MDT_SegaController,

	MDT_SegaVMU,
	MDT_Microphone,
	MDT_PurupuruPack,
	MDT_AsciiStick,
	MDT_Keyboard,
	MDT_Mouse,
	MDT_LightGun,
	MDT_TwinStick,

	MDT_NaomiJamma,

	MDT_None,
	MDT_Count
};

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
	MapleConfigMap(maple_device* dev) : dev(dev) {}
	void SetVibration(float power, float inclination, u32 duration_ms);
	void GetInput(PlainJoystickState* pjs);
	void GetAbsCoordinates(int& x, int& y);
	void GetMouseInput(u8& buttons, int& x, int& y, int& wheel);
	void GetKeyboardInput(u8& shift, u8 keys[6]);
	void SetImage(u8 *img);

	static void (*UpdateVibration)(u32 port, float power, float inclination, u32 duration_ms);
	bool invertMouseY = false;

private:
	u32 playerNum();

	maple_device* dev;
};

struct MapleInputState
{
	MapleInputState() : halfAxes{}, fullAxes{} {
		memset(keyboard.key, 0, sizeof(keyboard.key));
	}

	u32 kcode = ~0;
	u8 halfAxes[PJTI_Count];		// LT, RT
	int8_t fullAxes[PJAI_Count];	// Left X, Y, Right X, Y
	u8 mouseButtons = ~0;
	struct {
		int x = -1;
		int y = -1;
	} absPos;
	struct {
		int16_t x = 0;
		int16_t y = 0;
		int16_t wheel = 0;
	} relPos;
	struct {
		u8 shift = 0;				// modifier keys bitmask
		u8 key[6];					// normal keys pressed
	} keyboard;
};
extern MapleInputState mapleInputState[4];

void mcfg_CreateDevices();
void mcfg_DestroyDevices();
void mcfg_SerializeDevices(Serializer& ser);
void mcfg_DeserializeDevices(Deserializer& deser);

bool maple_atomiswave_coin_chute(int slot);
void push_vmu_screen(int bus_id, int bus_port, u8* buffer);
