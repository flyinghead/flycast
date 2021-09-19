#include "maple_cfg.h"
#include "maple_helper.h"
#include "maple_if.h"
#include "hw/naomi/naomi_cart.h"
#include "input/gamepad_device.h"
#include "cfg/option.h"

MapleInputState mapleInputState[4];

static u8 GetBtFromSgn(s8 val)
{
	return val+128;
}

u32 awave_button_mapping[32] = {
		AWAVE_BTN2_KEY,		// DC_BTN_C
		AWAVE_BTN1_KEY,		// DC_BTN_B
		AWAVE_BTN0_KEY,		// DC_BTN_A
		AWAVE_START_KEY,	// DC_BTN_START
		AWAVE_UP_KEY,		// DC_DPAD_UP
		AWAVE_DOWN_KEY,		// DC_DPAD_DOWN
		AWAVE_LEFT_KEY,		// DC_DPAD_LEFT
		AWAVE_RIGHT_KEY,	// DC_DPAD_RIGHT
		AWAVE_BTN4_KEY,		// DC_BTN_Z (duplicated)
		AWAVE_BTN4_KEY,		// DC_BTN_Y
		AWAVE_BTN3_KEY,		// DC_BTN_X
		AWAVE_COIN_KEY,		// DC_BTN_D
		AWAVE_SERVICE_KEY,	// DC_DPAD2_UP
		AWAVE_TEST_KEY,		// DC_DPAD2_DOWN
		0,					// DC_DPAD2_LEFT
		0,					// DC_DPAD2_RIGHT
};

u32 awavelg_button_mapping[32] = {
		AWAVE_BTN1_KEY,		// DC_BTN_C
		AWAVE_BTN0_KEY,		// DC_BTN_B
		AWAVE_TRIGGER_KEY,	// DC_BTN_A
		AWAVE_START_KEY,	// DC_BTN_START
		AWAVE_UP_KEY,		// DC_DPAD_UP
		AWAVE_DOWN_KEY,		// DC_DPAD_DOWN
		AWAVE_LEFT_KEY,		// DC_DPAD_LEFT
		AWAVE_RIGHT_KEY,	// DC_DPAD_RIGHT
		AWAVE_BTN4_KEY,		// DC_BTN_Z
		AWAVE_BTN3_KEY,		// DC_BTN_Y
		AWAVE_BTN2_KEY,		// DC_BTN_X
		AWAVE_COIN_KEY,		// DC_BTN_D
		AWAVE_SERVICE_KEY,	// DC_DPAD2_UP
		AWAVE_TEST_KEY,		// DC_DPAD2_DOWN
		0,					// DC_DPAD2_LEFT
		0,					// DC_DPAD2_RIGHT

		AWAVE_BTN0_KEY		// DC_BTN_RELOAD (not needed for AW, mapped to BTN0 = pump)
};

inline u32 MapleConfigMap::playerNum()
{
	return dev->player_num;
}

void MapleConfigMap::SetVibration(float power, float inclination, u32 duration_ms)
{
	UpdateVibration(playerNum(), power, inclination, duration_ms);
}

void MapleConfigMap::GetInput(PlainJoystickState* pjs)
{
	const MapleInputState& inputState = mapleInputState[playerNum()];

	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		pjs->kcode = inputState.kcode;
		pjs->joy[PJAI_X1] = GetBtFromSgn(inputState.fullAxes[PJAI_X1]);
		pjs->joy[PJAI_Y1] = GetBtFromSgn(inputState.fullAxes[PJAI_Y1]);
		pjs->trigger[PJTI_R] = inputState.halfAxes[PJTI_R];
		pjs->trigger[PJTI_L] = inputState.halfAxes[PJTI_L];
	}
	else if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
#ifdef LIBRETRO
		pjs->kcode = inputState.kcode;
#else
		const u32* mapping = settings.input.JammaSetup == JVS::LightGun ? awavelg_button_mapping : awave_button_mapping;
		pjs->kcode = ~0;
		for (u32 i = 0; i < ARRAY_SIZE(awave_button_mapping); i++)
		{
			if ((inputState.kcode & (1 << i)) == 0)
				pjs->kcode &= ~mapping[i];
		}
