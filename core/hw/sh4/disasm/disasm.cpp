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

#include "types.h"
#include "disasm.h"
#include "sh7091.h"

char* decode(u16 opcode, u32 cur_PC)
{
	uint16_t op;
	static char buf[256];
	int32_t temp;
	uint32_t reference = 0, reference2 = 0, literal = 0;

	memset(buf, 0, sizeof(buf));

	op = opcode&0xf000;
	switch (op) {
		case MOV0:
			temp = ((int32_t)IMM<<24)>>24;
			sprintf(buf,(temp<0) ? "mov H'%08x, R%d" : "mov H'%02x, R%d",temp,RN);
			return (buf);
			break;

		case MOVW0:
			reference = IMM*2+cur_PC+4;
			sprintf(buf,"mov.w @(H'%08x), R%d",reference,RN);
			return (buf);
			break;

		case MOVL0:
			reference = (IMM*4+(cur_PC&0xfffffffc)+4);
			sprintf(buf,"mov.l @(H'%08x), R%d",reference,RN);
			return (buf);
			break;

		case MOVL5:
			sprintf(buf,"mov.l R%d, @(%d, R%d)",RM,DISP,RN);
			return (buf);
			break;

		case MOVL6:
			sprintf(buf,"mov.l @(%d, R%d), R%d",DISP,RM,RN);
			return (buf);
			break;

		case ADD1:
			temp = ((int32_t)(IMM<<24))>>24;
			sprintf(buf,(temp<0) ? "sub H'%02x, R%d" : "add H'%02x, R%d", temp<0 ?  0-temp : temp,RN);
			return (buf);
			break;

		case BRA:
			temp = (((int32_t)(LABEL<<20))>>20) * 2;
				sprintf(buf,"bra H'%08x",temp + (cur_PC + 4));
			return (buf);
			break;

		case BSR:
			temp = (((int32_t)(LABEL<<20))>>20) * 2;
				sprintf(buf,"bsr H'%08x",temp + (cur_PC + 4));
			return (buf);
			break;

/* this seems to be needed, no? */
		default:
			break;
	}

	op = opcode&0xf1ff;
	switch(op) {
		case FABS1:
			sprintf(buf,"fabs DR%d",DRN);
			return (buf);
			break;

		case FCNVDS:
			sprintf(buf,"fcnvds DR%d, FPUL",DRN);
			return (buf);
			break;

		case FCNVSD:
			sprintf(buf,"fcnvsd FPUL, DR%d",DRN);
			return (buf);
			break;

		case FLOAT1:
			sprintf(buf,"float FPUL, DR%d",DRN);
			return (buf);
			break;

		case FNEG1:
			sprintf(buf,"fneg DR%d",DRN);
			return (buf);
			break;

		case FSQRT1:
			sprintf(buf,"fqsrt DR%d",DRN);
			return (buf);
			break;

		case FTRC1:
			sprintf(buf,"ftrc DR%d, FPUL",DRN);
			return (buf);
			break;

		case FSCA:
			sprintf(buf,"fsca FPUL, DR%d",DRN);
			return (buf);
			break;

		default:
			break;
	}

	op = opcode&0xf00f;
	switch (op) {
		case MOV1:
			sprintf(buf,"mov R%d, R%d",RM,RN);
			return (buf);
			break;

		case MOVB1:
			sprintf(buf,"mov.b R%d, @R%d",RM,RN);
			return (buf);
			break;

		case MOVW1:
			sprintf(buf,"mov.w R%d, @R%d",RM,RN);
			return (buf);
			break;

		case MOVL1:
			sprintf(buf,"mov.l R%d, @R%d",RM,RN);
			return (buf);
			break;

		case MOVB2:
			sprintf(buf,"mov.b @R%d, R%d",RM,RN);
			return (buf);
			break;

		case MOVW2:
			sprintf(buf,"mov.w @R%d, R%d",RM,RN);
			return (buf);
			break;

		case MOVL2:
			sprintf(buf,"mov.l @R%d, R%d",RM,RN);
			return (buf);
			break;

		case MOVB3:
			sprintf(buf,"mov.b R%d, @-R%d",RM,RN);
			return (buf);
			break;

		case MOVW3:
			sprintf(buf,"mov.w R%d, @-R%d",RM,RN);
			return (buf);
			break;

		case MOVL3:
			sprintf(buf,"mov.l R%d, @-R%d",RM,RN);
			return (buf);
			break;

		case MOVB4:
			sprintf(buf,"mov.b @R%d+, R%d",RM,RN);
			return (buf);
			break;

		case MOVW4:
			sprintf(buf,"mov.w @R%d+, R%d",RM,RN);
			return (buf);
			break;

		case MOVL4:
			sprintf(buf,"mov.l @R%d+, R%d",RM,RN);
			return (buf);
			break;

		case MOVB7:
			sprintf(buf,"mov.b R%d, @(R0, R%d)",RM,RN);
			return (buf);
			break;

		case MOVW7:
			sprintf(buf,"mov.w R%d, @(R0, R%d)",RM,RN);
			return (buf);
			break;

		case MOVL7:
			sprintf(buf,"mov.l R%d, @(R0, R%d)",RM,RN);
			return (buf);
			break;

		case MOVB8:
			sprintf(buf,"mov.b @(R0, R%d), R%d",RM,RN);
			return (buf);
			break;

		case MOVW8:
			sprintf(buf,"mov.w @(R0, R%d), R%d",RM,RN);
			return (buf);
			break;

		case MOVL8:
			sprintf(buf,"mov.l @(R0, R%d), R%d",RM,RN);
			return (buf);
			break;

		case SWAPB:
			sprintf(buf,"swap.b R%d, R%d",RM,RN);
			return (buf);
			break;

		case SWAPW:
			sprintf(buf,"swap.w R%d, R%d",RM,RN);
			return (buf);
			break;

		case XTRCT:
			sprintf(buf,"xtrct R%d, R%d",RM,RN);
			return (buf);
			break;

		case ADD0:
			sprintf(buf,"add R%d, R%d",RM,RN);
			return (buf);
			break;

		case ADDC:
			sprintf(buf,"addc R%d, R%d",RM,RN);
			return (buf);
			break;

		case ADDV:
			sprintf(buf,"addv R%d, R%d",RM,RN);
			return (buf);
			break;

		case CMPEQ1:
			sprintf(buf,"cmp/eq R%d, R%d",RM,RN);
			return (buf);
			break;
		case CMPHS:
			sprintf(buf,"cmp/hs R%d, R%d",RM,RN);
			return (buf);
			break;

		case CMPGE:
			sprintf(buf,"cmp/ge R%d, R%d",RM,RN);
			return (buf);
			break;

		case CMPHI:
			sprintf(buf,"cmp/hi R%d, R%d",RM,RN);
			return (buf);
			break;

		case CMPGT:
			sprintf(buf,"cmp/gt R%d, R%d",RM,RN);
			return (buf);
			break;

		case CMPSTR:
			sprintf(buf,"cmp/str R%d, R%d",RM,RN);
			return (buf);
			break;

		case DIV1:
			sprintf(buf,"div1 R%d, R%d",RM,RN);
			return (buf);
			break;

		case DIV0S:
			sprintf(buf,"div0s R%d, R%d",RM,RN);
			return (buf);
			break;

		case DMULSL:
			sprintf(buf,"dmuls.l R%d, R%d",RM,RN);
			return (buf);
			break;

		case DMULUL:
			sprintf(buf,"dmulu.l R%d, R%d",RM,RN);
			return (buf);
			break;

		case EXTSB:
			sprintf(buf,"exts.b R%d, R%d",RM,RN);
			return (buf);
			break;

		case EXTSW:
			sprintf(buf,"exts.w R%d, R%d",RM,RN);
			return (buf);
			break;

		case EXTUB:
			sprintf(buf,"extu.b R%d, R%d",RM,RN);
			return (buf);
			break;

		case EXTUW:
			sprintf(buf,"extu.w R%d, R%d",RM,RN);
			return (buf);
			break;

		case MACL:
			sprintf(buf,"mac.l @R%d+, @R%d+",RM,RN);
			return (buf);
			break;

		case MACW:
			sprintf(buf,"mac.w @R%d+, @R%d+",RM,RN);
			return (buf);
			break;

		case MULL:
			sprintf(buf,"mul.l R%d, R%d",RM,RN);
			return (buf);
			break;

		case MULSW:
			sprintf(buf,"muls.w R%d, R%d",RM,RN);
			return (buf);
			break;

		case MULUW:
			sprintf(buf,"mulu.w R%d, R%d",RM,RN);
			return (buf);
			break;

		case NEG:
			sprintf(buf,"neg R%d, R%d",RM,RN);
			return (buf);
			break;

		case NEGC:
			sprintf(buf,"negc R%d, R%d",RM,RN);
			return (buf);
			break;

		case SUB:
			sprintf(buf,"sub R%d, R%d",RM,RN);
			return (buf);
			break;

		case SUBC:
			sprintf(buf,"subc R%d, R%d",RM,RN);
			return (buf);
			break;

		case SUBV:
			sprintf(buf,"subv R%d, R%d",RM,RN);
			return (buf);
			break;

		case AND0:
			sprintf(buf,"and R%d, R%d",RM,RN);
			return (buf);
			break;

		case NOT:
			sprintf(buf,"not R%d, R%d",RM,RN);
			return (buf);
			break;

		case OR0:
			sprintf(buf,"or R%d, R%d",RM,RN);
			return (buf);
			break;

		case TST0:
			sprintf(buf,"tst R%d, R%d",RM,RN);
			return (buf);
			break;

		case XOR0:
			sprintf(buf,"xor R%d, R%d",RM,RN);
			return (buf);
			break;

		case SHAD:
			sprintf(buf,"shad R%d, R%d",RM,RN);
			return (buf);
			break;

		case SHLD:
			sprintf(buf,"shld R%d, R%d",RM,RN);
			return (buf);
			break;

		case FMOV0:
			sprintf(buf,"fmov FR%d, FR%d",RM,RN);
			return (buf);
			break;

		case FMOVS0:
			sprintf(buf,"fmov.s @R%d, FR%d",RM,RN);
			return (buf);
			break;

		case FMOVS1:
			sprintf(buf,"fmov.s @(R0, R%d), FR%d",RM,RN);
			return (buf);
			break;

		case FMOVS2:
			sprintf(buf,"fmov.s @R%d+, FR%d",RM,RN);
			return (buf);
			break;

		case FMOVS3:
			sprintf(buf,"fmov.s FR%d, @R%d",RM,RN);
			return (buf);
			break;

		case FMOVS4:
			sprintf(buf,"fmov.s FR%d, @-R%d",RM,RN);
			return (buf);
			break;

		case FMOVS5:
			sprintf(buf,"fmov.s FR%d, @(R0, R%d)",RM,RN);
			return (buf);
			break;

		case FADD0:
			sprintf(buf,"fadd FR%d, FR%d",RM,RN);
			return (buf);
			break;

		case FCMPEQ0:
			sprintf(buf,"fcmp/eq FR%d, FR%d",RM,RN);
			return (buf);
			break;

		case FCMPGT0:
			sprintf(buf,"fcmp/gt FR%d, FR%d",RM,RN);
			return (buf);
			break;

		case FDIV0:
			sprintf(buf,"fdiv FR%d, FR%d",RM,RN);
			return (buf);
			break;

		case FMAC:
			sprintf(buf,"fmac FR0, FR%d, FR%d",RM,RN);
			return (buf);
			break;

		case FMUL0:
			sprintf(buf,"fmul FR%d, FR%d",RM,RN);
			return (buf);
			break;

		case FSUB0:
			sprintf(buf,"fsub FR%d, FR%d",RM,RN);
			return (buf);
			break;
	}

	op = opcode&0xff00;
	switch (op) {
		case MOVB5:
			sprintf(buf,"mov.b R0, @(%d, R%d)",DISP,RM);
			return (buf);
			break;

		case MOVW5:
			sprintf(buf,"mov.w R0, @(%d, R%d)",DISP,RM);
			return (buf);
			break;

		case MOVB6:
			sprintf(buf,"mov.b @(%d, R%d), R0",DISP,RM);
			return (buf);
			break;

		case MOVW6:
			sprintf(buf,"mov.w @(%d, R%d), R0",DISP,RM);
			return (buf);
			break;

		case MOVB9:
			sprintf(buf,"mov.b R0, @(%d, GBR)",IMM);
			return (buf);
			break;

		case MOVW9:
			sprintf(buf,"mov.w R0, @(%d, GBR)",IMM);
			return (buf);
			break;

		case MOVL9:
			sprintf(buf,"mov.l R0, @(%d, GBR)",IMM);
			return (buf);
			break;

		case MOVB10:
			sprintf(buf,"mov.b @(%d, GBR), R0",IMM);
			return (buf);
			break;

		case MOVW10:
			sprintf(buf,"mov.w @(%d, GBR), R0",IMM);
			return (buf);
			break;

		case MOVL10:
			sprintf(buf,"mov.l @(%d, GBR), R0",IMM);
			return (buf);
			break;

		case MOVA:
/*			sprintf(buf,"mova @(H'%08x), R0",(IMM*4) + ((cur_PC&0xfffffffc) + 4));	*/
				sprintf(buf,"mova H'%08x, R0",(IMM*4) + ((cur_PC&0xfffffffc) + 4));
			return (buf);
			break;

		case CMPEQ0:
			temp = ((int32_t)(IMM<<24))>>24;
			sprintf(buf,"cmp/eq H'%02x, R0",temp);
			return (buf);
			break;

		case AND1:
			sprintf(buf,"and H'%02x, R0",IMM);
			return (buf);
			break;

		case ANDB:
			sprintf(buf,"and.b H'%02x, @(R0, GBR)",IMM);
			return (buf);
			break;

		case OR1:
			sprintf(buf,"or H'%02x, R0",IMM);
			return (buf);
			break;

		case ORB:
			sprintf(buf,"or.b H'%02x, @(R0, GBR)",IMM);
			return (buf);
			break;

		case TST1:
			sprintf(buf,"tst H'%02x, R0",IMM);
			return (buf);
			break;

		case TSTB:
			sprintf(buf,"tst.b H'%02x, @(R0, GBR)",IMM);
			return (buf);
			break;

		case XOR1:
			sprintf(buf,"xor H'%02x, R0",IMM);
			return (buf);
			break;

		case XORB:
			sprintf(buf,"xor.b H'%02x, @(R0, GBR)",IMM);
			return (buf);
			break;

		case BF:
			temp = (((int32_t)(IMM<<24))>>24) * 2;
				sprintf(buf,"bf H'%08x",temp + (cur_PC + 4));
			return (buf);
			break;

		case BFS:
			temp = (((int32_t)(IMM<<24))>>24) * 2;
				sprintf(buf,"bf/s H'%08x",temp + (cur_PC + 4));
			return (buf);
			break;

		case BT:
			temp = (((int32_t)(IMM<<24))>>24) * 2;
				sprintf(buf,"bt H'%08x",temp + (cur_PC + 4));
			return (buf);
			break;

		case BTS:
			temp = (((int32_t)(IMM<<24))>>24) * 2;
				sprintf(buf,"bt/s H'%08x",temp + (cur_PC + 4));
			return (buf);
			break;

		case TRAPA:
			sprintf(buf,"trapa H'%02x",IMM<<2);
			return (buf);
			break;

		default:
			break;
	}

	op = opcode&0xf0ff;
	switch (op) {
		case MOVT:
			sprintf(buf,"movt R%d",RN);
			return (buf);
			break;

		case CMPPZ:
			sprintf(buf,"cmp/pz R%d",RN);
			return (buf);
			break;

		case CMPPL:
			sprintf(buf,"cmp/pl R%d",RN);
			return (buf);
			break;

		case DT:
			sprintf(buf,"dt R%d",RN);
			return (buf);
			break;

		case TASB:
			sprintf(buf,"tas.b @R%d",RN);
			return (buf);
			break;

		case ROTL:
			sprintf(buf,"rotl R%d",RN);
			return (buf);
			break;

		case ROTR:
			sprintf(buf,"rotr R%d",RN);
			return (buf);
			break;

		case ROTCL:
			sprintf(buf,"rotcl R%d",RN);
			return (buf);
			break;

		case ROTCR:
			sprintf(buf,"rotcr R%d",RN);
			return (buf);
			break;

		case SHAL:
			sprintf(buf,"shal R%d",RN);
			return (buf);
			break;

		case SHAR:
			sprintf(buf,"shar R%d",RN);
			return (buf);
			break;

		case SHLL:
			sprintf(buf,"shll R%d",RN);
			return (buf);
			break;

		case SHLR:
			sprintf(buf,"shlr R%d",RN);
			return (buf);
			break;

		case SHLL2:
			sprintf(buf,"shll2 R%d",RN);
			return (buf);
			break;

		case SHLR2:
			sprintf(buf,"shlr2 R%d",RN);
			return (buf);
			break;

		case SHLL8:
			sprintf(buf,"shll8 R%d",RN);
			return (buf);
			break;

		case SHLR8:
			sprintf(buf,"shlr8 R%d",RN);
			return (buf);
			break;

		case SHLL16:
			sprintf(buf,"shll16 R%d",RN);
			return (buf);
			break;

		case SHLR16:
			sprintf(buf,"shlr16 R%d",RN);
			return (buf);
			break;

		case BRAF:
			sprintf(buf,"braf R%d",RN);
			return (buf);
			break;

		case BSRF:
			sprintf(buf,"bsrf R%d",RN);
			return (buf);
			break;

		case JMP:
			sprintf(buf,"jmp @R%d",RN);
			return (buf);
			break;

		case JSR:
			sprintf(buf,"jsr @R%d",RN);
			return (buf);
			break;

		case LDC0:
			sprintf(buf,"ldc R%d, SR",RN);
			return (buf);
			break;

		case LDC1:
			sprintf(buf,"ldc R%d, GBR",RN);
			return (buf);
			break;

		case LDC2:
			sprintf(buf,"ldc R%d, VBR",RN);
			return (buf);
			break;

		case LDC3:
			sprintf(buf,"ldc R%d, SSR",RN);
			return (buf);
			break;

		case LDC4:
			sprintf(buf,"ldc R%d, SPC",RN);
			return (buf);
			break;

		case LDC5:
			sprintf(buf,"ldc R%d, DBR",RN);
			return (buf);
			break;

		case LDCL0:
			sprintf(buf,"ldc.l @R%d+, SR",RN);
			return (buf);
			break;

		case LDCL1:
			sprintf(buf,"ldc.l @R%d+, GBR",RN);
			return (buf);
			break;

		case LDCL2:
			sprintf(buf,"ldc.l @R%d+, VBR",RN);
			return (buf);
			break;

		case LDCL3:
			sprintf(buf,"ldc.l @R%d+, SSR",RN);
			return (buf);
			break;

		case LDCL4:
			sprintf(buf,"ldc.l @R%d+, SPC",RN);
			return (buf);
			break;

		case LDCL5:
			sprintf(buf,"ldc.l @R%d+, DBR",RN);
			return (buf);
			break;

		case LDS0:
			sprintf(buf,"lds R%d, MACH",RN);
			return (buf);
			break;

		case LDS1:
			sprintf(buf,"lds R%d, MACL",RN);
			return (buf);
			break;

		case LDS2:
			sprintf(buf,"lds R%d, PR",RN);
			return (buf);
			break;

		case LDSL0:
			sprintf(buf,"lds.l @R%d+, MACH",RN);
			return (buf);
			break;

		case LDSL1:
			sprintf(buf,"lds.l @R%d+, MACL",RN);
			return (buf);
			break;

		case LDSL2:
			sprintf(buf,"lds.l @R%d+, PR",RN);
			return (buf);
			break;

		case MOVCAL:
			sprintf(buf,"movca.l R0, @R%d",RN);
			return (buf);
			break;

		case OCBI:
			sprintf(buf,"ocbi @R%d",RN);
			return (buf);
			break;

		case OCBP:
			sprintf(buf,"ocbp @R%d",RN);
			return (buf);
			break;

		case OCBWB:
			sprintf(buf,"ocbwb @R%d",RN);
			return (buf);
			break;

		case PREF:
			sprintf(buf,"pref @R%d",RN);
			return (buf);
			break;

		case STC0:
			sprintf(buf,"stc SR, R%d",RN);
			return (buf);
			break;

		case STC1:
			sprintf(buf,"stc GBR, R%d",RN);
			return (buf);
			break;

		case STC2:
			sprintf(buf,"stc VBR, R%d",RN);
			return (buf);
			break;

		case STC3:
			sprintf(buf,"stc SSR, R%d",RN);
			return (buf);
			break;

		case STC4:
			sprintf(buf,"stc SPC, R%d",RN);
			return (buf);
			break;

		case STC5:
			sprintf(buf,"stc SGR, R%d",RN);
			return (buf);
			break;

		case STC6:
			sprintf(buf,"stc DBR, R%d",RN);
			return (buf);
			break;

		case STCL0:
			sprintf(buf,"stc.l SR, @-R%d",RN);
			return (buf);
			break;

		case STCL1:
			sprintf(buf,"stc.l GBR, @-R%d",RN);
			return (buf);
			break;

		case STCL2:
			sprintf(buf,"stc.l VBR, @-R%d",RN);
			return (buf);
			break;

		case STCL3:
			sprintf(buf,"stc.l SSR, @-R%d",RN);
			return (buf);
			break;

		case STCL4:
			sprintf(buf,"stc.l SPC, @-R%d",RN);
			return (buf);
			break;

		case STCL5:
			sprintf(buf,"stc.l SGR, @-R%d",RN);
			return (buf);
			break;

		case STCL6:
			sprintf(buf,"stc.l DBR, @-R%d",RN);
			return (buf);
			break;

		case STS0:
			sprintf(buf,"sts MACH, R%d",RN);
			return (buf);
			break;

		case STS1:
			sprintf(buf,"sts MACL, R%d",RN);
			return (buf);
			break;

		case STS2:
			sprintf(buf,"sts PR, R%d",RN);
			return (buf);
			break;

		case STSL0:
			sprintf(buf,"sts.l MACH, @-R%d",RN);
			return (buf);
			break;

		case STSL1:
			sprintf(buf,"sts.l MACL, @-R%d",RN);
			return (buf);
			break;

		case STSL2:
			sprintf(buf,"sts.l PR, @-R%d",RN);
			return (buf);
			break;

		case FLDI0:
			sprintf(buf,"fldi0 FR%d",RN);
			return (buf);
			break;

		case FLDI1:
			sprintf(buf,"fldi1 FR%d",RN);
			return (buf);
			break;

		case FLDS:
			sprintf(buf,"flds FR%d, FPUL",RN);
			return (buf);
			break;

		case FSTS:
			sprintf(buf,"fsts FPUL, FR%d",RN);
			return (buf);
			break;

		case FABS0:
			sprintf(buf,"fabs FR%d",RN);
			return (buf);
			break;

		case FLOAT0:
			sprintf(buf,"float FPUL, FR%d",RN);
			return (buf);
			break;

		case FNEG0:
			sprintf(buf,"fneg FR%d",RN);
			return (buf);
			break;

		case FSQRT0:
			sprintf(buf,"fqsrt FR%d",RN);
			return (buf);
			break;

		case FTRC0:
			sprintf(buf,"ftc FR%d, FPUL",RN);
			return (buf);
			break;

		case LDS3:
			sprintf(buf,"lds R%d, FPSCR",RN);
			return (buf);
			break;

		case LDS4:
			sprintf(buf,"lds R%d, FPUL",RN);
			return (buf);
			break;

		case LDSL3:
			sprintf(buf,"lds.l @R%d+, FPSCR",RN);
			return (buf);
			break;

		case LDSL4:
			sprintf(buf,"ldl.l @R%d+, FPUL",RN);
			return (buf);
			break;

		case STS3:
			sprintf(buf,"sts FPSCR, R%d",RN);
			return (buf);
			break;

		case STS4:
			sprintf(buf,"sts FPUL, R%d",RN);
			return (buf);
			break;

		case STSL3:
			sprintf(buf,"sts.l FPSCR, @-R%d",RN);
			return (buf);
			break;

		case STSL4:
			sprintf(buf,"sts.l FPUL, @-R%d",RN);
			return (buf);
			break;

		case FIPR:
			sprintf(buf,"fipr FV%d, FV%d",(opcode&0x0300)>>8,(opcode&0x0c00)>>10);
			return (buf);
			break;

		case FSRRA:
			sprintf(buf,"fsrra FR%d",RN);
			return (buf);
			break;

		default:
			break;

	}

	op = opcode&0xf3ff;
	switch (op) {
		case FTRV:
			sprintf(buf,"ftrv XMTRX, FV%d",(opcode&0x0c00)>>10);
			return (buf);
			break;
	}

	op = opcode&0xffff;
	switch(op) {
		case DIV0U:
			sprintf(buf,"div0u");
			return (buf);
			break;

		case RTS:
			sprintf(buf,"rts");
			return (buf);
			break;

		case CLRMAC:
			sprintf(buf,"clrmac");
			return (buf);
			break;

		case CLRS:
			sprintf(buf,"clrs");
			return (buf);
			break;

		case CLRT:
			sprintf(buf,"clrt");
			return (buf);
			break;

		case LDTLB:
			sprintf(buf,"ldtlb");
			return (buf);
			break;

		case NOP:
			sprintf(buf,"nop");
			return (buf);
			break;

		case RTE:
			sprintf(buf,"rte");
			return (buf);
			break;

		case SETS:
			sprintf(buf,"sets");
			return (buf);
			break;

		case SETT:
			sprintf(buf,"sett");
			return (buf);
			break;

		case SLEEP:
			sprintf(buf,"sleep");
			return (buf);
			break;

		case FRCHG:
			sprintf(buf,"frchg");
			return (buf);
			break;

		case FSCHG:
			sprintf(buf,"fschg");
			return (buf);
			break;

		default:
			break;
	}

	op = opcode&0xf08f;
	switch(op) {
		case LDC6:
			sprintf(buf,"ldc R%d, R%d_BANK",RN,BANK);
			return (buf);
			break;

		case LDCL6:
			sprintf(buf,"ldc @R%d+, R%d_BANK",RN,BANK);
			return (buf);
			break;

		case STC7:
			sprintf(buf,"stc R%d_BANK, R%d",BANK,RN);
			return (buf);
			break;

		case STCL7:
			sprintf(buf,"stc.l R%d_BANK, @-R%d",BANK,RN);
			return (buf);
			break;
	}

	sprintf(buf,"???");
	return (buf);

}
