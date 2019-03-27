#include "types.h"
#include "maple_if.h"
#include "maple_helper.h"
#include "maple_devs.h"
#include "maple_cfg.h"
#include "cfg/cfg.h"

#define HAS_VMU
/*
bus_x=0{p0=1{config};p1=2{config};config;}
Plugins:
	Input Source
		EventMap -- 'Raw' interface, source_name[seid]:mode
		KeyMap -- translated chars ( no re-mapping possible)
	Output
		Image

*/
/*
	MapleConfig:
		InputUpdate(&fmt);
		ImageUpdate(data);
*/
void UpdateInputState(u32 port);
void UpdateVibration(u32 port, float power, float inclination, u32 duration_ms);

extern u16 kcode[4];
extern u32 vks[4];
extern s8 joyx[4],joyy[4];
extern u8 rt[4],lt[4];
extern bool naomi_test_button;

u8 GetBtFromSgn(s8 val)
{
	return val+128;
}

struct MapleConfigMap : IMapleConfigMap
{
	maple_device* dev;
	s32 player_num;

	MapleConfigMap(maple_device* dev, s32 player_num = -1)
	{
		this->dev=dev;
		this->player_num = player_num;
	}

	void SetVibration(float power, float inclination, u32 duration_ms)
	{
		int player_num = this->player_num == -1 ? dev->bus_id : this->player_num;
		UpdateVibration(player_num, power, inclination, duration_ms);
	}

	void GetInput(PlainJoystickState* pjs)
	{
		int player_num = this->player_num == -1 ? dev->bus_id : this->player_num;
		UpdateInputState(player_num);

		pjs->kcode=kcode[player_num];
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
		pjs->kcode |= 0xF901;
#elif DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
		if (naomi_test_button)
			pjs->kcode &= ~(1 << 14);
//		if (!(pjs->kcode & (1 << 9)))	// Hack (Y -> service btn)
//			pjs->kcode &= ~(1 << 13);
#endif
		pjs->joy[PJAI_X1]=GetBtFromSgn(joyx[player_num]);
		pjs->joy[PJAI_Y1]=GetBtFromSgn(joyy[player_num]);
		pjs->trigger[PJTI_R]=rt[player_num];
		pjs->trigger[PJTI_L]=lt[player_num];
	}
	void SetImage(void* img)
	{
		//?
	}
};

void mcfg_Create(MapleDeviceType type, u32 bus, u32 port, s32 player_num = -1)
{
	if (MapleDevices[bus][port] != NULL)
		delete MapleDevices[bus][port];
	maple_device* dev = maple_Create(type);
	dev->Setup(maple_GetAddress(bus, port));
	dev->config = new MapleConfigMap(dev, player_num);
	dev->OnSetup();
	MapleDevices[bus][port] = dev;
}

void mcfg_CreateNAOMIJamma()
{
	mcfg_Create(MDT_NaomiJamma, 0, 5);
//	mcfg_Create(MDT_Keyboard, 2, 5);
}

void mcfg_CreateAtomisWaveControllers()
{
	// Looks like two controllers needs to be on bus 0 and 1 for digital inputs
	// Then other devices on port 2 and 3 for analog axes, light guns, ...
	mcfg_Create(MDT_SegaController, 0, 5);
	mcfg_Create(MDT_SegaController, 1, 5);
//	mcfg_Create(MDT_SegaController, 2, 5, 0);
//	mcfg_Create(MDT_SegaController, 3, 5, 1);
//	mcfg_Create(MDT_LightGun, 2, 5, 0);
//	mcfg_Create(MDT_LightGun, 3, 5, 1);
//	mcfg_Create(MDT_Mouse, 2, 5, 0);
	// Guilty Gear Isuka (4P)		needs 4 std controllers
	// Faster Than Speed			needs 1 std controller on port 0 (digital inputs) and one on port 2 (analog axes)
	// Maximum Speed				same
	// Clay Challenge				needs 2 std controllers on port 0 & 1 (digital in) and light guns on port 2 & 3
	// Sports Shooting				same
	// Sega Bass Fishing Challenge  needs a mouse (track-ball) on port 3
}

void mcfg_CreateDevices()
{
	for (int bus = 0; bus < MAPLE_PORTS; ++bus)
	{
		switch ((MapleDeviceType)settings.input.maple_devices[bus])
		{
		case MDT_SegaController:
			mcfg_Create(MDT_SegaController, bus, 5);
			if (settings.input.maple_expansion_devices[bus][0] != MDT_None)
				mcfg_Create((MapleDeviceType)settings.input.maple_expansion_devices[bus][0], bus, 0);
			if (settings.input.maple_expansion_devices[bus][1] != MDT_None)
				mcfg_Create((MapleDeviceType)settings.input.maple_expansion_devices[bus][1], bus, 1);
			break;

		case MDT_Keyboard:
			mcfg_Create(MDT_Keyboard, bus, 5);
			break;

		case MDT_Mouse:
			mcfg_Create(MDT_Mouse, bus, 5);
			break;

		case MDT_LightGun:
			mcfg_Create(MDT_LightGun, bus, 5);
			if (settings.input.maple_expansion_devices[bus][0] != MDT_None)
				mcfg_Create((MapleDeviceType)settings.input.maple_expansion_devices[bus][0], bus, 0);
			break;
		}
	}
}

void mcfg_DestroyDevices()
{
	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j=0;j<=5;j++)
		{
			if (MapleDevices[i][j] != NULL)
			{
				delete MapleDevices[i][j];
				MapleDevices[i][j] = NULL;
			}
		}
}

void mcfg_SerializeDevices(void **data, unsigned int *total_size)
{
	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j < 6; j++)
		{
			u8 **p = (u8 **)data;
			if (MapleDevices[i][j] != NULL)
			{
				if (*p != NULL)
				{
					**p = MapleDevices[i][j]->get_device_type();
					*p = *p + 1;
				}
				MapleDevices[i][j]->maple_serialize(data, total_size);
			}
			else if (*p != NULL)
			{
				**p = MDT_None;
				*p = *p + 1;
			}
			*total_size = *total_size + 1;
		}
}

void mcfg_UnserializeDevices(void **data, unsigned int *total_size)
{
	mcfg_DestroyDevices();

	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j < 6; j++)
		{
			u8 **p = (u8 **)data;
			MapleDeviceType device_type = (MapleDeviceType)**p;
			*p = *p + 1;
			*total_size = *total_size + 1;
			if (device_type != MDT_None)
			{
				mcfg_Create(device_type, i, j);
				MapleDevices[i][j]->maple_unserialize(data, total_size);
			}
		}
}