#endif
		if (NaomiGameInputs != NULL)
		{
			for (u32 axis = 0; axis < PJAI_Count; axis++)
			{
				if (NaomiGameInputs->axes[axis].name != NULL)
				{
					if (NaomiGameInputs->axes[axis].type == Full)
					{
						switch (NaomiGameInputs->axes[axis].axis)
						{
						case 0:
							pjs->joy[axis] = GetBtFromSgn(inputState.fullAxes[PJAI_X1]);
							break;
						case 1:
							pjs->joy[axis] = GetBtFromSgn(inputState.fullAxes[PJAI_Y1]);
							break;
						case 2:
							pjs->joy[axis] = GetBtFromSgn(inputState.fullAxes[PJAI_X2]);
							break;
						case 3:
							pjs->joy[axis] = GetBtFromSgn(inputState.fullAxes[PJAI_Y2]);
							break;
						default:
							pjs->joy[axis] = 0x80;
							break;
						}
					}
					else
					{
						switch (NaomiGameInputs->axes[axis].axis)
						{
						case 4:
							pjs->joy[axis] = inputState.halfAxes[PJTI_R];
							break;
						case 5:
							pjs->joy[axis] = inputState.halfAxes[PJTI_L];
							break;
						default:
							pjs->joy[axis] = 0;
							break;
						}
					}
					if (NaomiGameInputs->axes[axis].inverted)
						pjs->joy[axis] = pjs->joy[axis] == 0 ? 0xff : 0x100 - pjs->joy[axis];
				}
				else
				{
					pjs->joy[axis] = 0x80;
				}
			}
		}
		else
		{
			pjs->joy[PJAI_X1] = GetBtFromSgn(inputState.fullAxes[PJAI_X1]);
			pjs->joy[PJAI_Y1] = GetBtFromSgn(inputState.fullAxes[PJAI_Y1]);
			pjs->joy[PJAI_X2] = inputState.halfAxes[PJTI_R];
			pjs->joy[PJAI_Y2] = inputState.halfAxes[PJTI_L];
		}
	}
}
void MapleConfigMap::SetImage(u8 *img)
{
	push_vmu_screen(dev->bus_id, dev->bus_port, img);
}

void MapleConfigMap::GetAbsCoordinates(int& x, int& y)
{
	x = mo_x_abs[playerNum()];
	y = mo_y_abs[playerNum()];
}

void MapleConfigMap::GetMouseInput(u8& buttons, int& x, int& y, int& wheel)
{
	u32 playerNum = this->playerNum();
	buttons = mo_buttons[playerNum] & 0xff;
	x = (int)std::round(mo_x_delta[playerNum]);
	y = (int)std::round(mo_y_delta[playerNum]);
	wheel = (int)std::round(mo_wheel_delta[playerNum]);
	mo_x_delta[playerNum] = 0;
	mo_y_delta[playerNum] = 0;
	mo_wheel_delta[playerNum] = 0;
}

bool maple_atomiswave_coin_chute(int slot)
{
#ifdef LIBRETRO
	return mapleInputState[slot].kcode & AWAVE_COIN_KEY;
#else
	for (int i = 0; i < 16; i++)
	{
		if ((mapleInputState[slot].kcode & (1 << i)) == 0 && awave_button_mapping[i] == AWAVE_COIN_KEY)
			return true;
	}
	return false;
#endif
}

static void mcfg_Create(MapleDeviceType type, u32 bus, u32 port, s32 player_num = -1)
{
	delete MapleDevices[bus][port];
	maple_device* dev = maple_Create(type);
	dev->Setup(maple_GetAddress(bus, port), player_num);
	dev->config = new MapleConfigMap(dev);
	dev->OnSetup();
	MapleDevices[bus][port] = dev;
}

void mcfg_CreateNAOMIJamma()
{
	mcfg_DestroyDevices();
	mcfg_Create(MDT_NaomiJamma, 0, 5);
	if (settings.input.JammaSetup == JVS::Keyboard)
		mcfg_Create(MDT_Keyboard, 1, 5, 0);
	else
	{
		// Connect VMU B1
		mcfg_Create(MDT_SegaController, 1, 5);
		mcfg_Create(MDT_SegaVMU, 1, 0);
		// Connect VMU C1
		mcfg_Create(MDT_SegaController, 2, 5);
		mcfg_Create(MDT_SegaVMU, 2, 0);
	}
}

