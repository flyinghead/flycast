#include "types.h"
#include <cmath>

#include "sh4_opcodes.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_rom.h"
#include "hw/sh4/sh4_mem.h"

static u32 GetN(u32 op) {
	return (op >> 8) & 0xf;
}
static u32 GetM(u32 op) {
	return (op >> 4) & 0xf;
}

static double getDRn(Sh4Context *ctx, u32 op) {
	return ctx->getDR((op >> 9) & 7);
}
static double getDRm(Sh4Context *ctx, u32 op) {
	return ctx->getDR((op >> 5) & 7);
}
static void setDRn(Sh4Context *ctx, u32 op, double d) {
	ctx->setDR((op >> 9) & 7, d);
}

static void iNimp(const char *str);

#define CHECK_FPU_32(v) v = fixNaN(v)

//fadd <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0000)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		ctx->fr[n] += ctx->fr[m];
		CHECK_FPU_32(ctx->fr[n]);
	}
	else
	{
		double d = getDRn(ctx, op) + getDRm(ctx, op);
		d = fixNaN64(d);
		setDRn(ctx, op, d);
	}
}

//fsub <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0001)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		ctx->fr[n] -= ctx->fr[m];
		CHECK_FPU_32(ctx->fr[n]);
	}
	else
	{
		double d = getDRn(ctx, op) - getDRm(ctx, op);
		d = fixNaN64(d);
		setDRn(ctx, op, d);
	}
}
//fmul <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0010)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		ctx->fr[n] *= ctx->fr[m];
		CHECK_FPU_32(ctx->fr[n]);
	}
	else
	{
		double d = getDRn(ctx, op) * getDRm(ctx, op);
		d = fixNaN64(d);
		setDRn(ctx, op, d);
	}
}
//fdiv <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0011)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		ctx->fr[n] /= ctx->fr[m];

		CHECK_FPU_32(ctx->fr[n]);
	}
	else
	{
		double d = getDRn(ctx, op) / getDRm(ctx, op);
		d = fixNaN64(d);
		setDRn(ctx, op, d);
	}
}
//fcmp/eq <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0100)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		ctx->sr.T = ctx->fr[m] == ctx->fr[n];
	}
	else
	{
		ctx->sr.T = getDRn(ctx, op) == getDRm(ctx, op);
	}
}
//fcmp/gt <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_0101)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		if (ctx->fr[n] > ctx->fr[m])
			ctx->sr.T = 1;
		else
			ctx->sr.T = 0;
	}
	else
	{
		ctx->sr.T = getDRn(ctx, op) > getDRm(ctx, op);
	}
}
//All memory opcodes are here
//fmov.s @(R0,<REG_M>),<FREG_N>
sh4op(i1111_nnnn_mmmm_0110)
{
	if (ctx->fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		ctx->fr_hex(n) = ReadMem32(ctx->r[m] + ctx->r[0]);
	}
	else
	{
		u32 n = GetN(op)>>1;
		u32 m = GetM(op);
		if (((op >> 8) & 1) == 0)
			ctx->dr_hex(n) = ReadMem64(ctx->r[m] + ctx->r[0]);
		else
			ctx->xd_hex(n) = ReadMem64(ctx->r[m] + ctx->r[0]);
	}
}


//fmov.s <FREG_M>,@(R0,<REG_N>)
sh4op(i1111_nnnn_mmmm_0111)
{
	if (ctx->fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		WriteMem32(ctx->r[0] + ctx->r[n], ctx->fr_hex(m));
	}
	else
	{
		u32 n = GetN(op);
		u32 m = GetM(op)>>1;
		if (((op >> 4) & 0x1) == 0)
			WriteMem64(ctx->r[n] + ctx->r[0], ctx->dr_hex(m));
		else
			WriteMem64(ctx->r[n] + ctx->r[0], ctx->xd_hex(m));
	}
}


//fmov.s @<REG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_1000)
{
	if (ctx->fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		ctx->fr_hex(n) = ReadMem32(ctx->r[m]);
	}
	else
	{
		u32 n = GetN(op)>>1;
		u32 m = GetM(op);
		if (((op >> 8) & 1) == 0)
			ctx->dr_hex(n) = ReadMem64(ctx->r[m]);
		else
			ctx->xd_hex(n) = ReadMem64(ctx->r[m]);
	}
}


//fmov.s @<REG_M>+,<FREG_N>
sh4op(i1111_nnnn_mmmm_1001)
{
	if (ctx->fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		ctx->fr_hex(n) = ReadMem32(ctx->r[m]);
		ctx->r[m] += 4;
	}
	else
	{
		u32 n = GetN(op)>>1;
		u32 m = GetM(op);
		if (((op >> 8) & 1) == 0)
			ctx->dr_hex(n) = ReadMem64(ctx->r[m]);
		else
			ctx->xd_hex(n) = ReadMem64(ctx->r[m]);
		ctx->r[m] += 8;
	}
}


