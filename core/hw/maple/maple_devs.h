#pragma once
#include "types.h"

enum MapleDeviceType
{
	MDT_SegaController,

	MDT_SegaVMU,
	MDT_Microphone,
	MDT_PurupuruPack,
	MDT_Keyboard,
	MDT_Mouse,
	MDT_LightGun,

	MDT_NaomiJamma,

	MDT_None,
	MDT_Count
};

enum NAOMI_KEYS
{
	NAOMI_START_KEY = 1 << 15,
	NAOMI_SERVICE_KEY = 1 << 14,

	NAOMI_UP_KEY = 1 << 13,
	NAOMI_DOWN_KEY = 1 << 12,
	NAOMI_LEFT_KEY = 1 << 11,
	NAOMI_RIGHT_KEY = 1 << 10,

	NAOMI_BTN0_KEY = 1 << 9,
	NAOMI_BTN1_KEY = 1 << 8,
	NAOMI_BTN2_KEY = 1 << 7,
	NAOMI_BTN3_KEY = 1 << 6,
	NAOMI_BTN4_KEY = 1 << 5,
	NAOMI_BTN5_KEY = 1 << 4,
	NAOMI_BTN6_KEY = 1 << 3,
	NAOMI_BTN7_KEY = 1 << 2,
	NAOMI_BTN8_KEY = 1 << 16,

	NAOMI_TEST_KEY = 1 << 1,

	// Not an actual button
	NAOMI_COIN_KEY = 1 << 0,
};

enum AWAVE_KEYS
{
	AWAVE_START_KEY = 1 << 3,

	AWAVE_BTN0_KEY  = 1 << 2,
	AWAVE_BTN1_KEY  = 1 << 1,
	AWAVE_BTN2_KEY  = 1 << 0,
	AWAVE_BTN3_KEY  = 1 << 10,
	AWAVE_BTN4_KEY  = 1 << 9,

	AWAVE_UP_KEY    = 1 << 4,
	AWAVE_DOWN_KEY  = 1 << 5,
	AWAVE_LEFT_KEY  = 1 << 6,
	AWAVE_RIGHT_KEY = 1 << 7,

	AWAVE_SERVICE_KEY = 1 << 13,
	AWAVE_TEST_KEY 	  = 1 << 14,

	// Not an actual button
	AWAVE_COIN_KEY    = 1 << 15,
	AWAVE_TRIGGER_KEY    = 1 << 12,
};

struct IMapleConfigMap;

struct maple_device
{
	u8 maple_port;          //raw maple port
	u8 bus_port;            //0 .. 5
	u8 bus_id;              //0 .. 3
	wchar logical_port[3];  //A0, etc
	IMapleConfigMap* config;

	//fill in the info
	void Setup(u32 prt);

	virtual void OnSetup(){};
	virtual ~maple_device();
	u32 Dma(u32 Command,u32* buffer_in,u32 buffer_in_len,u32* buffer_out,u32& buffer_out_len);
	virtual u32 RawDma(u32* buffer_in, u32 buffer_in_len, u32* buffer_out) = 0;
	virtual bool maple_serialize(void **data, unsigned int *total_size){return true;};
	virtual bool maple_unserialize(void **data, unsigned int *total_size){return true;};
	virtual MapleDeviceType get_device_type() = 0;
	virtual void get_lightgun_pos() {};
};

struct _NaomiState
{
	u8 Cmd;
	u8 Mode;
	u8 Node;
};

maple_device* maple_Create(MapleDeviceType type);
#define SIZE_OF_MIC_DATA	480 //ALSO DEFINED IN SipEmulator.java
#ifndef TARGET_PANDORA
int get_mic_data(u8* buffer); //implemented in Android.cpp
#endif
void push_vmu_screen(int bus_id, int bus_port, u8* buffer);
#define MAPLE_PORTS 4
