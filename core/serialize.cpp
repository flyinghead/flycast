// serialize.cpp : save states
#include "serialize.h"
#include "types.h"
#include "hw/aica/aica_if.h"
#include "hw/holly/sb.h"
#include "hw/flashrom/nvmem.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/maple/maple_cfg.h"
#include "hw/modem/modem.h"
#include "hw/pvr/pvr.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mmr.h"
#include "reios/gdrom_hle.h"
#include "hw/naomi/naomi.h"
#include "hw/naomi/naomi_cart.h"
#include "hw/bba/bba.h"
#include "cfg/option.h"

//./core/imgread/common.o
extern u32 NullDriveDiscType;
extern u8 q_subchannel[96];

void dc_serialize(Serializer& ser)
{
	aica::serialize(ser);

	sb_serialize(ser);

	nvmem::serialize(ser);

	gdrom::serialize(ser);

	mcfg_SerializeDevices(ser);

	pvr::serialize(ser);

	sh4::serialize(ser);

	ser << config::EmulateBBA.get();
	if (config::EmulateBBA)
		bba_Serialize(ser);
	ModemSerialize(ser);

	sh4::serialize2(ser);

	ser << NullDriveDiscType;
	ser << q_subchannel;

	naomi_Serialize(ser);

	ser << config::Broadcast.get();
	ser << config::Cable.get();
	ser << config::Region.get();

	naomi_cart_serialize(ser);
	gd_hle_state.Serialize(ser);

	DEBUG_LOG(SAVESTATE, "Saved %d bytes", (u32)ser.size());
}

static void dc_deserialize_libretro(Deserializer& deser)
{
	aica::deserialize(deser);

	sb_deserialize(deser);

	nvmem::deserialize(deser);

	gdrom::deserialize(deser);

	mcfg_DeserializeDevices(deser);

	pvr::deserialize(deser);

	sh4::deserialize(deser);

	if (deser.version() >= Deserializer::V13_LIBRETRO)
		deser.skip<bool>();		// settings.network.EmulateBBA
	config::EmulateBBA.override(false);

	ModemDeserialize(deser);

	sh4::deserialize2(deser);

	deser >> NullDriveDiscType;
	deser >> q_subchannel;

	deser.skip<u32>();	// FLASH_SIZE
	deser.skip<u32>();	// BBSRAM_SIZE
	deser.skip<u32>();	// BIOS_SIZE
	deser.skip<u32>();	// RAM_SIZE
	deser.skip<u32>();	// ARAM_SIZE
	deser.skip<u32>();	// VRAM_SIZE
	deser.skip<u32>();	// RAM_MASK
	deser.skip<u32>();	// ARAM_MASK
	deser.skip<u32>();	// VRAM_MASK

	naomi_Deserialize(deser);

	deser >> config::Broadcast.get();
	deser >> config::Cable.get();
	deser >> config::Region.get();

	naomi_cart_deserialize(deser);
	gd_hle_state.Deserialize(deser);

	DEBUG_LOG(SAVESTATE, "Loaded %d bytes (libretro compat)", (u32)deser.size());
}

void dc_deserialize(Deserializer& deser)
{
	if (deser.version() >= Deserializer::V9_LIBRETRO && deser.version() <= Deserializer::VLAST_LIBRETRO)
	{
		dc_deserialize_libretro(deser);
		sh4_sched_ffts();
		return;
	}
	DEBUG_LOG(SAVESTATE, "Loading state version %d", deser.version());

	aica::deserialize(deser);

	sb_deserialize(deser);

	nvmem::deserialize(deser);

	gdrom::deserialize(deser);

	mcfg_DeserializeDevices(deser);

	pvr::deserialize(deser);

	sh4::deserialize(deser);

	if (deser.version() >= Deserializer::V13)
		deser >> config::EmulateBBA.get();
	else
		config::EmulateBBA.override(false);
	if (config::EmulateBBA)
		bba_Deserialize(deser);
	ModemDeserialize(deser);

	sh4::deserialize2(deser);

	deser >> NullDriveDiscType;
	deser >> q_subchannel;

	naomi_Deserialize(deser);

	deser >> config::Broadcast.get();
	verify(config::Broadcast <= 4);
	deser >> config::Cable.get();
	verify(config::Cable <= 3);
	deser >> config::Region.get();
	verify(config::Region <= 3);

	naomi_cart_deserialize(deser);
	gd_hle_state.Deserialize(deser);
	sh4_sched_ffts();

	DEBUG_LOG(SAVESTATE, "Loaded %d bytes", (u32)deser.size());
}
