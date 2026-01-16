#include "maple_cfg.h"
#include "maple_helper.h"
#include "maple_if.h"
#include "hw/naomi/naomi_cart.h"
#include "hw/naomi/card_reader.h"
#include "cfg/option.h"
#include "stdclass.h"
#include "serialize.h"
#include "input/maplelink.h"

MapleInputState mapleInputState[4];
extern bool maple_ddt_pending_reset;
extern std::vector<std::pair<u32, std::vector<u32>>> mapleDmaOut;
extern bool SDCKBOccupied;

void (*MapleConfigMap::UpdateVibration)(u32 port, float power, float inclination, u32 duration_ms);

static u8 GetBtFromSgn(s16 val)
{
	return (val + 32768) >> 8;
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

	if (settings.platform.isConsole())
	{
		pjs->kcode = inputState.kcode;
		pjs->joy[PJAI_X1] = GetBtFromSgn(inputState.fullAxes[PJAI_X1]);
		pjs->joy[PJAI_Y1] = GetBtFromSgn(inputState.fullAxes[PJAI_Y1]);
		pjs->joy[PJAI_X2] = GetBtFromSgn(inputState.fullAxes[PJAI_X2]);
		pjs->joy[PJAI_Y2] = GetBtFromSgn(inputState.fullAxes[PJAI_Y2]);
		pjs->joy[PJAI_X3] = GetBtFromSgn(inputState.fullAxes[PJAI_X3]);
		pjs->joy[PJAI_Y3] = GetBtFromSgn(inputState.fullAxes[PJAI_Y3]);
		pjs->trigger[PJTI_R] = inputState.halfAxes[PJTI_R] >> 8;
		pjs->trigger[PJTI_L] = inputState.halfAxes[PJTI_L] >> 8;
		pjs->trigger[PJTI_L2] = inputState.halfAxes[PJTI_L2] >> 8;
		pjs->trigger[PJTI_R2] = inputState.halfAxes[PJTI_R2] >> 8;
	}
	else if (settings.platform.isAtomiswave())
	{
#ifdef LIBRETRO
		pjs->kcode = inputState.kcode;
#else
		const u32* mapping = settings.input.lightgunGame ? awavelg_button_mapping : awave_button_mapping;
		pjs->kcode = ~0;
		for (u32 i = 0; i < std::size(awave_button_mapping); i++)
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
							pjs->joy[axis] = inputState.halfAxes[PJTI_R] >> 8;
							break;
						case 5:
							pjs->joy[axis] = inputState.halfAxes[PJTI_L] >> 8;
							break;
						case 6:
							pjs->joy[axis] = inputState.halfAxes[PJTI_R2] >> 8;
							break;
						case 7:
							pjs->joy[axis] = inputState.halfAxes[PJTI_L2] >> 8;
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
			pjs->joy[PJAI_X2] = inputState.halfAxes[PJTI_R] >> 8;
			pjs->joy[PJAI_Y2] = inputState.halfAxes[PJTI_L] >> 8;
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
	MapleInputState& inputState = mapleInputState[playerNum()];
	buttons = inputState.mouseButtons;
	x = inputState.relPos.x;
	y = inputState.relPos.y * (invertMouseY ? -1 : 1);
	wheel = inputState.relPos.wheel;
	inputState.relPos.x = 0;
	inputState.relPos.y = 0;
	inputState.relPos.wheel = 0;
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
	for (size_t i = 0; i < std::size(awave_button_mapping); i++)
	{
		if ((mapleInputState[slot].kcode & (1 << i)) == 0 && awave_button_mapping[i] == AWAVE_COIN_KEY)
			return true;
	}
	return false;
#endif
}

static void mcfg_Create(MapleDeviceType type, u32 bus, u32 port, s32 player_num = -1)
{
	MapleDevices[bus][port].reset();
	if (type == MDT_SegaVMU)
	{
		MapleLink::Ptr link = MapleLink::GetMapleLink(bus, port);
		if (link != nullptr && link->storageEnabled()) {
			createMapleLinkVmu(bus, port);
			return;
		}
	}
	std::shared_ptr<maple_device> dev = maple_Create(type);
	dev->Setup(bus, port, player_num);
}

static void createNaomiDevices()
{
	const std::string& gameId = settings.content.gameId;
	mcfg_Create(MDT_NaomiJamma, 0, 5);
	if (gameId == "THE TYPING OF THE DEAD"
			|| gameId == " LUPIN THE THIRD  -THE TYPING-"
			|| gameId == "------La Keyboardxyu------")
	{
		INFO_LOG(MAPLE, "Enabling keyboard for game %s", gameId.c_str());
		mcfg_Create(MDT_Keyboard, 1, 5, 0);
		mcfg_Create(MDT_Keyboard, 2, 5, 1);
		settings.input.keyboardGame = true;
	}
	else if (gameId.substr(0, 8) == "MKG TKOB"
			|| gameId == "VIRTUA FIGHTER 4 JAPAN"
			|| gameId == "VF4 EVOLUTION JAPAN"
			|| gameId == "VF4 FINAL TUNED JAPAN")
	{
		mcfg_Create(MDT_RFIDReaderWriter, 1, 5, 0);
		mcfg_Create(MDT_RFIDReaderWriter, 2, 5, 1);
		if (gameId.substr(0, 8) == "MKG TKOB") {
			insertRfidCard(0);
			insertRfidCard(1);
		}
	}
	else if (gameId == "THE KING OF ROUTE66")
	{
		mcfg_Create(MDT_SegaController, 1, 5);
		mcfg_Create(MDT_Microphone, 1, 0);
	}
	else if (settings.platform.isNaomi1())
	{
		// Connect VMU B1
		mcfg_Create(MDT_SegaController, 1, 5);
		mcfg_Create(MDT_SegaVMU, 1, 0);
		// Connect VMU C1
		mcfg_Create(MDT_SegaController, 2, 5);
		mcfg_Create(MDT_SegaVMU, 2, 0);
	}
	if (gameId == " DERBY OWNERS CLUB WE ---------"
			|| gameId == " DERBY OWNERS CLUB ------------"
			|| gameId == " DERBY OWNERS CLUB II-----------")
		card_reader::derbyInit();
}

static void createAtomiswaveDevices()
{
	const std::string& gameId = settings.content.gameId;
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
	else if (gameId == "GUILTY GEAR isuka"
			|| gameId == "Dirty Pigskin Football")
	{
		// 4 players
		INFO_LOG(MAPLE, "Enabling 4-player setup for game %s", gameId.c_str());
		mcfg_Create(MDT_SegaController, 2, 5);
		mcfg_Create(MDT_SegaController, 3, 5);
		settings.input.fourPlayerGames = true;
	}
	else if (gameId == "Sports Shooting USA"
			|| gameId == "SEGA CLAY CHALLENGE"
			|| gameId == "RANGER MISSION"
			|| gameId == "EXTREME HUNTING"
			|| gameId == "Fixed BOOT strapper")	// Extreme hunting 2
	{
		// needs 2 std controllers on port 0 & 1 (digital in) and light guns on port 2 & 3
		INFO_LOG(MAPLE, "Enabling lightgun setup for game %s", gameId.c_str());
		mcfg_Create(MDT_LightGun, 2, 5, 0);
		mcfg_Create(MDT_LightGun, 3, 5, 1);
		settings.input.lightgunGame = true;
	}
	else if (gameId == "BASS FISHING SIMULATOR VER.A" || gameId == "DRIVE")
	{
		// Sega Bass Fishing Challenge  needs a mouse (track-ball) on port 2
		// Waiwai drive needs two track-balls
		mcfg_Create(MDT_Mouse, 2, 5, 0);
		mcfg_Create(MDT_Mouse, 3, 5, 1);
		if (gameId == "DRIVE")
		{
			MapleDevices[2][5]->config->invertMouseY = true;
			MapleDevices[3][5]->config->invertMouseY = true;
		}
		settings.input.mouseGame = true;
	}
}

static void createDreamcastDevices()
{
	for (int bus = 0; bus < MAPLE_PORTS; ++bus)
	{
		switch (config::MapleMainDevices[bus])
		{
		case MDT_SegaController:
		case MDT_SegaControllerXL:
			mcfg_Create(config::MapleMainDevices[bus], bus, 5);
			if (config::MapleExpansionDevices[bus][0] != MDT_None)
				mcfg_Create(config::MapleExpansionDevices[bus][0], bus, 0);
			if (config::MapleExpansionDevices[bus][1] != MDT_None)
				mcfg_Create(config::MapleExpansionDevices[bus][1], bus, 1);
			break;

		case MDT_Keyboard:
		case MDT_Mouse:
		case MDT_MaracasController:
		case MDT_FishingController:
		case MDT_PopnMusicController:
		case MDT_DenshaDeGoController:
		case MDT_Dreameye:
			mcfg_Create(config::MapleMainDevices[bus], bus, 5);
			if (config::MapleMainDevices[bus] == MDT_FishingController)
				// integrated vibration pack
				mcfg_Create(MDT_PurupuruPack, bus, 4);
			break;

		case MDT_LightGun:
		case MDT_TwinStick:
		case MDT_AsciiStick:
		case MDT_RacingController:
			mcfg_Create(config::MapleMainDevices[bus], bus, 5);
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
			std::shared_ptr<const maple_device> device = MapleDevices[i][j];
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
	settings.input.lightgunGame = false;
	settings.input.keyboardGame = false;
	settings.input.mouseGame = false;
	settings.input.fourPlayerGames = false;
	switch (settings.platform.system)
	{
	case DC_PLATFORM_DREAMCAST:
		createDreamcastDevices();
		break;
	case DC_PLATFORM_NAOMI:
	case DC_PLATFORM_NAOMI2:
		createNaomiDevices();
		break;
	case DC_PLATFORM_ATOMISWAVE:
		createAtomiswaveDevices();
		break;
	case DC_PLATFORM_SYSTEMSP:
		if (settings.content.gameId == "INW PUPPY 2008 VER1.001")
			settings.input.lightgunGame = true;
		return;
	default:
		die("Unknown system");
		break;
	}
	vmuDigest();
}

// Don't destroy the JVS MIE if full is false
void mcfg_DestroyDevices(bool full)
{
	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j <= 5; j++)
		{
			if (MapleDevices[i][j] != nullptr
					&& (full || MapleDevices[i][j]->get_device_type() != MDT_NaomiJamma))
			{
				MapleDevices[i][j].reset();
			}
		}
}

void mcfg_SerializeDevices(Serializer& ser)
{
	ser << maple_ddt_pending_reset;
	ser << SDCKBOccupied;
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
			std::shared_ptr<maple_device> device = MapleDevices[i][j];
			if (device != nullptr)
				deviceType =  device->get_device_type();
			ser << deviceType;
			if (device != nullptr)
				device->serialize(ser);
		}
}

void mcfg_DeserializeDevices(Deserializer& deser)
{
	if (!deser.rollback())
		mcfg_DestroyDevices(false);
	u8 eeprom[sizeof(maple_naomi_jamma::eeprom)];
	if (deser.version() < Deserializer::V23)
	{
		deser >> eeprom;
		deser.skip(128);	// Unused eeprom space
		deser.skip<bool>(); // EEPROM_loaded
	}
	deser >> maple_ddt_pending_reset;
	if (deser.version() >= Deserializer::V47)
		deser >> SDCKBOccupied;
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
				if (!deser.rollback() && deviceType != MDT_NaomiJamma)
					mcfg_Create((MapleDeviceType)deviceType, i, j);
				MapleDevices[i][j]->deserialize(deser);
			}
		}
	if (deser.version() < Deserializer::V23 && EEPROM != nullptr)
		memcpy(EEPROM, eeprom, sizeof(eeprom));
}

std::shared_ptr<maple_naomi_jamma> getMieDevice()
{
	if (MapleDevices[0][5] == nullptr || MapleDevices[0][5]->get_device_type() != MDT_NaomiJamma)
		return nullptr;
	return std::static_pointer_cast<maple_naomi_jamma>(MapleDevices[0][5]);
}
