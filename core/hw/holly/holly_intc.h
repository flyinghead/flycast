#pragma once

enum HollyInterruptType
{
	holly_nrm = 0x0000,
	holly_ext = 0x0100,
	holly_err = 0x0200,
};

enum HollyInterruptID
{
		// asic9a /sh4 external holly normal [internal]
		holly_RENDER_DONE_vd = holly_nrm | 0,	//bit 0 = End of Render interrupt : Video
		holly_RENDER_DONE_isp = holly_nrm | 1,	//bit 1 = End of Render interrupt : ISP
		holly_RENDER_DONE = holly_nrm | 2,		//bit 2 = End of Render interrupt : TSP

		holly_SCANINT1 = holly_nrm | 3,			//bit 3 = V Blank-in interrupt
		holly_SCANINT2 = holly_nrm | 4,			//bit 4 = V Blank-out interrupt
		holly_HBLank = holly_nrm | 5,			//bit 5 = H Blank-in interrupt

		holly_YUV_DMA = holly_nrm | 6,			//bit 6 = End of Transferring interrupt : YUV
		holly_OPAQUE = holly_nrm | 7,			//bit 7 = End of Transferring interrupt : Opaque List
		holly_OPAQUEMOD = holly_nrm | 8,		//bit 8 = End of Transferring interrupt : Opaque Modifier Volume List

		holly_TRANS = holly_nrm | 9,			//bit 9 = End of Transferring interrupt : Translucent List
		holly_TRANSMOD = holly_nrm | 10,		//bit 10 = End of Transferring interrupt : Translucent Modifier Volume List
		holly_PVR_DMA = holly_nrm | 11,			//bit 11 = End of DMA interrupt : PVR-DMA
		holly_MAPLE_DMA = holly_nrm | 12,		//bit 12 = End of DMA interrupt : Maple-DMA

		holly_MAPLE_VBOI = holly_nrm | 13,		//bit 13 = Maple V blank over interrupt
		holly_GDROM_DMA = holly_nrm | 14,		//bit 14 = End of DMA interrupt : GD-DMA
		holly_SPU_DMA = holly_nrm | 15,			//bit 15 = End of DMA interrupt : AICA-DMA

		holly_EXT_DMA1 = holly_nrm | 16,		//bit 16 = End of DMA interrupt : Ext-DMA1(External 1)
		holly_EXT_DMA2 = holly_nrm | 17,		//bit 17 = End of DMA interrupt : Ext-DMA2(External 2)
		holly_DEV_DMA = holly_nrm | 18,			//bit 18 = End of DMA interrupt : Dev-DMA(Development tool DMA)

		holly_CH2_DMA = holly_nrm | 19,			//bit 19 = End of DMA interrupt : ch2-DMA
		holly_PVR_SortDMA = holly_nrm | 20,		//bit 20 = End of DMA interrupt : Sort-DMA (Transferring for alpha sorting)
		holly_PUNCHTHRU = holly_nrm | 21,		//bit 21 = End of Transferring interrupt : Punch Through List

		// asic9c/sh4 external holly external [EXTERNAL]
		holly_GDROM_CMD = holly_ext | 0x00,	//bit 0 = GD-ROM interrupt
		holly_SPU_IRQ = holly_ext | 0x01,	//bit 1 = AICA interrupt
		holly_EXP_8BIT = holly_ext | 0x02,	//bit 2 = Modem interrupt
		holly_EXP_PCI = holly_ext | 0x03,	//bit 3 = External Device interrupt

		// asic9b/sh4 external holly err only error [error]
		//missing quite a few ehh ?
		//bit 0 = RENDER : ISP out of Cache(Buffer over flow)
		//bit 1 = RENDER : Hazard Processing of Strip Buffer
		holly_PRIM_NOMEM = holly_err | 0x02,	//bit 2 = TA : ISP/TSP Parameter Overflow
		holly_MATR_NOMEM = holly_err | 0x03,	//bit 3 = TA : Object List Pointer Overflow
		//bit 4 = TA : Illegal Parameter
		//bit 5 = TA : FIFO Overflow
		//bit 6 = PVRIF : Illegal Address set
		//bit 7 = PVRIF : DMA over run
		holly_MAPLE_ILLADDR = holly_err | 0x08,  //bit 8 = MAPLE : Illegal Address set
		holly_MAPLE_OVERRUN = holly_err | 0x09,  //bit 9 = MAPLE : DMA over run
		holly_MAPLE_FIFO = holly_err | 0x0a,     //bit 10 = MAPLE : Write FIFO overflow
		holly_MAPLE_ILLCMD = holly_err | 0x0b,   //bit 11 = MAPLE : Illegal command
		//bit 12 = G1 : Illegal Address set
		//bit 13 = G1 : GD-DMA over run
		//bit 14 = G1 : ROM/FLASH access at GD-DMA
		holly_AICA_ILLADDR = holly_err | 0x0f,   //bit 15 = G2 : AICA-DMA Illegal Address set
		holly_EXT1_ILLADDR = holly_err | 0x10,   //bit 16 = G2 : Ext-DMA1 Illegal Address set
		holly_EXT2_ILLADDR = holly_err | 0x11,   //bit 17 = G2 : Ext-DMA2 Illegal Address set
		holly_DEV_ILLADDR = holly_err | 0x12,    //bit 18 = G2 : Dev-DMA Illegal Address set
		holly_AICA_OVERRUN = holly_err | 0x13,   //bit 19 = G2 : AICA-DMA over run
		holly_EXT1_OVERRUN = holly_err | 0x14,   //bit 20 = G2 : Ext-DMA1 over run
		holly_EXT2_OVERRUN = holly_err | 0x15,   //bit 21 = G2 : Ext-DMA2 over run
		holly_DEV_OVERRUN = holly_err | 0x16,    //bit 22 = G2 : Dev-DMA over run
		//bit 23 = G2 : AICA-DMA Time out
		//bit 24 = G2 : Ext-DMA1 Time out
		//bit 25 = G2 : Ext-DMA2 Time out
		//bit 26 = G2 : Dev-DMA Time out
		//bit 27 = G2 : Time out in CPU accessing
};

void asic_RaiseInterrupt(HollyInterruptID inter);
void asic_CancelInterrupt(HollyInterruptID inter);
void asic_RaiseInterruptBothCLX(HollyInterruptID inter);

//Init/Res/Term for regs
void asic_reg_Init();
void asic_reg_Term();
void asic_reg_Reset(bool hard);
