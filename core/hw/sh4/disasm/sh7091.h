/*
	Copyright (C) 1999-2004 Lars Olsson (Maiwe)
    Copyright (C) 2019 Moopthehedgehog
    Copyright (C) 2020 SiZiOUS

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

#pragma once

/* Some bitfields */
#define RM		((opcode&0x00f0)>>4)
#define RN		((opcode&0x0f00)>>8)
#define DISP	(opcode&0x000f)
#define IMM		(opcode&0x00ff)
#define LABEL	(opcode&0x0fff)
#define DRM		((opcode&0x00e0)>>4)
#define DRN		((opcode&0x0e00)>>8)
#define BANK	((opcode&0x0070)>>4)

/* all instructions are 16-bit */

/* Fixed-Point Transfer Instructions */
#define MOV0	0xe000	/* MOV		#imm,Rn 		*/
#define MOVW0	0x9000	/* MOV.W	@(disp,PC),Rn	*/
#define MOVL0	0xd000	/* MOV.L	@(disp,PC),Rn	*/
#define MOV1	0x6003	/* MOV		Rm,Rn			*/
#define MOVB1	0x2000	/* MOV.B	Rm,@Rn			*/
#define MOVW1	0x2001	/* MOV.W	Rm,@Rn			*/
#define MOVL1	0x2002	/* MOV.L	Rm,@Rn			*/
#define MOVB2	0x6000	/* MOV.B	@Rm,Rn			*/
#define MOVW2	0x6001	/* MOV.W	@Rm,Rn			*/
#define MOVL2	0x6002	/* MOV.L	@Rm,Rn			*/
#define MOVB3	0x2004	/* MOV.B	Rm,@-Rn			*/
#define MOVW3	0x2005	/* MOV.W	Rm,@-Rn			*/
#define MOVL3	0x2006	/* MOV.L	Rm,@-Rn			*/
#define MOVB4	0x6004	/* MOV.B	@Rm+,Rn			*/
#define MOVW4	0x6005	/* MOV.W	@Rm+,Rn			*/
#define MOVL4	0x6006	/* MOV.L	@Rm+,Rn			*/
#define MOVB5	0x8000	/* MOV.B	R0,@(disp,Rn)	*/
#define MOVW5	0x8100	/* MOV.W	R0,@(disp,Rn)	*/
#define MOVL5	0x1000	/* MOV.L	Rm,@(disp,Rn)	*/
#define MOVB6	0x8400	/* MOV.B	@(disp,Rm),R0	*/
#define MOVW6	0x8500	/* MOV.W	@(disp,Rm),R0	*/
#define MOVL6	0x5000	/* MOV.L	@(disp,Rm),Rn	*/
#define MOVB7	0x0004	/* MOV.B	Rm,@(R0,Rn)		*/
#define MOVW7	0x0005	/* MOV.W	Rm,@(R0,Rn)		*/
#define MOVL7	0x0006	/* MOV.L	Rm,@(R0,Rn)		*/
#define MOVB8	0x000c	/* MOV.B	@(R0,Rm),Rn		*/
#define MOVW8	0x000d	/* MOV.W	@(R0,Rm),Rn		*/
#define MOVL8	0x000e	/* MOV.L	@(R0,Rm),Rn		*/
#define MOVB9	0xc000	/* MOV.B	R0,@(disp,GBR)	*/
#define MOVW9	0xc100	/* MOV.W	R0,@(disp,GBR)	*/
#define MOVL9	0xc200	/* MOV.L	R0,@(disp,GBR)	*/
#define MOVB10	0xc400	/* MOV.B	@(disp,GBR),R0	*/
#define MOVW10	0xc500	/* MOV.W	@(disp,GBR),R0	*/
#define MOVL10	0xc600	/* MOV.L	@(disp,GBR),R0	*/
#define MOVA	0xc700	/* MOVA		@(disp,PC),R0	*/
#define MOVT	0x0029	/* MOVT		Rn				*/
#define SWAPB	0x6008	/* SWAP.B	Rm,Rn			*/
#define SWAPW	0x6009	/* SWAP.W	Rm,Rn			*/
#define XTRCT	0x200d	/* XTRCT	Rm,Rn			*/

