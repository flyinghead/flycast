#include "types.h"
#include <cmath>

#include "sh4_opcodes.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_rom.h"
#include "hw/sh4/sh4_mem.h"

#define sh4op(str) void DYNACALL str (u32 op)

static u32 GetN(u32 op) {
	return (op >> 8) & 0xf;
}
static u32 GetM(u32 op) {
	return (op >> 4) & 0xf;
}

static double getDRn(u32 op) {
	return GetDR((op >> 9) & 7);
}
static double getDRm(u32 op) {
	return GetDR((op >> 5) & 7);
}
static void setDRn(u32 op, double d) {
	SetDR((op >> 9) & 7, d);
}

static void iNimp(const char *str);

#define CHECK_FPU_32(v) v = fixNaN(v)

//fadd <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0000)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		fr[n] += fr[m];
		CHECK_FPU_32(fr[n]);
	}
	else
	{
		double d = getDRn(op) + getDRm(op);
		d = fixNaN64(d);
		setDRn(op, d);
	}
}

//fsub <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0001)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		fr[n] -= fr[m];
		CHECK_FPU_32(fr[n]);
	}
	else
	{
		double d = getDRn(op) - getDRm(op);
		d = fixNaN64(d);
		setDRn(op, d);
	}
}
//fmul <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0010)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		fr[n] *= fr[m];
		CHECK_FPU_32(fr[n]);
	}
	else
	{
		double d = getDRn(op) * getDRm(op);
		d = fixNaN64(d);
		setDRn(op, d);
	}
}
//fdiv <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0011)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		fr[n] /= fr[m];

		CHECK_FPU_32(fr[n]);
	}
	else
	{
		double d = getDRn(op) / getDRm(op);
		d = fixNaN64(d);
		setDRn(op, d);
	}
}
//fcmp/eq <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0100)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		sr.T = fr[m] == fr[n];
	}
	else
	{
		sr.T = getDRn(op) == getDRm(op);
	}
}
//fcmp/gt <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0101)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		if (fr[n] > fr[m])
			sr.T = 1;
		else
			sr.T = 0;
	}
	else
	{
		sr.T = getDRn(op) > getDRm(op);
	}
}
//All memory opcodes are here
//fmov.s @(R0,<REG_M>),<FREG_N>
sh4op(i1111_nnnn_mmmm_0110)
{
	if (fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		fr_hex[n] = ReadMem32(r[m] + r[0]);
	}
	else
	{
		u32 n = GetN(op)>>1;
		u32 m = GetM(op);
		if (((op >> 8) & 1) == 0)
			dr_hex[n] = ReadMem64(r[m] + r[0]);
		else
			xd_hex[n] = ReadMem64(r[m] + r[0]);
	}
}


//fmov.s <FREG_M>,@(R0,<REG_N>)
sh4op(i1111_nnnn_mmmm_0111)
{
	if (fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		WriteMem32(r[0] + r[n], fr_hex[m]);
	}
	else
	{
		u32 n = GetN(op);
		u32 m = GetM(op)>>1;
		if (((op >> 4) & 0x1) == 0)
			WriteMem64(r[n] + r[0], dr_hex[m]);
		else
			WriteMem64(r[n] + r[0], xd_hex[m]);
	}
}


//fmov.s @<REG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_1000)
{
	if (fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		fr_hex[n] = ReadMem32(r[m]);
	}
	else
	{
		u32 n = GetN(op)>>1;
		u32 m = GetM(op);
		if (((op >> 8) & 1) == 0)
			dr_hex[n] = ReadMem64(r[m]);
		else
			xd_hex[n] = ReadMem64(r[m]);
	}
}


