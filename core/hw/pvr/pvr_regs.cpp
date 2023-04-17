#include "pvr_regs.h"
#include "pvr_mem.h"
#include "Renderer_if.h"
#include "ta.h"
#include "spg.h"
#include <map>

bool pal_needs_update=true;
bool fog_needs_update=true;

u8 pvr_regs[pvr_RegSize];

#define PVR_REG_NAME(r) { r##_addr, #r },
const std::map<u32, const char *> pvr_reg_names = {
		PVR_REG_NAME(ID)
		PVR_REG_NAME(REVISION)
		PVR_REG_NAME(SOFTRESET)
		PVR_REG_NAME(STARTRENDER)
		PVR_REG_NAME(TEST_SELECT)
		PVR_REG_NAME(PARAM_BASE)
		PVR_REG_NAME(REGION_BASE)
		PVR_REG_NAME(SPAN_SORT_CFG)
		PVR_REG_NAME(VO_BORDER_COL)
		PVR_REG_NAME(FB_R_CTRL)
		PVR_REG_NAME(FB_W_CTRL)
		PVR_REG_NAME(FB_W_LINESTRIDE)
		PVR_REG_NAME(FB_R_SOF1)
		PVR_REG_NAME(FB_R_SOF2)
		PVR_REG_NAME(FB_R_SIZE)
		PVR_REG_NAME(FB_W_SOF1)
		PVR_REG_NAME(FB_W_SOF2)
		PVR_REG_NAME(FB_X_CLIP)
		PVR_REG_NAME(FB_Y_CLIP)
		PVR_REG_NAME(FPU_SHAD_SCALE)
		PVR_REG_NAME(FPU_CULL_VAL)
		PVR_REG_NAME(FPU_PARAM_CFG)
		PVR_REG_NAME(HALF_OFFSET)
		PVR_REG_NAME(FPU_PERP_VAL)
		PVR_REG_NAME(ISP_BACKGND_D)
		PVR_REG_NAME(ISP_BACKGND_T)
		PVR_REG_NAME(ISP_FEED_CFG)
		PVR_REG_NAME(SDRAM_REFRESH)
		PVR_REG_NAME(SDRAM_ARB_CFG)
		PVR_REG_NAME(SDRAM_CFG)
		PVR_REG_NAME(FOG_COL_RAM)
		PVR_REG_NAME(FOG_COL_VERT)
		PVR_REG_NAME(FOG_DENSITY)
		PVR_REG_NAME(FOG_CLAMP_MAX)
		PVR_REG_NAME(FOG_CLAMP_MIN)
		PVR_REG_NAME(SPG_TRIGGER_POS)
		PVR_REG_NAME(SPG_HBLANK_INT)
		PVR_REG_NAME(SPG_VBLANK_INT)
		PVR_REG_NAME(SPG_CONTROL)
		PVR_REG_NAME(SPG_HBLANK)
		PVR_REG_NAME(SPG_LOAD)
		PVR_REG_NAME(SPG_VBLANK)
		PVR_REG_NAME(SPG_WIDTH)
		PVR_REG_NAME(TEXT_CONTROL)
		PVR_REG_NAME(VO_CONTROL)
		PVR_REG_NAME(VO_STARTX)
		PVR_REG_NAME(VO_STARTY)
		PVR_REG_NAME(SCALER_CTL)
		PVR_REG_NAME(PAL_RAM_CTRL)
		PVR_REG_NAME(SPG_STATUS)
		PVR_REG_NAME(FB_BURSTCTRL)
		PVR_REG_NAME(FB_C_SOF)
		PVR_REG_NAME(Y_COEFF)
		PVR_REG_NAME(PT_ALPHA_REF)
		PVR_REG_NAME(TA_OL_BASE)
		PVR_REG_NAME(TA_ISP_BASE)
		PVR_REG_NAME(TA_OL_LIMIT)
		PVR_REG_NAME(TA_ISP_LIMIT)
		PVR_REG_NAME(TA_NEXT_OPB)
		PVR_REG_NAME(TA_ITP_CURRENT)
		PVR_REG_NAME(TA_GLOB_TILE_CLIP)
		PVR_REG_NAME(TA_ALLOC_CTRL)
		PVR_REG_NAME(TA_LIST_INIT)
		PVR_REG_NAME(TA_YUV_TEX_BASE)
		PVR_REG_NAME(TA_YUV_TEX_CTRL)
		PVR_REG_NAME(TA_YUV_TEX_CNT)
		PVR_REG_NAME(TA_LIST_CONT)
		PVR_REG_NAME(TA_NEXT_OPB_INIT)
		PVR_REG_NAME(SIGNATURE1)
		PVR_REG_NAME(SIGNATURE2)
};
#undef PVR_REG_NAME

static const char *regName(u32 paddr)
{
	u32 addr = paddr & pvr_RegMask;
	static char regName[32];
	auto it = pvr_reg_names.find(addr);
	if (it == pvr_reg_names.end())
	{
		if (addr >= FOG_TABLE_START_addr && addr <= FOG_TABLE_END_addr)
			sprintf(regName, "FOG_TABLE[%x]", addr - FOG_TABLE_START_addr);
		else if (addr >= TA_OL_POINTERS_START_addr && addr <= TA_OL_POINTERS_END_addr)
			sprintf(regName, "TA_OL_POINTERS[%x]", addr - TA_OL_POINTERS_START_addr);
		else if (addr >= PALETTE_RAM_START_addr && addr <= PALETTE_RAM_END_addr)
			sprintf(regName, "PALETTE[%x]", addr - PALETTE_RAM_START_addr);
		else
			sprintf(regName, "?%08x", paddr);
		return regName;
	}
	else
		return it->second;
}