/* Arithmetic Operation Instructions */
#define ADD0	0x300c	/* ADD		Rm,Rn			*/
#define ADD1	0x7000	/* ADD		#imm,Rn			*/
#define ADDC	0x300e	/* ADDC		Rm,Rn			*/
#define ADDV	0x300f	/* ADDV		Rm,Rn			*/
#define CMPEQ0	0x8800	/* CMP/EQ	#imm,R0			*/
#define CMPEQ1	0x3000	/* CMP/EQ	Rm,Rn			*/
#define CMPHS	0x3002	/* CMP/HS	Rm,Rn			*/
#define CMPGE	0x3003	/* CMP/GE	Rm,Rn			*/
#define CMPHI	0x3006	/* CMP/HI	Rm,Rn			*/
#define CMPGT	0x3007	/* CMP/GT	Rm,Rn			*/
#define CMPPZ	0x4011	/* CMP/PZ	Rn				*/
#define CMPPL	0x4015	/* CMP/PL	Rn				*/
#define CMPSTR	0x200c	/* CMP/STR	Rm,Rn			*/
#define DIV1	0x3004	/* DIV1		Rm,Rn			*/
#define DIV0S	0x2007	/* DIV0S	Rm,Rn			*/
#define DIV0U	0x0019	/* DIV0U					*/
#define DMULSL	0x300d	/* DMULS.L	Rm,Rn			*/
#define DMULUL	0x3005	/* DMULU.L	Rm,Rn			*/
#define DT		0x4010	/* DT		Rn				*/
#define EXTSB	0x600e	/* EXTS.B	Rm,Rn			*/
#define EXTSW	0x600f	/* EXTS.W	Rm,Rn			*/
#define EXTUB	0x600c	/* EXTU.B	Rm,Rn			*/
#define EXTUW	0x600d	/* EXTU.W	Rm,Rn			*/
#define MACL	0x000f	/* MAC.L	@Rm+,@Rn+		*/
#define MACW	0x400f	/* MAC.W	@Rm+,@Rn+		*/
#define MULL	0x0007	/* MUL.L	Rm,Rn			*/
#define MULSW	0x200f	/* MULS.W	Rm,Rn			*/
#define MULUW	0x200e	/* MULU.W	Rm,Rn			*/
#define NEG		0x600b	/* NEG		Rm,Rn			*/
#define NEGC	0x600a	/* NEGC		Rm,Rn			*/
#define SUB		0x3008	/* SUB		Rm,Rn			*/
#define SUBC	0x300a	/* SUBC		Rm,Rn			*/
#define SUBV	0x300b	/* SUBV		Rm,Rn			*/

/* Logic Operation Instructions */
#define AND0	0x2009	/* AND		Rm,Rn			*/
#define AND1	0xc900	/* AND		#imm,R0			*/
#define ANDB	0xcd00	/* AND.B	#imm,@(R0,GBR)	*/
#define NOT		0x6007	/* NOT		Rm,Rn			*/
#define OR0		0x200b	/* OR		Rm,Rn			*/
#define OR1		0xcb00	/* OR		#imm,R0			*/
#define ORB		0xcf00	/* OR.B		#imm,@(R0,GBR)	*/
#define TASB	0x401b	/* TAS.B	@Rn				*/
#define TST0	0x2008	/* TST		Rm,Rn			*/
#define TST1	0xc800	/* TST		#imm,R0			*/
#define TSTB	0xcc00	/* TST.B	#imm,@(R0,GBR)	*/
#define XOR0	0x200a	/* XOR		Rm,Rn			*/
#define XOR1	0xca00	/* XOR		#imm,R0			*/
#define XORB	0xce00	/* XOR.B	#imm,@(R0,GBR)	*/

