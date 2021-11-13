#include "maple_cfg.h"
#include "maple_helper.h"
#include "maple_if.h"
#include "hw/naomi/naomi_cart.h"
#include "cfg/option.h"
#include "stdclass.h"
#include "serialize.h"

MapleInputState mapleInputState[4];
extern bool maple_ddt_pending_reset;
extern std::vector<std::pair<u32, std::vector<u32>>> mapleDmaOut;

void (*MapleConfigMap::UpdateVibration)(u32 port, float power, float inclination, u32 duration_ms);

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
	const MapleInputState& inputState = mapleInputState[playerNum()];
	x = inputState.absPos.x;
	y = inputState.absPos.y;
}

void MapleConfigMap::GetMouseInput(u8& buttons, int& x, int& y, int& wheel)
{
	const MapleInputState& inputState = mapleInputState[playerNum()];
	buttons = inputState.mouseButtons;
	x = inputState.relPos.x;
	y = inputState.relPos.y * (invertMouseY ? -1 : 1);
	wheel = inputState.relPos.wheel;
}

void MapleConfigMap::GetKeyboardInput(u8& shift, u8 keys[6])
{
	const MapleInputState& inputState = mapleInputState[playerNum()];
	shift = inputState.keyboard.shift;
	memcpy(keys, inputState.keyboard.key, sizeof(inputState.keyboard.key));
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

static void createNaomiDevices()
{
	mcfg_DestroyDevices();
	mcfg_Create(MDT_NaomiJamma, 0, 5);
	if (settings.input.JammaSetup == JVS::Keyboard)
	{
		mcfg_Create(MDT_Keyboard, 1, 5, 0);
		mcfg_Create(MDT_Keyboard, 2, 5, 1);
	}
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

static void createAtomiswaveDevices()
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
		if (settings.content.gameId == "DRIVE")
		{
			MapleDevices[2][5]->config->invertMouseY = true;
			MapleDevices[3][5]->config->invertMouseY = true;
		}
	}
}

static void createDreamcastDevices()
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

static void vmuDigest()
{
	if (!config::GGPOEnable)
		return;
	MD5Sum md5;
	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j < 6; j++)
		{
			const maple_device* device = MapleDevices[i][j];
			if (device != nullptr)
			{
				size_t size;
				const void *data = device->getData(size);
				if (data != nullptr)
					md5.add(data, size);
			}
		}
	md5.getDigest(settings.network.md5.vmu);
}

void mcfg_CreateDevices()
{
	switch (settings.platform.system)
	{
	case DC_PLATFORM_DREAMCAST:
		createDreamcastDevices();
		break;
	case DC_PLATFORM_NAOMI:
		createNaomiDevices();
		break;
	case DC_PLATFORM_ATOMISWAVE:
		createAtomiswaveDevices();
		break;
	default:
		die("Unknown system");
		break;
	}
	vmuDigest();
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

void mcfg_SerializeDevices(Serializer& ser)
{
	ser << maple_ddt_pending_reset;
	ser << (u32)mapleDmaOut.size();
	for (const auto& pair : mapleDmaOut)
	{
		ser << pair.first;	// u32 address
		ser << (u32)pair.second.size();
		ser.serialize(pair.second.data(), pair.second.size());
	}
	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j < 6; j++)
		{
			u8 deviceType = MDT_None;
			maple_device* device = MapleDevices[i][j];
			if (device != nullptr)
				deviceType =  device->get_device_type();
			ser << deviceType;
			if (device != nullptr)
				device->serialize(ser);
		}
}

void mcfg_DeserializeDevices(Deserializer& deser)
{
	mcfg_DestroyDevices();
	u8 eeprom[sizeof(maple_naomi_jamma::eeprom)];
	if (deser.version() < Deserializer::V23)
	{
		deser >> eeprom;
		deser.skip(128);	// Unused eeprom space
		deser.skip<bool>(); // EEPROM_loaded
	}
	deser >> maple_ddt_pending_reset;
	mapleDmaOut.clear();
	if (deser.version() >= Deserializer::V23)
	{
		u32 size;
		deser >> size;
		for (u32 i = 0; i < size; i++)
		{
			u32 address;
			deser >> address;
			u32 dataSize;
			deser >> dataSize;
			mapleDmaOut.emplace_back(address, std::vector<u32>(dataSize));
			deser.deserialize(mapleDmaOut.back().second.data(), dataSize);
		}
	}

	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j < 6; j++)
		{
			u8 deviceType;
			deser >> deviceType;
			if (deviceType != MDT_None)
			{
				mcfg_Create((MapleDeviceType)deviceType, i, j);
				MapleDevices[i][j]->deserialize(deser);
			}
		}
	if (deser.version() < Deserializer::V23 && EEPROM != nullptr)
		memcpy(EEPROM, eeprom, sizeof(eeprom));
}
