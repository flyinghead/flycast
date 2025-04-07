#include <stdio.h>
#include "xbyak/xbyak.h"
#include <stdlib.h>
#include <string.h>
#include "cybozu/inttype.hpp"
#define NUM_OF_ARRAY(x) (sizeof(x) / sizeof(x[0]))

using namespace Xbyak;

const int bitEnd = 64;

const uint64_t YMM_SAE = 1ULL << 0;
const uint64_t _XMM = 1ULL << 1;
const uint64_t _MEM = 1ULL << 2;
const uint64_t _REG32 = 1ULL << 3;
const uint64_t EAX = 1ULL << 4;
const uint64_t IMM32 = 1ULL << 5;
const uint64_t IMM8 = 1ULL << 6;
const uint64_t _REG8 = 1ULL << 7;
const uint64_t _REG16 = 1ULL << 8;
const uint64_t XMM_K = 1ULL << 9;
const uint64_t YMM_K = 1ULL << 10;
const uint64_t ZMM_K = 1ULL << 11;
const uint64_t AX = 1ULL << 12;
const uint64_t AL = 1ULL << 13;
const uint64_t IMM_1 = 1ULL << 14;
const uint64_t MEM8 = 1ULL << 15;
const uint64_t MEM16 = 1ULL << 16;
const uint64_t MEM32 = 1ULL << 17;
const uint64_t VM32Z = 1ULL << 19;
const uint64_t K_K = 1ULL << 20;
const uint64_t MEM_ONLY_DISP = 1ULL << 21;
const uint64_t VM32X_K = 1ULL << 23;
const uint64_t _YMM = 1ULL << 24;
const uint64_t VM32X_32 = 1ULL << 39;
const uint64_t VM32X_64 = 1ULL << 40;
const uint64_t VM32Y_32 = 1ULL << 41;
const uint64_t VM32Y_64 = 1ULL << 42;
const uint64_t VM32Z_K = 1ULL << 32;
#ifdef XBYAK64
const uint64_t _MEMe = 1ULL << 25;
const uint64_t REG32_2 = 1ULL << 26; // r8d, ...
const uint64_t REG16_2 = 1ULL << 27; // r8w, ...
const uint64_t REG8_2 = 1ULL << 28; // r8b, ...
const uint64_t REG8_3 = 1ULL << 29; // spl, ...
const uint64_t _REG64 = 1ULL << 30; // rax, ...
const uint64_t _REG64_2 = 1ULL << 31; // r8, ...
const uint64_t _XMM2 = 1ULL << 33;
const uint64_t _YMM2 = 1ULL << 34;
const uint64_t VM32X = VM32X_32 | VM32X_64;
const uint64_t VM32Y = VM32Y_32 | VM32Y_64;
#else
const uint64_t _MEMe = 0;
const uint64_t REG32_2 = 0;
const uint64_t REG16_2 = 0;
const uint64_t REG8_2 = 0;
const uint64_t REG8_3 = 0;
const uint64_t _REG64 = 0;
const uint64_t _REG64_2 = 0;
const uint64_t _XMM2 = 0;
const uint64_t _YMM2 = 0;
const uint64_t VM32X = VM32X_32;
const uint64_t VM32Y = VM32Y_32;
#endif
const uint64_t REG64 = _REG64 | _REG64_2;
const uint64_t REG32 = _REG32 | REG32_2 | EAX;
const uint64_t REG16 = _REG16 | REG16_2 | AX;
const uint64_t REG32e = REG32 | REG64;
const uint64_t REG8 = _REG8 | REG8_2|AL;
const uint64_t MEM = _MEM | _MEMe;
const uint64_t MEM64 = 1ULL << 35;
const uint64_t YMM_ER = 1ULL << 36;
const uint64_t VM32Y_K = 1ULL << 37;
const uint64_t IMM_2 = 1ULL << 38;
const uint64_t IMM = IMM_1 | IMM_2;
const uint64_t YMM = _YMM | _YMM2;
const uint64_t K = 1ULL << 43;
const uint64_t _ZMM = 1ULL << 44;
const uint64_t _ZMM2 = 1ULL << 45;
#ifdef XBYAK64
const uint64_t ZMM = _ZMM | _ZMM2;
const uint64_t _YMM3 = 1ULL << 46;
#else
const uint64_t ZMM = _ZMM;
const uint64_t _YMM3 = 0;
#endif
const uint64_t K2 = 1ULL << 47;
const uint64_t ZMM_SAE = 1ULL << 48;
const uint64_t ZMM_ER = 1ULL << 49;
#ifdef XBYAK64
const uint64_t _XMM3 = 1ULL << 50;
#else
const uint64_t _XMM3 = 0;
#endif
const uint64_t XMM = _XMM | _XMM2 | _XMM3;
const uint64_t XMM_SAE = 1ULL << 51;
#ifdef XBYAK64
const uint64_t XMM_KZ = 1ULL << 52;
const uint64_t YMM_KZ = 1ULL << 53;
const uint64_t ZMM_KZ = 1ULL << 54;
#else
const uint64_t XMM_KZ = 0;
const uint64_t YMM_KZ = 0;
const uint64_t ZMM_KZ = 0;
#endif
const uint64_t MEM_K = 1ULL << 55;
const uint64_t M_1to2 = 1ULL << 56;
const uint64_t M_1to4 = 1ULL << 57;
const uint64_t M_1to8 = 1ULL << 58;
const uint64_t M_1to16 = 1ULL << 59;
const uint64_t XMM_ER = 1ULL << 60;
const uint64_t M_xword = 1ULL << 61;
const uint64_t M_yword = 1ULL << 62;
const uint64_t MY_1to4 = 1ULL << 18;

const uint64_t NOPARA = 1ULL << (bitEnd - 1);

