/*

	This is a header file that can create 
	SHIL_MODE == 0) Shil opcode enums
	SHIL_MODE == 1) Shil opcode classes/portable C implementation ("canonical" implementation)
	SHIL_MODE == 2) Shil opcode classes declaration
	SHIL_MODE == 3) The routing table for canonical implementations
	SHIL_MODE == 4) opcode name list (for logging/disass)
*/

#define fsca_impl fsca_table

#if SHIL_MODE==0
//generate enums ..
	#define SHIL_START enum shilop {
	#define SHIL_END shop_max, };

	#define shil_opc(name) shop_##name,
	#define shil_opc_end()

	#define shil_canonical(rv,name,args,code)
	#define shil_compile(code)
#elif  SHIL_MODE==1
#include "hw/sh4/sh4_interrupts.h"
	//generate structs ...
	#define SHIL_START
	#define SHIL_END

	#define shil_opc(name) struct shil_opcl_##name { 
	#define shil_opc_end() };

	#define shil_canonical(rv,name,args,code) struct name { static rv impl args { code } };
	
	#define shil_cf_arg_u32(x) sh4Dynarec->canonParam(op, &op->x, CPT_u32);
	#define shil_cf_arg_f32(x) sh4Dynarec->canonParam(op, &op->x, CPT_f32);
	#define shil_cf_arg_ptr(x) sh4Dynarec->canonParam(op, &op->x, CPT_ptr);
	#define shil_cf_arg_sh4ctx() sh4Dynarec->canonParam(op, nullptr, CPT_sh4ctx);
	#define shil_cf_rv_u32(x) sh4Dynarec->canonParam(op, &op->x, CPT_u32rv);
	#define shil_cf_rv_f32(x) sh4Dynarec->canonParam(op, &op->x, CPT_f32rv);
	#define shil_cf_rv_u64(x) sh4Dynarec->canonParam(op, &op->rd, CPT_u64rvL); sh4Dynarec->canonParam(op, &op->rd2, CPT_u64rvH);
	#define shil_cf(x) sh4Dynarec->canonCall(op, (void *)&x::impl);

	#define shil_compile(code) static void compile(shil_opcode* op) { sh4Dynarec->canonStart(op); code sh4Dynarec->canonFinish(op); }
#elif  SHIL_MODE==2
	//generate struct declarations ...
	#define SHIL_START
	#define SHIL_END

	#define shil_opc(name) struct shil_opcl_##name { 
	#define shil_opc_end() };

	#define shil_canonical(rv,name,args,code) struct name { static rv impl args; };
	#define shil_compile(code) static void compile(shil_opcode* op);