/* Shift Instructions */
#define ROTL	0x4004	/* ROTL		Rn				*/
#define ROTR	0x4005	/* ROTR		Rn				*/
#define ROTCL	0x4024	/* ROTCL	Rn				*/
#define ROTCR	0x4025	/* ROTCR	Rn				*/
#define SHAD	0x400c	/* SHAD		Rm,Rn			*/
#define SHAL	0x4020	/* SHAL		Rn				*/
#define SHAR	0x4021	/* SHAR		Rn				*/
#define SHLD	0x400d	/* SHLD		Rm,Rn			*/
#define SHLL	0x4000	/* SHLL		Rn				*/
#define SHLR	0x4001	/* SHLR		Rn				*/
#define SHLL2	0x4008	/* SHLL2	Rn				*/
#define SHLR2	0x4009	/* SHLR2	Rn				*/
#define SHLL8	0x4018	/* SHLL8	Rn				*/
#define SHLR8	0x4019	/* SHLR8	Rn				*/
#define SHLL16	0x4028	/* SHLL16	Rn				*/
#define SHLR16	0x4029	/* SHLR16	Rn				*/

/* Branch Instructions */
#define BF		0x8b00	/* BF		label			*/
#define BFS		0x8f00	/* BF/S		label			*/
#define BT		0x8900	/* BT		label			*/
#define BTS		0x8d00	/* BT/S		label			*/
#define BRA		0xa000	/* BRA		label			*/
#define BRAF	0x0023	/* BRAF		Rn				*/
#define BSR		0xb000	/* BSR		label			*/
#define BSRF	0x0003	/* BSRF		Rn				*/
#define JMP		0x402b	/* JMP		@Rn				*/
#define JSR		0x400b	/* JSR		@Rn				*/
#define RTS		0x000b	/* RTS						*/

