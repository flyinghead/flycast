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
void UpdateVibration(u32 port, u32 value);

extern u16 kcode[4];
extern u32 vks[4];
extern s8 joyx[4],joyy[4];
extern u8 rt[4],lt[4];

u8 GetBtFromSgn(s8 val)
{
	return val+128;
}

struct MapleConfigMap : IMapleConfigMap
{
	maple_device* dev;

	MapleConfigMap(maple_device* dev)
	{
		this->dev=dev;
	}

	void SetVibration(u32 value)
	{
		UpdateVibration(dev->bus_id, value);
	}

	void GetInput(PlainJoystickState* pjs)
	{
		UpdateInputState(dev->bus_id);

		pjs->kcode=kcode[dev->bus_id] | 0xF901;
		pjs->joy[PJAI_X1]=GetBtFromSgn(joyx[dev->bus_id]);
		pjs->joy[PJAI_Y1]=GetBtFromSgn(joyy[dev->bus_id]);
		pjs->trigger[PJTI_R]=rt[dev->bus_id];
		pjs->trigger[PJTI_L]=lt[dev->bus_id];
	}
	void SetImage(void* img)
	{
		//?
	}
};

void mcfg_Create(MapleDeviceType type, u32 bus, u32 port)
{
	if (MapleDevices[bus][port] != NULL)
		delete MapleDevices[bus][port];
	maple_device* dev = maple_Create(type);
	dev->Setup(maple_GetAddress(bus, port));
	dev->config = new MapleConfigMap(dev);
	dev->OnSetup();
	MapleDevices[bus][port] = dev;
}

void mcfg_CreateNAOMIJamma()
{
	mcfg_Create(MDT_NaomiJamma, 0, 5);
}


void mcfg_CreateController(u32 bus, MapleDeviceType maple_type1, MapleDeviceType maple_type2)
{
	mcfg_Create(MDT_SegaController, bus, 5);

	if (maple_type1 != MDT_None)
		mcfg_Create(maple_type1, bus, 0);

	if (maple_type2 != MDT_None)
		mcfg_Create(maple_type2, bus, 1);
}

void mcfg_CreateDevicesFromConfig()
{
	// Create the configure controller count
	int numberOfControl = cfgLoadInt("players", "nb", 1);

	if (numberOfControl <= 0)
		numberOfControl = 1;
	if (numberOfControl > 4)
		numberOfControl = 4;

	for (int i = 0; i < numberOfControl; i++)
		// Default to two VMUs on each controller
		mcfg_CreateController(i, MDT_SegaVMU, MDT_SegaVMU);

	if (settings.input.DCKeyboard && numberOfControl < 4)
		mcfg_Create(MDT_Keyboard, numberOfControl++, 5);

	if (settings.input.DCMouse != 0 && numberOfControl < 4)
		mcfg_Create(MDT_Mouse, numberOfControl++, 5);
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