#elif  SHIL_MODE==3
	//generate struct list ...
	

	#define SHIL_START \
	shil_chfp* shil_chf[] = {

	#define SHIL_END };

	#define shil_opc(name) &shil_opcl_##name::compile,
	#define shil_opc_end()

	#define shil_canonical(rv,name,args,code)
	#define shil_compile(code)
#elif SHIL_MODE==4
//generate name strings ..
	#define SHIL_START const char* shilop_str[]={
	#define SHIL_END };

	#define shil_opc(name) #name,
	#define shil_opc_end()

	#define shil_canonical(rv,name,args,code)
	#define shil_compile(code)
#else
#error Invalid SHIL_MODE
#endif



#if SHIL_MODE==1 || SHIL_MODE==2
//only in structs we use the code :)
#include <cmath>
#include "types.h"
#include "shil.h"
#include "decoder.h"
#include "../sh4_rom.h"

#define BIN_OP_I_BASE(code,type,rtype) \
shil_canonical \
( \
rtype,f1,(type r1,type r2), \
	code \
) \
 \
shil_compile \
( \
	shil_cf_arg_##type(rs2); \
	shil_cf_arg_##type(rs1); \
	shil_cf(f1); \
	shil_cf_rv_##rtype(rd); \
)

#define UN_OP_I_BASE(code,type) \
shil_canonical \
( \
type,f1,(type r1), \
	code \
) \
 \
shil_compile \
( \
	shil_cf_arg_##type(rs1); \
	shil_cf(f1); \
	shil_cf_rv_##type(rd); \
)


#define BIN_OP_I(z) BIN_OP_I_BASE( return r1 z r2; ,u32,u32)

#define BIN_OP_I2(tp,z) BIN_OP_I_BASE( return ((tp) r1) z ((tp) r2); ,u32,u32)
#define BIN_OP_I3(z,w) BIN_OP_I_BASE( return (r1 z r2) w; ,u32,u32)
#define BIN_OP_I4(tp,z,rt,pt) BIN_OP_I_BASE( return ((tp)(pt)r1) z ((tp)(pt)r2); ,u32,rt)

#define BIN_OP_F(z) BIN_OP_I_BASE( return fixNaN(r1 z r2); ,f32,f32)
#define BIN_OP_FU(z) BIN_OP_I_BASE( return fixNaN(r1 z r2); ,f32,u32)

#define UN_OP_I(z) UN_OP_I_BASE( return z (r1); ,u32)
#define UN_OP_F(z) UN_OP_I_BASE( return z (r1); ,f32)

#define shil_recimp() \
shil_compile( \
	die("This opcode requires native dynarec implementation"); \
)

#if SHIL_MODE==1

template<int Stride = 1>
static inline float innerProduct(const float *f1, const float *f2)
{
	const double f = (double)f1[0] * f2[Stride * 0]
				   + (double)f1[1] * f2[Stride * 1]
				   + (double)f1[2] * f2[Stride * 2]
				   + (double)f1[3] * f2[Stride * 3];
	return fixNaN((float)f);
}

#endif

#else

#define BIN_OP_I(z)

#define BIN_OP_I2(tp,z)
#define BIN_OP_I3(z,w)
#define BIN_OP_I4(tp,z,rt,k)
	
#define BIN_OP_F(z)
#define BIN_OP_FU(z)

#define UN_OP_I(z)
#define UN_OP_F(z)
#define shil_recimp()
#endif



SHIL_START


//shop_mov32
shil_opc(mov32)
shil_recimp()
shil_opc_end()

//shop_mov64
shil_opc(mov64)
shil_recimp()
shil_opc_end()

//Special opcodes
shil_opc(jdyn)
shil_recimp()
shil_opc_end()

shil_opc(jcond)
shil_recimp()
shil_opc_end()

//shop_ifb
shil_opc(ifb)
shil_recimp()
shil_opc_end()

//mem io
shil_opc(readm)	
shil_recimp()
shil_opc_end()

shil_opc(writem)
shil_recimp()
shil_opc_end()

//Canonical impl. opcodes !
shil_opc(sync_sr)
shil_canonical
(
void, f1, (),
	UpdateSR();
)
shil_compile
(
	shil_cf(f1);
)
shil_opc_end()

shil_opc(sync_fpscr)
shil_canonical
(
void, f1, (Sh4Context *ctx),
	Sh4Context::UpdateFPSCR(ctx);
)
shil_compile
(
	shil_cf_arg_sh4ctx();
	shil_cf(f1);
)
shil_opc_end()

//shop_and
shil_opc(and)
BIN_OP_I(&)
shil_opc_end()

//shop_or
shil_opc(or)
BIN_OP_I(|)
shil_opc_end()

//shop_xor
shil_opc(xor)
BIN_OP_I(^)
shil_opc_end()

//shop_not
shil_opc(not)
UN_OP_I(~)
shil_opc_end()

//shop_add
shil_opc(add)
BIN_OP_I(+)
shil_opc_end()

//shop_sub
shil_opc(sub)
BIN_OP_I(-)
shil_opc_end()

//shop_neg
shil_opc(neg)
UN_OP_I(-)
shil_opc_end()

//shop_shl,
shil_opc(shl)
BIN_OP_I2(u32,<<)
shil_opc_end()

//shop_shr
shil_opc(shr)
BIN_OP_I2(u32,>>)
shil_opc_end()

//shop_sar
shil_opc(sar)
BIN_OP_I2(s32,>>)
shil_opc_end()

//shop_adc	//add with carry
shil_opc(adc)
shil_canonical
(
u64,f1,(u32 r1,u32 r2,u32 C),
	u64 res=(u64)r1+r2+C;

	u64 rv;
	((u32*)&rv)[0]=res;
	((u32*)&rv)[1]=res>>32;

	return rv;
)

shil_compile
(
	shil_cf_arg_u32(rs3);
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u64(rd);
)

shil_opc_end()


//shop_sbc	// substract with carry
shil_opc(sbc)
shil_canonical
(
u64,f1,(u32 r1,u32 r2,u32 C),
	u64 res=(u64)r1-r2-C;

	u64 rv;
	((u32*)&rv)[0]=res;
	((u32*)&rv)[1]=(res>>32)&1; //alternatively: res>>63

	return rv;
)

shil_compile
(
	shil_cf_arg_u32(rs3);
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u64(rd);
)
shil_opc_end()

//shop_negc - Negate with carry
shil_opc(negc)
shil_canonical
(
u64,f1,(u32 r1, u32 C),
	u64 res = -(u64)r1 - C;

	u64 rv;
	((u32*)&rv)[0]=res;
	((u32*)&rv)[1]=(res>>32)&1;

	return rv;
)

shil_compile
(
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u64(rd);
)
shil_opc_end()

//shop_ror
shil_opc(ror)
shil_canonical
(
u32,f1,(u32 r1,u32 amt),
	return (r1>>amt)|(r1<<(32-amt));
)

shil_compile
(
 	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)

shil_opc_end()


//shop_rocl
shil_opc(rocl)
shil_canonical
(
u64,f1,(u32 r1,u32 r2),
	u64 rv;
	((u32*)&rv)[0]=(r1<<1)|r2;
	((u32*)&rv)[1]=r1>>31;
	return rv;
)

shil_compile
(
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u64(rd);
)

shil_opc_end()

//shop_rocr
shil_opc(rocr)
shil_canonical
(
u64,f1,(u32 r1,u32 r2),
	u64 rv;
	((u32*)&rv)[0]=(r1>>1)|(r2<<31);
	((u32*)&rv)[1]=r1&1;
	return rv;
)

shil_compile
(
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u64(rd);
)

shil_opc_end()

//shop_swaplb -- swap low bytes
shil_opc(swaplb)
shil_canonical
(
u32,f1,(u32 r1),
	return (r1 & 0xFFFF0000) | ((r1&0xFF)<<8) | ((r1>>8)&0xFF);
)

shil_compile
(
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)

shil_opc_end()

//shop_shld
shil_opc(shld)
shil_canonical
(
u32,f1,(u32 r1,u32 r2),
	u32 sgn = r2 & 0x80000000;
	if (sgn == 0)
		return r1 << (r2 & 0x1F);
	else if ((r2 & 0x1F) == 0)
	{
		return 0;
	}
	else
		return r1 >> ((~r2 & 0x1F) + 1);
)

shil_compile
(
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)
shil_opc_end()

//shop_shad
shil_opc(shad)
shil_canonical
(
u32,f1,(s32 r1,u32 r2),
	u32 sgn = r2 & 0x80000000;
	if (sgn == 0)
		return r1 << (r2 & 0x1F);
	else if ((r2 & 0x1F) == 0)
	{
		return r1>>31;
	}
	else
		return r1 >> ((~r2 & 0x1F) + 1);
)

shil_compile
(
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)

shil_opc_end()

//shop_ext_s8
shil_opc(ext_s8)
shil_canonical
(
u32,f1,(u32 r1),
	return (s8)r1;
)

shil_compile
(
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)

shil_opc_end()


//shop_ext_s16
shil_opc(ext_s16)
shil_canonical
(
u32,f1,(u32 r1),
	return (s16)r1;
)

shil_compile
(
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)

shil_opc_end()

//shop_mul_u16
shil_opc(mul_u16)
BIN_OP_I4(u16,*,u32,u32)
shil_opc_end()

//shop_mul_s16
shil_opc(mul_s16)
BIN_OP_I4(s16,*,u32,u32)
shil_opc_end()

//no difference between signed and unsigned when only the lower
//32 bis are used !
//shop_mul_i32
shil_opc(mul_i32)
BIN_OP_I4(s32,*,u32,u32)
shil_opc_end()

//shop_mul_u64
shil_opc(mul_u64)
BIN_OP_I4(u64,*,u64,u32)
shil_opc_end()

//shop_mul_s64
shil_opc(mul_s64)
BIN_OP_I4(s64,*,u64,s32)
shil_opc_end()

//shop_div32u	//divide 32 bits, unsigned
shil_opc(div32u)
shil_canonical
(
u64,f1,(u32 r1, u32 r2, u32 r3),
	u64 dividend = ((u64)r3 << 32) | r1;
	u32 quo;
	u32 rem;
	if (r2)
	{
		quo = dividend / r2;
		rem = dividend % r2;
	}
	else
	{
		quo = 0;
		rem = dividend;
	}

	u64 rv;
	((u32*)&rv)[0]=quo;
	((u32*)&rv)[1]=rem;
	return rv;
)

shil_compile
(
	shil_cf_arg_u32(rs3);
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u64(rd);
)

shil_opc_end()

//shop_div32s	//divide 32 bits, signed
shil_opc(div32s)
shil_canonical
(
u64,f1,(u32 r1, s32 r2, s32 r3),
	s64 dividend = ((s64)r3 << 32) | r1;
	// 1's complement -> 2's complement
	if (dividend < 0)
		dividend++;

	s32 quo = (s32)(r2 ? dividend / r2 : 0);
	s32 rem = dividend - quo * r2;
	u32 negative = (r3 ^ r2) & 0x80000000;
	// 2's complement -> 1's complement
	if (negative)
		quo--;
	else if (r3 < 0)
		rem--;

	u64 rv;
	((u32*)&rv)[0]=quo;
	((u32*)&rv)[1]=rem;
	return rv;
)

shil_compile
(
	shil_cf_arg_u32(rs3);
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u64(rd);
)

shil_opc_end()

//shop_div32p2	//div32, fixup step (part 2)
shil_opc(div32p2)
shil_canonical
(
u32,f1,(s32 a,s32 b,u32 T),
	// the sign of the quotient is stored in bit 31 of T
	if (!(T & 0x80000000))
	{
		if (!(T & 1))
			a -= b;
	}
	else
	{
		// 2's complement -> 1's complement
		if (b > 0)
			a--;
		if (T & 1)
			a += b;
	}

	return a;
)

shil_compile
(
	shil_cf_arg_u32(rs3);
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)

shil_opc_end()

//shop_div1
shil_opc(div1)
shil_canonical
(
u64,f1,(u32 a, s32 b, u32 T, Sh4Context *ctx),
	sr_t& sr = ctx->sr;
	bool qxm = sr.Q ^ sr.M;
	sr.Q = (int)a < 0;
	a = (a << 1) | T;

	u32 oldA = a;
	a += (qxm ? 1 : -1) * b; 	// b if sr.Q != sr.M, -b otherwise
	sr.Q ^= sr.M ^ (qxm ? a < oldA : a > oldA);
	T = !(sr.Q ^ sr.M);

	return a | ((u64)T << 32);
)
shil_compile
(
	shil_cf_arg_sh4ctx();
	shil_cf_arg_u32(rs3);
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u64(rd);
)
shil_opc_end()

//debug_3
shil_opc(debug_3)
shil_canonical
(
void,f1,(u32 r1,u32 r2,u32 r3),
	INFO_LOG(DYNAREC, "debug_3: %08X, %08X, %08X", r1, r2, r3);
)

shil_compile
(
	shil_cf_arg_u32(rs3);
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
)

shil_opc_end()

//debug_1
shil_opc(debug_1)
shil_canonical
(
void,f1,(u32 r1),
	INFO_LOG(DYNAREC, "debug_1: %08X", r1);
)

shil_compile
(
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
)

shil_opc_end()

//shop_cvt_f2i_t	//float to integer : truncate
shil_opc(cvt_f2i_t)

#if HOST_CPU == CPU_X86 || HOST_CPU == CPU_X64
shil_canonical
(
u32,f1,(f32 f1),
	s32 res;
	if (f1 > 2147483520.0f) { // IEEE 754: 0x4effffff
		res = 0x7fffffff;
	}
	else {
		res = (s32)f1;
		// Fix result sign for Intel CPUs
		if ((u32)res == 0x80000000 && f1 > 0)
			res = 0x7fffffff;
	}
	return res;
)
#elif HOST_CPU == CPU_ARM || HOST_CPU == CPU_ARM64
shil_canonical
(
u32,f1,(f32 f1),
	s32 res;
	if (f1 > 2147483520.0f) { // IEEE 754: 0x4effffff
		res = 0x7fffffff;
	}
	else {
		res = (s32)f1;
		// conversion of NaN returns 0 on ARM
		if (std::isnan(f1))
			res = 0x80000000;
	}
	return res;
)
#endif

shil_compile
(
	shil_cf_arg_f32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)

shil_opc_end()

//shop_cvt_i2f_n	//integer to float : nearest
shil_opc(cvt_i2f_n)
shil_canonical
(
f32,f1,(u32 r1),
	return (float)(s32)r1;
)

shil_compile
(
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_f32(rd);
)

shil_opc_end()

//shop_cvt_i2f_z	//integer to float : round to zero
shil_opc(cvt_i2f_z)
shil_canonical
(
f32,f1,(u32 r1),
	return (float)(s32)r1;
)

shil_compile
(
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_f32(rd);
)

shil_opc_end()


//pref !
shil_opc(pref)
shil_canonical
(
void,f1,(u32 r1, Sh4Context *ctx),
	if ((r1 >> 26) == 0x38) ctx->doSqWrite(r1, ctx);
)

shil_compile
(
	shil_cf_arg_sh4ctx();
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
)

shil_opc_end()

//shop_test
shil_opc(test)
BIN_OP_I3(&,== 0)
shil_opc_end()

//shop_seteq	//equal
shil_opc(seteq)
BIN_OP_I2(s32,==)
shil_opc_end()

//shop_setge	//>=, signed (greater equal)
shil_opc(setge)
BIN_OP_I2(s32,>=)
shil_opc_end()

//shop_setgt //>, signed	 (greater than)
shil_opc(setgt)
BIN_OP_I2(s32,>)
shil_opc_end()

//shop_setae	//>=, unsigned (above equal)
shil_opc(setae)
BIN_OP_I2(u32,>=)
shil_opc_end()

//shop_setab	//>, unsigned (above)
shil_opc(setab)
BIN_OP_I2(u32,>)
shil_opc_end()

//shop_setpeq //set if any pair of bytes is equal
shil_opc(setpeq)
shil_canonical
(
u32,f1,(u32 r1,u32 r2),
	u32 temp = r1 ^ r2;

	if ( (temp&0xFF000000) && (temp&0x00FF0000) && (temp&0x0000FF00) && (temp&0x000000FF) )
		return 0;
	else
		return 1;		
)

shil_compile
(
 	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)

shil_opc_end()

//here come the moving points

//shop_fadd
shil_opc(fadd)
BIN_OP_F(+)
shil_opc_end()

//shop_fsub
shil_opc(fsub)
BIN_OP_F(-)
shil_opc_end()

//shop_fmul
shil_opc(fmul)
BIN_OP_F(*)
shil_opc_end()

//shop_fdiv
shil_opc(fdiv)
BIN_OP_F(/)
shil_opc_end()

//shop_fabs
shil_opc(fabs)
UN_OP_F(fabsf)
shil_opc_end()

//shop_fneg
shil_opc(fneg)
UN_OP_F(-)
shil_opc_end()

//shop_fsqrt
shil_opc(fsqrt)
UN_OP_F(sqrtf)
shil_opc_end()

//shop_fipr
shil_opc(fipr)

shil_canonical
(
f32,f1,(const float* fn, const float* fm),

	return innerProduct(fn, fm);
)

shil_compile
(
	shil_cf_arg_ptr(rs2);
	shil_cf_arg_ptr(rs1);
	shil_cf(f1);
	shil_cf_rv_f32(rd);
)
shil_opc_end()

//shop_ftrv
shil_opc(ftrv)
shil_canonical
(
void,f1,(float *fd, const float *fn, const float *fm),

	float v1 = innerProduct<4>(fn, fm);
	float v2 = innerProduct<4>(fn, fm + 1);
	float v3 = innerProduct<4>(fn, fm + 2);
	float v4 = innerProduct<4>(fn, fm + 3);
	fd[0] = v1;
	fd[1] = v2;
	fd[2] = v3;
	fd[3] = v4;
)

shil_compile
(
	shil_cf_arg_ptr(rs2);
	shil_cf_arg_ptr(rs1);
	shil_cf_arg_ptr(rd);
	shil_cf(f1);
)
shil_opc_end()

//shop_fmac
shil_opc(fmac)
shil_canonical
(
f32,f1,(float fn, float f0,float fm),
	return fixNaN(std::fma(f0, fm, fn));
)
shil_compile
(
	shil_cf_arg_f32(rs3);
	shil_cf_arg_f32(rs2);
	shil_cf_arg_f32(rs1);
	shil_cf(f1);
	shil_cf_rv_f32(rd);
)
shil_opc_end()

//shop_fsrra
shil_opc(fsrra)
UN_OP_F(1/sqrtf)
shil_opc_end()


//shop_fsca
shil_opc(fsca)

shil_canonical
(
void,fsca_native,(float* fd,u32 fixed),

	u32 pi_index=fixed&0xFFFF;

	float rads=pi_index/(65536.0f/2)*(3.14159265f)/*pi*/;

	fd[0] = sinf(rads);
	fd[1] = cosf(rads);
)
shil_canonical
(
void,fsca_table,(float* fd,u32 fixed),

	u32 pi_index=fixed&0xFFFF;

	fd[0] = sin_table[pi_index].u[0];
	fd[1] = sin_table[pi_index].u[1];
)

shil_compile
(
	shil_cf_arg_u32(rs1);
	shil_cf_arg_ptr(rd);
	shil_cf(fsca_impl);
)

shil_opc_end()


//shop_fseteq
shil_opc(fseteq)
BIN_OP_FU(==)
shil_opc_end()

//shop_fsetgt
shil_opc(fsetgt)
BIN_OP_FU(>)
shil_opc_end()



//shop_frswap
shil_opc(frswap)
shil_canonical
(
void,f1,(u64* fd1, u64* fd2, const u64* fs1, const u64* fs2),

	u64 temp;
	for (int i=0;i<8;i++)
	{
		temp=fs1[i];
		fd1[i]=fs2[i];
		fd2[i]=temp;
	}
)
shil_compile
(
	shil_cf_arg_ptr(rs2);
	shil_cf_arg_ptr(rs1);
	shil_cf_arg_ptr(rd);
	shil_cf_arg_ptr(rd2);
	shil_cf(f1);
)
shil_opc_end()

//shop_xtrct
shil_opc(xtrct)
shil_canonical
(
u32,f1,(u32 r1, u32 r2),
	return (r1 >> 16) | (r2 << 16);
)
shil_compile
(
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
	shil_cf_rv_u32(rd);
)
shil_opc_end()

// shop_illegal: illegal instruction
shil_opc(illegal)
shil_canonical
(
void,f1,(u32 epc, u32 delaySlot),
	if (delaySlot == 1)
		Do_Exception(epc - 2, Sh4Ex_SlotIllegalInstr);
	else
		Do_Exception(epc, Sh4Ex_IllegalInstr);
)
shil_compile
(
	shil_cf_arg_u32(rs2);
	shil_cf_arg_u32(rs1);
	shil_cf(f1);
)
shil_opc_end()

SHIL_END


//undefine stuff
#undef SHIL_MODE

#undef SHIL_START
#undef SHIL_END

#undef shil_opc
#undef shil_opc_end

#undef shil_canonical
#undef shil_compile


#undef BIN_OP_I

#undef BIN_OP_I2
#undef BIN_OP_I3
#undef BIN_OP_I4
	
#undef BIN_OP_F

#undef UN_OP_I
#undef UN_OP_F
#undef BIN_OP_FU
#undef shil_recimp