/* System Control Instructions */
#define CLRMAC	0x0028	/* CLRMAC					*/
#define CLRS	0x0048	/* CLRS						*/
#define CLRT	0x0008	/* CLRT						*/
#define LDC0	0x400e	/* LDC		Rm,SR			*/
#define LDC1	0x401e	/* LDC		Rm,GBR			*/
#define LDC2	0x402e	/* LDC		Rm,VBR			*/
#define LDC3	0x403e	/* LDC		Rm,SSR			*/
#define LDC4	0x404e	/* LDC		Rm,SPC			*/
#define LDC5	0x40fa	/* LDC		Rm,DBR			*/
#define LDC6	0x408e	/* LDC		Rm,Rn_BANK		*/
#define LDCL0	0x4007	/* LDC.L	@Rm+,SR			*/
#define LDCL1	0x4017	/* LDC.L	@Rm+,GBR		*/
#define LDCL2	0x4027	/* LDC.L	@Rm+,VBR		*/
#define LDCL3	0x4037	/* LDC.L	@Rm+,SSR		*/
#define LDCL4	0x4047	/* LDC.L	@Rm+,SPC		*/
#define LDCL5	0x40f6	/* LDC.L	@Rm+,DBR		*/
#define LDCL6	0x4087	/* LDC.L	@Rm+,Rn_BANK	*/
#define LDS0	0x400a	/* LDS		Rm,MACH			*/
#define LDS1	0x401a	/* LDS		Rm,MACL			*/
#define LDS2	0x402a	/* LDS		Rm,PR			*/
#define LDSL0	0x4006	/* LDS.L	@Rm+,MACH		*/
#define LDSL1	0x4016	/* LDS.L	@Rm+,MACL		*/
#define LDSL2	0x4026	/* LDS.L	@Rm+,PR			*/
#define LDTLB	0x0038	/* LDTLB					*/
#define MOVCAL	0x00c3	/* MOVCA.L	R0,@Rn			*/
#define NOP		0x0009	/* NOP						*/
#define OCBI	0x0093	/* OCBI		@Rn				*/
#define OCBP	0x00a3	/* OCBP		@Rn				*/
#define OCBWB	0x00b3	/* OCBWB	@Rn				*/
#define PREF	0x0083	/* PREF		@Rn				*/
#define RTE		0x002b	/* RTE						*/
#define SETS	0x0058	/* SETS						*/
#define SETT	0x0018	/* SETT						*/
#define SLEEP	0x001b	/* SLEEP					*/
#define STC0	0x0002	/* STC		SR,Rn			*/
#define STC1	0x0012	/* STC		GBR,Rn			*/
#define STC2	0x0022	/* STC		VBR,Rn			*/
#define STC3	0x0032	/* STC		SSR,Rn			*/
#define STC4	0x0042	/* STC		SPC,Rn			*/
#define STC5	0x003a	/* STC		SGR,Rn			*/
#define STC6	0x00fa	/* STC		DBR,Rn			*/
#define STC7	0x0082	/* STC		Rm_BANK,Rn		*/
#define STCL0	0x4003	/* STC.L	SR,@-Rn			*/
#define STCL1	0x4013	/* STC.L	GBR,@-Rn		*/
#define STCL2	0x4023	/* STC.L	VBR,@-Rn		*/
#define STCL3	0x4033	/* STC.L	SSR,@-Rn		*/
#define STCL4	0x4043	/* STC.L	SPC,@-Rn		*/
#define STCL5	0x4032	/* STC.L	SGR,@-Rn		*/
#define STCL6	0x40f2	/* STC.L	DBR,@-Rn		*/
#define STCL7	0x4083	/* STC.L	Rm_BANK,@-Rn	*/
#define STS0	0x000a	/* STS		MACH,Rn			*/
#define STS1	0x001a	/* STS		MACL,Rn			*/
#define STS2	0x002a	/* STS		PR,Rn			*/
#define STSL0	0x4002	/* STS.L	MACH,@-Rn		*/
#define STSL1	0x4012	/* STS.L	MACL,@-Rn		*/
#define STSL2	0x4022	/* STS.L	PR,@-Rn			*/
#define TRAPA	0xc300	/* TRAPA	#imm			*/

/* Floating-Point Single-Precision Instructions */
#define FLDI0	0xf08d	/* FLDI0	FRn				*/
#define FLDI1	0xf09d	/* FLDI1	FRn				*/
#define FMOV0	0xf00c	/* FMOV		FRm,FRn			*/
#define FMOVS0	0xf008	/* FMOV.S	@Rm,FRn			*/
#define FMOVS1	0xf006	/* FMOV.S	@(R0,Rm),FRn	*/
#define FMOVS2	0xf009	/* FMOV.S	@Rm+,FRn		*/
#define FMOVS3	0xf00a	/* FMOV.S	FRm,@Rn			*/
#define FMOVS4	0xf00b	/* FMOV.S	FRm,@-Rn		*/
#define FMOVS5	0xf007	/* FMOV.S	FRm,@(R0,Rn)	*/
#define FMOV1	0xf00c	/* FMOV		DRm,DRn			*/
#define FMOV2	0xf008	/* FMOV		@Rm,DRn			*/
#define FMOV3	0xf006	/* FMOV		@(R0,Rm),DRn	*/
#define FMOV4	0xf009	/* FMOV		@Rm+,DRn		*/
#define FMOV5	0xf00a	/* FMOV		DRm,@Rn			*/
#define FMOV6	0xf00b	/* FMOV		DRm,@-Rn		*/
#define FMOV7	0xf007	/* FMOV		DRm,@(R0,Rn)	*/
#define FLDS	0xf01d	/* FLDS		FRm,FPUL		*/
#define FSTS	0xf00d	/* FSTS		FPUL,FRn		*/
#define FABS0	0xf05d	/* FABS		FRn				*/
#define FADD0	0xf000	/* FADD		FRm,FRn			*/
#define FCMPEQ0	0xf004	/* FCMP/EQ	FRm,FRn			*/
#define FCMPGT0	0xf005	/* FCMP/GT	FRm,FRn			*/
#define FDIV0	0xf003	/* FDIV		FRm,FRn			*/
#define FLOAT0	0xf02d	/* FLOAT	FPUL,FRn		*/
#define FMAC	0xf00e	/* FMAC		FR0,FRm,FRn		*/
#define FMUL0	0xf002	/* FMUL		FRm,FRn			*/
#define FNEG0	0xf04d	/* FNEG		FRn				*/
#define FSQRT0	0xf06d	/* FSQRT	FRn				*/
#define FSUB0	0xf001	/* FSUB		FRm,FRn			*/
#define FTRC0	0xf03d	/* FTRC		FRm,FPUL		*/

