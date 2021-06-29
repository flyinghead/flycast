#pragma once
#include "types.h"
#include "ta_ctx.h"
#include "hw/sh4/sh4_if.h"

struct TA_context;

void ta_vtx_ListCont();
void ta_vtx_ListInit();
void ta_vtx_SoftReset();

void DYNACALL ta_vtx_data32(const SQBuffer *data);
void ta_vtx_data(const SQBuffer *data, u32 size);

bool ta_parse_vdrc(TA_context* ctx);

class TaTypeLut
{
public:
	static const TaTypeLut& instance() {
		static TaTypeLut _instance;
		return _instance;
	}
	u32 table[256];

private:
	TaTypeLut();
};