//fmov.s @<REG_M>+,<FREG_N>
sh4op(i1111_nnnn_mmmm_1001)
{
	if (fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		fr_hex[n] = ReadMem32(r[m]);
		r[m] += 4;
	}
	else
	{
		u32 n = GetN(op)>>1;
		u32 m = GetM(op);
		if (((op >> 8) & 1) == 0)
			dr_hex[n] = ReadMem64(r[m]);
		else
			xd_hex[n] = ReadMem64(r[m]);
		r[m] += 8;
	}
}


//fmov.s <FREG_M>,@<REG_N>
sh4op(i1111_nnnn_mmmm_1010)
{
	if (fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		WriteMem32(r[n], fr_hex[m]);
	}
	else
	{
		u32 n = GetN(op);
		u32 m = GetM(op)>>1;

		if (((op >> 4) & 0x1) == 0)
			WriteMem64(r[n], dr_hex[m]);
		else
			WriteMem64(r[n], xd_hex[m]);
	}
}

//fmov.s <FREG_M>,@-<REG_N>
sh4op(i1111_nnnn_mmmm_1011)
{
	if (fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		u32 addr = r[n] - 4;

		WriteMem32(addr, fr_hex[m]);

		r[n] = addr;
	}
	else
	{
		u32 n = GetN(op);
		u32 m = GetM(op)>>1;

		u32 addr = r[n] - 8;
		if (((op >> 4) & 0x1) == 0)
			WriteMem64(addr, dr_hex[m]);
		else
			WriteMem64(addr, xd_hex[m]);

		r[n] = addr;
	}
}

//end of memory opcodes

//fmov <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_1100)
{
	if (fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		fr[n] = fr[m];
	}
	else
	{
		u32 n = GetN(op)>>1;
		u32 m = GetM(op)>>1;
		switch ((op >> 4) & 0x11)
		{
			case 0x00:
				//dr[n] = dr[m];
				dr_hex[n] = dr_hex[m];
				break;

			case 0x01:
				//dr[n] = xd[m];
				dr_hex[n] = xd_hex[m];
				break;

			case 0x10:
				//xd[n] = dr[m];
				xd_hex[n] = dr_hex[m];
				break;

			case 0x11:
				//xd[n] = xd[m];
				xd_hex[n] = xd_hex[m];
				break;
		}
	}
}


//fabs <FREG_N>
sh4op(i1111_nnnn_0101_1101)
{
	int n=GetN(op);

	if (fpscr.PR ==0)
		fr_hex[n]&=0x7FFFFFFF;
	else
		fr_hex[(n&0xE)]&=0x7FFFFFFF;

}

//FSCA FPUL, DRn//F0FD//1111_nnn0_1111_1101
sh4op(i1111_nnn0_1111_1101)
{
	int n=GetN(op) & 0xE;


	//cosine(x) = sine(pi/2 + x).
	if (fpscr.PR==0)
	{
		u32 pi_index=fpul&0xFFFF;

	#ifdef NATIVE_FSCA
			float rads = pi_index / (65536.0f / 2) * float(M_PI);

			fr[n + 0] = sinf(rads);
			fr[n + 1] = cosf(rads);

			CHECK_FPU_32(fr[n]);
			CHECK_FPU_32(fr[n+1]);
	#else
			fr[n + 0] = sin_table[pi_index].u[0];
			fr[n + 1] = sin_table[pi_index].u[1];
	#endif

	}
	else
		iNimp("FSCA : Double precision mode");
}

//FSRRA //1111_nnnn_0111_1101
sh4op(i1111_nnnn_0111_1101)
{
	u32 n = GetN(op);
	if (fpscr.PR==0)
	{
		fr[n] = (float)(1/sqrtf(fr[n]));
		CHECK_FPU_32(fr[n]);
	}
	else
		iNimp("FSRRA : Double precision mode");
}

//fcnvds <DR_N>,FPUL
sh4op(i1111_nnnn_1011_1101)
{

	if (fpscr.PR == 1)
	{
		u32 *p = &fpul;
		*((float *)p) = (float)getDRn(op);
	}
	else
	{
		iNimp("FCNVDS: Single precision mode");
	}
}