//fmov.s <FREG_M>,@<REG_N>
sh4op(i1111_nnnn_mmmm_1010)
{
	if (ctx->fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		WriteMem32(ctx->r[n], ctx->fr_hex(m));
	}
	else
	{
		u32 n = GetN(op);
		u32 m = GetM(op)>>1;

		if (((op >> 4) & 0x1) == 0)
			WriteMem64(ctx->r[n], ctx->dr_hex(m));
		else
			WriteMem64(ctx->r[n], ctx->xd_hex(m));
	}
}

//fmov.s <FREG_M>,@-<REG_N>
sh4op(i1111_nnnn_mmmm_1011)
{
	if (ctx->fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		u32 addr = ctx->r[n] - 4;

		WriteMem32(addr, ctx->fr_hex(m));

		ctx->r[n] = addr;
	}
	else
	{
		u32 n = GetN(op);
		u32 m = GetM(op)>>1;

		u32 addr = ctx->r[n] - 8;
		if (((op >> 4) & 0x1) == 0)
			WriteMem64(addr, ctx->dr_hex(m));
		else
			WriteMem64(addr, ctx->xd_hex(m));

		ctx->r[n] = addr;
	}
}

//end of memory opcodes

//fmov <FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_1100)
{
	if (ctx->fpscr.SZ == 0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);
		ctx->fr[n] = ctx->fr[m];
	}
	else
	{
		u32 n = GetN(op)>>1;
		u32 m = GetM(op)>>1;
		switch ((op >> 4) & 0x11)
		{
			case 0x00:
				//dr[n] = dr[m];
				ctx->dr_hex(n) = ctx->dr_hex(m);
				break;

			case 0x01:
				//dr[n] = xd[m];
				ctx->dr_hex(n) = ctx->xd_hex(m);
				break;

			case 0x10:
				//xd[n] = dr[m];
				ctx->xd_hex(n) = ctx->dr_hex(m);
				break;

			case 0x11:
				//xd[n] = xd[m];
				ctx->xd_hex(n) = ctx->xd_hex(m);
				break;
		}
	}
}


//fabs <FREG_N>
sh4op(i1111_nnnn_0101_1101)
{
	int n=GetN(op);

	if (ctx->fpscr.PR == 0)
		ctx->fr_hex(n) &= 0x7FFFFFFF;
	else
		ctx->fr_hex(n & 0xE) &= 0x7FFFFFFF;

}

//FSCA FPUL, DRn//F0FD//1111_nnn0_1111_1101
sh4op(i1111_nnn0_1111_1101)
{
	int n=GetN(op) & 0xE;


	//cosine(x) = sine(pi/2 + x).
	if (ctx->fpscr.PR==0)
	{
		u32 pi_index = ctx->fpul & 0xFFFF;

	#ifdef NATIVE_FSCA
			float rads = pi_index / (65536.0f / 2) * float(M_PI);

			ctx->fr[n + 0] = sinf(rads);
			ctx->fr[n + 1] = cosf(rads);

			CHECK_FPU_32(ctx->fr[n]);
			CHECK_FPU_32(ctx->fr[n + 1]);
	#else
			ctx->fr[n + 0] = sin_table[pi_index].u[0];
			ctx->fr[n + 1] = sin_table[pi_index].u[1];
	#endif

	}
	else
		iNimp("FSCA : Double precision mode");
}

//FSRRA //1111_nnnn_0111_1101
sh4op(i1111_nnnn_0111_1101)
{
	u32 n = GetN(op);
	if (ctx->fpscr.PR==0)
	{
		ctx->fr[n] = 1.f / sqrtf(ctx->fr[n]);
		CHECK_FPU_32(ctx->fr[n]);
	}
	else
		iNimp("FSRRA : Double precision mode");
}

//fcnvds <DR_N>,FPUL
sh4op(i1111_nnnn_1011_1101)
{

	if (ctx->fpscr.PR == 1)
	{
		u32 *p = &ctx->fpul;
		*((float *)p) = (float)getDRn(ctx, op);
	}
	else
	{
		iNimp("FCNVDS: Single precision mode");
	}
}


//fcnvsd FPUL,<DR_N>
sh4op(i1111_nnnn_1010_1101)
{
	if (ctx->fpscr.PR == 1)
	{
		u32 *p = &ctx->fpul;
		setDRn(ctx, op, (double)*((float *)p));
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
	if (ctx->fpscr.PR == 0)
	{
		double idp = (double)ctx->fr[n + 0] * ctx->fr[m + 0];
		idp += (double)ctx->fr[n + 1] * ctx->fr[m + 1];
		idp += (double)ctx->fr[n + 2] * ctx->fr[m + 2];
		idp += (double)ctx->fr[n + 3] * ctx->fr[m + 3];

		ctx->fr[n + 3] = fixNaN((float)idp);
	}
	else
	{
		die("FIPR Precision=1");
	}
}

//fldi0 <FREG_N>
sh4op(i1111_nnnn_1000_1101)
{
	if (ctx->fpscr.PR!=0)
		return;

	u32 n = GetN(op);

	ctx->fr[n] = 0.0f;

}

//fldi1 <FREG_N>
sh4op(i1111_nnnn_1001_1101)
{
	if (ctx->fpscr.PR!=0)
		return;

	u32 n = GetN(op);

	ctx->fr[n] = 1.0f;
}

//flds <FREG_N>,FPUL
sh4op(i1111_nnnn_0001_1101)
{
	u32 n = GetN(op);

	ctx->fpul = ctx->fr_hex(n);
}

//fsts FPUL,<FREG_N>
sh4op(i1111_nnnn_0000_1101)
{
	u32 n = GetN(op);
	ctx->fr_hex(n) = ctx->fpul;
}

//float FPUL,<FREG_N>
sh4op(i1111_nnnn_0010_1101)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);
		ctx->fr[n] = (float)(int)ctx->fpul;
	}
	else
	{
		setDRn(ctx, op, (double)(int)ctx->fpul);
	}
}


