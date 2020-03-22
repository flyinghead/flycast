#define XBYAK_NO_OP_NAMES
#include <xbyak/xbyak.h>
#include <cybozu/inttype.hpp>
#include <cybozu/test.hpp>

using namespace Xbyak;
using namespace Xbyak::util;

#ifdef XBYAK64
const struct Ptn {
	const Reg8 *reg8;
	Reg16 reg16;
	Reg32 reg32;
	Reg64 reg64;
} tbl[] = {
	{ &al, ax, eax, rax },
	{ &bl, bx, ebx, rbx },
	{ &cl, cx, ecx, rcx },
	{ &dl, dx, edx, rdx },
	{ &sil, si, esi, rsi },
	{ &dil, di, edi, rdi },
	{ &bpl, bp, ebp, rbp },
	{ &spl, sp, esp, rsp },
	{ &r8b, r8w, r8d, r8 },
	{ &r9b, r9w, r9d, r9 },
	{ &r10b, r10w, r10d, r10 },
	{ &r11b, r11w, r11d, r11 },
	{ &r12b, r12w, r12d, r12 },
	{ &r13b, r13w, r13d, r13 },
	{ &r14b, r14w, r14d, r14 },
	{ &r15b, r15w, r15d, r15 },
};
#else
const struct Ptn {
	const Reg8 *reg8;
	Reg16 reg16;
	Reg32 reg32;
} tbl[] = {
	{ &al, ax, eax },
	{ &bl, bx, ebx },
	{ &cl, cx, ecx },
	{ &dl, dx, edx },
	{ 0, si, esi },
	{ 0, di, edi },
	{ 0, bp, ebp },
	{ 0, sp, esp },
};
#endif

CYBOZU_TEST_AUTO(cvt)
{
	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
		if (tbl[i].reg8) {
			CYBOZU_TEST_ASSERT(tbl[i].reg8->cvt8() == *tbl[i].reg8);
			CYBOZU_TEST_ASSERT(tbl[i].reg8->cvt16() == tbl[i].reg16);
			CYBOZU_TEST_ASSERT(tbl[i].reg8->cvt32() == tbl[i].reg32);
			CYBOZU_TEST_ASSERT(tbl[i].reg16.cvt8() == *tbl[i].reg8);
			CYBOZU_TEST_ASSERT(tbl[i].reg32.cvt8() == *tbl[i].reg8);
		}
		CYBOZU_TEST_ASSERT(tbl[i].reg16.cvt16() == tbl[i].reg16);
		CYBOZU_TEST_ASSERT(tbl[i].reg16.cvt32() == tbl[i].reg32);
		CYBOZU_TEST_ASSERT(tbl[i].reg32.cvt16() == tbl[i].reg16);
		CYBOZU_TEST_ASSERT(tbl[i].reg32.cvt32() == tbl[i].reg32);
#ifdef XBYAK64
		if (tbl[i].reg8) {
			CYBOZU_TEST_ASSERT(tbl[i].reg64.cvt8() == *tbl[i].reg8);
			CYBOZU_TEST_ASSERT(tbl[i].reg8->cvt64() == tbl[i].reg64);
		}
		CYBOZU_TEST_ASSERT(tbl[i].reg64.cvt16() == tbl[i].reg16);
		CYBOZU_TEST_ASSERT(tbl[i].reg64.cvt32() == tbl[i].reg32);
		CYBOZU_TEST_ASSERT(tbl[i].reg64.cvt64() == tbl[i].reg64);
		CYBOZU_TEST_ASSERT(tbl[i].reg16.cvt64() == tbl[i].reg64);
		CYBOZU_TEST_ASSERT(tbl[i].reg32.cvt64() == tbl[i].reg64);
#endif
	}
	{
		const Reg8 errTbl[] = {
			ah, bh, ch, dh
		};
		for (size_t i = 0; i < sizeof(errTbl) / sizeof(errTbl[0]); i++) {
			CYBOZU_TEST_EXCEPTION(errTbl[i].cvt16(), std::exception);
		}
	}
#ifdef XBYAK32
	{
		const Reg16 errTbl[] = {
			si, di, bp, sp
		};
		for (size_t i = 0; i < sizeof(errTbl) / sizeof(errTbl[0]); i++) {
			CYBOZU_TEST_EXCEPTION(errTbl[i].cvt8(), std::exception);
		}
	}
#endif
}

CYBOZU_TEST_AUTO(changeBit)
{
	using namespace Xbyak::util;
#ifdef XBYAK64
	const size_t N = 7;
	const Reg* tbl[][N] = {
		{ &al, &ax, &eax, &rax, &xmm0, &ymm0, &zmm0 },
		{ &cl, &cx, &ecx, &rcx, &xmm1, &ymm1, &zmm1 },
		{ &dl, &dx, &edx, &rdx, &xmm2, &ymm2, &zmm2 },
		{ &bl, &bx, &ebx, &rbx, &xmm3, &ymm3, &zmm3 },
		{ &spl, &sp, &esp, &rsp, &xmm4, &ymm4, &zmm4 },
		{ &bpl, &bp, &ebp, &rbp, &xmm5, &ymm5, &zmm5 },
		{ &sil, &si, &esi, &rsi, &xmm6, &ymm6, &zmm6 },
		{ &dil, &di, &edi, &rdi, &xmm7, &ymm7, &zmm7 },
		{ &r8b, &r8w, &r8d, &r8, &xmm8, &ymm8, &zmm8 },
		{ &r15b, &r15w, &r15d, &r15, &xmm15, &ymm15, &zmm15 },
		{ 0, 0, 0, 0, &xmm16, &ymm16, &zmm16 },
		{ 0, 0, 0, 0, &xmm31, &ymm31, &zmm31 },
	};
	const int bitTbl[N] = { 8, 16, 32, 64, 128, 256, 512 };
#else
	const size_t N = 6;
	const Reg* tbl[][N] = {
		{ &al, &ax, &eax, &xmm0, &ymm0, &zmm0 },
		{ &cl, &cx, &ecx, &xmm1, &ymm1, &zmm1 },
		{ &dl, &dx, &edx, &xmm2, &ymm2, &zmm2 },
		{ &bl, &bx, &ebx, &xmm3, &ymm3, &zmm3 },
		{ 0, &sp, &esp, &xmm4, &ymm4, &zmm4 },
		{ 0, &bp, &ebp, &xmm5, &ymm5, &zmm5 },
		{ 0, &si, &esi, &xmm6, &ymm6, &zmm6 },
		{ 0, &di, &edi,  &xmm7, &ymm7, &zmm7 },
	};
	const int bitTbl[N] = { 8, 16, 32, 128, 256, 512 };
#endif

	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
		for (size_t j = 0; j < N; j++) {
			const Reg *r1 = tbl[i][j];
			if (r1 == 0) continue;
			for (size_t k = 0; k < N; k++) {
				if (tbl[i][k]) {
					CYBOZU_TEST_ASSERT(*tbl[i][k] == r1->changeBit(bitTbl[k]));
// printf("%s->changeBit(%d)=%s %s\n", r1->toString(), bitTbl[k], r1->changeBit(bitTbl[k]).toString(), tbl[i][k]->toString());
				} else {
					CYBOZU_TEST_EXCEPTION(r1->changeBit(bitTbl[k]), std::exception);
				}
			}
		}
	}
#ifdef XBYAK64
	const Reg8 *special8bitTbl[] = { &ah, &bh, &ch, &dh };
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(special8bitTbl); i++) {
		CYBOZU_TEST_EXCEPTION(special8bitTbl[i]->changeBit(16), std::exception);
	}
#endif
}
