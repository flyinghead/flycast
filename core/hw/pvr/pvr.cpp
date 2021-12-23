/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "pvr.h"
#include "spg.h"
#include "pvr_regs.h"
#include "Renderer_if.h"
#include "ta_ctx.h"
#include "rend/TexCache.h"
#include "serialize.h"
#include "pvr_mem.h"

// ta.cpp
extern u8 ta_fsm[2049];	//[2048] stores the current state
extern u32 ta_fsm_cl;
// pvr_regs.cpp
extern bool fog_needs_update;
extern bool pal_needs_update;

namespace pvr
{

void reset(bool hard)
{
	KillTex = true;
	Regs_Reset(hard);
	spg_Reset(hard);
	if (hard)
		rend_reset();
	tactx_Term();
}

void init()
{
	spg_Init();
}

void term()
{
	tactx_Term();
	spg_Term();
}

void serialize(Serializer& ser)
{
	YUV_serialize(ser);

	ser << pvr_regs;

	spg_Serialize(ser);
	rend_serialize(ser);

	ser << ta_fsm[2048];
	ser << ta_fsm_cl;

	SerializeTAContext(ser);

	if (!ser.rollback())
		ser.serialize(vram.data, vram.size);
}

void deserialize(Deserializer& deser)
{
	if (deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip<u32>();		// FrameCount
		deser.skip<bool>();		// pend_rend
	}

	YUV_deserialize(deser);

	if (deser.version() >= Deserializer::V5_LIBRETRO && deser.version() < Deserializer::V9_LIBRETRO)
		deser.skip<bool>();	// fog_needs_update
	deser >> pvr_regs;
	fog_needs_update = true;

	spg_Deserialize(deser);

	if (deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip(4 * 256);	// ta_type_lut
		deser.skip(2048);		// ta_fsm
	}
	rend_deserialize(deser);

	deser >> ta_fsm[2048];
	deser >> ta_fsm_cl;
	if (deser.version() >= Deserializer::V5_LIBRETRO && deser.version() < Deserializer::V9_LIBRETRO)
	{
		deser.skip<bool>();		// pal_needs_update
		deser.skip(4 * 4);		// _pal_rev_256
		deser.skip(4 * 64);		// _pal_rev_16
		deser.skip(4 * 4);		// pal_rev_256
		deser.skip(4 * 64);		// pal_rev_16
		deser.skip(4 * 65536 * 3); // decoded_colors
		deser.skip(4);			// tileclip_val
		deser.skip(65536);		// f32_su8_tbl
		deser.skip(4);			// FaceBaseColor
		deser.skip(4);			// FaceOffsColor
		deser.skip(4);			// SFaceBaseColor
		deser.skip(4);			// SFaceOffsColor

		deser.skip(4);			// palette_index
		deser.skip<bool>();		// KillTex
		deser.skip(4 * 1024); 	// palette16_ram
		deser.skip(4 * 1024); 	// palette32_ram
		deser.skip(4 * 1024 * 8 * 2); // detwiddle
	}
	else if (deser.version() <= Deserializer::V4)
	{
		deser.skip(4);
		deser.skip(65536);
		deser.skip(4);
		deser.skip(4);
		deser.skip(4);
		deser.skip(4);
	}
	if (deser.version() >= Deserializer::V11 || (deser.version() >= Deserializer::V10_LIBRETRO && deser.version() <= Deserializer::VLAST_LIBRETRO))
		DeserializeTAContext(deser);

	if (!deser.rollback())
		deser.deserialize(vram.data, vram.size);
	pal_needs_update = true;
}

}