class Test {
	Test(const Test&);
	void operator=(const Test&);
	const bool isXbyak_;
	int funcNum_;
	// check all op1, op2, op3
	void put(const std::string& nm, uint64_t op1 = NOPARA, uint64_t op2 = NOPARA, uint64_t op3 = NOPARA, uint64_t op4 = NOPARA) const
	{
		for (int i = 0; i < bitEnd; i++) {
			if ((op1 & (1ULL << i)) == 0) continue;
			for (int j = 0; j < bitEnd; j++) {
				if ((op2 & (1ULL << j)) == 0) continue;
				for (int k = 0; k < bitEnd; k++) {
					if ((op3 & (1ULL << k)) == 0) continue;
					for (int s = 0; s < bitEnd; s++) {
						if ((op4 & (1ULL << s)) == 0) continue;
						printf("%s ", nm.c_str());
						if (isXbyak_) printf("(");
						if (!(op1 & NOPARA)) printf("%s", get(1ULL << i));
						if (!(op2 & NOPARA)) printf(", %s", get(1ULL << j));
						if (!(op3 & NOPARA)) printf(", %s", get(1ULL << k));
						if (!(op4 & NOPARA)) printf(", %s", get(1ULL << s));
						if (isXbyak_) printf("); dump();");
						printf("\n");
					}
				}
			}
		}
	}
	void put(const char *nm, uint64_t op, const char *xbyak, const char *nasm) const
	{
		for (int i = 0; i < bitEnd; i++) {
			if ((op & (1ULL << i)) == 0) continue;
			printf("%s ", nm);
			if (isXbyak_) printf("(");
			if (!(op & NOPARA)) printf("%s", get(1ULL << i));
			printf(", %s", isXbyak_ ? xbyak : nasm);
			if (isXbyak_) printf("); dump();");
			printf("\n");
		}
	}
	void put(const char *nm, const char *xbyak, const char *nasm = 0, uint64_t op = NOPARA) const
	{
		if (nasm == 0) nasm = xbyak;
		for (int i = 0; i < bitEnd; i++) {
			if ((op & (1ULL << i)) == 0) continue;
			printf("%s ", nm);
			if (isXbyak_) printf("(");
			printf("%s ", isXbyak_ ? xbyak : nasm);
			if (!(op & NOPARA)) printf(", %s", get(1ULL << i));
			if (isXbyak_) printf("); dump();");
			printf("\n");
		}
	}
	const char *get(uint64_t type) const
	{
		int idx = (rand() / 31) & 7;
		switch (type) {
		case _XMM:
			{
				static const char tbl[][6] = {
					"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
				};
				return tbl[idx];
			}
		case _YMM:
			{
				static const char tbl[][6] = {
					"ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7"
				};
				return tbl[idx];
			}
		case _ZMM:
			{
				static const char tbl[][6] = {
					"zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7"
				};
				return tbl[idx];
			}
#ifdef XBYAK64
		case _XMM2:
			{
				static const char tbl[][6] = {
					"xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
				};
				return tbl[idx];
			}
		case _XMM3:
			{
				static const char tbl[][6] = {
					"xmm16", "xmm17", "xmm18", "xmm19", "xmm20", "xmm21", "xmm22", "xmm23"
				};
				return tbl[idx];
			}
		case _YMM2:
			{
				static const char tbl[][6] = {
					"ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15",
				};
				return tbl[idx];
			}
		case _YMM3:
			{
				static const char tbl[][6] = {
					"ymm16", "ymm17", "ymm18", "ymm19", "ymm20", "ymm21", "ymm22", "ymm23",
				};
				return tbl[idx];
			}
		case _ZMM2:
			{
				static const char tbl[][6] = {
					"zmm8", "zmm9", "zmm10", "zmm11", "zmm28", "zmm29", "zmm30", "zmm31",
				};
				return tbl[idx];
			}
#endif
		case _MEM:
			return isXbyak_ ? "ptr[eax+ecx+64]" : "[eax+ecx+64]"; // QQQ
//			return isXbyak_ ? "ptr[eax+ecx+6]" : "[eax+ecx+6]";
		case _MEMe:
			{
				static int ccc = 1;
#ifdef USE_YASM
				ccc++;
#endif
				if (ccc & 1) {
					return isXbyak_ ? "ptr[rdx+r15+0x12]" : "[rdx+r15+0x12]";
				} else {
					return isXbyak_ ? "ptr[rip - 0x13456+1-3]" : "[rip - 0x13456+1-3]";
				}
			}
		case MEM8:
			return "byte [eax+edx]";
		case MEM16:
			return "word [esi]";
		case MEM32:
			return "dword [eax+64]";
		case MEM64:
			return "qword [rax+64]";
		case MEM_ONLY_DISP:
			return isXbyak_ ? "ptr[(void*)0x123]" : "[0x123]";
		case _REG16: // not ax
			{
				static const char Reg16Tbl[][4] = {
					"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
				};
				return Reg16Tbl[(idx % 7) + 1];
			}
		case _REG8: // not al
			{
				static const char Reg8Tbl[][4] = {
#ifdef XBYAK64 // QQQ
					"al", "cl", "dl", "bl", "al", "cl", "dl", "bl"
#else
					"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
#endif
				};
				return Reg8Tbl[(idx % 7) + 1];
			}
		case _REG32: // not eax
			{
				static const char Reg32Tbl[][4] = {
					"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
				};
				return Reg32Tbl[(idx % 7) + 1];
			}
#ifdef XBYAK64
		case _REG64: // not rax
			{
				static const char Reg64Tbl[][4] = {
					"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"
				};
				return Reg64Tbl[(idx % 7) + 1];
			}
		case _REG64_2:
			{
				static const char Reg64_2Tbl[][4] = {
					"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
				};
				return Reg64_2Tbl[idx];
			}
		case REG32_2:
			{
				static const char Reg32eTbl[][5] = {
					"r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
				};
				return Reg32eTbl[idx];
			}
		case REG16_2:
			{
				static const char Reg16eTbl[][5] = {
					"r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
				};
				return Reg16eTbl[idx];
			}
		case REG8_2:
			{
				static const char Reg8_2Tbl[][5] = {
					"r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
				};
				return Reg8_2Tbl[idx];
			}
		case REG8_3:
			{
				static const char Reg8_3Tbl[][5] = {
					"spl", "bpl", "sil", "dil", "spl", "bpl", "sil", "dil"
				};
				return Reg8_3Tbl[idx];
			}
#endif
		case EAX:
			return "eax";
		case AX:
			return "ax";
		case AL:
			return "al";
		case K_K:
			return isXbyak_ ? "k5 | k3" : "k5{k3}";
		case IMM32:
			return isXbyak_ ? "12345678" : "dword 12345678";
		case IMM8:
			return isXbyak_ ? "4" : "byte 4";
		case IMM_1:
			return "4";
		case IMM_2:
			return isXbyak_ ? "0xda" : "0xda";
		case VM32X_32:
			return isXbyak_ ? "ptr [ebp+64+xmm1*8]" : "[ebp+64+xmm1*8]";
		case VM32X_64:
			return isXbyak_ ? "ptr [rax+64+xmm13*2]" : "[rax+64+xmm13*2]";
		case VM32Y_32:
			return isXbyak_ ? "ptr [ymm4]" : "[ymm4]";
		case VM32Y_64:
			return isXbyak_ ? "ptr [64+ymm13*2+r13]" : "[64+ymm13*2+r13]";
		case VM32X_K:
			return isXbyak_ ? "ptr [64+xmm13*2+r13] | k6" : "[64+xmm13*2+r13]{k6}";
		case VM32Y_K:
			return isXbyak_ ? "ptr [64+ymm13*2+r13] | k6" : "[64+ymm13*2+r13]{k6}";
		case VM32Z_K:
			if (idx & 1) return isXbyak_ ? "ptr [64+zmm10*8+r9] | k6" : "[64+zmm10*8+r9]{k6}";
			return isXbyak_ ? "ptr [64+zmm30*2+r13] | k6" : "[64+zmm30*2+r13]{k6}";
		case VM32Z:
			return isXbyak_ ? "ptr [64+zmm13*2+rcx]" : "[64+zmm13*2+rcx]";
		case M_1to2: return isXbyak_ ? "ptr_b [eax+32]" : "[eax+32]{1to2}";
		case M_1to4: return isXbyak_ ? "ptr_b [eax+32]" : "[eax+32]{1to4}";
		case M_1to8: return isXbyak_ ? "ptr_b [eax+32]" : "[eax+32]{1to8}";
		case M_1to16: return isXbyak_ ? "ptr_b [eax+32]" : "[eax+32]{1to16}";

		case M_xword: return isXbyak_ ? "ptr [eax+32]" : "oword [eax+32]";
		case M_yword: return isXbyak_ ? "yword [eax+32]" : "yword [eax+32]";
		case MY_1to4: return isXbyak_ ? "yword_b [eax+32]" : "[eax+32]{1to4}";
		case K:
			{
				static const char kTbl[][5] = {
					"k1", "k2", "k3", "k4", "k5", "k6", "k7",
				};
				return kTbl[idx % 7];
			}
		case K2:
			return isXbyak_ ? "k3 | k5" : "k3{k5}";
#ifdef XBYAK64
		case XMM_SAE:
			return isXbyak_ ? "xmm25 | T_sae" : "xmm25, {sae}";
		case YMM_SAE:
			return isXbyak_ ? "ymm25 | T_sae" : "ymm25, {sae}";
		case ZMM_SAE:
			return isXbyak_ ? "zmm25 | T_sae" : "zmm25, {sae}";
		case XMM_ER:
			return isXbyak_ ? "xmm4 | T_rd_sae" : "xmm4, {rd-sae}";
		case YMM_ER:
			return isXbyak_ ? "ymm20 | T_rd_sae" : "ymm20, {rd-sae}";
		case ZMM_ER:
			return isXbyak_ ? "zmm20 | T_rd_sae" : "zmm20, {rd-sae}";
		case XMM_KZ:
			return isXbyak_ ? "xmm5 | k5" : "xmm5{k5}";
		case YMM_KZ:
			return isXbyak_ ? "ymm2 |k3|T_z" : "ymm2{k3}{z}";
		case ZMM_KZ:
			return isXbyak_ ? "zmm7|k1" : "zmm7{k1}";
		case MEM_K:
			return isXbyak_ ? "ptr [rax] | k1" : "[rax]{k1}";
#else
		case XMM_SAE:
			return isXbyak_ ? "xmm5 | T_sae" : "xmm5, {sae}";
		case YMM_SAE:
			return isXbyak_ ? "ymm5 | T_sae" : "ymm5, {sae}";
		case ZMM_SAE:
			return isXbyak_ ? "zmm5 | T_sae" : "zmm5, {sae}";
		case XMM_ER:
			return isXbyak_ ? "xmm30 | T_rd_sae" : "xmm30, {rd-sae}";
		case YMM_ER:
			return isXbyak_ ? "ymm2 | T_rd_sae" : "ymm2, {rd-sae}";
		case ZMM_ER:
			return isXbyak_ ? "zmm2 | T_rd_sae" : "zmm2, {rd-sae}";
		case MEM_K:
			return isXbyak_ ? "ptr [eax] | k1" : "[eax]{k1}";
#endif
		case XMM_K:
			return isXbyak_ ? "xmm5 | k7" : "xmm5{k7}";
		case YMM_K:
			return isXbyak_ ? "ymm5 | k4" : "ymm5{k4}";
		case ZMM_K:
			return isXbyak_ ? "zmm5 | k3" : "zmm5{k3}";
		}
		return 0;
	}
public:
	Test(bool isXbyak)
		: isXbyak_(isXbyak)
		, funcNum_(1)
	{
		if (!isXbyak_) return;
		printf("%s",
			"    void gen0()\n"
			"    {\n");
	}
	/*
		gcc and vc give up to compile this source,
		so I split functions.
	*/
	void separateFunc()
	{
		if (!isXbyak_) return;
		printf(
			"    }\n"
			"    void gen%d()\n"
			"    {\n", funcNum_++);
	}
	~Test()
	{
		if (!isXbyak_) return;
		printf("%s",
			"    }\n"
			"    void gen()\n"
			"    {\n");
		for (int i = 0; i < funcNum_; i++) {
			printf(
			"        gen%d();\n", i);
		}
		printf(
			"    }\n");
	}
	void put()
	{
		putAVX512();
	}
	void putOpmask()
	{
		{
			const char *tbl[] = {
				"kadd",
				"kand",
				"kandn",
				"kor",
				"kxnor",
				"kxor",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				std::string name = tbl[i];
				put(name + "b", K, K, K);
				put(name + "w", K, K, K);
				put(name + "q", K, K, K);
				put(name + "d", K, K, K);
			}
			put("kunpckbw", K, K, K);
			put("kunpckwd", K, K, K);
			put("kunpckdq", K, K, K);
		}
		{
			const char *tbl[] = {
				"knot",
				"kortest",
				"ktest",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				std::string name = tbl[i];
				put(name + "b", K, K);
				put(name + "w", K, K);
				put(name + "q", K, K);
				put(name + "d", K, K);
			}
		}
		{
			const char *tbl[] = {
				"kshiftl",
				"kshiftr",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				std::string name = tbl[i];
				put(name + "b", K, K, IMM8);
				put(name + "w", K, K, IMM8);
				put(name + "q", K, K, IMM8);
				put(name + "d", K, K, IMM8);
			}
		}
		put("kmovw", K, K | MEM | REG32);
		put("kmovq", K, K | MEM);
		put("kmovb", K, K | MEM | REG32);
		put("kmovd", K, K | MEM | REG32);

		put("kmovw", MEM | REG32, K);
		put("kmovq", MEM, K);
		put("kmovb", MEM | REG32, K);
		put("kmovd", MEM | REG32, K);
#ifdef XBYAK64
		put("kmovq", K, REG64);
		put("kmovq", REG64, K);
#endif
	}
	void put_vaddpd(const char *r1, const char *r2, const char *r3, int kIdx = 0, bool z = false, int sae = 0)
	{
		std::string modifier;
		char pk[16] = "";
		const char *pz = "";
		const char *saeTblXbyak[] = { "", "|T_rn_sae", "|T_rd_sae", "|T_ru_sae", "|T_rz_sae" };
		const char *saeTblNASM[] = { "", ",{rn-sae}", ",{rd-sae}", ",{ru-sae}", ",{rz-sae}" };
		if (isXbyak_) {
			if (kIdx) CYBOZU_SNPRINTF(pk, sizeof(pk), "|k%d", kIdx);
			if (z) pz = "|T_z";
			printf("vaddpd(%s%s%s, %s, %s%s); dump();\n", r1, pk, pz, r2, r3, saeTblXbyak[sae]);
		} else {
			if (kIdx) CYBOZU_SNPRINTF(pk, sizeof(pk), "{k%d}", kIdx);
			if (z && kIdx) pz = "{z}";
			printf("vaddpd %s%s%s, %s, %s%s\n", r1, pk, pz, r2, r3, saeTblNASM[sae]);
		}
	}
	void putCombi()
	{
		const char *xTbl[] = {
			"xmm2",
#ifdef XBYAK64
			"xmm8", "xmm31"
#else
			"xmm5", "xmm6"
#endif
		};
		const char *yTbl[] = {
			"ymm0",
#ifdef XBYAK64
			"ymm15", "ymm31"
#else
			"ymm4", "ymm2"
#endif
		};
		const char *zTbl[] = {
			"zmm1",
#ifdef XBYAK64
			"zmm9", "zmm30"
#else
			"zmm3", "zmm7"
#endif
		};
		const size_t N = NUM_OF_ARRAY(zTbl);
		for (size_t i = 0; i < N; i++) {
			for (size_t j = 0; j < N; j++) {
				separateFunc();
				for (size_t k = 0; k < N; k++) {
#ifdef XBYAK64
					for (int kIdx = 0; kIdx < 8; kIdx++) {
						put_vaddpd(xTbl[i], xTbl[j], xTbl[k], kIdx);
						put_vaddpd(yTbl[i], yTbl[j], yTbl[k], kIdx);
						for (int z = 0; z < 2; z++) {
							for (int sae = 0; sae < 5; sae++) {
								put_vaddpd(zTbl[i], zTbl[j], zTbl[k], kIdx, z == 1, sae);
							}
						}
					}
#else
					put_vaddpd(xTbl[i], xTbl[j], xTbl[k]);
					put_vaddpd(yTbl[i], yTbl[j], yTbl[k]);
					for (int sae = 0; sae < 5; sae++) {
						put_vaddpd(zTbl[i], zTbl[j], zTbl[k], sae);
					}
#endif
				}
			}
		}
		put("vaddpd", XMM, XMM, _MEM);
		put("vaddpd", YMM, YMM, _MEM);
		put("vaddpd", ZMM, ZMM, _MEM);
	}
	void putCmpK()
	{
		{
			const struct Tbl {
				const char *name;
				bool supportYMM;
			} tbl[] = {
				{ "vcmppd", true },
				{ "vcmpps", true },
				{ "vcmpsd", false },
				{ "vcmpss", false },
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				const Tbl *p = &tbl[i];
				put(p->name, K, XMM, _XMM | MEM, IMM8);
				if (!p->supportYMM) continue;
				put(p->name, K, _YMM, _YMM | MEM, IMM8);
				put(p->name, K, _ZMM, _ZMM | MEM, IMM8);
			}
			put("vcmppd", K, XMM, M_1to2, IMM8);
			put("vcmppd", K, YMM, M_1to4, IMM8);
			put("vcmppd", K, ZMM, M_1to8, IMM8);

			put("vcmpps", K, XMM, M_1to4, IMM8);
			put("vcmpps", K, YMM, M_1to8, IMM8);
			put("vcmpps", K, ZMM, M_1to16, IMM8);
		}
		put("vcmppd", K2, ZMM, ZMM_SAE, IMM);
#ifdef XBYAK64
		{
			const struct Tbl {
				const char *name;
			} tbl[] = {
				{ "vcomisd" },
				{ "vcomiss" },
				{ "vucomisd" },
				{ "vucomiss" },
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				const Tbl *p = &tbl[i];
				put(p->name, XMM, XMM_SAE | XMM | MEM);
			}
		}
		put("vcomiss", XMM, _XMM3 | MEM);
		put("vcomiss", XMM, XMM_SAE);
#endif
	}
	void putBroadcastSub(int idx, int disp)
	{
#ifdef XBYAK64
		const char *a = "rax";
#else
		const char *a = "eax";
#endif
		if (isXbyak_) {
			printf("vaddpd(zmm%d, zmm1, ptr_b[%s+%d]);dump();\n", idx, a, disp);
			printf("vaddpd(ymm%d, ymm1, ptr_b[%s+%d]);dump();\n", idx, a, disp);
			printf("vaddpd(xmm%d, xmm1, ptr_b[%s+%d]);dump();\n", idx, a, disp);
		} else {
			printf("vaddpd zmm%d, zmm1, [%s+%d]{1to8}\n", idx, a, disp);
			printf("vaddpd ymm%d, ymm1, [%s+%d]{1to4}\n", idx, a, disp);
			printf("vaddpd xmm%d, xmm1, [%s+%d]{1to2}\n", idx, a, disp);
		}
	}
	void putBroadcast()
	{
		for (int i = 0; i < 9; i++) {
			putBroadcastSub(0, i);
#ifdef XBYAK64
			putBroadcastSub(10, i);
			putBroadcastSub(20, i);
#endif
		}
		put("vpbroadcastb", XMM_KZ | ZMM_KZ, REG8 | _MEM);
		put("vpbroadcastw", XMM_KZ | ZMM_KZ, REG16 | _MEM);
		put("vpbroadcastd", XMM_KZ | ZMM_KZ, REG32 | _MEM);
#ifdef XBYAK64
		put("vpbroadcastq", XMM_KZ | ZMM_KZ, REG64 | _MEM);
#endif
		{
			const char *tbl[] = {
				"vpbroadcastb",
				"vpbroadcastw",
				"vpbroadcastd",
				"vpbroadcastq",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				put(tbl[i], XMM_KZ | ZMM_KZ, XMM | _MEM);
			}
		}
		put("vbroadcasti32x2", XMM_KZ | YMM_KZ | ZMM_KZ, XMM | _MEM);
		put("vbroadcasti32x4", YMM_KZ | ZMM_KZ, _MEM);
		put("vbroadcasti64x2", YMM_KZ | ZMM_KZ, _MEM);
		put("vbroadcasti32x8", ZMM_KZ, _MEM);
		put("vbroadcasti64x4", ZMM_KZ, _MEM);
	}
	void putMisc1()
	{
		put("vmaskmovps", _XMM, _XMM, MEM);
		put("vmaskmovps", YMM, YMM, MEM);

		put("vmaskmovpd", YMM, YMM, MEM);
		put("vmaskmovpd", _XMM, _XMM, MEM);

		put("vmaskmovps", MEM, _XMM, _XMM);
		put("vmaskmovpd", MEM, _XMM, _XMM);

		put("vbroadcastf128", YMM, MEM);
		put("vbroadcasti128", YMM, MEM);
		put("vbroadcastsd", YMM|_YMM3, XMM|MEM);
		put("vbroadcastsd", ZMM, XMM|MEM);
		{
			const char *tbl[] = {
				"vbroadcastss",
				"vpbroadcastb",
				"vpbroadcastw",
				"vpbroadcastd",
				"vpbroadcastq",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				put(tbl[i], XMM | YMM | ZMM, XMM|MEM);
			}
		}

		put("vinsertf128", YMM, YMM, _XMM | _XMM2 | MEM, IMM8);
		put("vinserti128", YMM, YMM, _XMM | _XMM2  | MEM, IMM8);
		put("vperm2f128", YMM, YMM, YMM | MEM, IMM8);
		put("vperm2i128", YMM, YMM, YMM | MEM, IMM8);

		{
			const char *tbl[] = {
				"vpmaskmovd", "vpmaskmovq"
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				const char *name = tbl[i];
				put(name, _XMM, _XMM, MEM);
				put(name, YMM, YMM, MEM);
				put(name, MEM, _XMM, _XMM);
				put(name, MEM, YMM, YMM);
			}
		}
		{
			const char *tbl[] = {
				"vpermd", "vpermps",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				const char *name = tbl[i];
				put(name, YMM, YMM, YMM | MEM);
			}
		}
		{
			const char *tbl[] = {
				"vpermq", "vpermpd",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				const char *name = tbl[i];
				put(name, YMM, YMM | MEM, IMM8);
			}
		}
		put("vpextrw", REG32e | MEM, XMM, IMM); // nasm is ok, yasm generate redundant code
	}
	void putAVX512_M_X()
	{
		const char *tbl[] = {
			"vmovapd",
			"vmovaps",
			"vmovupd",
			"vmovups",
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const char *name = tbl[i];
			put(name, MEM, ZMM);
			put(name, ZMM, MEM);
#ifdef XBYAK64
			put(name, MEM, XMM);
			put(name, XMM, MEM);
#endif
		}
	}
	void put_vmov()
	{
#ifdef XBYAK64
		put("vmovd", XMM, MEM|REG32);
		put("vmovd", MEM|REG32, XMM);
		put("vmovq", XMM, MEM|REG64|XMM);
		put("vmovq", MEM|REG64|XMM, XMM);
		put("vmovhlps", XMM, _XMM3, _XMM3);
		put("vmovlhps", XMM, _XMM3, _XMM3);
		put("vmovntdqa", XMM|_YMM3|ZMM, MEM);
		put("vmovntdq", MEM, XMM | _YMM3 | ZMM);
		put("vmovntpd", MEM, XMM | _YMM3 | ZMM);
		put("vmovntps", MEM, XMM | _YMM3 | ZMM);

		put("vmovsd", XMM_KZ, XMM, _XMM3);
		put("vmovsd", XMM_KZ, MEM);
		put("vmovsd", MEM_K, XMM);
		put("vmovss", XMM_KZ, XMM, _XMM3);
		put("vmovss", XMM_KZ, MEM);
		put("vmovss", MEM_K, XMM);

		put("vmovshdup", _ZMM, _ZMM);
		put("vmovsldup", _ZMM, _ZMM);


		{
			const char *tbl[] = {
				"valignd",
				"valignq",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				const char *name = tbl[i];
				put(name, XMM_KZ, XMM, _XMM | MEM, IMM);
				put(name, _YMM3, _YMM3, _YMM3 | _MEM, IMM);
				put(name, _ZMM, _ZMM, _ZMM | _MEM, IMM);
			}
		}
		{
			const char tbl[][16] = {
				"vmovhpd",
				"vmovhps",
				"vmovlpd",
				"vmovlps",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				put(tbl[i], XMM, _XMM3, MEM);
				put(tbl[i], MEM, _XMM3);
			}
		}
#endif
	}
	void put512_X_XM()
	{
		const struct Tbl {
			const char *name;
			bool M_X;
		} tbl[] = {
			{ "vmovddup", false },
			{ "vmovdqa32", true },
			{ "vmovdqa64", true },
			{ "vmovdqu8", true },
			{ "vmovdqu16", true },
			{ "vmovdqu32", true },
			{ "vmovdqu64", true },
			{ "vpabsb", false },
			{ "vpabsw", false },
			{ "vpabsd", false },
			{ "vpabsq", false },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			put(p.name, XMM|XMM_KZ, _XMM|MEM);
			put(p.name, _YMM|YMM_KZ, _YMM|MEM);
			put(p.name, _ZMM|ZMM_KZ, _ZMM|MEM);
			if (!p.M_X) continue;
			put(p.name, MEM|MEM_K, XMM);
			put(p.name, MEM|MEM_K, _YMM);
			put(p.name, MEM|MEM_K, _ZMM);
		}
		put("vsqrtpd", XMM_KZ, M_1to2 | _MEM);
		put("vsqrtpd", YMM_KZ, M_1to4 | _MEM);
		put("vsqrtpd", ZMM_KZ, M_1to8 | _MEM);
		put("vsqrtpd", ZMM_KZ, ZMM_ER);

		put("vsqrtps", XMM_KZ, M_1to4 | _MEM);
		put("vsqrtps", YMM_KZ, M_1to8 | _MEM);
		put("vsqrtps", ZMM_KZ, M_1to16 | _MEM);
		put("vsqrtps", ZMM_KZ, ZMM_ER);

		put("vpabsd", ZMM_KZ, M_1to16 | _MEM);
		put("vpabsq", ZMM_KZ, M_1to8 | _MEM);

		put("vbroadcastf32x2", YMM_KZ | ZMM_KZ, XMM | _MEM);
		put("vbroadcastf32x4", YMM_KZ | ZMM_KZ, _MEM);

		put("vbroadcastf64x2", YMM_KZ | ZMM_KZ, _MEM);
		put("vbroadcastf64x4", ZMM_KZ, _MEM);
		put("vbroadcastf32x8", ZMM_KZ, _MEM);
	}
	void put512_X_X_XM()
	{
		const struct Tbl {
			const char *name;
			uint64_t mem;
		} tbl[] = {
			{ "vsqrtsd", MEM },
			{ "vsqrtss", MEM },
			{ "vunpckhpd", M_1to2 },
			{ "vunpckhps", M_1to4 },
			{ "vunpcklpd", M_1to2 },
			{ "vunpcklps", M_1to4 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			put(p.name, XMM_KZ, XMM, _XMM|p.mem);
		}
	}
	void put512_X3()
	{
#ifdef XBYAK64
		const struct Tbl {
			const char *name;
			uint64_t x1;
			uint64_t x2;
			uint64_t xm;
		} tbl[] = {
			{ "vpacksswb", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpacksswb", YMM_KZ, _YMM, _YMM | _MEM },
			{ "vpacksswb", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpackssdw", XMM_KZ, XMM, _XMM | M_1to4 | _MEM },
			{ "vpackssdw", YMM_KZ, _YMM, _YMM | M_1to8 | _MEM },
			{ "vpackssdw", ZMM_KZ, _ZMM, _ZMM | M_1to16 | _MEM },

			{ "vpackusdw", XMM_KZ, XMM, _XMM | M_1to4 | _MEM },
			{ "vpackusdw", YMM_KZ, _YMM, _YMM | M_1to8 | _MEM },
			{ "vpackusdw", ZMM_KZ, _ZMM, _ZMM | M_1to16 | _MEM },

			{ "vpackuswb", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpackuswb", YMM_KZ, _YMM, _YMM | _MEM },
			{ "vpackuswb", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpaddb", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpaddw", XMM_KZ, _XMM, _XMM | _MEM },
			{ "vpaddd", XMM_KZ, _XMM, _XMM | M_1to4 | _MEM },
			{ "vpaddq", ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vpaddsb", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpaddsb", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpaddsw", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpaddsw", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpaddusb", XMM_KZ, XMM, _XMM | MEM },
			{ "vpaddusb", ZMM_KZ, _ZMM, _ZMM | MEM },

			{ "vpaddusw", XMM_KZ, XMM, _XMM | MEM },
			{ "vpaddusw", ZMM_KZ, _ZMM, _ZMM | MEM },

			{ "vpsubb", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpsubw", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpsubd", XMM_KZ, XMM, _XMM | M_1to4 | _MEM },
			{ "vpsubq", ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vpsubsb", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpsubsb", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpsubsw", XMM_KZ, XMM, _XMM | _MEM },
			{ "vpsubsw", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpsubusb", XMM_KZ, XMM, _XMM | MEM },
			{ "vpsubusb", ZMM_KZ, _ZMM, _ZMM | MEM },

			{ "vpsubusw", XMM_KZ, XMM, _XMM | MEM },
			{ "vpsubusw", ZMM_KZ, _ZMM, _ZMM | MEM },

			{ "vpandd", ZMM_KZ, _ZMM, _ZMM | M_1to16 | _MEM },
			{ "vpandq", ZMM_KZ, _ZMM, _ZMM | M_1to8 | _MEM },

			{ "vpandnd", ZMM_KZ, _ZMM, _ZMM | M_1to16 | _MEM },
			{ "vpandnq", ZMM_KZ, _ZMM, _ZMM | M_1to8 | _MEM },

			{ "vpavgb", ZMM_KZ, _ZMM, _ZMM },
			{ "vpavgw", ZMM_KZ, _ZMM, _ZMM },

			{ "vpcmpeqb", K2, _ZMM, _ZMM | _MEM },
			{ "vpcmpeqw", K2, _ZMM, _ZMM | _MEM },
			{ "vpcmpeqd", K2, _ZMM, _ZMM | M_1to16 | _MEM },
			{ "vpcmpeqq", K2, _ZMM, _ZMM | M_1to8 | _MEM },

			{ "vpcmpgtb", K2, _ZMM, _ZMM | _MEM },
			{ "vpcmpgtw", K2, _ZMM, _ZMM | _MEM },
			{ "vpcmpgtd", K2, _ZMM, _ZMM | M_1to16 | _MEM },
			{ "vpcmpgtq", K2, _ZMM, _ZMM | M_1to8 | _MEM },

			{ "vpmaddubsw", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpmaddwd", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpmaxsb", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpmaxsw", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpmaxsd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16 },
			{ "vpmaxsq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8 },

			{ "vpmaxub", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpmaxuw", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpmaxud", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16 },
			{ "vpmaxuq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8 },

			{ "vpminsb", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpminsw", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpminsd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16 },
			{ "vpminsq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8 },

			{ "vpminub", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpminuw", ZMM_KZ, _ZMM, _ZMM | _MEM },
			{ "vpminud", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16 },
			{ "vpminuq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8 },

			{ "vpslldq", XMM, _XMM3 | _MEM, IMM8 },
			{ "vpslldq", _YMM3, _YMM3 | _MEM, IMM8 },
			{ "vpslldq", _ZMM, _ZMM | _MEM, IMM8 },

			{ "vpsrldq", XMM, _XMM3 | _MEM, IMM8 },
			{ "vpsrldq", _YMM3, _YMM3 | _MEM, IMM8 },
			{ "vpsrldq", _ZMM, _ZMM | _MEM, IMM8 },

			{ "vpsraw", XMM_KZ, XMM | _MEM, IMM8 },
			{ "vpsraw", ZMM_KZ, _ZMM | _MEM, IMM8 },

			{ "vpsrad", XMM_KZ, XMM | M_1to4 | _MEM, IMM8 },
			{ "vpsrad", ZMM_KZ, _ZMM | M_1to16 | _MEM, IMM8 },

			{ "vpsraq", XMM, XMM, IMM8 },
			{ "vpsraq", XMM_KZ, XMM | M_1to2 | _MEM, IMM8 },
			{ "vpsraq", ZMM_KZ, _ZMM | M_1to8 | _MEM, IMM8 },

			{ "vpsllw", XMM, _XMM3 | _MEM, IMM8 },
			{ "vpslld", XMM, _XMM3 | _MEM | M_1to4, IMM8 },
			{ "vpsllq", XMM, _XMM3 | _MEM | M_1to2, IMM8 },

			{ "vpsrlw", XMM_KZ, XMM | _MEM, IMM8 },
			{ "vpsrlw", ZMM_KZ, _ZMM | _MEM, IMM8 },

			{ "vpsrld", XMM_KZ, XMM | M_1to4 | _MEM, IMM8 },
			{ "vpsrld", ZMM_KZ, _ZMM | M_1to16 | _MEM, IMM8 },

			{ "vpsrlq", XMM, _XMM3 | _MEM | M_1to2, IMM8 },
			{ "vpsrlq", _ZMM, _ZMM | _MEM | M_1to8, IMM8 },

			{ "vpsravw", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsravw", _ZMM, _ZMM, _MEM },

			{ "vpsravd", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsravd", _ZMM, _ZMM, M_1to16 | _MEM },

			{ "vpsravq", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsravq", _ZMM, _ZMM, M_1to8 | _MEM },

			{ "vpsllvw", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsllvw", _ZMM, _ZMM, _MEM },

			{ "vpsllvd", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsllvd", _ZMM, _ZMM, M_1to16 | _MEM },

			{ "vpsllvq", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsllvq", _ZMM, _ZMM, M_1to8 | _MEM },

			{ "vpsrlvw", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsrlvw", _ZMM, _ZMM, _MEM },

			{ "vpsrlvd", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsrlvd", _ZMM, _ZMM, M_1to16 | _MEM },

			{ "vpsrlvq", XMM_KZ | XMM, _XMM, _XMM | _MEM },
			{ "vpsrlvq", _ZMM, _ZMM, M_1to8 | _MEM },

			{ "vpshufb", XMM | XMM_KZ, _XMM, _XMM | _MEM },
			{ "vpshufb", ZMM_KZ, _ZMM, _MEM },

			{ "vpshufhw", XMM | XMM_KZ, _XMM | _MEM, IMM8 },
			{ "vpshufhw", ZMM_KZ, _MEM, IMM8 },

			{ "vpshuflw", XMM | XMM_KZ, _XMM | _MEM, IMM8 },
			{ "vpshuflw", ZMM_KZ, _MEM, IMM8 },

			{ "vpshufd", XMM | XMM_KZ, _XMM | M_1to4 | _MEM, IMM8 },
			{ "vpshufd", _ZMM | ZMM_KZ, _ZMM | M_1to16 | _MEM, IMM8 },

			{ "vpord", XMM | XMM_KZ, _XMM, _XMM | M_1to4 | _MEM },
			{ "vpord", _ZMM | ZMM_KZ, _ZMM, M_1to16 | _MEM },

			{ "vporq", XMM | XMM_KZ, _XMM, _XMM | M_1to2 | _MEM },
			{ "vporq", _ZMM | ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vpxord", XMM | XMM_KZ, _XMM, _XMM | M_1to4 | _MEM },
			{ "vpxord", _ZMM | ZMM_KZ, _ZMM, M_1to16 | _MEM },

			{ "vpxorq", XMM | XMM_KZ, _XMM, _XMM | M_1to2 | _MEM },
			{ "vpxorq", _ZMM | ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vpsadbw", XMM, _XMM, _XMM | _MEM },
			{ "vpsadbw", _ZMM, _ZMM, _MEM },

			{ "vpmuldq", XMM, _XMM, _XMM | M_1to2 | _MEM },
			{ "vpmuldq", ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vpmulhrsw", XMM, _XMM, _XMM | _MEM },
			{ "vpmulhrsw", ZMM_KZ, _ZMM, _MEM },

			{ "vpmulhuw", XMM, _XMM, _XMM | _MEM },
			{ "vpmulhuw", ZMM_KZ, _ZMM, _MEM },

			{ "vpmulhw", XMM, _XMM, _XMM | _MEM },
			{ "vpmulhw", ZMM_KZ, _ZMM, _MEM },

			{ "vpmullw", XMM, _XMM, _XMM | _MEM },
			{ "vpmullw", ZMM_KZ, _ZMM, _MEM },

			{ "vpmulld", XMM, _XMM, M_1to4 | _MEM },
			{ "vpmulld", ZMM_KZ, _ZMM, M_1to16 | _MEM },

			{ "vpmullq", XMM, _XMM, M_1to2 | _MEM },
			{ "vpmullq", ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vpmuludq", XMM, _XMM, M_1to2 | _MEM },
			{ "vpmuludq", ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vpunpckhbw", XMM, _XMM, _XMM | _MEM },
			{ "vpunpckhbw", _ZMM, _ZMM, _MEM },

			{ "vpunpckhwd", XMM, _XMM, _XMM | _MEM },
			{ "vpunpckhwd", _ZMM, _ZMM, _MEM },

			{ "vpunpckhdq", XMM, _XMM, M_1to4 | _MEM },
			{ "vpunpckhdq", _ZMM, _ZMM, M_1to16 | _MEM },

			{ "vpunpckhqdq", XMM, _XMM, M_1to2 | _MEM },
			{ "vpunpckhqdq", _ZMM, _ZMM, M_1to8 | _MEM },

			{ "vpunpcklbw", XMM, _XMM, _XMM | _MEM },
			{ "vpunpcklbw", _ZMM, _ZMM, _MEM },

			{ "vpunpcklwd", XMM, _XMM, _XMM | _MEM },
			{ "vpunpcklwd", _ZMM, _ZMM, _MEM },

			{ "vpunpckldq", XMM, _XMM, M_1to4 | _MEM },
			{ "vpunpckldq", _ZMM, _ZMM, M_1to16 | _MEM },

			{ "vpunpcklqdq", XMM, _XMM, M_1to2 | _MEM },
			{ "vpunpcklqdq", _ZMM, _ZMM, M_1to8 | _MEM },

			{ "vextractf32x4", _XMM | XMM_KZ | _MEM, _YMM | _ZMM, IMM8 },
			{ "vextractf64x2", _XMM | XMM_KZ | _MEM, _YMM | _ZMM, IMM8 },
			{ "vextractf32x8", _YMM | YMM_KZ | _MEM, _ZMM, IMM8 },
			{ "vextractf64x4", _YMM | YMM_KZ | _MEM, _ZMM, IMM8 },

			{ "vextracti32x4", _XMM | XMM_KZ | _MEM, _YMM | _ZMM, IMM8 },
			{ "vextracti64x2", _XMM | XMM_KZ | _MEM, _YMM | _ZMM, IMM8 },
			{ "vextracti32x8", _YMM | YMM_KZ | _MEM, _ZMM, IMM8 },
			{ "vextracti64x4", _YMM | YMM_KZ | _MEM, _ZMM, IMM8 },

			{ "vextractps", REG32 | _MEM, XMM, IMM8 },

			{ "vpermb", XMM_KZ, _XMM, _XMM | _MEM },
			{ "vpermb", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpermw", XMM_KZ, _XMM, _XMM | _MEM },
			{ "vpermw", ZMM_KZ, _ZMM, _ZMM | _MEM },

			{ "vpermd", YMM_KZ, _YMM, _YMM | M_1to8 | _MEM },
			{ "vpermd", ZMM_KZ, _ZMM, _ZMM | M_1to16 | _MEM },

			{ "vpermilpd", XMM_KZ, _XMM, _XMM | M_1to2 | _MEM },
			{ "vpermilpd", ZMM_KZ, _ZMM, M_1to8 | _MEM },
			{ "vpermilpd", XMM_KZ, M_1to2 | _MEM, IMM8 },
			{ "vpermilpd", ZMM_KZ, M_1to8 | _MEM, IMM8 },

			{ "vpermilps", XMM_KZ, _XMM, _XMM | _MEM | M_1to4 },
			{ "vpermilps", ZMM_KZ, _ZMM, _MEM | M_1to16 },
			{ "vpermilps", XMM_KZ, _MEM | M_1to4 | _MEM, IMM8 },
			{ "vpermilps", ZMM_KZ, _MEM | M_1to16 | _MEM, IMM8 },

			{ "vpermpd", YMM_KZ, _YMM | M_1to4 | _MEM, IMM8 },
			{ "vpermpd", ZMM_KZ, _ZMM | M_1to8 | _MEM, IMM8 },
			{ "vpermpd", YMM_KZ, _YMM, M_1to4 | _MEM },
			{ "vpermpd", ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vpermps", YMM_KZ, _YMM, M_1to8 | _MEM },
			{ "vpermps", ZMM_KZ, _ZMM, M_1to16 | _MEM },

			{ "vpermq", YMM_KZ, _YMM | M_1to4 | _MEM, IMM8 },
			{ "vpermq", ZMM_KZ, _ZMM | M_1to8 | _MEM, IMM8 },
			{ "vpermq", YMM_KZ, _YMM, M_1to4 | _MEM },
			{ "vpermq", ZMM_KZ, _ZMM, M_1to8 | _MEM },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			put(p.name, p.x1, p.x2, p.xm);
		}
#endif
	}
	void put512_X3_I()
	{
		const struct Tbl {
			const char *name;
			uint64_t x1;
			uint64_t x2;
			uint64_t xm;
		} tbl[] = {
#ifdef XBYAK64
			{ "vinsertps", XMM, _XMM, _XMM3 | _MEM },

			{ "vshufpd", XMM_KZ, _XMM, M_1to2 | _MEM },
			{ "vshufpd", ZMM_KZ, _ZMM, M_1to8 | _MEM },

			{ "vshufps", XMM_KZ, _XMM, M_1to4 | _MEM },
			{ "vshufps", ZMM_KZ, _ZMM, M_1to16 | _MEM },

			{ "vinsertf32x4", _YMM | YMM_KZ, _YMM, _XMM | _MEM },
			{ "vinsertf32x4", _ZMM | ZMM_KZ, _ZMM, _XMM | _MEM },

			{ "vinsertf64x2", _YMM | YMM_KZ, _YMM, _XMM | _MEM },
			{ "vinsertf64x2", _ZMM | ZMM_KZ, _ZMM, _XMM | _MEM },

			{ "vinsertf32x8", _ZMM | ZMM_KZ, _ZMM, _YMM | _MEM },
			{ "vinsertf64x4", _ZMM | ZMM_KZ, _ZMM, _YMM | _MEM },

			{ "vinserti32x4", _YMM | YMM_KZ, _YMM, _XMM | _MEM },
			{ "vinserti32x4", _ZMM | ZMM_KZ, _ZMM, _XMM | _MEM },

			{ "vinserti64x2", _YMM | YMM_KZ, _YMM, _XMM | _MEM },
			{ "vinserti64x2", _ZMM | ZMM_KZ, _ZMM, _XMM | _MEM },

			{ "vinserti32x8", _ZMM | ZMM_KZ, _ZMM, _YMM | _MEM },
			{ "vinserti64x4", _ZMM | ZMM_KZ, _ZMM, _YMM | _MEM },
#endif
			{ "vpalignr", ZMM_KZ, _ZMM, _ZMM | _MEM },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			put(p.name, p.x1, p.x2, p.xm, IMM8);
		}
#ifdef XBYAK64
		put("vpextrb", _REG64 | _MEM, XMM, IMM8);
		put("vpextrw", _REG64 | _MEM, XMM, IMM8);
		put("vpextrd", _REG32 | _MEM, XMM, IMM8);
		put("vpextrq", _REG64 | _MEM, XMM, IMM8);
		put("vpinsrb", XMM, _XMM3, _REG32 | _MEM, IMM8);
		put("vpinsrw", XMM, _XMM3, _REG32 | _MEM, IMM8);
		put("vpinsrd", XMM, _XMM3, _REG32 | _MEM, IMM8);
		put("vpinsrq", XMM, _XMM3, _REG64 | _MEM, IMM8);
#endif
	}
	void put512_FMA()
	{
		const struct Tbl {
			const char *name;
			bool supportYMM;
		} tbl[] = {
			{ "vfmadd", true },
			{ "vfmadd", false },
			{ "vfmaddsub", true },
			{ "vfmsubadd", true },
			{ "vfmsub", true },
			{ "vfmsub", false },
			{ "vfnmadd", true },
			{ "vfnmadd", false },
			{ "vfnmsub", true },
			{ "vfnmsub", false },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			const struct Ord {
				const char *name;
			} ord[] = {
				{ "132" },
				{ "213" },
				{ "231" },
			};
			for (size_t j = 0; j < NUM_OF_ARRAY(ord); j++) {
				const char sufTbl[][2][8] = {
					{ "pd", "ps" },
					{ "sd", "ss" },
				};
				for (size_t k = 0; k < 2; k++) {
					const std::string suf = sufTbl[p.supportYMM ? 0 : 1][k];
					uint64_t mem = 0;
					if (suf == "pd") {
						mem = M_1to2;
					} else if (suf == "ps") {
						mem = M_1to4;
					} else {
						mem = XMM_ER;
					}
					std::string name = std::string(p.name) + ord[j].name + suf;
					const char *q = name.c_str();
					put(q, XMM_KZ, _XMM, mem | _MEM);
					if (!p.supportYMM) continue;
					if (suf == "pd") {
						mem = M_1to8;
					} else if (suf == "ps") {
						mem = M_1to16;
					} else {
						mem = XMM_ER;
					}
					put(q, _ZMM, _ZMM, mem | _MEM);
				}
			}
		}
	}
	void put512_Y_XM()
	{
		const struct Tbl {
			const char *name;
			bool all_xmm; // 2nd param
		} tbl[] = {
			{ "vpmovsxbw", false },
			{ "vpmovsxbd", true },
			{ "vpmovsxbq", true },
			{ "vpmovsxwd", false },
			{ "vpmovsxwq", true },
			{ "vpmovsxdq", false },

			{ "vpmovzxbw", false },
			{ "vpmovzxbd", true },
			{ "vpmovzxbq", true },
			{ "vpmovzxwd", false },
			{ "vpmovzxwq", true },
			{ "vpmovzxdq", false },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			const char *name = p.name;
			put(name, XMM_KZ | YMM, _XMM | _MEM);
			if (p.all_xmm) {
				put(name, ZMM, _XMM | _MEM);
			} else {
				put(name, ZMM, YMM | _MEM);
			}
		}
	}
	void put512_AVX1()
	{
#ifdef XBYAK64
		const struct Tbl {
			std::string name;
			bool only_pd_ps;
		} tbl[] = {
			{ "vadd", false },
			{ "vsub", false },
			{ "vmul", false },
			{ "vdiv", false },
			{ "vmax", false },
			{ "vmin", false },
			{ "vand", true },
			{ "vandn", true },
			{ "vor", true },
			{ "vxor", true },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const struct Suf {
				const char *suf;
				bool supportYMM;
			} sufTbl[] = {
				{ "pd", true },
				{ "ps", true },
				{ "sd", false },
				{ "ss", false },
			};
			for (size_t j = 0; j < NUM_OF_ARRAY(sufTbl); j++) {
				if (tbl[i].only_pd_ps && j == 2) break;
				std::string suf = sufTbl[j].suf;
				std::string name = tbl[i].name + suf;
				const char *p = name.c_str();
				uint64_t mem = 0;
				if (suf == "pd") {
					mem = M_1to2;
				} else if (suf == "ps") {
					mem = M_1to4;
				}
				put(p, XMM | XMM_KZ, _XMM, mem | _MEM);
				if (!sufTbl[j].supportYMM) continue;
				mem = 0;
				if (suf == "pd") {
					mem = M_1to8;
				} else if (suf == "ps") {
					mem = M_1to16;
				}
				put(p, _ZMM, _ZMM, mem | _MEM);
			}
		}
		put("vaddss", XMM, _XMM, XMM_ER);
		put("vaddsd", XMM, _XMM, XMM_ER);
#endif
	}
	void putAVX1()
	{
		const struct Tbl {
			const char *name;
			bool only_pd_ps;
		} tbl[] = {
			{ "add", false },
			{ "sub", false },
			{ "mul", false },
			{ "div", false },
			{ "max", false },
			{ "min", false },
			{ "and", true },
			{ "andn", true },
			{ "or", true },
			{ "xor", true },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const struct Suf {
				const char *suf;
				bool supportYMM;
			} suf[] = {
				{ "pd", true },
				{ "ps", true },
				{ "sd", false },
				{ "ss", false },
			};
			for (size_t j = 0; j < NUM_OF_ARRAY(suf); j++) {
				if (tbl[i].only_pd_ps && j == 2) break;
				std::string name = std::string("v") + tbl[i].name + suf[j].suf;
				const char *p = name.c_str();
				put(p, XMM, XMM | MEM);
				put(p, XMM, XMM, XMM | MEM);
				if (!suf[j].supportYMM) continue;
				put(p, YMM, YMM | MEM);
				put(p, YMM, YMM, YMM | MEM);
				put(p, ZMM, ZMM, ZMM | MEM);
			}
		}
	}
	void put512_cvt()
	{
#ifdef XBYAK64
		put("vcvtdq2pd", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtdq2pd", YMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvtdq2pd", ZMM_KZ, _YMM | _MEM | M_1to8);

		put("vcvtdq2ps", XMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvtdq2ps", YMM_KZ, _YMM | _MEM | M_1to8);
		put("vcvtdq2ps", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_ER);

		put("vcvtpd2dq", XMM_KZ, _XMM | M_xword | M_1to2);
		put("vcvtpd2dq", XMM_KZ, _YMM | M_yword | MY_1to4);
		put("vcvtpd2dq", YMM_KZ, ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtpd2ps", XMM_KZ, _XMM | M_xword | M_1to2);
		put("vcvtpd2ps", XMM_KZ, _YMM | M_yword | MY_1to4);
		put("vcvtpd2ps", YMM_KZ, ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtpd2qq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtpd2qq", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vcvtpd2qq", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtpd2udq", XMM_KZ, _XMM | M_xword | M_1to2);
		put("vcvtpd2udq", XMM_KZ, _YMM | M_yword | MY_1to4);
		put("vcvtpd2udq", YMM_KZ, ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtpd2uqq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtpd2uqq", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vcvtpd2uqq", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtph2ps", XMM_KZ, _XMM | _MEM);
		put("vcvtph2ps", YMM_KZ, _XMM | _MEM);
		put("vcvtph2ps", ZMM_KZ, _YMM | _MEM | YMM_SAE);

		put("vcvtps2ph", XMM_KZ | _MEM, _XMM, IMM8);
		put("vcvtps2ph", XMM_KZ | _MEM, _YMM, IMM8);
		put("vcvtps2ph", YMM_KZ | _MEM, _ZMM, IMM8);
		put("vcvtps2ph", YMM_KZ, ZMM_SAE, IMM8);

		put("vcvtps2dq", XMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvtps2dq", YMM_KZ, _YMM | _MEM | M_1to8);
		put("vcvtps2dq", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_ER);

		put("vcvtps2udq", XMM_KZ, _XMM | M_1to4);
		put("vcvtps2udq", YMM_KZ, _YMM | M_1to8);
		put("vcvtps2udq", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_ER);

		put("vcvtps2qq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtps2qq", YMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvtps2qq", ZMM_KZ, _YMM | _MEM | M_1to8 | YMM_ER);

		put("vcvtps2uqq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtps2uqq", YMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvtps2uqq", ZMM_KZ, _YMM | _MEM | M_1to8 | YMM_ER);

		put("vcvtps2pd", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtps2pd", YMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvtps2pd", ZMM_KZ, _YMM | _MEM | M_1to8 | YMM_SAE);

		put("vcvtqq2pd", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtqq2pd", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vcvtqq2pd", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtqq2ps", XMM_KZ, _XMM | M_xword | M_1to2);
		put("vcvtqq2ps", XMM_KZ, _YMM | M_yword | MY_1to4);
		put("vcvtqq2ps", YMM_KZ, ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtsd2si", REG32 | REG64, XMM | _MEM | XMM_ER);

		put("vcvtsd2usi", REG32 | REG64, XMM | _MEM | XMM_ER);

		put("vcvtsd2ss", XMM_KZ, XMM, _XMM3 | _MEM | XMM_ER);

		put("vcvtsi2sd", XMM, _XMM3, REG32 | REG64 | MEM32 | MEM64);
		put("vcvtsi2sd", XMM, XMM_ER, REG64);

		put("vcvtsi2ss", XMM, _XMM3, REG32 | REG64 | MEM32 | MEM64);
		put("vcvtsi2ss", XMM, XMM_ER, REG32 | REG64);

		put("vcvtss2sd", XMM_KZ, XMM, _XMM3 | _MEM | XMM_SAE);

		put("vcvtss2si", REG32 | REG64, XMM | _MEM | XMM_ER);

		put("vcvtss2usi", REG32 | REG64, XMM | _MEM | XMM_ER);

		put("vcvtpd2dq", XMM_KZ, _XMM | M_xword | M_1to2);
		put("vcvtpd2dq", XMM_KZ, _YMM | M_yword | MY_1to4);
		put("vcvtpd2dq", YMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvttpd2qq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvttpd2qq", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vcvttpd2qq", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_SAE);

		put("vcvttpd2udq", XMM_KZ, _XMM | M_xword | M_1to2);
		put("vcvttpd2udq", XMM_KZ, _YMM | M_yword | MY_1to4);
		put("vcvttpd2udq", YMM_KZ, ZMM | _MEM | M_1to8 | ZMM_SAE);

		put("vcvttpd2uqq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvttpd2uqq", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vcvttpd2uqq", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_SAE);

		put("vcvttps2dq", XMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvttps2dq", YMM_KZ, _YMM | _MEM | M_1to8);
		put("vcvttps2dq", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_SAE);

		put("vcvttps2udq", XMM_KZ, _XMM | M_1to4);
		put("vcvttps2udq", YMM_KZ, _YMM | M_1to8);
		put("vcvttps2udq", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_SAE);

		put("vcvttps2qq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvttps2qq", YMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvttps2qq", ZMM_KZ, _YMM | _MEM | M_1to8 | YMM_SAE);

		put("vcvttps2uqq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvttps2uqq", YMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvttps2uqq", ZMM_KZ, _YMM | _MEM | M_1to8 | YMM_SAE);

		put("vcvttsd2si", REG32 | REG64, XMM | _MEM | XMM_SAE);

		put("vcvttsd2usi", REG32 | REG64, XMM | _MEM | XMM_SAE);

		put("vcvttss2si", REG32 | REG64, XMM | _MEM | XMM_SAE);

		put("vcvttss2usi", REG32 | REG64, XMM | _MEM | XMM_SAE);

		put("vcvtudq2pd", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtudq2pd", YMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvtudq2pd", ZMM_KZ, _YMM | _MEM | M_1to8);

		put("vcvtudq2ps", XMM_KZ, _XMM | _MEM | M_1to4);
		put("vcvtudq2ps", YMM_KZ, _YMM | _MEM | M_1to8);
		put("vcvtudq2ps", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_ER);

		put("vcvtuqq2pd", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vcvtuqq2pd", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vcvtuqq2pd", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtuqq2ps", XMM_KZ, _XMM | M_xword | M_1to2);
		put("vcvtuqq2ps", XMM_KZ, _YMM | M_yword | MY_1to4);
		put("vcvtuqq2ps", YMM_KZ, ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vcvtusi2sd", XMM, _XMM3, REG32 | REG64 | MEM32 | MEM64);
		put("vcvtusi2sd", XMM, XMM_ER, REG64);

		put("vcvtusi2ss", XMM, _XMM3, REG32 | REG64 | MEM32 | MEM64);
		put("vcvtusi2ss", XMM, XMM_ER, REG32 | REG64);
#endif
	}
	enum {
		xx_yy_zz,
		xx_yx_zy,
		xx_xy_yz
	};
	void putGather()
	{
#ifdef XBYAK64
		const struct Tbl {
			const char *name;
			int mode;
		} tbl[] = {
			{ "vpgatherdd", xx_yy_zz },
			{ "vpgatherdq", xx_yx_zy },
			{ "vpgatherqd", xx_xy_yz },
			{ "vpgatherqq", xx_yy_zz },
			{ "vgatherdps", xx_yy_zz },
			{ "vgatherdpd", xx_yx_zy },
			{ "vgatherqps", xx_xy_yz },
			{ "vgatherqpd", xx_yy_zz },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			switch (p.mode) {
			case xx_yy_zz:
				put(p.name, XMM_K, VM32X);
				put(p.name, YMM_K, VM32Y);
				put(p.name, ZMM_K, VM32Z);
				break;
			case xx_yx_zy:
				put(p.name, XMM_K, VM32X);
				put(p.name, YMM_K, VM32X);
				put(p.name, ZMM_K, VM32Y);
				break;
			case xx_xy_yz:
				put(p.name, XMM_K, VM32X);
				put(p.name, XMM_K, VM32Y);
				put(p.name, YMM_K, VM32Z);
				break;
			}
		}
#endif
	}
	void putScatter()
	{
#ifdef XBYAK64
		const struct Tbl {
			const char *name;
			int mode;
		} tbl[] = {
			{ "vpscatterdd", xx_yy_zz },
			{ "vpscatterdq", xx_xy_yz },
			{ "vpscatterqd", xx_yx_zy },
			{ "vpscatterqq", xx_yy_zz },

			{ "vscatterdps", xx_yy_zz },
			{ "vscatterdpd", xx_xy_yz },
			{ "vscatterqps", xx_yx_zy },
			{ "vscatterqpd", xx_yy_zz },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			switch (p.mode) {
			case xx_yy_zz:
				put(p.name, VM32X_K, _XMM);
				put(p.name, VM32Y_K, _YMM);
				put(p.name, VM32Z_K, _ZMM);
				break;
			case xx_yx_zy:
				put(p.name, VM32X_K, _XMM);
				put(p.name, VM32Y_K, _XMM);
				put(p.name, VM32Z_K, _YMM);
				break;
			case xx_xy_yz:
				put(p.name, VM32X_K, _XMM);
				put(p.name, VM32X_K, _YMM);
				put(p.name, VM32Y_K, _ZMM);
				break;
			}
		}
#endif
	}
	void putBlend()
	{
		put("vblendmpd", XMM_KZ, _XMM, _XMM | _MEM | M_1to2);
		put("vblendmpd", YMM_KZ, _YMM, _YMM | _MEM | M_1to4);
		put("vblendmpd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8);

		put("vblendmps", XMM_KZ, _XMM, _XMM | _MEM | M_1to4);
		put("vblendmps", YMM_KZ, _YMM, _YMM | _MEM | M_1to8);
		put("vblendmps", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16);

		put("vpblendmb", XMM_KZ, _XMM, _XMM | _MEM);
		put("vpblendmb", YMM_KZ, _YMM, _YMM | _MEM);
		put("vpblendmb", ZMM_KZ, _ZMM, _ZMM | _MEM);

		put("vpblendmb", XMM_KZ, _XMM, _XMM | _MEM);
		put("vpblendmb", YMM_KZ, _YMM, _YMM | _MEM);
		put("vpblendmb", ZMM_KZ, _ZMM, _ZMM | _MEM);

		put("vpblendmd", XMM_KZ, _XMM, _XMM | _MEM | M_1to4);
		put("vpblendmd", YMM_KZ, _YMM, _YMM | _MEM | M_1to8);
		put("vpblendmd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16);

		put("vpblendmq", XMM_KZ, _XMM, _XMM | _MEM | M_1to2);
		put("vpblendmq", YMM_KZ, _YMM, _YMM | _MEM | M_1to4);
		put("vpblendmq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8);
	}
	void putVpcmp()
	{
		const uint64_t b0Tbl[] = { 0, 0, 0 };
		const uint64_t b4Tbl[] = { M_1to4, M_1to8, M_1to16 };
		const uint64_t b2Tbl[] = { M_1to2, M_1to4, M_1to8 };
		const struct Tbl {
			const char *name;
			uint64_t b;
		} tbl[] = {
			{ "vpcmpb", 0 },
			{ "vpcmpub", 0 },
			{ "vpcmpw", 0 },
			{ "vpcmpuw", 0 },
			{ "vpcmpd", M_1to4 },
			{ "vpcmpud", M_1to4 },
			{ "vpcmpq", M_1to2 },
			{ "vpcmpuq", M_1to2 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			const uint64_t *bTbl = p.b == 0 ? b0Tbl : p.b == M_1to4 ? b4Tbl : b2Tbl;
			put(p.name, K_K, _XMM, _XMM | _MEM | bTbl[0], IMM8);
			put(p.name, K_K, _YMM, _YMM | _MEM | bTbl[1], IMM8);
			put(p.name, K_K, _ZMM, _ZMM | _MEM | bTbl[2], IMM8);
		}
	}
	void putVtest()
	{
		const uint64_t b0Tbl[] = { 0, 0, 0 };
		const uint64_t b4Tbl[] = { M_1to4, M_1to8, M_1to16 };
		const uint64_t b2Tbl[] = { M_1to2, M_1to4, M_1to8 };
		const struct Tbl {
			const char *name;
			uint64_t b;
		} tbl[] = {
			{ "vptestmb", 0 },
			{ "vptestmw", 0 },
			{ "vptestmd", M_1to4 },
			{ "vptestmq", M_1to2 },

			{ "vptestnmb", 0 },
			{ "vptestnmw", 0 },
			{ "vptestnmd", M_1to4 },
			{ "vptestnmq", M_1to2 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			const uint64_t *bTbl = p.b == 0 ? b0Tbl : p.b == M_1to4 ? b4Tbl : b2Tbl;
			put(p.name, K_K, _XMM, _XMM | _MEM | bTbl[0]);
			put(p.name, K_K, _YMM, _YMM | _MEM | bTbl[1]);
			put(p.name, K_K, _ZMM, _ZMM | _MEM | bTbl[2]);
		}
	}
	void putCompExp()
	{
		{
			const char *tbl[] = {
				"vcompresspd",
				"vcompressps",
				"vpcompressd",
				"vpcompressq",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				const char *name = tbl[i];
				put(name, XMM_KZ | _MEM, _XMM);
				put(name, YMM_KZ | _MEM, _YMM);
				put(name, ZMM_KZ | _MEM, _ZMM);
			}
		}
		{
			const char *tbl[] = {
				"vexpandpd",
				"vexpandps",
				"vpexpandd",
				"vpexpandq",
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				const char *name = tbl[i];
				put(name, XMM_KZ, _XMM | _MEM);
				put(name, YMM_KZ, _YMM | _MEM);
				put(name, ZMM_KZ, _ZMM | _MEM);
			}
		}
	}
	void putPerm()
	{
		const uint64_t b0Tbl[] = { 0, 0, 0 };
		const uint64_t b4Tbl[] = { M_1to4, M_1to8, M_1to16 };
		const uint64_t b2Tbl[] = { M_1to2, M_1to4, M_1to8 };
		const struct Tbl {
			const char *name;
			uint64_t b;
		} tbl[] = {
			{ "vpermt2b", 0 },
			{ "vpermt2w", 0 },
			{ "vpermt2d", M_1to4 },
			{ "vpermt2q", M_1to2 },
			{ "vpermt2ps", M_1to4 },
			{ "vpermt2pd", M_1to2 },

			{ "vpermi2b", 0 },
			{ "vpermi2w", 0 },
			{ "vpermi2d", M_1to4 },
			{ "vpermi2q", M_1to2 },
			{ "vpermi2ps", M_1to4 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			const uint64_t *bTbl = p.b == 0 ? b0Tbl : p.b == M_1to4 ? b4Tbl : b2Tbl;
			put(p.name, XMM_KZ, _XMM, _XMM | _MEM | bTbl[0]);
			put(p.name, YMM_KZ, _YMM, _YMM | _MEM | bTbl[1]);
			put(p.name, ZMM_KZ, _ZMM, _ZMM | _MEM | bTbl[2]);
		}
	}
	void putShuff()
	{
		put("vshuff32x4", YMM_KZ, _YMM, _YMM | _MEM | M_1to8, IMM8);
		put("vshuff32x4", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16, IMM8);

		put("vshuff64x2", YMM_KZ, _YMM, _YMM | _MEM | M_1to4, IMM8);
		put("vshuff64x2", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8, IMM8);

		put("vshufi32x4", YMM_KZ, _YMM, _YMM | _MEM | M_1to8, IMM8);
		put("vshufi32x4", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16, IMM8);

		put("vshufi64x2", YMM_KZ, _YMM, _YMM | _MEM | M_1to4, IMM8);
		put("vshufi64x2", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8, IMM8);
	}
	void putMov()
	{
		put("vpmovm2b", _XMM | _YMM | _ZMM, K);
		put("vpmovm2w", _XMM | _YMM | _ZMM, K);
		put("vpmovm2d", _XMM | _YMM | _ZMM, K);
		put("vpmovm2q", _XMM | _YMM | _ZMM, K);

		put("vpmovb2m", K, _XMM | _YMM | _ZMM);
		put("vpmovw2m", K, _XMM | _YMM | _ZMM);
		put("vpmovd2m", K, _XMM | _YMM | _ZMM);
		put("vpmovq2m", K, _XMM | _YMM | _ZMM);

		put("vpmovqb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);
		put("vpmovsqb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);
		put("vpmovusqb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);

		put("vpmovqw", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);
		put("vpmovsqw", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);
		put("vpmovusqw", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);

		put("vpmovqd", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovqd", YMM_KZ | _MEM | MEM_K, _ZMM);

		put("vpmovsqd", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovsqd", YMM_KZ | _MEM | MEM_K, _ZMM);

		put("vpmovusqd", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovusqd", YMM_KZ | _MEM | MEM_K, _ZMM);

		put("vpmovdb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);
		put("vpmovsdb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);
		put("vpmovusdb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM | _ZMM);

		put("vpmovdw", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovdw", YMM_KZ | _MEM | MEM_K, _ZMM);

		put("vpmovsdw", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovsdw", YMM_KZ | _MEM | MEM_K, _ZMM);

		put("vpmovusdw", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovusdw", YMM_KZ | _MEM | MEM_K, _ZMM);

		put("vpmovwb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovwb", YMM_KZ | _MEM | MEM_K, _ZMM);

		put("vpmovswb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovswb", YMM_KZ | _MEM | MEM_K, _ZMM);

		put("vpmovuswb", XMM_KZ | _MEM | MEM_K, _XMM | _YMM);
		put("vpmovuswb", YMM_KZ | _MEM | MEM_K, _ZMM);
	}
	void putRot()
	{
		put("vprolvd", XMM_KZ, _XMM, _XMM | _MEM | M_1to4);
		put("vprolvd", YMM_KZ, _YMM, _YMM | _MEM | M_1to8);
		put("vprolvd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16);

		put("vprolvq", XMM_KZ, _XMM, _XMM | _MEM | M_1to2);
		put("vprolvq", YMM_KZ, _YMM, _YMM | _MEM | M_1to4);
		put("vprolvq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8);

		put("vprorvd", XMM_KZ, _XMM, _XMM | _MEM | M_1to4);
		put("vprorvd", YMM_KZ, _YMM, _YMM | _MEM | M_1to8);
		put("vprorvd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16);

		put("vprorvq", XMM_KZ, _XMM, _XMM | _MEM | M_1to2);
		put("vprorvq", YMM_KZ, _YMM, _YMM | _MEM | M_1to4);
		put("vprorvq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8);

		put("vprold", XMM_KZ, _XMM | _MEM | M_1to4, IMM8);
		put("vprold", YMM_KZ, _YMM | _MEM | M_1to8, IMM8);
		put("vprold", ZMM_KZ, _ZMM | _MEM | M_1to16, IMM8);

		put("vprolq", XMM_KZ, _XMM | _MEM | M_1to2, IMM8);
		put("vprolq", YMM_KZ, _YMM | _MEM | M_1to4, IMM8);
		put("vprolq", ZMM_KZ, _ZMM | _MEM | M_1to8, IMM8);

		put("vprord", XMM_KZ, _XMM | _MEM | M_1to4, IMM8);
		put("vprord", YMM_KZ, _YMM | _MEM | M_1to8, IMM8);
		put("vprord", ZMM_KZ, _ZMM | _MEM | M_1to16, IMM8);

		put("vprorq", XMM_KZ, _XMM | _MEM | M_1to2, IMM8);
		put("vprorq", YMM_KZ, _YMM | _MEM | M_1to4, IMM8);
		put("vprorq", ZMM_KZ, _ZMM | _MEM | M_1to8, IMM8);
	}
	void putMisc2()
	{
#ifdef XBYAK64
		put("vpternlogd", XMM_KZ, _XMM, _XMM | _MEM | M_1to4, IMM8);
		put("vpternlogd", YMM_KZ, _YMM, _YMM | _MEM | M_1to8, IMM8);
		put("vpternlogd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16, IMM8);

		put("vpternlogq", XMM_KZ, _XMM, _XMM | _MEM | M_1to2, IMM8);
		put("vpternlogq", YMM_KZ, _YMM, _YMM | _MEM | M_1to4, IMM8);
		put("vpternlogq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8, IMM8);

		put("vgetexppd", XMM_KZ, _XMM | MEM | M_1to2);
		put("vgetexppd", YMM_KZ, _YMM | MEM | M_1to4);
		put("vgetexppd", ZMM_KZ, _ZMM | MEM | M_1to8 | ZMM_SAE);

		put("vgetexpps", XMM_KZ, _XMM | MEM | M_1to4);
		put("vgetexpps", YMM_KZ, _YMM | MEM | M_1to8);
		put("vgetexpps", ZMM_KZ, _ZMM | MEM | M_1to16 | ZMM_SAE);

		put("vgetexpsd", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE);
		put("vgetexpss", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE);

		put("vgetmantpd", XMM_KZ, _XMM | _MEM | M_1to2, IMM8);
		put("vgetmantpd", YMM_KZ, _YMM | _MEM | M_1to4, IMM8);
		put("vgetmantpd", ZMM_KZ, _ZMM | _MEM | M_1to8, IMM8);

		put("vgetmantps", XMM_KZ, _XMM | _MEM | M_1to4, IMM8);
		put("vgetmantps", YMM_KZ, _YMM | _MEM | M_1to8, IMM8);
		put("vgetmantps", ZMM_KZ, _ZMM | _MEM | M_1to16, IMM8);

		put("vgetmantsd", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE, IMM8);
		put("vgetmantss", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE, IMM8);

		put("vfixupimmpd", XMM_KZ, _XMM, _XMM | _MEM | M_1to2, IMM8);
		put("vfixupimmpd", YMM_KZ, _YMM, _YMM | _MEM | M_1to4, IMM8);
		put("vfixupimmpd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8, IMM8);

		put("vfixupimmps", XMM_KZ, _XMM, _XMM | _MEM | M_1to4, IMM8);
		put("vfixupimmps", YMM_KZ, _YMM, _YMM | _MEM | M_1to8, IMM8);
		put("vfixupimmps", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16, IMM8);

		put("vfixupimmsd", XMM_KZ, _XMM, _XMM | _MEM, IMM8);
		put("vfixupimmss", XMM_KZ, _XMM, _XMM | _MEM, IMM8);

		put("vrcp14pd", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vrcp14pd", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vrcp14pd", ZMM_KZ, _ZMM | _MEM | M_1to8);

		put("vrcp14ps", XMM_KZ, _XMM | _MEM | M_1to4);
		put("vrcp14ps", YMM_KZ, _YMM | _MEM | M_1to8);
		put("vrcp14ps", ZMM_KZ, _ZMM | _MEM | M_1to16);

		put("vrcp14sd", XMM_KZ, _XMM, _XMM | _MEM);

		put("vrcp14ss", XMM_KZ, _XMM, _XMM | _MEM);

		put("vrsqrt14pd", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vrsqrt14pd", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vrsqrt14pd", ZMM_KZ, _ZMM | _MEM | M_1to8);

		put("vrsqrt14ps", XMM_KZ, _XMM | _MEM | M_1to4);
		put("vrsqrt14ps", YMM_KZ, _YMM | _MEM | M_1to8);
		put("vrsqrt14ps", ZMM_KZ, _ZMM | _MEM | M_1to16);

		put("vrsqrt14sd", XMM_KZ, _XMM, _XMM | _MEM);

		put("vrsqrt14ss", XMM_KZ, _XMM, _XMM | _MEM);

		put("vrndscalepd", XMM_KZ, _XMM | _MEM | M_1to2, IMM8);
		put("vrndscalepd", YMM_KZ, _YMM | _MEM | M_1to4, IMM8);
		put("vrndscalepd", ZMM_KZ, _ZMM | _MEM | M_1to8, IMM8);
		put("vrndscalepd", ZMM_KZ, _ZMM | ZMM_SAE, IMM8);

		put("vrndscaleps", XMM_KZ, _XMM | _MEM | M_1to4, IMM8);
		put("vrndscaleps", YMM_KZ, _YMM | _MEM | M_1to8, IMM8);
		put("vrndscaleps", ZMM_KZ, _ZMM | _MEM | M_1to16, IMM8);
		put("vrndscaleps", ZMM_KZ, _ZMM | ZMM_SAE, IMM8);

		put("vrndscalesd", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE, IMM8);

		put("vrndscaless", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE, IMM8);

		put("vscalefpd", XMM_KZ, _XMM, _XMM | _MEM | M_1to2);
		put("vscalefpd", YMM_KZ, _YMM, _YMM | _MEM | M_1to4);
		put("vscalefpd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8 | ZMM_ER);

		put("vscalefps", XMM_KZ, _XMM, _XMM | _MEM | M_1to4);
		put("vscalefps", YMM_KZ, _YMM, _YMM | _MEM | M_1to8);
		put("vscalefps", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16 | ZMM_ER);

		put("vscalefsd", XMM_KZ, _XMM, _XMM | _MEM | XMM_ER);
		put("vscalefss", XMM_KZ, _XMM, _XMM | _MEM | XMM_ER);

		put("vdbpsadbw", XMM_KZ, _XMM, _XMM | _MEM, IMM8);
		put("vdbpsadbw", YMM_KZ, _YMM, _YMM | _MEM, IMM8);
		put("vdbpsadbw", ZMM_KZ, _ZMM, _ZMM | _MEM, IMM8);

		put("vpmultishiftqb", XMM_KZ, _XMM, _XMM | _MEM | M_1to2);
		put("vpmultishiftqb", YMM_KZ, _YMM, _YMM | _MEM | M_1to4);
		put("vpmultishiftqb", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8);

		put("vpconflictd", XMM_KZ, _XMM | _MEM | M_1to4);
		put("vpconflictd", YMM_KZ, _YMM | _MEM | M_1to8);
		put("vpconflictd", ZMM_KZ, _ZMM | _MEM | M_1to16);

		put("vpconflictq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vpconflictq", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vpconflictq", ZMM_KZ, _ZMM | _MEM | M_1to8);

		put("vplzcntd", XMM_KZ, _XMM | _MEM | M_1to4);
		put("vplzcntd", YMM_KZ, _YMM | _MEM | M_1to8);
		put("vplzcntd", ZMM_KZ, _ZMM | _MEM | M_1to16);

		put("vplzcntq", XMM_KZ, _XMM | _MEM | M_1to2);
		put("vplzcntq", YMM_KZ, _YMM | _MEM | M_1to4);
		put("vplzcntq", ZMM_KZ, _ZMM | _MEM | M_1to8);

		put("vpbroadcastmb2q", _XMM | _YMM | _ZMM, K);
		put("vpbroadcastmw2d", _XMM | _YMM | _ZMM, K);

		put("vexp2pd", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_SAE);
		put("vexp2ps", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_SAE);

		put("vrcp28pd", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_SAE);
		put("vrcp28ps", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_SAE);
		put("vrcp28sd", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE);
		put("vrcp28ss", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE);

		put("vrsqrt28pd", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_SAE);
		put("vrsqrt28ps", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_SAE);
		put("vrsqrt28sd", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE);
		put("vrsqrt28ss", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE);

		put("vgatherpf0dps", VM32Z_K);
		put("vgatherpf0qps", VM32Z_K);
		put("vgatherpf0dpd", VM32Y_K);
		put("vgatherpf0qpd", VM32Z_K);

		put("vgatherpf1dps", VM32Z_K);
		put("vgatherpf1qps", VM32Z_K);
		put("vgatherpf1dpd", VM32Y_K);
		put("vgatherpf1qpd", VM32Z_K);

		put("vscatterpf0dps", VM32Z_K);
		put("vscatterpf0qps", VM32Z_K);
		put("vscatterpf0dpd", VM32Y_K);
		put("vscatterpf0qpd", VM32Z_K);

		put("vscatterpf1dps", VM32Z_K);
		put("vscatterpf1qps", VM32Z_K);
		put("vscatterpf1dpd", VM32Y_K);
		put("vscatterpf1qpd", VM32Z_K);

		put("vrangepd", XMM_KZ, _XMM, _XMM | _MEM | M_1to2, IMM8);
		put("vrangepd", YMM_KZ, _YMM, _YMM | _MEM | M_1to4, IMM8);
		put("vrangepd", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8 | ZMM_SAE, IMM8);

		put("vrangeps", XMM_KZ, _XMM, _XMM | _MEM | M_1to4, IMM8);
		put("vrangeps", YMM_KZ, _YMM, _YMM | _MEM | M_1to8, IMM8);
		put("vrangeps", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to16 | ZMM_SAE, IMM8);

		put("vrangesd", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE, IMM8);
		put("vrangess", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE, IMM8);

		put("vreducepd", XMM_KZ, _XMM | _MEM | M_1to2, IMM8);
		put("vreducepd", YMM_KZ, _YMM | _MEM | M_1to4, IMM8);
		put("vreducepd", ZMM_KZ, _ZMM | _MEM | M_1to8 | ZMM_SAE, IMM8);

		put("vreduceps", XMM_KZ, _XMM | _MEM | M_1to4, IMM8);
		put("vreduceps", YMM_KZ, _YMM | _MEM | M_1to8, IMM8);
		put("vreduceps", ZMM_KZ, _ZMM | _MEM | M_1to16 | ZMM_SAE, IMM8);

		put("vreducesd", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE, IMM8);
		put("vreducess", XMM_KZ, _XMM, _XMM | _MEM | XMM_SAE, IMM8);

		put("vpmadd52luq", XMM_KZ, _XMM, _XMM | _MEM | M_1to2);
		put("vpmadd52luq", YMM_KZ, _YMM, _YMM | _MEM | M_1to4);
		put("vpmadd52luq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8);

		put("vpmadd52huq", XMM_KZ, _XMM, _XMM | _MEM | M_1to2);
		put("vpmadd52huq", YMM_KZ, _YMM, _YMM | _MEM | M_1to4);
		put("vpmadd52huq", ZMM_KZ, _ZMM, _ZMM | _MEM | M_1to8);
#endif
	}
	void classSubMem(const char *nm, char x, bool broadcast, int size)
	{
		printf("%s ", nm);
		if (isXbyak_) {
			printf("(k5|k3, %cword%s [rax+64], 5);dump();\n", x, broadcast ? "_b" : "");
		} else {
			if (broadcast) {
				int d = x == 'x' ? 128 / size : x == 'y' ? 256 / size : 512 / size;
				printf("k5{k3}, [rax+64]{1to%d}, 5\n", d);
			} else {
				if (x == 'x') x = 'o'; // nasm
				printf("k5{k3}, %cword [rax+64], 5\n", x);
			}
		}
	}
	void putClassSub(const char *name, int size)
	{
		put(name, K_K, _XMM | _YMM | _ZMM, IMM8);
		for (int i = 0; i < 2; i++) {
			classSubMem(name, 'x', i == 0, size);
			classSubMem(name, 'y', i == 0, size);
			classSubMem(name, 'z', i == 0, size);
		}
	}
	void putClass()
	{
#ifdef XBYAK64
		putClassSub("vfpclasspd", 64);
		putClassSub("vfpclassps", 32);
		put("vfpclasssd", K_K, _XMM | _MEM, IMM8);
		put("vfpclassss", K_K, _XMM | _MEM, IMM8);
#endif
	}
	void putMin()
	{
#ifdef XBYAK64
		put("vextractf32x4", XMM_KZ, _YMM, IMM8);
#endif
	}
	void putDisp8N()
	{
		{
			const int tbl[] = {
				-129, -128, -127, 0, 1, 64, 65, 127, 128
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				char xs[128], ns[128];
				int v = tbl[i];
				CYBOZU_SNPRINTF(xs, sizeof(xs), "xmm0, ptr[eax%+d]", v);
				CYBOZU_SNPRINTF(ns, sizeof(ns), "xmm0,    [eax%+d]", v);
				put("vpbroadcastb", xs, ns);
			}
		}
		{
			const int tbl[] = {
				-1024, -512 -256, -128, -64, -32, -16, -8, -4, -2, -1,
				0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512
			};
			for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
				char xs[128], ns[128];
				int v = tbl[i];
				CYBOZU_SNPRINTF(xs, sizeof(xs), "zmm0, zmm1, ptr_b[eax%+d]", v);
				CYBOZU_SNPRINTF(ns, sizeof(ns), "zmm0, zmm1, [eax%+d]{1to16}", v);
				put("vaddps", xs, ns);
			}
		}
#ifdef XBYAK64
		put("vfmadd231ps", "zmm8, zmm31, ptr_b[r14+rbp-0x1e4]", "zmm8, zmm31, [r14+rbp-0x1e4]{1to16}");
#endif
	}
	void putAVX512()
	{
#ifdef MIN_TEST
		putMin();
#else
		putOpmask();
		separateFunc();
		putCombi();
		separateFunc();
		putCmpK();
		separateFunc();
		putBroadcast();
		separateFunc();
		putAVX512_M_X();
		separateFunc();
		put_vmov();
		separateFunc();
		put512_X_XM();
		separateFunc();
		put512_X_X_XM();
		separateFunc();
		put512_X3();
		separateFunc();
		put512_X3_I();
		separateFunc();
		put512_FMA();
		separateFunc();
		put512_Y_XM();
		separateFunc();
		put512_AVX1();
		separateFunc();
		put512_cvt();
		separateFunc();
		putMisc1();
		separateFunc();
		putGather();
		separateFunc();
		putBlend();
		separateFunc();
		putVpcmp();
		separateFunc();
		putVtest();
		separateFunc();
		putCompExp();
		separateFunc();
		putPerm();
		separateFunc();
		putShuff();
		separateFunc();
		putMisc2();
		separateFunc();
		putMov();
		separateFunc();
		putRot();
		separateFunc();
		putScatter();
		separateFunc();
		putClass();
		putDisp8N();
#endif
	}
};

int main(int argc, char *[])
{
	Test test(argc > 1);
	test.put();
}