u32 pvr_ReadReg(u32 addr)
{
	if ((addr & pvr_RegMask) != SPG_STATUS_addr)
		DEBUG_LOG(PVR, "read %s.%c == %x", regName(addr),
				((addr >> 26) & 7) == 2 ? 'b' : (addr & 0x2000000) ? '1' : '0',
						PvrReg(addr, u32));
	return PvrReg(addr,u32);
}

void pvr_WriteReg(u32 paddr,u32 data)
{
	u32 addr = paddr & pvr_RegMask;
	DEBUG_LOG(PVR, "write %s.%c = %x", regName(paddr),
			((paddr >> 26) & 7) == 2 ? 'b' : (paddr & 0x2000000) ? '1' : '0',
					data);

	switch (addr)
	{
	case ID_addr:
	case REVISION_addr:
	case TA_YUV_TEX_CNT_addr:
		return; // read only

	case STARTRENDER_addr:
		//start render
		rend_start_render();
		return;

	case TA_LIST_INIT_addr:
		if (data >> 31)
		{
			ta_vtx_ListInit(false);
			TA_NEXT_OPB = TA_NEXT_OPB_INIT;
			TA_ITP_CURRENT = TA_ISP_BASE;
		}
		return;

	case SOFTRESET_addr:
		if (data & 1)
			ta_vtx_SoftReset();
		return;

	case TA_LIST_CONT_addr:
		//a write of anything works ?
		ta_vtx_ListInit(true);
		break;
	
	case SPG_CONTROL_addr:
	case SPG_LOAD_addr:
		if (PvrReg(addr, u32) != data)
		{
			PvrReg(addr, u32) = data;
			CalculateSync();
		}
		return;

	case FB_R_CTRL_addr:
		{
			bool vclk_div_changed = (PvrReg(addr, u32) ^ data) & (1 << 23);
			PvrReg(addr, u32) = data;
			if (vclk_div_changed)
				CalculateSync();
		}
		return;

	case FB_R_SIZE_addr:
		if (PvrReg(addr, u32) != data)
		{
			PvrReg(addr, u32) = data;
			fb_dirty = false;
			check_framebuffer_write();
		}
		return;

	case TA_YUV_TEX_BASE_addr:
		PvrReg(addr, u32) = data & 0x00FFFFF8;
		YUV_init();
		return;

	case TA_YUV_TEX_CTRL_addr:
		PvrReg(addr, u32) = data;
		YUV_init();
		return;

	case FB_R_SOF1_addr:
	case FB_R_SOF2_addr:
		data &= 0x00fffffc;
		rend_swap_frame(data);
		break;

	case FB_W_SOF1_addr:
		data &= 0x01fffffc;
		rend_set_fb_write_addr(data);
		break;

	case FB_W_SOF2_addr:
		data &= 0x01fffffc;
		break;

	case SPG_HBLANK_INT_addr:
		data &= 0x03FF33FF;
		if (data != SPG_HBLANK_INT.full) {
			SPG_HBLANK_INT.full = data;
			rescheduleSPG();
		}
		return;

	case PAL_RAM_CTRL_addr:
		pal_needs_update = pal_needs_update || ((data ^ PAL_RAM_CTRL) & 3) != 0;
		break;

	default:
		if (addr >= PALETTE_RAM_START_addr && PvrReg(addr,u32) != data)
			pal_needs_update = true;
		else if (addr >= FOG_TABLE_START_addr && addr <= FOG_TABLE_END_addr && PvrReg(addr,u32) != data)
			fog_needs_update = true;
		break;
	}
	PvrReg(addr, u32) = data;
}

void Regs_Reset(bool hard)
{
	if (hard)
		memset(&pvr_regs[0], 0, sizeof(pvr_regs));
	ID_Reg              = 0x17FD11DB;
	REVISION            = 0x00000011;
	SOFTRESET           = 0x00000007;
	SPG_HBLANK_INT.full = 0x031D0000;
	SPG_VBLANK_INT.full = 0x00150104;
	FPU_PARAM_CFG       = 0x0007DF77;
	HALF_OFFSET         = 0x00000007;
	ISP_FEED_CFG        = 0x00402000;
	SDRAM_REFRESH       = 0x00000020;
	SDRAM_ARB_CFG       = 0x0000001F;
	SDRAM_CFG           = 0x15F28997;
	SPG_HBLANK.full     = 0x007E0345;
	SPG_LOAD.full       = 0x01060359;
	SPG_VBLANK.full     = 0x01500104;
	SPG_WIDTH.full      = 0x07F1933F;
	VO_CONTROL.full     = 0x00000108;
	VO_STARTX.full      = 0x0000009D;
	VO_STARTY.full      = 0x00150015;
	SCALER_CTL.full     = 0x00000400;
	FB_BURSTCTRL        = 0x00090639;
	PT_ALPHA_REF        = 0x000000FF;
}