void mcfg_CreateAtomisWaveControllers()
{
	mcfg_DestroyDevices();
	// Looks like two controllers needs to be on bus 0 and 1 for digital inputs
	// Then other devices on port 2 and 3 for analog axes, light guns, ...
	mcfg_Create(MDT_SegaController, 0, 5);
	mcfg_Create(MDT_SegaController, 1, 5);
	if (NaomiGameInputs != NULL && NaomiGameInputs->axes[0].name != NULL)
	{
		// Game needs analog axes
		mcfg_Create(MDT_SegaController, 2, 5, 0);
		mcfg_Create(MDT_SegaController, 3, 5, 1);
		// Faster Than Speed			needs 1 std controller on port 0 (digital inputs) and one on port 2 (analog axes)
		// Maximum Speed				same
	}
	else if (settings.input.JammaSetup == JVS::FourPlayers)
	{
		// 4 players
		mcfg_Create(MDT_SegaController, 2, 5);
		mcfg_Create(MDT_SegaController, 3, 5);
	}
	else if (settings.input.JammaSetup == JVS::LightGun)
	{
		// Clay Challenge				needs 2 std controllers on port 0 & 1 (digital in) and light guns on port 2 & 3
		// Sports Shooting				same
		mcfg_Create(MDT_LightGun, 2, 5, 0);
		mcfg_Create(MDT_LightGun, 3, 5, 1);
	}
	else if (settings.input.JammaSetup == JVS::SegaMarineFishing || settings.input.JammaSetup == JVS::RotaryEncoders)
	{
		// Sega Bass Fishing Challenge  needs a mouse (track-ball) on port 2
		// Waiwai drive needs two track-balls
		mcfg_Create(MDT_Mouse, 2, 5, 0);
		mcfg_Create(MDT_Mouse, 3, 5, 1);
	}
}

void mcfg_CreateDevices()
{
	for (int bus = 0; bus < MAPLE_PORTS; ++bus)
	{
		switch (config::MapleMainDevices[bus])
		{
		case MDT_SegaController:
			mcfg_Create(MDT_SegaController, bus, 5);
			if (config::MapleExpansionDevices[bus][0] != MDT_None)
				mcfg_Create(config::MapleExpansionDevices[bus][0], bus, 0);
			if (config::MapleExpansionDevices[bus][1] != MDT_None)
				mcfg_Create(config::MapleExpansionDevices[bus][1], bus, 1);
			break;

		case MDT_Keyboard:
			mcfg_Create(MDT_Keyboard, bus, 5);
			break;

		case MDT_Mouse:
			mcfg_Create(MDT_Mouse, bus, 5);
			break;

		case MDT_LightGun:
			mcfg_Create(MDT_LightGun, bus, 5);
			if (config::MapleExpansionDevices[bus][0] != MDT_None)
				mcfg_Create(config::MapleExpansionDevices[bus][0], bus, 0);
			break;

		case MDT_TwinStick:
			mcfg_Create(MDT_TwinStick, bus, 5);
			if (config::MapleExpansionDevices[bus][0] != MDT_None)
				mcfg_Create(config::MapleExpansionDevices[bus][0], bus, 0);
			break;

		case MDT_AsciiStick:
			mcfg_Create(MDT_AsciiStick, bus, 5);
			if (config::MapleExpansionDevices[bus][0] != MDT_None)
				mcfg_Create(config::MapleExpansionDevices[bus][0], bus, 0);
			break;

		case MDT_None:
			break;

		default:
			WARN_LOG(MAPLE, "Invalid device type %d for port %d", (MapleDeviceType)config::MapleMainDevices[bus], bus);
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
			u8 deviceType = MDT_None;
			maple_device* device = MapleDevices[i][j];
			if (device != nullptr)
				deviceType =  device->get_device_type();
			REICAST_S(deviceType);
			if (device != nullptr)
				device->serialize(data, total_size);
		}
}

void mcfg_UnserializeDevices(void **data, unsigned int *total_size, serialize_version_enum version)
{
	mcfg_DestroyDevices();

	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j < 6; j++)
		{
			u8 deviceType;
			REICAST_US(deviceType);
			if (deviceType != MDT_None)
			{
				mcfg_Create((MapleDeviceType)deviceType, i, j);
				MapleDevices[i][j]->unserialize(data, total_size, version);
			}
		}
}