/* Floating-Point Double-Precision Instructions */
#define FABS1	0xf05d	/* FABS		DRn				*/
#define FADD1	0xf000	/* FABS		DRM,DRn			*/
#define FCMPEQ1	0xf004	/* FCMP/EQ	DRm,DRn			*/
#define FCMPGT1	0xf005	/* FCMP/GT	DRm,DRn			*/
#define FDIV1	0xf003	/* FDIV		DRm,DRn			*/
#define FCNVDS	0xf0bd	/* FCNVDS	DRm,FPUL		*/
#define FCNVSD	0xf0ad	/* FCNVSD	FPUL,DRn		*/
#define FLOAT1	0xf02d	/* FLOAT	FPUL,DRn		*/
#define FMUL1	0xf002	/* FMUL		DRm,DRn			*/
#define FNEG1	0xf04d	/* FNEG		DRn				*/
#define FSQRT1	0xf06d	/* FSQRT	DRn				*/
#define FSUB1	0xf001	/* FSUB		DRm,DRn			*/
#define FTRC1	0xf03d	/* FTRC		DRm,FPUL		*/

/* Special Dreamcast(tm) instructions */
#define FSCA	0xf0fd	/* FSCA		FPUL, DRn		*/
#define FSRRA	0xf07d	/* FSRRA	FRn				*/

/* Floating-Point Control Instructions */
#define LDS3	0x406a	/* LDS		Rm,FPSCR		*/
#define LDS4	0x405a	/* LDS		Rm,FPUL			*/
#define LDSL3	0x4066	/* LDS.L	@Rm+,FPSCR		*/
#define LDSL4	0x4056	/* LDS.L	@Rm+,FPUL		*/
#define STS3	0x006a	/* STS		FPSCR,Rn		*/
#define STS4	0x005a	/* STS		FPUL,Rn			*/
#define STSL3	0x4062	/* STS.L	FPSCR,@-Rn		*/
#define STSL4	0x4052	/* STS.L	FPUL,@-Rn		*/

/* Floating-Point Graphics Acceleration Instructions */
#define FMOV8	0xf10c	/* FMOV		DRm,XDn			*/
#define FMOV9	0xf01c	/* FMOV		XDm,DRn			*/
#define FMOV10	0xf11c	/* FMOV		XDm,XDn			*/
#define FMOV11	0xf108	/* FMOV		@Rm,XDn			*/
#define FMOV12	0xf109	/* FMOV		@Rm+,XDn		*/
#define FMOV13	0xf106	/* FMOV		@(R0,Rm),DRn	*/
#define FMOV14	0xf01a	/* FMOV		XDm,@Rn			*/
#define FMOV15	0xf01b	/* FMOV		XDm,@-Rn		*/
#define FMOV16	0xf017	/* FMOV		XDm,@(R0,Rn)	*/
#define FIPR	0xf0ed	/* FIPR		FVm,FVn			*/
#define FTRV	0xf1fd	/* FTRV		XMTRX,FVn		*/
#define FRCHG	0xfbfd	/* FRCHG					*/
#define FSCHG	0xf3fd	/* FSCHG					*/