//fneg <FREG_N>
sh4op(i1111_nnnn_0100_1101)
{
	u32 n = GetN(op);

	if (ctx->fpscr.PR == 0)
		ctx->fr_hex(n) ^= 0x80000000;
	else
		ctx->fr_hex(n & 0xE) ^= 0x80000000;
}


//frchg
sh4op(i1111_1011_1111_1101)
{
 	ctx->fpscr.FR = 1 - ctx->fpscr.FR;

	Sh4Context::UpdateFPSCR(ctx);
}

//fschg
sh4op(i1111_0011_1111_1101)
{
	ctx->fpscr.SZ = 1 - ctx->fpscr.SZ;
}

//fsqrt <FREG_N>
sh4op(i1111_nnnn_0110_1101)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);

		ctx->fr[n] = sqrtf(ctx->fr[n]);
		CHECK_FPU_32(ctx->fr[n]);
	}
	else
	{
		setDRn(ctx, op, fixNaN64(sqrt(getDRn(ctx, op))));
	}
}


//ftrc <FREG_N>, FPUL
sh4op(i1111_nnnn_0011_1101)
{
	if (ctx->fpscr.PR == 0)
	{
		u32 n = GetN(op);
		if (std::isnan(ctx->fr[n])) {
			ctx->fpul = 0x80000000;
		}
		else
		{
			ctx->fpul = (u32)(s32)ctx->fr[n];
			if ((s32)ctx->fpul > 0x7fffff80)
				ctx->fpul = 0x7fffffff;
#if HOST_CPU == CPU_X86 || HOST_CPU == CPU_X64
			// Intel CPUs convert out of range float numbers to 0x80000000. Manually set the correct sign
			else if (ctx->fpul == 0x80000000 && ctx->fr[n] > 0)
				ctx->fpul--;
#endif
		}
	}
	else
	{
		f64 f = getDRn(ctx, op);
		if (std::isnan(f)) {
			ctx->fpul = 0x80000000;
		}
		else
		{
			ctx->fpul = (u32)(s32)f;
#if HOST_CPU == CPU_X86 || HOST_CPU == CPU_X64
			// Intel CPUs convert out of range float numbers to 0x80000000. Manually set the correct sign
			if (ctx->fpul == 0x80000000 && f > 0)
				ctx->fpul--;
#endif
		}
	}
}


//fmac <FREG_0>,<FREG_M>,<FREG_N>
sh4op(i1111_nnnn_mmmm_1110)
{
	if (ctx->fpscr.PR==0)
	{
		u32 n = GetN(op);
		u32 m = GetM(op);

		ctx->fr[n] = std::fma(ctx->fr[0], ctx->fr[m], ctx->fr[n]);
		CHECK_FPU_32(ctx->fr[n]);
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

	if (ctx->fpscr.PR==0)
	{
		double v1 = (double)ctx->xf[0]  * ctx->fr[n + 0] +
					(double)ctx->xf[4]  * ctx->fr[n + 1] +
					(double)ctx->xf[8]  * ctx->fr[n + 2] +
					(double)ctx->xf[12] * ctx->fr[n + 3];

		double v2 = (double)ctx->xf[1]  * ctx->fr[n + 0] +
					(double)ctx->xf[5]  * ctx->fr[n + 1] +
					(double)ctx->xf[9]  * ctx->fr[n + 2] +
					(double)ctx->xf[13] * ctx->fr[n + 3];

		double v3 = (double)ctx->xf[2]  * ctx->fr[n + 0] +
					(double)ctx->xf[6]  * ctx->fr[n + 1] +
					(double)ctx->xf[10] * ctx->fr[n + 2] +
					(double)ctx->xf[14] * ctx->fr[n + 3];

		double v4 = (double)ctx->xf[3]  * ctx->fr[n + 0] +
					(double)ctx->xf[7]  * ctx->fr[n + 1] +
					(double)ctx->xf[11] * ctx->fr[n + 2] +
					(double)ctx->xf[15] * ctx->fr[n + 3];

		ctx->fr[n + 0] = fixNaN((float)v1);
		ctx->fr[n + 1] = fixNaN((float)v2);
		ctx->fr[n + 2] = fixNaN((float)v3);
		ctx->fr[n + 3] = fixNaN((float)v4);
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