//fcnvsd FPUL,<DR_N>
sh4op(i1111_nnnn_1010_1101)
{
	if (fpscr.PR == 1)
	{
		u32 *p = &fpul;
		setDRn(op, (double)*((float *)p));
	}
	else
	{
		iNimp("FCNVSD: Single precision mode");
	}
}

//fipr <FV_M>,<FV_N>
sh4op(i1111_nnmm_1110_1101)
{
	int n=GetN(op)&0xC;
	int m=(GetN(op)&0x3)<<2;
	if (fpscr.PR == 0)
	{
#if HOST_CPU == CPU_X86 || HOST_CPU == CPU_X64
		// multiplications are done with 28 bits of precision (53 - 25) and the final sum at 30 bits
		double idp = (double)fr[n + 0] * fr[m + 0];
		idp += (double)fr[n + 1] * fr[m + 1];
		idp += (double)fr[n + 2] * fr[m + 2];
		idp += (double)fr[n + 3] * fr[m + 3];

		fr[n + 3] = fixNaN((float)idp);
#else
		float rv = fr[n + 0] * fr[m + 0];
		rv += fr[n + 1] * fr[m + 1];
		rv += fr[n + 2] * fr[m + 2];
		rv += fr[n + 3] * fr[m + 3];

		CHECK_FPU_32(rv);
		fr[n + 3] = rv;
#endif
	}
	else
	{
		die("FIPR Precision=1");
	}
}

//fldi0 <FREG_N>
sh4op(i1111_nnnn_1000_1101)
{
	if (fpscr.PR!=0)
		return;

	u32 n = GetN(op);

	fr[n] = 0.0f;

}

//fldi1 <FREG_N>
sh4op(i1111_nnnn_1001_1101)
{
	if (fpscr.PR!=0)
		return;

	u32 n = GetN(op);

	fr[n] = 1.0f;
}

//flds <FREG_N>,FPUL
sh4op(i1111_nnnn_0001_1101)
{
	u32 n = GetN(op);

	fpul = fr_hex[n];
}

//fsts FPUL,<FREG_N>
sh4op(i1111_nnnn_0000_1101)
{
	u32 n = GetN(op);
	fr_hex[n] = fpul;
}

//float FPUL,<FREG_N>
sh4op(i1111_nnnn_0010_1101)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);
		fr[n] = (float)(int)fpul;
	}
	else
	{
		setDRn(op, (double)(int)fpul);
	}
}


//fneg <FREG_N>
sh4op(i1111_nnnn_0100_1101)
{
	u32 n = GetN(op);

	if (fpscr.PR ==0)
		fr_hex[n]^=0x80000000;
	else
		fr_hex[(n&0xE)]^=0x80000000;
}


//frchg
sh4op(i1111_1011_1111_1101)
{
 	fpscr.FR = 1 - fpscr.FR;

	UpdateFPSCR();
}

//fschg
sh4op(i1111_0011_1111_1101)
{
	fpscr.SZ = 1 - fpscr.SZ;
}

//fsqrt <FREG_N>
sh4op(i1111_nnnn_0110_1101)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);

		fr[n] = sqrtf(fr[n]);
		CHECK_FPU_32(fr[n]);
	}
	else
	{
		setDRn(op, fixNaN64(sqrt(getDRn(op))));
	}
}


//ftrc <FREG_N>, FPUL
sh4op(i1111_nnnn_0011_1101)
{
	if (fpscr.PR == 0)
	{
		u32 n = GetN(op);
		fpul = (u32)(s32)fr[n];

		if ((s32)fpul > 0x7fffff80)
			fpul = 0x7fffffff;
		// Intel CPUs convert out of range float numbers to 0x80000000. Manually set the correct sign
		else if (fpul == 0x80000000 && fr[n] == fr[n])
		{
			if (*(int *)&fr[n] > 0) // Using integer math to avoid issues with Inf and NaN
				fpul--;
		}
	}
	else
	{
		f64 f = getDRn(op);
		fpul = (u32)(s32)f;

		// TODO saturate
		// Intel CPUs convert out of range float numbers to 0x80000000. Manually set the correct sign
		if (fpul == 0x80000000 && f == f)
		{
			if (*(s64 *)&f > 0)     // Using integer math to avoid issues with Inf and NaN
				fpul--;
		}
	}
}


