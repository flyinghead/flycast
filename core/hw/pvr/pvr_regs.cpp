#include "pvr_regs.h"
#include "pvr_mem.h"
#include "Renderer_if.h"
#include "ta.h"
#include "spg.h"

bool pal_needs_update=true;
bool fog_needs_update=true;

u8 pvr_regs[pvr_RegSize];

u32 pvr_ReadReg(u32 addr)
{
	return PvrReg(addr,u32);
}

void pvr_WriteReg(u32 paddr,u32 data)
{
	u32 addr = paddr & pvr_RegMask;

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
			ta_vtx_ListInit();
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
		ta_vtx_ListCont();
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