//fmac <FREG_0>,<FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_1110)
{
	if (fpscr.PR==0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		fr[n] =(f32) ((f64)fr[n]+(f64)fr[0] * (f64)fr[m]);
		CHECK_FPU_32(fr[n]);
	}
	else
	{
		iNimp("fmac <DREG_0>,<DREG_M>,<DREG_N>");
	}
}


//ftrv xmtrx,<FV_N>
sh4op(i1111_nn01_1111_1101)
{
	/*
	XF[0] XF[4] XF[8] XF[12]    FR[n]      FR[n]
	XF[1] XF[5] XF[9] XF[13]  *	FR[n+1] -> FR[n+1]
	XF[2] XF[6] XF[10] XF[14]   FR[n+2]    FR[n+2]
	XF[3] XF[7] XF[11] XF[15]   FR[n+3]    FR[n+3]
	*/

	u32 n=GetN(op)&0xC;

	if (fpscr.PR==0)
	{
#if HOST_CPU == CPU_X86 || HOST_CPU == CPU_X64
		double v1 = (double)xf[0]  * fr[n + 0] +
					(double)xf[4]  * fr[n + 1] +
					(double)xf[8]  * fr[n + 2] +
					(double)xf[12] * fr[n + 3];

		double v2 = (double)xf[1]  * fr[n + 0] +
					(double)xf[5]  * fr[n + 1] +
					(double)xf[9]  * fr[n + 2] +
					(double)xf[13] * fr[n + 3];

		double v3 = (double)xf[2]  * fr[n + 0] +
					(double)xf[6]  * fr[n + 1] +
					(double)xf[10] * fr[n + 2] +
					(double)xf[14] * fr[n + 3];

		double v4 = (double)xf[3]  * fr[n + 0] +
					(double)xf[7]  * fr[n + 1] +
					(double)xf[11] * fr[n + 2] +
					(double)xf[15] * fr[n + 3];

		fr[n + 0] = fixNaN((float)v1);
		fr[n + 1] = fixNaN((float)v2);
		fr[n + 2] = fixNaN((float)v3);
		fr[n + 3] = fixNaN((float)v4);
#else
		float v1, v2, v3, v4;

		v1 = xf[0]  * fr[n + 0] +
			 xf[4]  * fr[n + 1] +
			 xf[8]  * fr[n + 2] +
			 xf[12] * fr[n + 3];

		v2 = xf[1]  * fr[n + 0] +
			 xf[5]  * fr[n + 1] +
			 xf[9]  * fr[n + 2] +
			 xf[13] * fr[n + 3];

		v3 = xf[2]  * fr[n + 0] +
			 xf[6]  * fr[n + 1] +
			 xf[10] * fr[n + 2] +
			 xf[14] * fr[n + 3];

		v4 = xf[3]  * fr[n + 0] +
			 xf[7]  * fr[n + 1] +
			 xf[11] * fr[n + 2] +
			 xf[15] * fr[n + 3];

		CHECK_FPU_32(v1);
		CHECK_FPU_32(v2);
		CHECK_FPU_32(v3);
		CHECK_FPU_32(v4);

		fr[n + 0] = v1;
		fr[n + 1] = v2;
		fr[n + 2] = v3;
		fr[n + 3] = v4;
#endif
	}
	else
	{
		iNimp("FTRV in dp mode");
	}
}

static void iNimp(const char *str)
{
	WARN_LOG(INTERPRETER, "Unimplemented sh4 FPU instruction: %s", str);
}
